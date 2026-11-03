#include "core/audio_pipeline.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace fourbyfour;
void require(bool value,const char* text) {if(!value) throw std::runtime_error(text);}
int wmain(int argc,wchar_t** argv) {
    try {
        require(argc==3,"AudioGpuTests SDK_ROOT mono.f32");
        std::ifstream file(std::filesystem::path(argv[2]),std::ios::binary|std::ios::ate);const auto bytes=file.tellg();file.seekg(0);
        require(bytes>0&&bytes%4==0,"Invalid float speech sample");
        std::vector<float> mono(static_cast<std::size_t>(bytes)/4);file.read(reinterpret_cast<char*>(mono.data()),bytes);
        mono.resize(std::min<std::size_t>(mono.size(),48000*2+127));
        for(int rate:{44100,48000,96000}) {
            AudioSettings settings{AudioEffect::noise,2,-1};
            std::vector<float> input(mono.size()*2);for(std::size_t i=0;i<mono.size();++i) input[i*2]=mono[i];
            auto reference=processAudio(input,rate,settings,1,0,argv[1]);
            const auto monoReference=processAudio(mono,rate,{AudioEffect::noise,1,-1},1,0,argv[1]);
            for(std::size_t i=0;i<mono.size();++i) require(std::abs(reference[i*2]-monoReference[i])<2.e-5f,"Stereo processing changed the independent mono channel");
            AudioPipeline pipeline(settings,rate,argv[1]);
            std::vector<float> rendered(input.size());
            std::array<std::array<float,1024>,2> in{},out{};
            const float* sources[]{in[0].data(),in[1].data()};float* targets[]{out[0].data(),out[1].data()};
            const std::array<int,7> sizes{1,13,57,441,512,1024,127};std::size_t index=0;
            const auto total=mono.size()+pipeline.latency();
            for(std::size_t offset=0;offset<total;) {
                const int count=static_cast<int>(std::min<std::size_t>(sizes[index++%sizes.size()],total-offset));
                for(int i=0;i<count;++i) {in[0][i]=offset+i<mono.size()?mono[offset+i]:0;in[1][i]=0;}
                pipeline.process(sources,targets,count);
                for(int i=0;i<count;++i) if(offset+i>=static_cast<std::size_t>(pipeline.latency())) {
                    const auto target=offset+i-pipeline.latency();rendered[target*2]=out[0][i];rendered[target*2+1]=out[1][i];
                }
                offset+=count;
            }
            float maximum=0,right=0;
            for(std::size_t i=0;i<rendered.size();++i) {require(std::isfinite(rendered[i]),"Non-finite audio output");maximum=std::max(maximum,std::abs(reference[i]-rendered[i]));if(i%2) right=std::max(right,std::abs(rendered[i]));}
            std::cout<<"Measured rate="<<rate<<" partition_error="<<maximum<<" silent_channel_peak="<<right<<'\n';
            require(maximum<2.e-5f,"Host buffer partitioning changed the audio");require(right<1.e-5f,"Speech crossed into the silent stereo channel");
            const auto dry=processAudio(input,rate,settings,0,0,argv[1]);require(dry==input,"Latency-compensated dry bypass is not sample exact");
            std::cout<<"rate="<<rate<<" latency="<<pipeline.latency()<<" irregular_buffer_error="<<maximum<<" silent_channel_peak="<<right<<" bypass=exact\n";
        }
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
