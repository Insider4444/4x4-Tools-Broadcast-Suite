#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include "AEConfig.h"
#include "AE_Effect.h"
#include "AE_EffectCB.h"
#include "AE_PluginData.h"
#include "metadata.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>
using Main = PF_Err (*)(PF_Cmd, PF_InData*, PF_OutData*, PF_ParamDef*[], PF_LayerDef*, void*);
static PF_ParamDef controls[5]{};
static PF_Err add(PF_ProgPtr, PF_ParamIndex index, PF_ParamDef* def) {
    if (index != -1 || def->uu.id < 1 || def->uu.id > 4) return 999;
    controls[def->uu.id] = *def; return 0;
}
static PF_Err abortRender(PF_ProgPtr) { return 0; }
static void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
int wmain(int argc, wchar_t** argv) {
    HMODULE module{};
    try {
        require(argc == 2, "Plugin path missing");
        module = LoadLibraryExW(argv[1], nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
        require(module != nullptr, "Adobe effect DLL failed to load without NVIDIA runtime initialization");
        auto effect = reinterpret_cast<Main>(GetProcAddress(module, "EffectMain"));
        require(effect && GetProcAddress(module, "PluginDataEntryFunction2"), "Adobe entry points missing");
        require(FindResourceW(module, MAKEINTRESOURCEW(16000), L"PiPL") != nullptr, "PiPL registration resource missing");
        PF_InData in{}; PF_OutData out{}; in.inter.add_param = add; in.inter.abort = abortRender;
        require(!effect(PF_Cmd_GLOBAL_SETUP, &in, &out, nullptr, nullptr, nullptr), "Global setup failed");
        require(out.my_version == FOUR_VERSION && out.out_flags == FOUR_FLAGS && out.out_flags2 == FOUR_FLAGS2, "SDK flags differ from PiPL metadata");
        require(!effect(PF_Cmd_PARAMS_SETUP, &in, &out, nullptr, nullptr, nullptr) && out.num_params == 5, "Parameter setup failed");
        require(controls[1].u.bd.value && controls[2].u.fs_d.value == 100 && controls[3].u.sd.value == 3, "Parameter defaults incorrect");
        controls[1].u.bd.value = 0;
        PF_ParamDef* params[5]{}; for (int i = 0; i < 5; ++i) params[i] = &controls[i];
        for (const int depth : {8, 16}) {
            const int width = 19, height = 7, bpp = depth == 8 ? 4 : 8;
            const int srcStride = width * bpp + 12, dstStride = width * bpp + 28;
            std::vector<unsigned char> src(srcStride * height), dst(dstStride * height, 0xcd);
            for (std::size_t i = 0; i < src.size(); ++i) src[i] = static_cast<unsigned char>(i * 19);
            auto& source = controls[0].u.ld;
            source.width = width; source.height = height; source.rowbytes = srcStride;
            source.data = reinterpret_cast<PF_Pixel*>(src.data()); source.world_flags = depth == 16 ? PF_WorldFlag_DEEP : 0;
            PF_LayerDef output = source; output.rowbytes = dstStride; output.data = reinterpret_cast<PF_Pixel*>(dst.data());
            require(!effect(PF_Cmd_RENDER, &in, &out, params, &output, nullptr), "Bypass render failed");
            for (int y = 0; y < height; ++y) {
                require(!std::memcmp(src.data() + y * srcStride, dst.data() + y * dstStride, width * bpp), "Bypass changed pixels or alpha");
                for (int x = width * bpp; x < dstStride; ++x) require(dst[y * dstStride + x] == 0xcd, "Plugin overwrote row padding");
            }
        }
        require(!effect(PF_Cmd_GLOBAL_SETDOWN, &in, &out, nullptr, nullptr, nullptr), "Plugin shutdown failed");
        FreeLibrary(module);
        std::cout << "Adobe entry points, PiPL, parameter controls and 8/16-bit bypass passed.\n";
        return 0;
    } catch (const std::exception& error) { if (module) FreeLibrary(module); std::cerr << error.what() << '\n'; return 1; }
}
