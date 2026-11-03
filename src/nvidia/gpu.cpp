#include "gpu.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace fourbyfour {
bool supportedArchitecture(const Gpu& gpu) {
    const int capability = gpu.major * 10 + gpu.minor;
    if (gpu.driverApi < 12080) return false; // SDK ships CUDA 12.8.
    if (capability == 86 || capability == 89 || capability == 120) return true;
    // GTX 16-series also reports SM75 but has no Tensor Cores.
    std::string name = gpu.name;
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return char(std::toupper(c)); });
    return capability == 75 && (name.find("RTX") != std::string::npos || name == "TESLA T4" || name == "NVIDIA T4" || name == "T4");
}
std::vector<Gpu> detectGpus() {
    HMODULE driver = LoadLibraryExW(L"nvcuda.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!driver) throw std::runtime_error("No NVIDIA CUDA driver found. A supported NVIDIA RTX GPU is required.");
    struct Cleanup { HMODULE h; ~Cleanup() { FreeLibrary(h); } } cleanup{driver};
    auto init = reinterpret_cast<int(__stdcall*)(unsigned)>(GetProcAddress(driver, "cuInit"));
    auto count = reinterpret_cast<int(__stdcall*)(int*)>(GetProcAddress(driver, "cuDeviceGetCount"));
    auto device = reinterpret_cast<int(__stdcall*)(int*, int)>(GetProcAddress(driver, "cuDeviceGet"));
    auto attr = reinterpret_cast<int(__stdcall*)(int*, int, int)>(GetProcAddress(driver, "cuDeviceGetAttribute"));
    auto name = reinterpret_cast<int(__stdcall*)(char*, int, int)>(GetProcAddress(driver, "cuDeviceGetName"));
    auto memory = reinterpret_cast<int(__stdcall*)(std::size_t*, int)>(GetProcAddress(driver, "cuDeviceTotalMem_v2"));
    auto version = reinterpret_cast<int(__stdcall*)(int*)>(GetProcAddress(driver, "cuDriverGetVersion"));
    if (!init || !count || !device || !attr || !name || !memory || !version || init(0))
        throw std::runtime_error("The NVIDIA CUDA driver could not initialize. Update the NVIDIA driver and retry.");
    int n = 0, api = 0;
    if (count(&n) || version(&api)) throw std::runtime_error("Could not query NVIDIA GPU capabilities.");
    std::vector<Gpu> result;
    for (int ordinal = 0; ordinal < n; ++ordinal) {
        int dev = 0; char text[256]{}; std::size_t bytes = 0;
        Gpu g; g.ordinal = ordinal; g.driverApi = api;
        if (device(&dev, ordinal) || attr(&g.major, 75, dev) || attr(&g.minor, 76, dev) ||
            name(text, sizeof(text), dev) || memory(&bytes, dev)) continue;
        g.name = text; g.memory = bytes; result.push_back(g);
    }
    return result;
}
Gpu selectGpu() {
    const auto gpus = detectGpus();
    for (const auto& gpu : gpus) if (supportedArchitecture(gpu)) return gpu;
    throw std::runtime_error("No supported NVIDIA Tensor Core GPU found. GTX cards and drivers older than CUDA 12.8 are unsupported.");
}
}
