#include "core/audio_pipeline.h"
#include "core/audio_level.h"
#include "core/image_ops.h"
#include "core/lighting_color.h"
#include "core/frame.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace fourbyfour;
void require(bool result,const char* message) {if(!result) throw std::runtime_error(message);}
std::vector<float> tone(int frequency) {
    AudioResampler converter(48000,16000,2);std::vector<float> result;
    for(int i=0;i<48000+200;++i) {
        converter.push({static_cast<float>(std::sin(i*frequency*6.283185307179586/48000)),0});
        std::array<float,2> frame;
        while(converter.take(frame)) {result.push_back(frame[0]);require(frame[1]==0,"Sample-rate conversion crossed stereo channels");}
    }
    return result;
}
int main() {
    try {
        const auto low=tone(1000),high=tone(14000);
        auto rms=[](const auto& a) {double sum=0;for(std::size_t i=200;i<a.size()-200;++i) sum+=double(a[i])*a[i];return std::sqrt(sum/(a.size()-400));};
        require(std::abs(rms(low)-std::sqrt(.5))<.003,"Audio resampler attenuates the voice band");
        require(rms(high)<.001,"Downsampling aliases out-of-band energy into speech");
        AudioResampler converter(44100,48000,1);std::vector<float> impulse;
        for(int i=0;i<5000;++i) {
            converter.push({i==2205?1.f:0.f,0});std::array<float,2> frame;
            while(converter.take(frame)) impulse.push_back(frame[0]);
        }
        require(std::max_element(impulse.begin(),impulse.end())-impulse.begin()==2400,"Resampling shifts audio time");
        converter.reset();std::array<float,2> emptyFrame{};require(!converter.take(emptyFrame),"Reset retained output");
        AudioLevelMatch match(48000);float matched=1;
        for(int i=0;i<48000*3;++i)matched=match.process(.01f,.04f);
        require(std::abs(matched-.5f)<.001f,"Neural output was not matched to the quieter input");
        for(int i=0;i<48000*3;++i)matched=match.process(.04f,.01f);
        require(matched<=1&&matched>.999f,"Input matching amplified noise or failed to release attenuation");
        match.reset();require(match.process(0,0)==1,"Level matching changed silence after reset");
        std::vector<float> constant(19*11*4,.357f);
        const auto resized=resample(constant,19,11,4,7,5);
        for(const float value:resized) require(std::abs(value-.357f)<1.e-5,"Resampling does not preserve constant colors");
        for(const auto dims:{std::array<int,2>{7,5},std::array<int,2>{37,23}}) {
            for(const auto value:resampleProxy(constant,19,11,4,dims[0],dims[1]))
                require(std::abs(value-.357f)<1.e-5,"Proxy resize changes constant colors");
        }
        std::vector<float> checker(64*64*3),lit(checker.size());
        for(int y=0;y<64;++y)for(int x=0;x<64;++x)for(int c=0;c<3;++c) {
            checker[(y*64+x)*3+c]=(x+y)%2?.25f:.5f;
            lit[(y*64+x)*3+c]=.75f; // deliberately textureless model reconstruction
        }
        const auto gains=lightingGain(checker,lit,64,64);
        require(gains[0]>1&&std::abs(checker[0]*gains[0]/(checker[3]*gains[3])-2)<1.e-4,
            "Relighting must change illumination while preserving the source texture contrast");
        std::vector<float> portrait(32*16*3,.25f),warm(portrait.size()),matte(32*16,0.f);
        for(int y=0;y<16;++y)for(int x=0;x<32;++x){const int i=y*32+x;matte[i]=x>=16?1.f:0.f;warm[i*3]=.1f;warm[i*3+1]=.5f;warm[i*3+2]=.9f;}
        const auto neutral=lightingGain(portrait,warm,32,16,matte);
        for(std::size_t i=0;i<matte.size();++i)require(neutral[i*3]==neutral[i*3+1]&&neutral[i*3+1]==neutral[i*3+2],"Relighting transferred reconstructed skin colour");
        for(std::size_t i=0;i<matte.size();++i)if(!matte[i])for(int c=0;c<3;++c)warm[i*3+c]=100;
        require(lightingGain(portrait,warm,32,16,matte)==neutral,"Invalid model background contaminated foreground lighting");
        std::fill(matte.begin(),matte.end(),0.f);for(float value:lightingGain(portrait,warm,32,16,matte))require(value==1.f,"An absent subject changed the lighting");
        require(std::abs(lightingFromLinear(.18f,LightingEncoding::appleLog)-.48827246f)<1.e-6,"Apple Log middle grey is incorrect");
        require(std::abs(lightingToLinear(lightingFromLinear(0,LightingEncoding::appleLog),LightingEncoding::appleLog))<1.e-7,"Apple Log black is incorrect");
        for(auto encoding:{LightingEncoding::rec709,LightingEncoding::appleLog})for(float linear:{.001f,.01f,.18f,.9f,2.f,8.f})
            require(std::abs(lightingToLinear(lightingFromLinear(linear,encoding),encoding)-linear)<5.e-6,"Lighting transfer roundtrip lost scene values");
        std::vector<float> logPixels{lightingFromLinear(.07f,LightingEncoding::appleLog),lightingFromLinear(.12f,LightingEncoding::appleLog),lightingFromLinear(.18f,LightingEncoding::appleLog)};
        const std::vector<float> unitGain(3,1.f),doubleGain(3,2.f);
        require(illuminateSource(logPixels,unitGain,LightingEncoding::appleLog)==logPixels,"Neutral log lighting is not bit-exact");
        const auto brighter=illuminateSource(logPixels,doubleGain,LightingEncoding::appleLog);
        for(int c=0;c<3;++c)require(std::abs(lightingToLinear(brighter[c],LightingEncoding::appleLog)/lightingToLinear(logPixels[c],LightingEncoding::appleLog)-2)<1.e-5,"Apple Log lighting changed colour ratios or exposure");
        const std::array<float,1> halfMatte{.5f};
        const auto edge=illuminateSource(logPixels,doubleGain,LightingEncoding::appleLog,0,0,halfMatte);
        for(int c=0;c<3;++c)require(std::abs(lightingToLinear(edge[c],LightingEncoding::appleLog)/lightingToLinear(logPixels[c],LightingEncoding::appleLog)-1.5f)<1.e-5,"Soft lighting edges were blended in encoded Log RGB");
        // Check the accelerated full-frame path against the reference curve,
        // including shadows, extended range and attenuation/brightening.
        std::vector<float> sweep(4097*3),sweepGains(sweep.size());
        for(std::size_t i=0;i<sweep.size();++i)sweep[i]=-.05f+1.3f*static_cast<float>(i)/(sweep.size()-1);
        for(auto encoding:{LightingEncoding::rec709,LightingEncoding::appleLog})for(float gain:{.125f,.5f,1.f,1.75f,8.f}){
            std::fill(sweepGains.begin(),sweepGains.end(),gain);const auto corrected=illuminateSource(sweep,sweepGains,encoding);
            for(std::size_t i=0;i<sweep.size();++i){const float expected=gain==1?sweep[i]:lightingFromLinear(lightingToLinear(sweep[i],encoding)*gain,encoding);require(std::abs(corrected[i]-expected)<2.e-6f,"Fast lighting transfer exceeds its precision budget");}
        }
        {
            const std::size_t pixels=1920*1080+3;std::vector<float> large(pixels*3),gains(large.size(),1.6f),mask(pixels);
            std::array<std::array<float,3>,17> expected{};
            for(int step=0;step<17;++step)for(int c=0;c<3;++c){const float value=.4f+.1f*c,gain=1+float(step)/16*.6f;expected[step][c]=step?lightingFromLinear(lightingToLinear(value,LightingEncoding::appleLog)*gain,LightingEncoding::appleLog):value;}
            for(std::size_t i=0;i<pixels;++i){mask[i]=float(i%17)/16;for(int c=0;c<3;++c)large[i*3+c]=.4f+.1f*c;}
            const auto corrected=illuminateSource(large,gains,LightingEncoding::appleLog,0,0,mask);
            for(std::size_t i=0;i<pixels;++i)for(int c=0;c<3;++c)require(std::abs(corrected[i*3+c]-expected[i%17][c])<2.e-6f,"Parallel lighting left a gap or changed a pixel");
            gains[4]=std::numeric_limits<float>::quiet_NaN();bool rejected=false;
            try{illuminateSource(large,gains,LightingEncoding::appleLog,0,0,mask);}catch(const std::invalid_argument&){rejected=true;}
            require(rejected,"Invalid lighting did not return safely from its workers");
        }
        std::vector<float> quantized(10009);
        for(std::size_t i=0;i<quantized.size();++i)quantized[i]=static_cast<float>(i)/(quantized.size()-1);
        const auto exact=quantized;quantizeSdrProxy(quantized);
        for(std::size_t i=0;i<quantized.size();++i)require(quantized[i]==std::round(exact[i]*255.f)/255.f,"SIMD proxy quantization changed rounding");
        std::array<float,8> pixels{1.f,2.75f,.25f,.125f,.5f,.2f,.1f,.05f},result{};
        Frame source{pixels.data(),2,1,sizeof(pixels),PixelLayout::argb32f},destination{result.data(),2,1,sizeof(result),PixelLayout::argb32f};
        const auto proxy=toBgrFloat(source);const std::array<float,2> mask{1,.25f};
        applyFloatDelta(source,destination,proxy,proxy,1,false,mask);
        require(!std::memcmp(pixels.data(),result.data(),4*sizeof(float)),"Untouched HDR pixels lost precision");
        for(int c=0;c<4;++c) require(result[4+c]==pixels[4+c]*.25f,"Matte multiplication broke premultiplied alpha");
        // A correction stays additive on HDR source pixels and preserves alpha,
        // including Adobe's alternate channel order and negative row stride.
        for(auto layout:{PixelLayout::argb32f,PixelLayout::bgra32f}) {
            const bool bgra=layout==PixelLayout::bgra32f;
            std::array<float,12> input{},output{};
            for(int p=0;p<3;++p){input[p*4+(bgra?3:0)]=.5f;input[p*4+(bgra?0:3)]=.125f;input[p*4+(bgra?1:2)]=-.125f;input[p*4+(bgra?2:1)]=2.75f;}
            Frame a{input.data()+8,1,3,-4*static_cast<int>(sizeof(float)),layout},b{output.data()+8,1,3,a.stride,layout};
            const auto normalized=toBgrFloat(a);
            require(normalized[0]==.25f&&normalized[1]==0&&normalized[2]==1,"Float proxy channel order, unpremultiplication or SDR clamp changed");
            const auto extended=toBgrFloat(a,false);
            require(extended[0]==.25f&&extended[1]==-.25f&&extended[2]==5.5f,"Colour-aware lighting clipped extended-range source values");
            const std::array<float,9> delta{.25f,-.125f,0,.25f,-.125f,0,.25f,-.125f,0};
            applyModelDelta(a,b,delta,.5f);
            for(int p=0;p<3;++p){require(output[p*4+(bgra?3:0)]==.5f,"Correction changed alpha");require(output[p*4+(bgra?0:3)]==.1875f&&output[p*4+(bgra?1:2)]==-.15625f&&output[p*4+(bgra?2:1)]==2.75f,"Correction clipped HDR or changed its premultiplied strength");}
            input[bgra?0:3]=std::numeric_limits<float>::quiet_NaN();
            input[bgra?1:2]=-0.f;
            const auto unchanged=toBgrFloat(a);applyFloatDelta(a,b,unchanged,unchanged,1);
            require(!std::memcmp(input.data(),output.data(),sizeof(input)),"Identity changed non-finite pixels or signed zero");
        }
        std::vector<float> empty(64*36),solid(empty.size(),1);
        require(!subjectCenter(empty,64,36).found,"Empty matte was framed as a person");
        auto center=subjectCenter(solid,64,36);require(center.found&&std::abs(center.centerX-.5f)<.001,"Subject center is biased horizontally");
        std::cout<<"Audio passband, anti-alias rejection, sample timing, channel isolation, image resampling and premultiplied HDR/matte checks passed.\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
