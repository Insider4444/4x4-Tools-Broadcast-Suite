#pragma once
#include <windows.h>
#include <stdexcept>

namespace fourbyfour {
// Adobe may move a request between threads with different current CUDA
// contexts. GPU allocations/models must always use the context that owns them.
class CudaContext {
    HMODULE driver{};
    void* context{};
    int device{};
    int (__stdcall* push)(void*){};
    int (__stdcall* pop)(void**){};
    int (__stdcall* release)(int){};
public:
    explicit CudaContext(int ordinal):device(ordinal) {
        driver=LoadLibraryExW(L"nvcuda.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
        if(!driver) throw std::runtime_error("NVIDIA CUDA driver is unavailable.");
        auto retain=reinterpret_cast<int(__stdcall*)(void**,int)>(GetProcAddress(driver,"cuDevicePrimaryCtxRetain"));
        push=reinterpret_cast<decltype(push)>(GetProcAddress(driver,"cuCtxPushCurrent_v2"));
        pop=reinterpret_cast<decltype(pop)>(GetProcAddress(driver,"cuCtxPopCurrent_v2"));
        release=reinterpret_cast<decltype(release)>(GetProcAddress(driver,"cuDevicePrimaryCtxRelease_v2"));
        if(!retain||!push||!pop||!release||retain(&context,device)) {
            FreeLibrary(driver);driver=nullptr;throw std::runtime_error("Cannot retain the NVIDIA processing context.");
        }
    }
    ~CudaContext(){if(context)release(device);if(driver)FreeLibrary(driver);}
    CudaContext(const CudaContext&)=delete;
    class Guard {
        CudaContext& owner;
    public:
        explicit Guard(CudaContext& value):owner(value){if(owner.push(owner.context))throw std::runtime_error("Cannot enter the NVIDIA processing context.");}
        ~Guard(){void* previous{};owner.pop(&previous);}
    };
};
}
