#include "core/frame.h"
#include "nvidia/gpu.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <stdexcept>
using namespace fourbyfour;
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
int main() {
    try {
        // Row padding, channel order, hidden RGB, alpha and negative strides.
        for (auto layout : {PixelLayout::argb8, PixelLayout::bgra8, PixelLayout::argb16, PixelLayout::argb32f, PixelLayout::bgra32f}) {
            const auto bytes = pixelBytes(layout); const int w = 7, h = 3, row = int(w * bytes) + 16;
            std::vector<std::uint8_t> src(row * h), dst(row * h, 0xcd);
            for (std::size_t i = 0; i < src.size(); ++i) src[i] = std::uint8_t(i * 17);
            if (bytes == 16) for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) {
                float rgba[4]{0.0f, -0.125f, 2.75f, 0.353535f};
                std::memcpy(src.data() + y * row + x * bytes, rgba, sizeof(rgba));
            }
            Frame input{src.data() + (h - 1) * row, w, h, -row, layout};
            Frame output{dst.data() + (h - 1) * row, w, h, -row, layout};
            auto proxy = toBgr(input);
            applyBgrDelta(input, output, proxy, proxy, 1);
            for (int y = 0; y < h; ++y) {
                require(!std::memcmp(src.data() + y * row, dst.data() + y * row, w * bytes), "Identity lost precision or channel data");
                for (int i = int(w * bytes); i < row; ++i) require(dst[y * row + i] == 0xcd, "Padding overwritten");
            }
        }
        std::uint8_t argb[8]{77, 10, 20, 30, 88, 40, 50, 60}, output[8]{};
        Frame a{argb, 2, 1, 8, PixelLayout::argb8}, b{output, 2, 1, 8, PixelLayout::argb8};
        auto original = toBgr(a); require(original[0] == 30 && original[2] == 10, "ARGB to BGR order incorrect");
        auto correction = original; for (auto& v : correction) v += 20;
        applyBgrDelta(a, b, original, correction, .5f, true);
        require(!std::memcmp(argb, output, 4) && output[4] == 88 && output[5] == 50 && output[7] == 70, "Split compare, strength or alpha incorrect");
        bool rejected = false;
        try { Frame invalid{argb, 20, 1, 4, PixelLayout::argb8}; (void)toBgr(invalid); } catch (const std::invalid_argument&) { rejected = true; }
        require(rejected, "Invalid stride accepted");
        require(!supportedArchitecture({0, 7, 5, 12080, 4ULL << 30, "NVIDIA GeForce GTX 1650"}), "GTX without Tensor Cores accepted");
        require(!supportedArchitecture({0, 12, 0, 12070, 12ULL << 30, "NVIDIA GeForce RTX 5070"}), "Old driver accepted");
        require(supportedArchitecture({0, 12, 0, 12080, 12ULL << 30, "NVIDIA GeForce RTX 5070"}), "Blackwell rejected");
        require(supportedArchitecture({0, 7, 5, 12080, 8ULL << 30, "NVIDIA GeForce RTX 2080"}), "Turing RTX rejected");
        require(!supportedArchitecture({0, 6, 1, 12080, 8ULL << 30, "NVIDIA GTX 1080"}), "Pascal accepted");
        std::cout << "Frame preservation, pixel conversion and GPU rejection tests passed.\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
