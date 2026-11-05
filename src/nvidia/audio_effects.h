#pragma once
#include <filesystem>
#include <memory>
#include <span>
#include <vector>
namespace fourbyfour {
enum class AudioEffect { noise, echo, clean, studioPreview, studioQuality, restore };
struct AudioSettings { AudioEffect effect{AudioEffect::clean}; int channels{1}; int micProfile{-1}; int statusEffect{-1}; };
class AudioEngine {
    struct Impl;
    std::unique_ptr<Impl> impl;
public:
    explicit AudioEngine(const AudioSettings&,const std::filesystem::path& sdk = {});
    ~AudioEngine();
    AudioEngine(const AudioEngine&)=delete;
    int inputRate() const; int outputRate() const;
    int inputFrames() const; int outputFrames() const; int channels() const;
    void reset();
    // Planar floats, one fixed SDK block. Each channel has a separate model
    // history; channels and neighboring plugin instances never share samples.
    void run(std::span<const float* const> input,std::span<float* const> output);
    void profile(int id);
};
std::filesystem::path audioRuntimeRoot();
}
