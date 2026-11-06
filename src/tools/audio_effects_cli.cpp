#include "core/audio_pipeline.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>

namespace {
template<class T> T read(std::istream& stream) { T value{};if(!stream.read(reinterpret_cast<char*>(&value),sizeof(value))) throw std::runtime_error("Truncated WAV.");return value; }
template<class T> void write(std::ostream& stream,T value) { stream.write(reinterpret_cast<const char*>(&value),sizeof(value)); }
}
int wmain(int argc,wchar_t** argv) {
    using namespace fourbyfour;
    try {
        if(argc<5) { std::cerr<<"AudioEffectsCli SDK_ROOT noise|echo|clean|preview|studio|restore input.wav output.wav [mix] [mic_profile_-1_to_5] [channels 0=independent|1=dialogue] [match-level 0|1]\n";return 2; }
        const std::map<std::wstring,AudioEffect> kinds{{L"noise",AudioEffect::noise},{L"echo",AudioEffect::echo},{L"clean",AudioEffect::clean},
            {L"preview",AudioEffect::studioPreview},{L"studio",AudioEffect::studioQuality},{L"restore",AudioEffect::restore}};
        AudioSettings settings;settings.effect=kinds.at(argv[2]);
        std::ifstream input(std::filesystem::path(argv[3]),std::ios::binary);
        if(read<std::uint32_t>(input)!=0x46464952) throw std::runtime_error("Use a RIFF WAV input.");
        read<std::uint32_t>(input); if(read<std::uint32_t>(input)!=0x45564157) throw std::runtime_error("Not a WAVE file.");
        int sampleRate{},channels{},format{},bits{};std::vector<char> raw;
        while(input&&raw.empty()) {
            const auto tag=read<std::uint32_t>(input),bytes=read<std::uint32_t>(input);
            if(bytes>1024*1024*1024) throw std::runtime_error("WAV chunk exceeds 1 GiB.");
            if(tag==0x20746d66) {
                std::vector<char> fmt(bytes);input.read(fmt.data(),bytes);
                if(bytes<16) throw std::runtime_error("Invalid WAV format.");
                std::uint16_t f{},c{},b{};std::uint32_t rate{};
                std::memcpy(&f,fmt.data(),2);std::memcpy(&c,fmt.data()+2,2);std::memcpy(&rate,fmt.data()+4,4);std::memcpy(&b,fmt.data()+14,2);
                if(f==0xfffe&&bytes>=40) std::memcpy(&f,fmt.data()+24,2);
                format=f;channels=c;sampleRate=rate;bits=b;
            } else if(tag==0x61746164) { raw.resize(bytes);if(!input.read(raw.data(),bytes)) throw std::runtime_error("Truncated WAV data."); }
            else input.seekg(bytes,std::ios::cur);
            if(bytes&1) input.get();
        }
        if(channels<1||channels>2||sampleRate<8000||sampleRate>192000||!((format==3&&bits==32)||(format==1&&(bits==16||bits==24))))
            throw std::runtime_error("Use mono/stereo PCM16/PCM24 or float32 WAV, 8-192 kHz.");
        settings.channels=channels;
        if(argc>6) settings.micProfile=std::stoi(argv[6]);
        if(argc>7){const int mode=std::stoi(argv[7]);if(mode<0||mode>1)throw std::invalid_argument("Invalid channel mode.");settings.channelMode=static_cast<AudioChannelMode>(mode);}
        if(argc>8)settings.matchLevel=std::stoi(argv[8])!=0;
        if(argc>9)settings.intensity=std::stof(argv[9]);
        std::vector<float> audio(raw.size()/(bits/8));
        for(std::size_t i=0;i<audio.size();++i) {
            if(format==3) std::memcpy(&audio[i],raw.data()+i*4,4);
            else if(bits==16) {std::int16_t value{};std::memcpy(&value,raw.data()+i*2,2);audio[i]=value/32768.f;}
            else { const auto* p=reinterpret_cast<const unsigned char*>(raw.data()+i*3);int v=p[0]|(p[1]<<8)|(p[2]<<16);if(v&0x800000) v|=~0xffffff;audio[i]=v/8388608.f; }
        }
        const float mix=argc>5?std::stof(argv[5]):1.f;
        auto start=std::chrono::steady_clock::now();
        AudioReport report;
        auto result=processAudio(audio,sampleRate,settings,mix,0,argv[1],&report);
        auto end=std::chrono::steady_clock::now();
        double square=0,difference=0;float peak=0;
        for(std::size_t i=0;i<result.size();++i) { if(!std::isfinite(result[i])) throw std::runtime_error("Non-finite output.");square+=double(result[i])*result[i];peak=std::max(peak,std::abs(result[i]));difference+=std::abs(result[i]-audio[i]); }
        if(result.size()!=audio.size()) throw std::runtime_error("Output duration changed.");
        if(mix==0&&result!=audio) throw std::runtime_error("Dry bypass is not sample-exact.");
        std::ofstream output(std::filesystem::path(argv[4]),std::ios::binary);
        const auto size=static_cast<std::uint32_t>(result.size()*4);
        output.write("RIFF",4);write(output,size+48);output.write("WAVEfmt ",8);write<std::uint32_t>(output,16);
        write<std::uint16_t>(output,3);write<std::uint16_t>(output,static_cast<std::uint16_t>(channels));write<std::uint32_t>(output,sampleRate);
        write<std::uint32_t>(output,sampleRate*channels*4);write<std::uint16_t>(output,static_cast<std::uint16_t>(channels*4));write<std::uint16_t>(output,32);
        output.write("fact",4);write<std::uint32_t>(output,4);write<std::uint32_t>(output,static_cast<std::uint32_t>(result.size()/channels));
        output.write("data",4);write(output,size);output.write(reinterpret_cast<const char*>(result.data()),size);
        const double seconds=double(audio.size())/sampleRate/channels,wall=std::chrono::duration<double>(end-start).count();
        std::cout<<"rate="<<sampleRate<<" channels="<<channels<<" frames="<<audio.size()/channels<<" duration_s="<<seconds<<" peak="<<peak
            <<" rms="<<std::sqrt(square/result.size())<<" mean_change="<<difference/result.size()<<" latency_frames="<<report.latencyFrames
            <<" sdk_input="<<report.sdkInputFrames<<"@"<<report.sdkInputRate<<" sdk_output="<<report.sdkOutputFrames<<"@48000 process_s="<<wall<<" total_throughput="<<seconds/wall<<"x\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; }
}
