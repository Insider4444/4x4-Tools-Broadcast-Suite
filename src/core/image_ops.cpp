#include "image_ops.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <emmintrin.h>

namespace fourbyfour {
namespace {
float sinc(float x) { if (std::abs(x)<1.e-6f) return 1.f; x*=3.141592654f; return std::sin(x)/x; }
struct Tap { int pixel; float weight; };
using Weights = std::vector<std::vector<Tap>>;
Weights weights(int input, int output, float start, float extent) {
    const float scale=extent/output, support=std::max(1.f,scale), radius=3.f*support;
    Weights result(output);
    for(int o=0;o<output;++o) {
        const float center=start+(o+.5f)*scale-.5f;
        float total=0;
        for(int i=static_cast<int>(std::ceil(center-radius));i<=static_cast<int>(std::floor(center+radius));++i) {
            const float d=(center-i)/support;
            if(std::abs(d)>=3) continue;
            const float w=sinc(d)*sinc(d/3);
            result[o].push_back({std::clamp(i,0,input-1),w}); total+=w;
        }
        for(auto& tap:result[o]) tap.weight/=total;
    }
    return result;
}
Weights proxyWeights(int input,int output) {
    Weights result(output);
    const double scale=double(input)/output;
    for(int o=0;o<output;++o) {
        if(output<input) {
            const double begin=o*scale,end=(o+1)*scale;
            for(int p=static_cast<int>(begin);p<std::ceil(end);++p) {
                const float weight=static_cast<float>((std::min(end,double(p+1))-std::max(begin,double(p)))/scale);
                if(weight>0) result[o].push_back({std::min(p,input-1),weight});
            }
        } else {
            const double center=(o+.5)*scale-.5;
            const int p=static_cast<int>(std::floor(center));const float fraction=static_cast<float>(center-p);
            result[o]={{std::clamp(p,0,input-1),1-fraction},{std::clamp(p+1,0,input-1),fraction}};
        }
    }
    return result;
}
}
std::vector<float> resampleProxy(std::span<const float> data,int w,int h,int channels,int ow,int oh) {
    if(w<1||h<1||ow<1||oh<1||w>16384||h>16384||ow>16384||oh>16384||channels<1||channels>4||data.size()!=static_cast<std::size_t>(w)*h*channels)
        throw std::invalid_argument("Invalid proxy dimensions.");
    if(w==ow&&h==oh) return {data.begin(),data.end()};
    const auto wx=proxyWeights(w,ow),wy=proxyWeights(h,oh);
    std::vector<float> temp(static_cast<std::size_t>(ow)*h*channels),result(static_cast<std::size_t>(ow)*oh*channels);
    for(int y=0;y<h;++y)for(int x=0;x<ow;++x) {
        auto* destination=temp.data()+(static_cast<std::size_t>(y)*ow+x)*channels;
        if(channels==3) {
            auto value=_mm_setzero_ps();
            for(const auto& tap:wx[x]) {
                const auto offset=(static_cast<std::size_t>(y)*w+tap.pixel)*3;const auto* source=data.data()+offset;
                const auto pixel=offset+4<=data.size()?_mm_loadu_ps(source):_mm_set_ps(0,source[2],source[1],source[0]);
                value=_mm_add_ps(value,_mm_mul_ps(pixel,_mm_set1_ps(tap.weight)));
            }
            _mm_storel_epi64(reinterpret_cast<__m128i*>(destination),_mm_castps_si128(value));
            _mm_store_ss(destination+2,_mm_shuffle_ps(value,value,_MM_SHUFFLE(2,2,2,2)));
        } else for(const auto& tap:wx[x])for(int c=0;c<channels;++c)
            destination[c]+=data[(static_cast<std::size_t>(y)*w+tap.pixel)*channels+c]*tap.weight;
    }
    const int row=ow*channels;
    for(int y=0;y<oh;++y) {
        auto* destination=result.data()+static_cast<std::size_t>(y)*row;
        for(const auto& tap:wy[y]) {
            const auto* source=temp.data()+static_cast<std::size_t>(tap.pixel)*row;const auto weight=_mm_set1_ps(tap.weight);
            int i=0;for(;i+4<=row;i+=4)_mm_storeu_ps(destination+i,_mm_add_ps(_mm_loadu_ps(destination+i),_mm_mul_ps(_mm_loadu_ps(source+i),weight)));
            for(;i<row;++i)destination[i]+=source[i]*tap.weight;
        }
    }
    return result;
}
std::vector<float> lightingGain(std::span<const float> source,std::span<const float> relit,int w,int h) {
    if(w<1||h<1||source.size()!=static_cast<std::size_t>(w)*h*3||relit.size()!=source.size())
        throw std::invalid_argument("Invalid lighting images.");
    // The model has a low-resolution reconstructed subject. Estimate only its
    // broad lighting field; original eyelashes, fabric and skin texture survive.
    const int gw=std::max(1,(w+15)/16),gh=std::max(1,(h+15)/16);
    const auto a=resampleProxy(source,w,h,3,gw,gh),b=resampleProxy(relit,w,h,3,gw,gh);
    std::vector<float> gain(a.size());
    for(std::size_t i=0;i<gain.size();++i) gain[i]=std::clamp((b[i]+.04f)/(a[i]+.04f),.25f,4.f);
    return resampleProxy(gain,gw,gh,3,w,h);
}
std::vector<float> resample(std::span<const float> data,int w,int h,int channels,int ow,int oh,float left,float top,float cw,float ch) {
    if(w<1||h<1||ow<1||oh<1||w>16384||h>16384||ow>16384||oh>16384||channels<1||channels>4||data.size()!=static_cast<std::size_t>(w)*h*channels)
        throw std::invalid_argument("Invalid resampling dimensions.");
    if(cw==0) cw=static_cast<float>(w); if(ch==0) ch=static_cast<float>(h);
    if(!std::isfinite(left)||!std::isfinite(top)||!std::isfinite(cw)||!std::isfinite(ch)||cw<=0||ch<=0) throw std::invalid_argument("Invalid crop region.");
    if(w==ow&&h==oh&&left==0&&top==0&&cw==w&&ch==h) return {data.begin(),data.end()};
    const auto wx=weights(w,ow,left,cw), wy=weights(h,oh,top,ch);
    std::vector<float> temp(static_cast<std::size_t>(ow)*h*channels), result(static_cast<std::size_t>(ow)*oh*channels);
    for(int y=0;y<h;++y) for(int x=0;x<ow;++x) for(const auto& tap:wx[x]) for(int c=0;c<channels;++c)
        temp[(static_cast<std::size_t>(y)*ow+x)*channels+c]+=data[(static_cast<std::size_t>(y)*w+tap.pixel)*channels+c]*tap.weight;
    for(int y=0;y<oh;++y) for(int x=0;x<ow;++x) for(const auto& tap:wy[y]) for(int c=0;c<channels;++c)
        result[(static_cast<std::size_t>(y)*ow+x)*channels+c]+=temp[(static_cast<std::size_t>(tap.pixel)*ow+x)*channels+c]*tap.weight;
    return result;
}
void refineMatte(std::vector<float>& m,std::span<const float> bgr,int w,int h,float adjustment,float feather) {
    if(m.size()!=static_cast<std::size_t>(w)*h||bgr.size()!=m.size()*3||!std::isfinite(adjustment)||!std::isfinite(feather))
        throw std::invalid_argument("Invalid matte refinement input.");
    // Keep confident pixels and sub-pixel hair coverage. Apply a small joint
    // bilateral pass only on uncertain edges; color differences stop edge bleed.
    const int radius=std::clamp(static_cast<int>(std::ceil(feather)),0,6);
    if(!radius&&adjustment==0)return;
    const auto source=radius?m:std::vector<float>{};
    if(radius) for(int y=0;y<h;++y) for(int x=0;x<w;++x) {
        const auto i=static_cast<std::size_t>(y)*w+x;
        if(source[i]<=.001f||source[i]>=.999f) continue;
        float total=0,value=0;
        for(int dy=-radius;dy<=radius;++dy) for(int dx=-radius;dx<=radius;++dx) {
            const auto j=static_cast<std::size_t>(std::clamp(y+dy,0,h-1))*w+std::clamp(x+dx,0,w-1);
            float difference=0;
            for(int c=0;c<3;++c) { const float d=bgr[i*3+c]-bgr[j*3+c]; difference+=d*d; }
            const float weight=std::exp(-(dx*dx+dy*dy)/(2*std::max(.25f,feather*feather))-difference*35.f);
            total+=weight; value+=weight*source[j];
        }
        m[i]=value/total;
    }
    for(auto& value:m) {
        value=std::clamp(value,0.f,1.f);
        // Bias confidence without converting semi-transparent hair to a binary key.
        value=std::clamp(value+adjustment*4.f*value*(1.f-value),0.f,1.f);
    }
}
Framing subjectCenter(std::span<const float> matte,int w,int h) {
    if(w<1||h<1||matte.size()!=static_cast<std::size_t>(w)*h) throw std::invalid_argument("Invalid framing matte.");
    double mass=0,xsum=0,ysum=0;
    int top=h,bottom=-1;
    for(int y=0;y<h;++y) for(int x=0;x<w;++x) if(matte[static_cast<std::size_t>(y)*w+x]>.6f) { top=std::min(top,y); bottom=std::max(bottom,y); }
    if(bottom<top) return {};
    // Favor the upper subject (head/shoulders) over a chair or an extended arm.
    const int band=top+std::max(1,(bottom-top)*2/5);
    for(int y=top;y<=band;++y) for(int x=0;x<w;++x) {
        const float v=matte[static_cast<std::size_t>(y)*w+x]; const double weight=v*v;
        mass+=weight; xsum+=weight*(x+.5); ysum+=weight*(y+.5);
    }
    if(mass<static_cast<double>(w)*h*.002) return {};
    return {static_cast<float>(xsum/mass/w),static_cast<float>(ysum/mass/h),true};
}
}
