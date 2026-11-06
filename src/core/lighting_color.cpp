#include "lighting_color.h"
#include <algorithm>
#include <array>
#include <bit>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <thread>

namespace fourbyfour {
namespace {
// Interpolated mantissa tables avoid scalar CRT transcendentals per source
// pixel. Values outside the normal float range use the reference functions.
// Dense sweeps below compare the complete correction against those equations.
struct TransferTables {
    static constexpr int size=4096;
    std::array<float,size+1> logarithm{},exponential{};
    TransferTables(){for(int i=0;i<=size;++i){logarithm[i]=static_cast<float>(std::log2(1.+double(i)/size));exponential[i]=static_cast<float>(std::exp2(double(i)/size));}}
};
const TransferTables& tables(){static const TransferTables value;return value;}
float fastLog2(float x,const TransferTables& t){
    const auto bits=std::bit_cast<std::uint32_t>(x);const unsigned exponent=(bits>>23)&255;
    if(!exponent||exponent==255||x<0)return std::log2(x);
    const unsigned fraction=bits&0x7fffff,index=fraction>>11;
    const float amount=(fraction&2047)*(1.f/2048);
    return static_cast<float>(int(exponent)-127)+t.logarithm[index]+amount*(t.logarithm[index+1]-t.logarithm[index]);
}
float fastExp2(float x,const TransferTables& t){
    if(!(x>=-126&&x<127))return std::exp2(x);
    const int whole=static_cast<int>(std::floor(x));const float position=(x-whole)*TransferTables::size;
    const auto index=static_cast<unsigned>(position);const float amount=position-index;
    return std::bit_cast<float>(static_cast<std::uint32_t>(whole+127)<<23)*(t.exponential[index]+amount*(t.exponential[index+1]-t.exponential[index]));
}
float fastToLinear(float p,LightingEncoding encoding,const TransferTables& t){
    if(encoding==LightingEncoding::rec709)return p<.081f?p/4.5f:fastExp2(fastLog2((p+.099f)/1.099f,t)*(1.f/.45f),t);
    constexpr float r0=-.05641088f,rt=.01f,c=47.28711236f,b=.00964052f,g=.08550479f,d=.69336945f,pt=c*(rt-r0)*(rt-r0);
    return p>=pt?fastExp2((p-d)/g,t)-b:p>=0?std::sqrt(p/c)+r0:r0;
}
float fastFromLinear(float r,LightingEncoding encoding,const TransferTables& t){
    if(encoding==LightingEncoding::rec709)return r<.018f?4.5f*r:1.099f*fastExp2(fastLog2(r,t)*.45f,t)-.099f;
    constexpr float r0=-.05641088f,rt=.01f,c=47.28711236f,b=.00964052f,g=.08550479f,d=.69336945f;
    return r>=rt?g*fastLog2(r+b,t)+d:r>=r0?c*(r-r0)*(r-r0):0;
}
}
// Apple Log Profile's published transfer curve; Apple Log 1 uses BT.2020/D65.
// This implements the equations, and does not distribute an Apple LUT or SDK.
float lightingToLinear(float value,LightingEncoding encoding) {
    const double p=value;
    if(encoding==LightingEncoding::rec709)return static_cast<float>(p<.081?p/4.5:std::pow((p+.099)/1.099,1/.45));
    constexpr double r0=-.05641088,rt=.01,c=47.28711236,b=.00964052,g=.08550479,d=.69336945;
    constexpr double pt=c*(rt-r0)*(rt-r0);
    return static_cast<float>(p>=pt?std::exp2((p-d)/g)-b:p>=0?std::sqrt(p/c)+r0:r0);
}
float lightingFromLinear(float value,LightingEncoding encoding) {
    const double r=value;
    if(encoding==LightingEncoding::rec709)return static_cast<float>(r<.018?4.5*r:1.099*std::pow(r,.45)-.099);
    constexpr double r0=-.05641088,rt=.01,c=47.28711236,b=.00964052,g=.08550479,d=.69336945;
    return static_cast<float>(r>=rt?g*std::log2(r+b)+d:r>=r0?c*(r-r0)*(r-r0):0);
}
std::vector<float> lightingModelProxy(std::span<const float> source,LightingEncoding encoding) {
    if(source.size()%3)throw std::invalid_argument("Invalid lighting colour image.");
    if(encoding==LightingEncoding::rec709){std::vector<float> result(source.begin(),source.end());for(auto& v:result)v=std::clamp(v,0.f,1.f);return result;}
    std::vector<float> result(source.size());const auto& transfer=tables();
    for(std::size_t i=0;i<source.size();i+=3) {
        const double b=fastToLinear(source[i],encoding,transfer),g=fastToLinear(source[i+1],encoding,transfer),r=fastToLinear(source[i+2],encoding,transfer);
        // BT.2020 -> BT.709, both D65. Only the inference proxy is restricted to
        // SDR; original wide-gamut/log pixels remain the source of the result.
        const std::array<double,3> rgb{1.660491*r-.587641*g-.072850*b,
            -.124550*r+1.132900*g-.008349*b,-.018151*r-.100579*g+1.118730*b};
        for(int c=0;c<3;++c)result[i+2-c]=fastFromLinear(static_cast<float>(std::clamp(rgb[c],0.,1.)),LightingEncoding::rec709,transfer);
    }
    return result;
}
std::vector<float> lightingLuminance(std::span<const float> source){
    if(source.size()%3)throw std::invalid_argument("Invalid lighting luminance image.");
    const auto& t=tables();std::vector<float> result(source.size()/3);
    for(std::size_t i=0;i<result.size();++i)result[i]=.0722f*fastToLinear(source[i*3],LightingEncoding::rec709,t)+.7152f*fastToLinear(source[i*3+1],LightingEncoding::rec709,t)+.2126f*fastToLinear(source[i*3+2],LightingEncoding::rec709,t);
    return result;
}
std::vector<float> illuminateSource(std::span<const float> source,std::span<const float> gains,LightingEncoding encoding,float exposure,float warmth,std::span<const float> matte,float mix) {
    if(source.size()%3||source.size()!=gains.size()||(!matte.empty()&&matte.size()!=source.size()/3)||!std::isfinite(exposure)||!std::isfinite(warmth)||!std::isfinite(mix))throw std::invalid_argument("Invalid lighting correction.");
    const float e=std::exp2(std::clamp(exposure,-4.f,4.f)),w=std::clamp(warmth,-100.f,100.f)/100.f;
    const std::array<float,3> tint{std::exp2(-.3f*w),1.f,std::exp2(.3f*w)};
    std::vector<float> result(source.begin(),source.end());const auto& transfer=tables();std::atomic<bool> invalid{};
    const auto pixels=source.size()/3;
    auto correct=[&](std::size_t first,std::size_t last){for(std::size_t pixel=first;pixel<last;++pixel) {
        // Blend the illumination in linear light, including soft matte edges;
        // blending encoded Log RGB would introduce a small colour shift.
        const float strength=std::clamp(mix,0.f,1.f)*(matte.empty()?1.f:std::clamp(matte[pixel],0.f,1.f));
        if(strength==0)continue;
        for(int c=0;c<3;++c){const auto i=pixel*3+c;const float gain=1.f+strength*(gains[i]*e*tint[c]-1.f);
            if(!std::isfinite(gain)||gain<0){invalid.store(true,std::memory_order_relaxed);continue;}
            if(gain!=1)result[i]=fastFromLinear(fastToLinear(source[i],encoding,transfer)*gain,encoding,transfer);
        }
    }};
    // Large-frame colour correction is CPU work independent of Adobe/CUDA.
    // At most three short-lived workers join before this call returns. No
    // threads are started during discovery or left running after a render.
    const unsigned workers=pixels>=1920*1080?std::clamp(std::thread::hardware_concurrency(),1u,4u):1;
    {std::vector<std::jthread> jobs;for(unsigned i=1;i<workers;++i)jobs.emplace_back(correct,pixels*i/workers,pixels*(i+1)/workers);correct(0,pixels/workers);}
    if(invalid.load(std::memory_order_relaxed))throw std::invalid_argument("Invalid lighting gain.");
    return result;
}
}
