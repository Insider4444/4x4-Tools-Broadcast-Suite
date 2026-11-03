#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace fourbyfour {
enum class PixelLayout { argb8, argb16, argb32f, bgra8, bgra32f };
struct Frame {
    void* pixels{};
    int width{}, height{};
    std::ptrdiff_t stride{};
    PixelLayout layout{PixelLayout::argb8};
};
std::size_t pixelBytes(PixelLayout layout);
void validate(const Frame& frame);
std::vector<std::uint8_t> toBgr(const Frame& frame);
// Apply only the model's RGB delta. Untouched pixels retain their original
// precision, HDR values and alpha. The model itself receives an 8-bit SDR proxy.
void applyBgrDelta(const Frame& input, const Frame& output,
                   std::span<const std::uint8_t> original,
                   std::span<const std::uint8_t> corrected, float strength,
                   bool compare = false);
// Straight-alpha, normalized SDR proxy, retaining 16/32-bit source precision.
std::vector<float> toBgrFloat(const Frame& frame);
void applyFloatDelta(const Frame& input, const Frame& output,
                     std::span<const float> original, std::span<const float> corrected,
                     float strength, bool compare = false,
                     std::span<const float> alphaMask = {});
void showMatte(const Frame& output, std::span<const float> matte);
// Spatial transforms operate on premultiplied RGBA to avoid colored fringes.
std::vector<float> toRgbaFloat(const Frame& frame);
void fromRgbaFloat(const Frame& output, std::span<const float> rgba);
}
