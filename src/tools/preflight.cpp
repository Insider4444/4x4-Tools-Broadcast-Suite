#include "nvidia/gpu.h"
#include <cstdio>
#include <exception>
#include <string_view>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <tlhelp32.h>
int main(int argc, char** argv) {
    try {
        const bool hostsOnly = argc > 1 && std::string_view(argv[1]) == "--check-hosts";
        if (!hostsOnly) {
            const auto gpu = fourbyfour::selectGpu();
            if(argc>1&&std::string_view(argv[1])=="--architecture") {
                std::printf("%d",gpu.major*10+gpu.minor);return 0;
            }
            std::printf("Supported NVIDIA GPU: %s | SM %d.%d | VRAM %llu MiB | CUDA driver API %d\n",
                gpu.name.c_str(), gpu.major, gpu.minor, gpu.memory / (1024 * 1024), gpu.driverApi);
        }
        if (hostsOnly || (argc > 1 && std::string_view(argv[1]) == "--install")) {
            const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
            if (snapshot == INVALID_HANDLE_VALUE) { std::fprintf(stderr, "Could not check whether Adobe apps are running.\n"); return 20; }
            PROCESSENTRY32W process{}; process.dwSize = sizeof(process);
            bool running = false;
            if (Process32FirstW(snapshot, &process)) do {
                if (!_wcsicmp(process.szExeFile, L"AfterFX.exe") || !_wcsicmp(process.szExeFile, L"Adobe Premiere Pro.exe") ||
                    !_wcsicmp(process.szExeFile, L"Adobe Media Encoder.exe")) { running = true; break; }
            } while (Process32NextW(snapshot, &process));
            CloseHandle(snapshot);
            if (running) { std::fprintf(stderr, "Save your work and close Premiere, After Effects and Media Encoder before installing.\n"); return 20; }
        }
        return 0;
    } catch (const std::exception& error) { std::fprintf(stderr, "%s\n", error.what()); return 10; }
}
