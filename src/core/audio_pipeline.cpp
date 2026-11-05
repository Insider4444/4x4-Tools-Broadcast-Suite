#include "audio_pipeline.h"
#include <algorithm>
#include <cmath>
#include <climits>
#include <stdexcept>

namespace fourbyfour {
AudioPipeline::AudioPipeline(const AudioSettings& settings,int sampleRate,const std::filesystem::path& sdk):
    engine(settings,sdk),rate(sampleRate),channelCount(settings.channels),delay(0),modelDelay(0),
    inputConverter(rate,engine.inputRate(),channelCount),outputConverter(engine.outputRate(),rate,channelCount),
    input(channelCount,std::vector<float>(engine.inputFrames())),output(channelCount,std::vector<float>(engine.outputFrames())) {
    if(sampleRate<8000||sampleRate>192000) throw std::invalid_argument("Supported host sample rates are 8 to 192 kHz.");
    const double duration=double(engine.inputFrames())/engine.inputRate();
    if(std::abs(duration-double(engine.outputFrames())/engine.outputRate())>1.e-7) throw std::runtime_error("Audio model changes duration unexpectedly.");
    // R22/3.0.0 model timing calibration. The SDK does not expose algorithmic
    // delay: two speech recordings measured 3360 samples for cleanup/LL and
    // 2880 for super-resolution at 48 kHz. Studio HQ resynthesizes waveform
    // timing; use its nominal 140 ms alignment (see QUALITY-REPORT.md).
    const int model48=settings.effect==AudioEffect::restore?2880:settings.effect==AudioEffect::studioQuality?6720:3360;
    modelDelay=static_cast<int>(std::lround(double(model48)*rate/48000));
    delay=modelDelay+static_cast<int>(std::ceil(duration*rate))+inputConverter.lookahead()+
        static_cast<int>(std::ceil(double(outputConverter.lookahead())*rate/engine.outputRate()))+2;
    for(int c=0;c<channelCount;++c) { inputs[c]=input[c].data();outputs[c]=output[c].data(); }
    reset();
}
void AudioPipeline::reset() {
    engine.reset(); inputConverter.reset();outputConverter.reset();position=0;
    wetQueue.assign(static_cast<std::size_t>(delay-modelDelay),{}); dryQueue.assign(static_cast<std::size_t>(delay),{});
    for(auto& channel:input) std::fill(channel.begin(),channel.end(),0);
    for(auto& channel:output) std::fill(channel.begin(),channel.end(),0);
    currentMix=1;currentGain=1;first=true;suspended=false;
}
void AudioPipeline::process(const float* const* src,float* const* dst,int frames,float mix,float gainDb,bool bypass) {
    if(!src||!dst||frames<0||!std::isfinite(mix)||!std::isfinite(gainDb)) throw std::invalid_argument("Invalid audio process buffer.");
    for(int c=0;c<channelCount;++c) if(!src[c]||!dst[c]) throw std::invalid_argument("Missing audio channel buffer.");
    const float targetMix=bypass?0.f:std::clamp(mix,0.f,1.f),targetGain=std::pow(10.f,std::clamp(gainDb,-60.f,24.f)/20);
    if(targetMix==0) {
        // Bypass must stop inference, not merely mix its expensive output away.
        // Retain the advertised delay so the host's latency compensation holds.
        for(int i=0;i<frames;++i) {
            std::array<float,2> frame{};
            for(int c=0;c<channelCount;++c)frame[c]=std::isfinite(src[c][i])?src[c][i]:0.f;
            dryQueue.push_back(frame);const auto dry=dryQueue.front();dryQueue.pop_front();
            for(int c=0;c<channelCount;++c)dst[c][i]=dry[c]*targetGain;
        }
        suspended=true;return;
    }
    if(suspended) {
        // Resume a clean wet history while preserving the running dry clock.
        engine.reset();inputConverter.reset();outputConverter.reset();position=0;
        wetQueue.assign(static_cast<std::size_t>(delay-modelDelay),{});
        for(auto& channel:input)std::fill(channel.begin(),channel.end(),0.f);
        currentMix=targetMix;currentGain=targetGain;suspended=false;
    }
    if(first) { currentMix=targetMix;currentGain=targetGain;first=false; }
    const float smoothing=1.f-std::exp(-1.f/(rate*.02f));
    for(int i=0;i<frames;++i) {
        std::array<float,2> frame{};
        for(int c=0;c<channelCount;++c) frame[c]=std::isfinite(src[c][i])?src[c][i]:0.f;
        dryQueue.push_back(frame); inputConverter.push(frame);
        while(inputConverter.take(frame)) {
            for(int c=0;c<channelCount;++c) input[c][position]=frame[c];
            if(++position==engine.inputFrames()) {
                position=0;
                try{engine.run(std::span(inputs.data(),channelCount),std::span(outputs.data(),channelCount));}
                catch(...){suspended=true;throw;}
                for(int j=0;j<engine.outputFrames();++j) {
                    for(int c=0;c<channelCount;++c) frame[c]=output[c][j];
                    outputConverter.push(frame);
                }
                while(outputConverter.take(frame)) wetQueue.push_back(frame);
            }
        }
        if(wetQueue.empty()||dryQueue.empty()) throw std::runtime_error("Voice adapter ran out of buffered audio.");
        const auto dry=dryQueue.front(),wet=wetQueue.front(); dryQueue.pop_front();wetQueue.pop_front();
        currentMix+=(targetMix-currentMix)*smoothing; currentGain+=(targetGain-currentGain)*smoothing;
        for(int c=0;c<channelCount;++c) dst[c][i]=(dry[c]+currentMix*(wet[c]-dry[c]))*currentGain;
    }
}
std::vector<float> processAudio(std::span<const float> interleaved,int sampleRate,const AudioSettings& settings,float mix,float gainDb,const std::filesystem::path& sdk,AudioReport* report) {
    if(settings.channels<1||settings.channels>2||interleaved.size()%settings.channels) throw std::invalid_argument("Invalid interleaved audio length.");
    if(interleaved.empty()) return {};
    AudioPipeline pipeline(settings,sampleRate,sdk);
    if(report) *report={pipeline.latency(),pipeline.sdkInputFrames(),pipeline.sdkOutputFrames(),pipeline.sdkInputRate()};
    const auto frames=interleaved.size()/settings.channels;
    if(frames>static_cast<std::size_t>(INT_MAX)-pipeline.latency()) throw std::invalid_argument("Audio request exceeds the supported length.");
    constexpr int block=2048;
    std::array<std::array<float,block>,2> input{},output{};
    const float* inputs[]{input[0].data(),input[1].data()};float* outputs[]{output[0].data(),output[1].data()};
    std::vector<float> result(interleaved.size());
    const auto total=frames+pipeline.latency();
    for(std::size_t offset=0;offset<total;offset+=block) {
        const int count=static_cast<int>(std::min<std::size_t>(block,total-offset));
        for(int i=0;i<count;++i) for(int c=0;c<settings.channels;++c) input[c][i]=offset+i<frames?interleaved[(offset+i)*settings.channels+c]:0.f;
        pipeline.process(inputs,outputs,count,mix,gainDb);
        for(int i=0;i<count;++i) if(offset+i>=static_cast<std::size_t>(pipeline.latency())) for(int c=0;c<settings.channels;++c)
            result[(offset+i-pipeline.latency())*settings.channels+c]=output[c][i];
    }
    return result;
}
}
