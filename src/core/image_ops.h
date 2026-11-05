#pragma once
#include <span>
#include <vector>
namespace fourbyfour {
// Pixel-center coordinates; anti-aliasing on reduction, Lanczos-3 on enlargement.
std::vector<float> resample(std::span<const float> data, int width, int height, int channels,
                            int outputWidth, int outputHeight,
                            float left = 0, float top = 0, float cropWidth = 0, float cropHeight = 0);
// Area reduction / bilinear enlargement for model proxies and smooth fields.
std::vector<float> resampleProxy(std::span<const float> data, int width, int height, int channels,
                                 int outputWidth, int outputHeight);
// Transfer a smooth lighting gain, retaining source texture instead of model reconstruction.
std::vector<float> lightingGain(std::span<const float> source, std::span<const float> relit,
                                int width, int height);
void refineMatte(std::vector<float>& matte, std::span<const float> bgr, int width, int height,
                 float edgeAdjustment = 0, float feather = 0);
struct Framing { float centerX{.5f}, centerY{.5f}; bool found{}; };
Framing subjectCenter(std::span<const float> matte, int width, int height);
}
