#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include "AEConfig.h"
#include "AE_Effect.h"
#include "AE_EffectCB.h"
#include "AE_EffectCBSuites.h"
#include "SPBasic.h"
#include "PrSDKAESupport.h"
#include "core/frame.h"
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
using Main = PF_Err (*)(PF_Cmd, PF_InData*, PF_OutData*, PF_ParamDef*[], PF_LayerDef*, void*);
using namespace fourbyfour;
static PrPixelFormat currentFormat{};
static PF_Err clear(PF_ProgPtr) { return 0; }
static PF_Err add(PF_ProgPtr, PrPixelFormat format) {
    return format == PrPixelFormat_BGRA_4444_8u || format == PrPixelFormat_BGRA_4444_32f ? 0 : 999;
}
static PF_Err format(PF_EffectWorld*, PrPixelFormat* result) { *result = currentFormat; return 0; }
static PF_PixelFormatSuite1 pixels{};
static SPErr acquire(const char* name, int32 version, const void** result) {
    if (std::strcmp(name, kPFPixelFormatSuite) || version != kPFPixelFormatSuiteVersion1) return 999;
    *result = &pixels; return 0;
}
static SPErr release(const char*, int32) { return 0; }
static PF_Err keepGoing(PF_ProgPtr) { return 0; }
static void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
int wmain(int argc, wchar_t** argv) {
    HMODULE module{};
    try {
        require(argc == 4, "Usage: HostGpuTests plugin.aex input.ppm output.ppm");
        std::ifstream file(std::filesystem::path(argv[2]), std::ios::binary);
        std::string magic; int w{}, h{}, max{}; file >> magic >> w >> h >> max; file.get();
        require(magic == "P6" && w > 0 && h > 0 && w <= 32768 && h <= 32768 && max == 255, "Invalid test image");
        std::vector<unsigned char> rgb(static_cast<std::size_t>(w) * h * 3);
        require(bool(file.read(reinterpret_cast<char*>(rgb.data()), rgb.size())), "Truncated test image");
        module = LoadLibraryExW(argv[1], nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
        require(module != nullptr, "Cannot load Adobe plugin");
        auto effect = reinterpret_cast<Main>(GetProcAddress(module, "EffectMain")); require(effect != nullptr, "EffectMain is missing");
        pixels.ClearSupportedPixelFormats = clear; pixels.AddSupportedPixelFormat = add; pixels.GetPixelFormat = format;
        SPBasicSuite basic{}; basic.AcquireSuite = acquire; basic.ReleaseSuite = release;
        for (auto pixelLayout : {PixelLayout::argb8, PixelLayout::argb16, PixelLayout::bgra8, PixelLayout::bgra32f}) {
            const bool isPremiere = pixelLayout == PixelLayout::bgra8 || pixelLayout == PixelLayout::bgra32f;
            const auto bpp = pixelBytes(pixelLayout);
            const int sourceStride = int(w * bpp) + 16, destStride = int(w * bpp) + 32;
            std::vector<unsigned char> source(static_cast<std::size_t>(sourceStride) * h), result(static_cast<std::size_t>(destStride) * h, 0xcd);
            for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) {
                const auto* color = rgb.data() + (static_cast<std::size_t>(y) * w + x) * 3;
                auto* p = source.data() + static_cast<std::size_t>(y) * sourceStride + x * bpp;
                if (pixelLayout == PixelLayout::argb16) {
                    const std::uint16_t data[4]{std::uint16_t(x == 0 ? 0 : 32768), std::uint16_t(std::lround(color[0] * 32768.0 / 255)),
                        std::uint16_t(std::lround(color[1] * 32768.0 / 255)), std::uint16_t(std::lround(color[2] * 32768.0 / 255))};
                    std::memcpy(p, data, sizeof(data));
                } else if (pixelLayout == PixelLayout::bgra32f) {
                    const float data[4]{color[2] / 255.0f, x == 0 ? 2.75f : color[1] / 255.0f, color[0] / 255.0f, x == 0 ? 0.0f : 1.0f};
                    std::memcpy(p, data, sizeof(data));
                } else if (isPremiere) {
                    p[0] = color[2]; p[1] = color[1]; p[2] = color[0]; p[3] = x == 0 ? 0 : 255;
                } else {
                    p[0] = x == 0 ? 0 : 255; p[1] = color[0]; p[2] = color[1]; p[3] = color[2];
                }
            }
            PF_InData in{}; PF_OutData out{}; in.width = w; in.height = h;
            in.inter.abort = keepGoing; in.pica_basicP = &basic; in.appl_id = isPremiere ? 'PrMr' : 'FXTC';
            currentFormat = pixelLayout == PixelLayout::bgra32f ? PrPixelFormat_BGRA_4444_32f : PrPixelFormat_BGRA_4444_8u;
            require(!effect(PF_Cmd_GLOBAL_SETUP, &in, &out, nullptr, nullptr, nullptr), "Host global setup failed");
            PF_ParamDef controls[5]{}; PF_ParamDef* params[5]{};
            for (int i = 0; i < 5; ++i) params[i] = &controls[i];
            controls[1].u.bd.value = 1; controls[2].u.fs_d.value = 100; controls[3].u.sd.value = 3;
            auto& world = controls[0].u.ld;
            world.width = w; world.height = h; world.rowbytes = sourceStride; world.data = reinterpret_cast<PF_Pixel*>(source.data());
            world.world_flags = pixelLayout == PixelLayout::argb16 ? PF_WorldFlag_DEEP : 0;
            PF_LayerDef target = world; target.rowbytes = destStride; target.data = reinterpret_cast<PF_Pixel*>(result.data());
            const auto code = effect(PF_Cmd_RENDER, &in, &out, params, &target, nullptr);
            if (code) throw std::runtime_error(out.return_msg);
            const std::size_t channelBytes = bpp / 4, alphaOffset = isPremiere ? 3 * channelBytes : 0;
            std::size_t changed{};
            for (int y = 0; y < h; ++y) {
                for (int x = 0; x < w; ++x) {
                    const auto* a = source.data() + static_cast<std::size_t>(y) * sourceStride + x * bpp;
                    const auto* b = result.data() + static_cast<std::size_t>(y) * destStride + x * bpp;
                    require(!std::memcmp(a + alphaOffset, b + alphaOffset, channelBytes), "GPU render changed alpha");
                    if (std::memcmp(a, b, bpp)) ++changed;
                    if (x == 0) require(!std::memcmp(a, b, bpp), "GPU render changed hidden/HDR data outside the eye patch");
                }
                for (int x = int(w * bpp); x < destStride; ++x) require(result[static_cast<std::size_t>(y) * destStride + x] == 0xcd, "GPU render overwrote padding");
            }
            require(changed > 0, "No eye correction was produced for the test face");
            std::cout << (isPremiere ? "Premiere " : "AE ") << bpp * 8 << " bits/pixel: " << changed << " pixels corrected; alpha and padding preserved.\n";
            if (pixelLayout == PixelLayout::argb8) {
                auto corrected = toBgr({result.data(), w, h, destStride, pixelLayout});
                for (std::size_t i = 0; i < corrected.size(); i += 3) std::swap(corrected[i], corrected[i + 2]);
                std::ofstream output(std::filesystem::path(argv[3]), std::ios::binary);
                output << "P6\n" << w << ' ' << h << "\n255\n";
                output.write(reinterpret_cast<const char*>(corrected.data()), corrected.size());
                require(bool(output), "Cannot save host-processed test image");
            }
        }
        PF_InData in{}; PF_OutData out{};
        effect(PF_Cmd_GLOBAL_SETDOWN, &in, &out, nullptr, nullptr, nullptr);
        FreeLibrary(module); return 0;
    } catch (const std::exception& error) { if (module) FreeLibrary(module); std::cerr << error.what() << '\n'; return 1; }
}
