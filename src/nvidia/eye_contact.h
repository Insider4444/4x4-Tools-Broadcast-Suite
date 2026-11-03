#pragma once
#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>
namespace fourbyfour {
struct EyeSettings { int eyeSize{3}; };
// Serializes the GPU session. Temporal history and random look-away are disabled
// so scrubbing, reverse playback and parallel host requests are order independent.
std::vector<std::uint8_t> correctEyes(std::span<const std::uint8_t> bgr, int width, int height,
                                    const EyeSettings& settings = {},
                                    const std::filesystem::path& sdk = {});
std::filesystem::path runtimeRoot();
void shutdownEyeContact();
}
