#pragma once
#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

namespace fourbyfour {
enum class VideoEffect { matte, blur, relight, denoise, superResolution, upscale };
struct VideoSettings {
    bool preview{false}, keepChair{false};
    float strength{1.f};
    float lightAzimuth{-35.f}, lightElevation{25.f}, lightExposure{0.f}, lightWarmth{0.f};
    int outputWidth{}, outputHeight{};
    std::uintptr_t contextId{};
    std::int64_t time{},timeStep{};
    int statusEffect{-1};
};
struct VideoResult {
    int width{}, height{};
    std::vector<float> bgr;
    std::vector<float> matte;
};
// Independent requests: stateful SDK filters are reset for every image, so
// parallel Adobe renders, reverse playback and seeking cannot share history.
VideoResult processVideo(VideoEffect effect, std::span<const float> bgr, int width, int height,
                         const VideoSettings& settings = {}, const std::filesystem::path& sdk = {});
// Oldest context image first, current image last. History is reset once before
// the window; only the final result is returned. No state crosses requests.
VideoResult processVideoWindow(VideoEffect effect,std::span<const std::vector<float>> frames,int width,int height,
                               const VideoSettings& settings = {},const std::filesystem::path& sdk = {});
void shutdownVideo();
void releaseVideo(std::uintptr_t owner);
std::filesystem::path videoRuntimeRoot();
}
