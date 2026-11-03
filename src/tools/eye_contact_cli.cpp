#include "nvidia/eye_contact.h"
#include "nvidia/gpu.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
int wmain(int argc, wchar_t** argv) {
    try {
        if (argc != 4) { std::cerr << "Usage: EyeContactCli.exe ARSDK_ROOT input.ppm output.ppm\n"; return 2; }
        std::ifstream input(std::filesystem::path(argv[2]), std::ios::binary);
        std::string magic; int w{}, h{}, max{};
        if (!(input >> magic >> w >> h >> max) || magic != "P6" || max != 255 || w <= 0 || h <= 0 || w > 32768 || h > 32768)
            throw std::runtime_error("Expected a P6 RGB PPM file with 8-bit components and no comment lines.");
        const int separator = input.get();
        if (separator == '\r' && input.peek() == '\n') input.get();
        else if (separator != ' ' && separator != '\n' && separator != '\t') throw std::runtime_error("Invalid PPM separator.");
        std::vector<std::uint8_t> bgr(static_cast<std::size_t>(w) * h * 3);
        if (!input.read(reinterpret_cast<char*>(bgr.data()), bgr.size())) throw std::runtime_error("Truncated input image.");
        for (std::size_t i = 0; i < bgr.size(); i += 3) std::swap(bgr[i], bgr[i + 2]);
        const auto start = std::chrono::steady_clock::now();
        auto corrected = fourbyfour::correctEyes(bgr, w, h, {}, argv[1]);
        std::size_t changed{};
        for (std::size_t i = 0; i < bgr.size(); ++i) if (corrected[i] != bgr[i]) ++changed;
        // Repeating the identical frame must be independent of request history.
        const auto repeat = fourbyfour::correctEyes(bgr, w, h, {}, argv[1]);
        if (repeat != corrected) throw std::runtime_error("Repeated frame produced a different result.");
        constexpr int warmRuns = 5;
        const auto warmStart = std::chrono::steady_clock::now();
        for (int i = 0; i < warmRuns; ++i) (void)fourbyfour::correctEyes(bgr, w, h, {}, argv[1]);
        const double warmMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - warmStart).count() / warmRuns;
        for (std::size_t i = 0; i < corrected.size(); i += 3) std::swap(corrected[i], corrected[i + 2]);
        std::ofstream output(std::filesystem::path(argv[3]), std::ios::binary);
        output << "P6\n" << w << ' ' << h << "\n255\n";
        output.write(reinterpret_cast<const char*>(corrected.data()), corrected.size());
        if (!output) throw std::runtime_error("Could not save corrected image.");
        fourbyfour::shutdownEyeContact();
        std::cout << "Processed " << w << 'x' << h << "; changed " << changed << " RGB components; repeated frame matched.\n"
                  << "Warm inference including CPU/GPU transfers: " << warmMs << " ms/frame; " << 1000 / warmMs << " fps.\n"
                  << "Total including model startup: " << std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() << " s.\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
