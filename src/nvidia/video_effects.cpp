#include "video_effects.h"
#include "gpu.h"
#include "runtime_path.h"
#include "cuda_context.h"
#include "core/processing_status.h"
#include "nvVideoEffects.h"
#include "nvVFXGreenScreen.h"
#include "nvVFXBackgroundBlur.h"
#include "nvVFXDenoising.h"
#include "nvVFXRelighting.h"
#include "nvVFXVideoSuperRes.h"
#include "nvVFXUpscale.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <memory>
#include <mutex>

namespace fs = std::filesystem;
using fourbyfour::runtime::symbol;
using fourbyfour::runtime::utf8;
namespace {
int anchor;
std::atomic<HMODULE> imageModule{};
struct Api {
    fourbyfour::runtime::Libraries libraries;
    decltype(&NvVFX_CreateEffect) create{}; decltype(&NvVFX_DestroyEffect) destroy{};
    decltype(&NvVFX_SetU32) setU32{}; decltype(&NvVFX_SetF32) setF32{};
    decltype(&NvVFX_SetString) setString{}; decltype(&NvVFX_SetImage) setImage{};
    decltype(&NvVFX_SetCudaStream) setStream{};
    decltype(&NvVFX_SetStateObjectHandleArray) setState{};
    decltype(&NvVFX_AllocateState) allocateState{}; decltype(&NvVFX_DeallocateState) deallocateState{};
    decltype(&NvVFX_ResetState) resetState{};
    decltype(&NvVFX_Load) load{}; decltype(&NvVFX_Run) run{};
    decltype(&NvVFX_CudaStreamCreate) streamCreate{}; decltype(&NvVFX_CudaStreamDestroy) streamDestroy{};
    decltype(&NvVFX_CudaStreamSynchronize) streamSync{};
    decltype(&NvCVImage_Init) init{}; decltype(&NvCVImage_Alloc) alloc{};
    decltype(&NvCVImage_Transfer) transfer{};
    int (*getDevice)(int*){}; int (*setDevice)(int){};
    const char* (*errorText)(NvCV_Status){};
    explicit Api(const fs::path& root) {
        libraries.add(root / "bin");
        for (const auto* feature : {"nvvfxgreenscreen", "nvvfxbackgroundblur", "nvvfxdenoising", "nvvfxrelighting", "nvvfxvideosuperres", "nvvfxupscale"})
            libraries.add(root / "features" / feature / "bin");
        auto cuda = libraries.open(root / "bin/cudart64_12.dll");
        getDevice = symbol<decltype(getDevice)>(cuda, "cudaGetDevice");
        setDevice = symbol<decltype(setDevice)>(cuda, "cudaSetDevice");
        auto image = libraries.open(root / "bin/NVCVImage.dll");
        init = symbol<decltype(init)>(image, "NvCVImage_Init"); alloc = symbol<decltype(alloc)>(image, "NvCVImage_Alloc");
        transfer = symbol<decltype(transfer)>(image, "NvCVImage_Transfer");
        errorText = symbol<decltype(errorText)>(image, "NvCV_GetErrorStringFromCode");
        auto video = libraries.open(root / "bin/NVVideoEffects.dll");
        if (GetEnvironmentVariableW(L"FOURBYFOUR_SDK_LOG", nullptr, 0)) {
            const auto logger = symbol<decltype(&NvVFX_ConfigureLogger)>(video, "NvVFX_ConfigureLogger");
            logger(NVCV_LOG_INFO, "stderr", nullptr, nullptr);
        }
#define BIND(member, name) member = symbol<decltype(member)>(video, #name)
        BIND(create, NvVFX_CreateEffect); BIND(destroy, NvVFX_DestroyEffect);
        BIND(setU32, NvVFX_SetU32); BIND(setF32, NvVFX_SetF32); BIND(setString, NvVFX_SetString);
        BIND(setImage, NvVFX_SetImage); BIND(setStream, NvVFX_SetCudaStream);
        BIND(setState, NvVFX_SetStateObjectHandleArray); BIND(allocateState, NvVFX_AllocateState);
        BIND(deallocateState, NvVFX_DeallocateState); BIND(resetState, NvVFX_ResetState);
        BIND(load, NvVFX_Load); BIND(run, NvVFX_Run);
        BIND(streamCreate, NvVFX_CudaStreamCreate); BIND(streamDestroy, NvVFX_CudaStreamDestroy);
        BIND(streamSync, NvVFX_CudaStreamSynchronize);
#undef BIND
        for (const auto& item : {std::pair{"nvvfxgreenscreen", "NVVFXGreenScreen.dll"},
             {"nvvfxbackgroundblur", "NVVFXBackgroundBlur.dll"}, {"nvvfxdenoising", "NVVFXDenoising.dll"},
             {"nvvfxrelighting", "NVVFXRelighting.dll"}, {"nvvfxvideosuperres", "NVVFXVideoSuperRes.dll"}, {"nvvfxupscale", "NVVFXUpscale.dll"}})
            libraries.open(root / "features" / item.first / "bin" / item.second);
        imageModule.store(image);
    }
    ~Api() { imageModule.store(nullptr); }
    void check(NvCV_Status status, const char* operation) const {
        if (status != NVCV_SUCCESS) throw std::runtime_error(std::string(operation) + ": " +
            (errorText ? errorText(status) : "NVIDIA error") + " (" + std::to_string(int(status)) + ")");
    }
};
struct Filter {
    Api& api; NvVFX_Handle handle{}; NvVFX_StateObjectHandle state{};
    explicit Filter(Api& a) : api(a) {}
    ~Filter() { clear(); }
    void clear() noexcept {
        const auto oldHandle=handle;const auto oldState=state;handle=nullptr;state=nullptr;
        // A vendor C++ exception during cleanup must not terminate Adobe from
        // an implicitly-noexcept destructor while handling the original error.
        if(oldHandle&&oldState)try{api.deallocateState(oldHandle,oldState);}catch(...){}
        if(oldHandle)try{api.destroy(oldHandle);}catch(...){}
    }
    void create(const char* name, const fs::path& root, CUstream stream) {
        clear(); api.check(api.create(name, &handle), name);
        const std::string selector(name);
        if (selector == NVVFX_FX_GREEN_SCREEN || selector == NVVFX_FX_DENOISING || selector == NVVFX_FX_RELIGHTING) {
            api.check(api.setString(handle, NVVFX_MODEL_DIRECTORY, utf8(root / "bin/models").c_str()), "Set video models");
            // Runtime caches must never be written into the read-only installation.
            api.check(api.setU32(handle, NVVFX_MODEL_CACHE_MODE, 1), "Disable installation-directory cache");
        }
        api.check(api.setStream(handle, NVVFX_CUDA_STREAM, stream), "Set video CUDA stream");
    }
    void image(const char* key, NvCVImage& value) { api.check(api.setImage(handle, key, &value), "Bind video image"); }
    void load(bool stateful, bool stateBeforeLoad = false) {
        if (!stateBeforeLoad) api.check(api.load(handle), "Load video model");
        if (stateful) {
            api.check(api.allocateState(handle, &state), "Allocate video state");
            api.check(api.setState(handle, NVVFX_STATE, &state), "Bind video state");
        }
        if (stateBeforeLoad) api.check(api.load(handle), "Load stateful video model");
    }
    void run(bool reset = true) {
        if (state && reset) {
            api.check(api.resetState(handle, state), "Reset video state");
            api.check(api.setState(handle, NVVFX_STATE, &state), "Rebind reset video state");
        }
        api.check(api.run(handle, 0), "Process video");
    }
};
struct Engine {
    fs::path root; fourbyfour::Gpu gpu; fourbyfour::CudaContext context; Api api;
    CUstream stream{};
    Filter segment{api}, filter{api};
    std::unique_ptr<NvCVImage> input, output, mask, projection, temp;
    NvCVImage environment;
    std::vector<float> hdr;
    int width{}, height{}, outWidth{}, outHeight{}, selectedMode{-1};
    fourbyfour::VideoEffect selected{fourbyfour::VideoEffect::matte};
    fourbyfour::VideoSettings previous{};
    explicit Engine(fs::path sdk) : root(std::move(sdk)), gpu(fourbyfour::selectGpu()), context(gpu.ordinal), api(root) {
        fourbyfour::CudaContext::Guard guard(context); api.check(api.streamCreate(&stream), "Create video stream");
    }
    void clear() {
        if (stream) api.streamSync(stream);
        filter.clear(); segment.clear();
        output.reset(); mask.reset(); projection.reset(); input.reset(); temp.reset();
        width = height = outWidth = outHeight = 0;
    }
    ~Engine() {
        try { fourbyfour::CudaContext::Guard guard(context); clear(); if (stream) api.streamDestroy(stream); } catch (...) {}
    }
    std::unique_ptr<NvCVImage> image(int w, int h, NvCVImage_PixelFormat format,
                                     NvCVImage_ComponentType type, unsigned planar = NVCV_CHUNKY) {
        auto result = std::make_unique<NvCVImage>();
        api.check(api.alloc(result.get(), w, h, format, type, planar, NVCV_GPU, 1), "Allocate video image"); return result;
    }
    void configure(fourbyfour::VideoEffect effect, int w, int h, const fourbyfour::VideoSettings& s) {
        using E = fourbyfour::VideoEffect;
        const int mode = (s.keepChair ? 0 : 2) + (s.preview ? 1 : 0);
        const int ow = s.outputWidth ? s.outputWidth : w, oh = s.outputHeight ? s.outputHeight : h;
        if (input && selected == effect && width == w && height == h && outWidth == ow && outHeight == oh && selectedMode == mode &&
            (effect != E::denoise || previous.strength == s.strength)) {
            if (effect == E::relight) {
                const bool sequential=s.preview&&s.contextId&&s.contextId==previous.contextId&&s.timeStep>0&&s.time==previous.time+s.timeStep&&
                    s.lightAzimuth==previous.lightAzimuth&&s.lightElevation==previous.lightElevation&&s.lightExposure==previous.lightExposure&&s.lightWarmth==previous.lightWarmth;
                if(!sequential) configureRelight(s);
            }
            return;
        }
        clear(); width = w; height = h; outWidth = ow; outHeight = oh; selected = effect; selectedMode = mode;
        fourbyfour::processing::report(s.statusEffect,L"Loading NVIDIA picture models…");
        const bool segmentation = effect == E::matte || effect == E::blur || effect == E::relight;
        const bool floating = effect == E::denoise;
        const bool vsr = effect == E::superResolution;
        const bool rgba = vsr || effect == E::upscale;
        input = image(w, h, rgba ? NVCV_RGBA : NVCV_BGR, floating ? NVCV_F32 : NVCV_U8, floating ? NVCV_PLANAR : NVCV_CHUNKY);
        temp = std::make_unique<NvCVImage>();
        if (segmentation) {
            mask = image(w, h, NVCV_A, NVCV_U8);
            segment.create(NVVFX_FX_GREEN_SCREEN, root, stream);
            api.check(api.setU32(segment.handle, NVVFX_MODE, mode), "Set matte quality");
            api.check(api.setU32(segment.handle, NVVFX_MAX_INPUT_WIDTH, w), "Set matte width");
            api.check(api.setU32(segment.handle, NVVFX_MAX_INPUT_HEIGHT, h), "Set matte height");
            segment.image(NVVFX_INPUT_IMAGE, *input); segment.image(NVVFX_OUTPUT_IMAGE, *mask); segment.load(true);
        }
        if (effect == E::matte) return;
        output = image(ow, oh, rgba ? NVCV_RGBA : NVCV_BGR, floating ? NVCV_F32 : NVCV_U8, floating ? NVCV_PLANAR : NVCV_CHUNKY);
        if (effect == E::relight) { configureRelight(s); return; }
        const char* name = effect == E::blur ? NVVFX_FX_BGBLUR : effect == E::relight ? NVVFX_FX_RELIGHTING :
                           effect == E::denoise ? NVVFX_FX_DENOISING : effect == E::upscale ? NVVFX_FX_SR_UPSCALE : NVVFX_FX_VIDEO_SUPER_RES;
        filter.create(name, root, stream);
        if (vsr) {
            api.check(api.setU32(filter.handle, NVVFX_IMAGE_ENCODING_MODE, NVVFX_IMAGE_ENCODING_RGB8), "Set super-resolution encoding");
            const bool resize = w != ow || h != oh;
            api.check(api.setU32(filter.handle, NVVFX_QUALITY_LEVEL, resize ? (s.preview ? 1 : 4) : (s.preview ? 16 : 19)), "Set super-resolution quality");
        }
        filter.image(NVVFX_INPUT_IMAGE, *input); filter.image(NVVFX_OUTPUT_IMAGE, *output);
        if (effect == E::blur || effect == E::relight) filter.image(NVVFX_INPUT_IMAGE_1, *mask);
        if (effect == E::denoise) api.check(api.setF32(filter.handle, NVVFX_STRENGTH, s.strength), "Set denoise mode before loading");
        filter.load(effect == E::denoise, effect == E::denoise);
    }
    void configureRelight(const fourbyfour::VideoSettings& s) {
        // This SDK version keeps an internal previous-frame image that its
        // public ResetState API does not reset correctly. A fresh relighting
        // handle per image makes timeline order independent; keep the shared
        // DLLs, segmentation model and GPU image allocations warm.
        filter.create(NVVFX_FX_RELIGHTING, root, stream);
        filter.image(NVVFX_INPUT_IMAGE_0, *input); filter.image(NVVFX_INPUT_IMAGE_1, *mask);
        filter.image(NVVFX_OUTPUT_IMAGE_0, *output);
        hdr.resize(512 * 256 * 3); studioLight(s);
        api.check(api.init(&environment, 512, 256, 512 * 3 * sizeof(float), hdr.data(), NVCV_RGB, NVCV_F32, NVCV_CHUNKY, NVCV_CPU), "Describe studio light");
        filter.image(NVVFX_INPUT_IMAGE_2, environment);
        if(!projection) projection=image(width,height,NVCV_BGR,NVCV_U8);
        filter.image(NVVFX_OUTPUT_IMAGE_1, *projection);
        api.check(api.setF32(filter.handle,NVVFX_ANGLE_VFOV,1.04719755f),"Set relighting field of view");
        filter.load(false);
    }
    void studioLight(const fourbyfour::VideoSettings& s) {
        constexpr float pi = 3.141592654f;
        const float az = s.lightAzimuth * pi / 180.f, el = s.lightElevation * pi / 180.f;
        const float lx = std::cos(el) * std::sin(az), ly = std::sin(el), lz = std::cos(el) * std::cos(az);
        const float exposure = std::exp2(s.lightExposure), warmth = s.lightWarmth / 100.f;
        for (int y = 0; y < 256; ++y) for (int x = 0; x < 512; ++x) {
            const float phi = (float(x) + .5f) * 2 * pi / 512 - pi, theta = (float(y) + .5f) * pi / 256;
            const float dot = std::sin(theta) * std::sin(phi) * lx + std::cos(theta) * ly + std::sin(theta) * std::cos(phi) * lz;
            const float key = std::exp((dot - 1.f) / .045f) * 12.f;
            const float fill = .3f + .12f * std::max(0.f, std::cos(theta));
            const auto p = (static_cast<std::size_t>(y) * 512 + x) * 3;
            hdr[p] = exposure * (fill + key * (1.f + .35f * warmth));
            hdr[p + 1] = exposure * (fill + key);
            hdr[p + 2] = exposure * (fill + key * (1.f - .35f * warmth));
        }
    }
    fourbyfour::VideoResult process(std::span<const float> bgr, const fourbyfour::VideoSettings& s,bool reset = true) {
        using E = fourbyfour::VideoEffect;
        fourbyfour::CudaContext::Guard guard(context);
        NvCVImage host;
        api.check(api.init(&host, width, height, width * 3 * sizeof(float), const_cast<float*>(bgr.data()), NVCV_BGR, NVCV_F32, NVCV_CHUNKY, NVCV_CPU), "Describe input image");
        const bool floating = selected == E::denoise;
        api.check(api.transfer(&host, input.get(), floating ? 1.f : 255.f, stream, temp.get()), "Upload video image");
        fourbyfour::processing::report(s.statusEffect,L"Processing the NVIDIA model…");
        if (mask) segment.run(reset);
        if (selected == E::relight) { studioLight(s); filter.image(NVVFX_INPUT_IMAGE_2, environment); }
        if (output && width == outWidth && height == outHeight)
            api.check(api.transfer(input.get(), output.get(), 1.f, stream, temp.get()), "Initialize untouched video output");
        if (selected == E::blur) api.check(api.setF32(filter.handle, NVVFX_STRENGTH, s.strength), "Set background blur amount");
        if (selected == E::denoise || selected == E::upscale || selected == E::superResolution)
            api.check(api.setF32(filter.handle, NVVFX_STRENGTH, s.strength), "Set enhancement amount");
        if (filter.handle) filter.run(reset);
        fourbyfour::VideoResult result{outWidth, outHeight};
        if (mask) {
            result.matte.resize(static_cast<std::size_t>(width) * height);
            NvCVImage matteHost;
            api.check(api.init(&matteHost, width, height, width * sizeof(float), result.matte.data(), NVCV_A, NVCV_F32, NVCV_CHUNKY, NVCV_CPU), "Describe output matte");
            api.check(api.transfer(mask.get(), &matteHost, 1.f / 255.f, stream, temp.get()), "Download matte");
        }
        if (output) {
            result.bgr.resize(static_cast<std::size_t>(outWidth) * outHeight * 3);
            NvCVImage destination;
            api.check(api.init(&destination, outWidth, outHeight, outWidth * 3 * sizeof(float), result.bgr.data(), NVCV_BGR, NVCV_F32, NVCV_CHUNKY, NVCV_CPU), "Describe output image");
            api.check(api.transfer(output.get(), &destination, floating ? 1.f : 1.f / 255.f, stream, temp.get()), "Download video image");
        }
        api.check(api.streamSync(stream), "Synchronize video output");
        previous=s;
        return result;
    }
};
std::mutex mutex;
std::unique_ptr<Engine> engine;
}
// The SDK exports inline NvCVImage methods that require these forwarding APIs.
extern "C" NvCV_Status NvCV_API NvCVImage_Alloc(NvCVImage* i, unsigned w, unsigned h, NvCVImage_PixelFormat f, NvCVImage_ComponentType t, unsigned p, unsigned m, unsigned a) {
    const auto mod = imageModule.load(); const auto fn = mod ? reinterpret_cast<decltype(&NvCVImage_Alloc)>(GetProcAddress(mod, "NvCVImage_Alloc")) : nullptr;
    return fn ? fn(i,w,h,f,t,p,m,a) : NVCV_ERR_LIBRARY;
}
extern "C" void NvCV_API NvCVImage_Dealloc(NvCVImage* i) {
    const auto mod = imageModule.load(); const auto fn = mod ? reinterpret_cast<decltype(&NvCVImage_Dealloc)>(GetProcAddress(mod, "NvCVImage_Dealloc")) : nullptr;
    if(fn)try{fn(i);}catch(...){}
}
extern "C" void NvCV_API NvCVImage_InitView(NvCVImage* i, NvCVImage* f, int x, int y, unsigned w, unsigned h) {
    const auto mod = imageModule.load(); const auto fn = mod ? reinterpret_cast<decltype(&NvCVImage_InitView)>(GetProcAddress(mod, "NvCVImage_InitView")) : nullptr;
    if (fn) fn(i,f,x,y,w,h);
}
extern "C" NvCV_Status NvCV_API NvCVImage_Transfer(const NvCVImage* s, NvCVImage* d, float scale, CUstream stream, NvCVImage* t) {
    const auto mod = imageModule.load(); const auto fn = mod ? reinterpret_cast<decltype(&NvCVImage_Transfer)>(GetProcAddress(mod, "NvCVImage_Transfer")) : nullptr;
    return fn ? fn(s,d,scale,stream,t) : NVCV_ERR_LIBRARY;
}
extern "C" NvCV_Status NvCV_API NvCVImage_TransferRect(const NvCVImage* s, const NvCVRect2i* r, NvCVImage* d, const NvCVPoint2i* p, float scale, CUstream stream, NvCVImage* t) {
    const auto mod = imageModule.load(); const auto fn = mod ? reinterpret_cast<decltype(&NvCVImage_TransferRect)>(GetProcAddress(mod, "NvCVImage_TransferRect")) : nullptr;
    return fn ? fn(s,r,d,p,scale,stream,t) : NVCV_ERR_LIBRARY;
}
namespace fourbyfour {
fs::path videoRuntimeRoot() { return runtime::root(&anchor, L"VFXSDK_ROOT", L"FOURBYFOUR_VFX_SDK_ROOT", L"VideoFX"); }
VideoResult processVideo(VideoEffect effect, std::span<const float> bgr, int w, int h, const VideoSettings& settings, const fs::path& sdk) {
    if (w < 1 || h < 1 || w > 16384 || h > 16384 || bgr.size() != static_cast<std::size_t>(w) * h * 3 ||
        !std::isfinite(settings.strength) || settings.strength < 0 || settings.strength > 1)
        throw std::invalid_argument("Invalid video image or strength.");
    std::lock_guard lock(mutex);
    const auto root = sdk.empty() ? videoRuntimeRoot() : fs::absolute(sdk);
    try {
        if (!engine || engine->root != root) { engine.reset(); engine = std::make_unique<Engine>(root); }
        CudaContext::Guard guard(engine->context);
        engine->configure(effect,w,h,settings);
        return engine->process(bgr,settings);
    } catch (...) { engine.reset(); throw; }
}
void shutdownVideo() { std::lock_guard lock(mutex); engine.reset(); }
VideoResult processVideoWindow(VideoEffect effect,std::span<const std::vector<float>> frames,int w,int h,const VideoSettings& settings,const fs::path& sdk) {
    if(frames.empty()||frames.size()>5||w<1||h<1||w>16384||h>16384||!std::isfinite(settings.strength)||settings.strength<0||settings.strength>1)
        throw std::invalid_argument("Invalid video context window.");
    for(const auto& frame:frames) if(frame.size()!=static_cast<std::size_t>(w)*h*3) throw std::invalid_argument("Context images must have matching dimensions.");
    std::lock_guard lock(mutex);
    const auto root=sdk.empty()?videoRuntimeRoot():fs::absolute(sdk);
    try {
        if(!engine||engine->root!=root) {engine.reset();engine=std::make_unique<Engine>(root);}
        CudaContext::Guard guard(engine->context);
        engine->configure(effect,w,h,settings);
        VideoResult result;
        for(std::size_t i=0;i<frames.size();++i) result=engine->process(frames[i],settings,i==0);
        return result;
    } catch(...) {engine.reset();throw;}
}
}
