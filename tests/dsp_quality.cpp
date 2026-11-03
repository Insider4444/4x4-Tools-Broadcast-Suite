#include "core/audio_pipeline.h"
#include "core/image_ops.h"
#include "core/frame.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
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
        std::vector<float> constant(19*11*4,.357f);
        const auto resized=resample(constant,19,11,4,7,5);
        for(const float value:resized) require(std::abs(value-.357f)<1.e-5,"Resampling does not preserve constant colors");
        std::array<float,8> pixels{1.f,2.75f,.25f,.125f,.5f,.2f,.1f,.05f},result{};
        Frame source{pixels.data(),2,1,sizeof(pixels),PixelLayout::argb32f},destination{result.data(),2,1,sizeof(result),PixelLayout::argb32f};
        const auto proxy=toBgrFloat(source);const std::array<float,2> mask{1,.25f};
        applyFloatDelta(source,destination,proxy,proxy,1,false,mask);
        require(!std::memcmp(pixels.data(),result.data(),4*sizeof(float)),"Untouched HDR pixels lost precision");
        for(int c=0;c<4;++c) require(result[4+c]==pixels[4+c]*.25f,"Matte multiplication broke premultiplied alpha");
        std::vector<float> empty(64*36),solid(empty.size(),1);
        require(!subjectCenter(empty,64,36).found,"Empty matte was framed as a person");
        auto center=subjectCenter(solid,64,36);require(center.found&&std::abs(center.centerX-.5f)<.001,"Subject center is biased horizontally");
        std::cout<<"Audio passband, anti-alias rejection, sample timing, channel isolation, image resampling and premultiplied HDR/matte checks passed.\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
