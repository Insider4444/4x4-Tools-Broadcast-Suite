#pragma once
#include "nvidia/audio_effects.h"
#include "audio_level.h"
#include <array>
#include <cstdint>
#include <deque>
#include <memory>
#include <span>
#include <vector>

namespace fourbyfour {
// Streaming, band-limited sample-rate conversion. Look-ahead and phase are
// retained across host buffers; SDK block boundaries never reset the filter.
class AudioResampler {
    int channels{},sourceRate{},destinationRate{},radius{};
    double cutoff{};
    struct Phase { int first{}; std::vector<double> weights; };
    std::vector<Phase> phases;
    std::int64_t base{},received{},generated{};
    std::deque<std::array<float,2>> history,ready;
public:
    AudioResampler(int source,int destination,int channelCount);
    void reset();
    int lookahead() const { return radius; }
    void push(const std::array<float,2>& frame);
    bool take(std::array<float,2>& frame);
};
class AudioPipeline {
    AudioEngine engine;
    int rate,channelCount,modelChannels,delay,modelDelay;
    bool dialogue,matchInputLevel;
    AudioLevelMatch levelMatch;
    AudioResampler inputConverter,outputConverter;
    std::vector<std::vector<float>> input,output;
    std::array<const float*,2> inputs{};
    std::array<float*,2> outputs{};
    int position{};
    std::deque<std::array<float,2>> wetQueue,dryQueue;
    float currentMix{1},currentGain{1};
    bool first{true};
    bool suspended{};
public:
    AudioPipeline(const AudioSettings&,int sampleRate,const std::filesystem::path& sdk = {});
    int latency() const { return delay; }
    int sdkInputFrames() const { return engine.inputFrames(); }
    int sdkOutputFrames() const { return engine.outputFrames(); }
    int sdkInputRate() const { return engine.inputRate(); }
    void reset();
    void profile(int id) { engine.profile(id); }
    void matchLevel(bool enabled) { matchInputLevel=enabled; }
    void intensity(float value) { engine.intensity(value); }
    void process(const float* const* in,float* const* out,int frames,float mix=1,float gainDb=0,bool bypass=false);
};
// Interleaved offline processing, with adapter delay removed and a zero-padded
// final SDK block. Exactly the original number of frames is returned.
struct AudioReport { int latencyFrames{},sdkInputFrames{},sdkOutputFrames{},sdkInputRate{}; };
std::vector<float> processAudio(std::span<const float> interleaved,int sampleRate,const AudioSettings&,
                                float mix=1,float gainDb=0,const std::filesystem::path& sdk = {},AudioReport* report=nullptr);
}
