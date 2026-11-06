#include "frame.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <emmintrin.h>

namespace fourbyfour {
std::size_t pixelBytes(PixelLayout layout) {
    switch (layout) {
    case PixelLayout::argb16: return 8;
    case PixelLayout::argb32f: case PixelLayout::bgra32f: return 16;
    default: return 4;
    }
}
void validate(const Frame& frame) {
    if (!frame.pixels || frame.width <= 0 || frame.height <= 0 ||
        frame.width > 32768 || frame.height > 32768 ||
        frame.stride == std::numeric_limits<std::ptrdiff_t>::min() ||
        std::abs(frame.stride) < static_cast<std::ptrdiff_t>(frame.width * pixelBytes(frame.layout)))
        throw std::invalid_argument("Invalid or oversized image buffer.");
}
namespace {
int channel(PixelLayout layout, int bgr) {
    return (layout == PixelLayout::bgra8 || layout == PixelLayout::bgra32f) ? bgr : 3 - bgr;
}
float read(const std::byte* pixel, PixelLayout layout, int component) {
    if (layout == PixelLayout::argb16) {
        std::uint16_t value; std::memcpy(&value, pixel + component * 2, 2);
        return value / 32768.0f; // Adobe's 16-bpc white is 32768.
    }
    if (layout == PixelLayout::argb32f || layout == PixelLayout::bgra32f) {
        float value; std::memcpy(&value, pixel + component * 4, 4); return value;
    }
    return std::to_integer<unsigned char>(pixel[component]) / 255.0f;
}
void write(std::byte* pixel, PixelLayout layout, int component, float value) {
    if (layout == PixelLayout::argb16) {
        auto result = static_cast<std::uint16_t>(std::lround(std::clamp(value, 0.0f, 1.0f) * 32768));
        std::memcpy(pixel + component * 2, &result, 2);
    } else if (layout == PixelLayout::argb32f || layout == PixelLayout::bgra32f) {
        std::memcpy(pixel + component * 4, &value, 4);
    } else {
        pixel[component] = static_cast<std::byte>(std::lround(std::clamp(value, 0.0f, 1.0f) * 255));
    }
}
}
std::vector<std::uint8_t> toBgr(const Frame& frame) {
    validate(frame);
    std::vector<std::uint8_t> result(static_cast<std::size_t>(frame.width) * frame.height * 3);
    for (int y = 0; y < frame.height; ++y) {
        auto* row = static_cast<const std::byte*>(frame.pixels) + y * frame.stride;
        for (int x = 0; x < frame.width; ++x) for (int c = 0; c < 3; ++c) {
            float value = read(row + x * pixelBytes(frame.layout), frame.layout, channel(frame.layout, c));
            if (!std::isfinite(value)) value = 0;
            result[(static_cast<std::size_t>(y) * frame.width + x) * 3 + c] =
                static_cast<std::uint8_t>(std::lround(std::clamp(value, 0.0f, 1.0f) * 255));
        }
    }
    return result;
}
void applyBgrDelta(const Frame& input, const Frame& output,
                   std::span<const std::uint8_t> original,
                   std::span<const std::uint8_t> corrected, float strength, bool compare) {
    validate(input); validate(output);
    const auto count = static_cast<std::size_t>(input.width) * input.height * 3;
    if (input.width != output.width || input.height != output.height || input.layout != output.layout ||
        original.size() != count || corrected.size() != count || !std::isfinite(strength))
        throw std::invalid_argument("Mismatched frame buffers or invalid effect strength.");
    strength = std::clamp(strength, 0.0f, 1.0f);
    for (int y = 0; y < input.height; ++y) {
        const auto* src = static_cast<const std::byte*>(input.pixels) + y * input.stride;
        auto* dst = static_cast<std::byte*>(output.pixels) + y * output.stride;
        std::memmove(dst, src, input.width * pixelBytes(input.layout));
        for (int x = 0; x < input.width; ++x) {
            if (strength == 0 || (compare && x < input.width / 2)) continue;
            for (int c = 0; c < 3; ++c) {
                const auto i = (static_cast<std::size_t>(y) * input.width + x) * 3 + c;
                const int delta = int(corrected[i]) - int(original[i]);
                if (!delta) continue;
                const int component = channel(input.layout, c);
                auto* pixel = dst + x * pixelBytes(input.layout);
                const float value = read(pixel, input.layout, component);
                if (std::isfinite(value)) write(pixel, input.layout, component, value + delta * strength / 255.0f);
            }
        }
    }
}
std::vector<float> toBgrFloat(const Frame& frame,bool sdr) {
    validate(frame);
    std::vector<float> result(static_cast<std::size_t>(frame.width) * frame.height * 3);
    const int alpha = frame.layout == PixelLayout::bgra8 || frame.layout == PixelLayout::bgra32f ? 3 : 0;
    if(frame.layout==PixelLayout::bgra32f||frame.layout==PixelLayout::argb32f) {
        const auto zero=_mm_setzero_ps(),one=_mm_set1_ps(1.f),maximum=_mm_set1_ps(std::numeric_limits<float>::max());
        const auto absolute=_mm_castsi128_ps(_mm_set1_epi32(0x7fffffff));
        for(int y=0;y<frame.height;++y)for(int x=0;x<frame.width;++x) {
            const auto* source=reinterpret_cast<const float*>(static_cast<const std::byte*>(frame.pixels)+y*frame.stride)+x*4;
            auto v=_mm_loadu_ps(source);
            if(alpha==0)v=_mm_shuffle_ps(v,v,_MM_SHUFFLE(0,1,2,3));
            const auto a=_mm_shuffle_ps(v,v,_MM_SHUFFLE(3,3,3,3));
            v=_mm_div_ps(v,a);
            const auto valid=_mm_and_ps(_mm_cmpgt_ps(a,_mm_set1_ps(1.e-6f)),_mm_cmple_ps(_mm_and_ps(v,absolute),maximum));
            if(sdr)v=_mm_min_ps(one,_mm_max_ps(zero,v));
            v=_mm_and_ps(valid,v);
            auto* target=result.data()+(static_cast<std::size_t>(y)*frame.width+x)*3;
            _mm_storel_epi64(reinterpret_cast<__m128i*>(target),_mm_castps_si128(v));
            _mm_store_ss(target+2,_mm_shuffle_ps(v,v,_MM_SHUFFLE(2,2,2,2)));
        }
        return result;
    }
    for (int y = 0; y < frame.height; ++y) for (int x = 0; x < frame.width; ++x) {
        const auto* p = static_cast<const std::byte*>(frame.pixels) + y * frame.stride + x * pixelBytes(frame.layout);
        const float a = read(p, frame.layout, alpha);
        for (int c = 0; c < 3; ++c) {
            float v = a > 1.e-6f ? read(p,frame.layout,channel(frame.layout,c)) / a : 0.f;
            result[(static_cast<std::size_t>(y) * frame.width + x) * 3 + c] = std::isfinite(v) ? (sdr?std::clamp(v,0.f,1.f):v) : 0.f;
        }
    }
    return result;
}
void quantizeSdrProxy(std::span<float> values) {
    const auto scale=_mm_set1_ps(255.f),half=_mm_set1_ps(.5f);
    const auto one=_mm_set1_epi32(1);
    std::size_t i=0;
    for(;i+4<=values.size();i+=4) {
        const auto scaled=_mm_mul_ps(_mm_loadu_ps(values.data()+i),scale);
        const auto whole=_mm_cvttps_epi32(scaled);
        const auto increment=_mm_and_si128(_mm_castps_si128(_mm_cmpge_ps(_mm_sub_ps(scaled,_mm_cvtepi32_ps(whole)),half)),one);
        _mm_storeu_ps(values.data()+i,_mm_div_ps(_mm_cvtepi32_ps(_mm_add_epi32(whole,increment)),scale));
    }
    for(;i<values.size();++i) values[i]=std::round(values[i]*255.f)/255.f;
}
namespace {
void applyFloatCorrection(const Frame& input, const Frame& output, std::span<const float> original,
                     std::span<const float> corrected, float strength, bool compare, std::span<const float> mask,bool isDelta) {
    validate(input); validate(output);
    const auto pixels = static_cast<std::size_t>(input.width) * input.height;
    if (input.width != output.width || input.height != output.height || input.layout != output.layout ||
        (!isDelta&&original.size() != pixels * 3) || corrected.size() != pixels * 3 || (!mask.empty() && mask.size() != pixels) || !std::isfinite(strength))
        throw std::invalid_argument("Mismatched floating point image buffers.");
    strength = std::clamp(strength,0.f,1.f);
    const int alpha = input.layout == PixelLayout::bgra8 || input.layout == PixelLayout::bgra32f ? 3 : 0;
    if(input.layout==PixelLayout::bgra32f||input.layout==PixelLayout::argb32f) {
        const auto maximum=_mm_set1_ps(std::numeric_limits<float>::max()),absolute=_mm_castsi128_ps(_mm_set1_epi32(0x7fffffff));
        const auto rgbMask=_mm_castsi128_ps(alpha==3?_mm_set_epi32(0,-1,-1,-1):_mm_set_epi32(-1,-1,-1,0));
        for(int y=0;y<input.height;++y) {
            const auto* src=reinterpret_cast<const float*>(static_cast<const std::byte*>(input.pixels)+y*input.stride);
            auto* dst=reinterpret_cast<float*>(static_cast<std::byte*>(output.pixels)+y*output.stride);
            for(int x=0;x<input.width;++x) {
                const auto source=_mm_loadu_ps(src+x*4);
                if(strength==0||(compare&&x<input.width/2)){_mm_storeu_ps(dst+x*4,source);continue;}
                const auto i=static_cast<std::size_t>(y)*input.width+x,offset=i*3;
                auto load=[&](std::span<const float> values){const auto* p=values.data()+offset;return offset+4<=values.size()?_mm_loadu_ps(p):_mm_set_ps(0,p[2],p[1],p[0]);};
                auto delta=load(corrected);if(!isDelta)delta=_mm_sub_ps(delta,load(original));
                if(alpha==0)delta=_mm_shuffle_ps(delta,delta,_MM_SHUFFLE(0,1,2,3));
                delta=_mm_and_ps(delta,rgbMask);
                const float a=src[x*4+alpha];
                const float visibility=mask.empty()?1.f:1.f+strength*(std::clamp(mask[i],0.f,1.f)-1.f);
                auto value=_mm_mul_ps(_mm_add_ps(source,_mm_mul_ps(_mm_mul_ps(delta,_mm_set1_ps(strength)),_mm_set1_ps(a))),_mm_set1_ps(visibility));
                // Alpha only receives the visibility mask; zero deltas preserve
                // original HDR values, NaN payloads, hidden RGB and signed zero.
                auto valid=_mm_and_ps(_mm_cmple_ps(_mm_and_ps(source,absolute),maximum),_mm_cmple_ps(_mm_and_ps(delta,absolute),maximum));
                if(visibility==1)valid=_mm_and_ps(valid,_mm_cmpneq_ps(delta,_mm_setzero_ps()));
                _mm_storeu_ps(dst+x*4,_mm_or_ps(_mm_and_ps(valid,value),_mm_andnot_ps(valid,source)));
            }
        }
        return;
    }
    for (int y = 0; y < input.height; ++y) {
        const auto* src = static_cast<const std::byte*>(input.pixels) + y * input.stride;
        auto* dst = static_cast<std::byte*>(output.pixels) + y * output.stride;
        std::memmove(dst,src,input.width * pixelBytes(input.layout));
        for (int x = 0; x < input.width; ++x) {
            if (strength == 0 || (compare && x < input.width / 2)) continue;
            auto* p = dst + x * pixelBytes(input.layout);
            const auto i = static_cast<std::size_t>(y) * input.width + x;
            const float a = read(p,input.layout,alpha);
            const float visibility = mask.empty() ? 1.f : 1.f + strength * (std::clamp(mask[i],0.f,1.f) - 1.f);
            for (int c = 0; c < 3; ++c) {
                const int component = channel(input.layout,c);
                const float delta = isDelta?corrected[i*3+c]:corrected[i*3+c] - original[i*3+c];
                if (delta == 0 && visibility == 1) continue;
                const float value = read(p,input.layout,component);
                if (std::isfinite(value) && std::isfinite(delta)) write(p,input.layout,component,(value + delta * strength * a) * visibility);
            }
            if (visibility != 1 && std::isfinite(a)) write(p,input.layout,alpha,a * visibility);
        }
    }
}
}
void applyFloatDelta(const Frame& input,const Frame& output,std::span<const float> original,std::span<const float> corrected,float strength,bool compare,std::span<const float> mask) {
    applyFloatCorrection(input,output,original,corrected,strength,compare,mask,false);
}
void applyModelDelta(const Frame& input,const Frame& output,std::span<const float> delta,float strength,bool compare) {
    applyFloatCorrection(input,output,{},delta,strength,compare,{},true);
}
void showMatte(const Frame& out, std::span<const float> matte) {
    validate(out);
    if (matte.size() != static_cast<std::size_t>(out.width) * out.height) throw std::invalid_argument("Invalid matte size.");
    const int alpha = out.layout == PixelLayout::bgra8 || out.layout == PixelLayout::bgra32f ? 3 : 0;
    for (int y=0;y<out.height;++y) for (int x=0;x<out.width;++x) {
        auto* p=static_cast<std::byte*>(out.pixels)+y*out.stride+x*pixelBytes(out.layout);
        for (int c=0;c<3;++c) write(p,out.layout,channel(out.layout,c),matte[static_cast<std::size_t>(y)*out.width+x]);
        write(p,out.layout,alpha,1.f);
    }
}
std::vector<float> toRgbaFloat(const Frame& frame) {
    validate(frame);
    std::vector<float> result(static_cast<std::size_t>(frame.width)*frame.height*4);
    const int alpha = frame.layout==PixelLayout::bgra8 || frame.layout==PixelLayout::bgra32f ? 3:0;
    for(int y=0;y<frame.height;++y) for(int x=0;x<frame.width;++x) {
        const auto* p=static_cast<const std::byte*>(frame.pixels)+y*frame.stride+x*pixelBytes(frame.layout);
        const auto i=(static_cast<std::size_t>(y)*frame.width+x)*4;
        for(int c=0;c<3;++c) result[i+c]=read(p,frame.layout,channel(frame.layout,2-c));
        result[i+3]=read(p,frame.layout,alpha);
    }
    return result;
}
void fromRgbaFloat(const Frame& out, std::span<const float> rgba) {
    validate(out);
    if(rgba.size()!=static_cast<std::size_t>(out.width)*out.height*4) throw std::invalid_argument("Invalid spatial output size.");
    const int alpha=out.layout==PixelLayout::bgra8 || out.layout==PixelLayout::bgra32f ? 3:0;
    for(int y=0;y<out.height;++y) for(int x=0;x<out.width;++x) {
        auto* p=static_cast<std::byte*>(out.pixels)+y*out.stride+x*pixelBytes(out.layout);
        const auto i=(static_cast<std::size_t>(y)*out.width+x)*4;
        const float a=std::clamp(rgba[i+3],0.f,1.f);
        for(int c=0;c<3;++c) write(p,out.layout,channel(out.layout,2-c),rgba[i+c]);
        write(p,out.layout,alpha,a);
    }
}
}
