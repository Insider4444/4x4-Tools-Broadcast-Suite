#pragma once
#include <span>
#include <vector>
namespace fourbyfour {
enum class LightingEncoding { rec709,appleLog };
float lightingToLinear(float value,LightingEncoding);
float lightingFromLinear(float value,LightingEncoding);
std::vector<float> lightingModelProxy(std::span<const float> bgr,LightingEncoding);
std::vector<float> lightingLuminance(std::span<const float> rec709Bgr);
std::vector<float> illuminateSource(std::span<const float> bgr,std::span<const float> gains,
                                  LightingEncoding,float exposure=0,float warmth=0,
                                  std::span<const float> matte={},float mix=1);
}
