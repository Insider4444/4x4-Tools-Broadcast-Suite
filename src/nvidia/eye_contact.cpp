#include "eye_contact.h"
#include "gpu.h"
#include "cuda_context.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include "nvAR.h"
#include "nvARGazeRedirection.h"
#include <atomic>
#include <cstring>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;
namespace {
// The SDK's inline NvCVImage constructors/destructors call these two C APIs.
// Forward them explicitly so the Adobe module has no mandatory NVIDIA imports.
std::atomic<decltype(&NvCVImage_Alloc)> imageAllocate{};
std::atomic<decltype(&NvCVImage_Dealloc)> imageDeallocate{};
std::atomic<HMODULE> imageLibrary{};
int moduleAnchor;
template<class T> T symbol(HMODULE module, const char* name) {
    auto result = reinterpret_cast<T>(GetProcAddress(module, name));
    if (!result) throw std::runtime_error(std::string("Missing NVIDIA API: ") + name);
    return result;
}
std::wstring environment(const wchar_t* name) {
    const DWORD n = GetEnvironmentVariableW(name, nullptr, 0);
    if (!n) return {};
    std::wstring value(n, L'\0'); GetEnvironmentVariableW(name, value.data(), n); value.resize(n - 1); return value;
}
fs::path moduleDirectory() {
    HMODULE module{};
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(&moduleAnchor), &module))
        throw std::runtime_error("Cannot locate the plugin module.");
    std::wstring buffer(32768, L'\0');
    const DWORD length = GetModuleFileNameW(module, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (!length || length >= buffer.size()) throw std::runtime_error("Cannot locate the plugin directory.");
    buffer.resize(length); return fs::path(buffer).parent_path();
}
std::string utf8(const fs::path& path) {
    const auto value = path.u8string(); return {reinterpret_cast<const char*>(value.data()), value.size()};
}
struct Api {
    std::vector<HMODULE> libraries;
    std::vector<DLL_DIRECTORY_COOKIE> directories;
    decltype(&NvAR_Create) create{}; decltype(&NvAR_Destroy) destroy{};
    decltype(&NvAR_Load) load{}; decltype(&NvAR_Run) run{};
    decltype(&NvAR_SetU32) setU32{}; decltype(&NvAR_SetS32) setS32{};
    decltype(&NvAR_SetString) setString{}; decltype(&NvAR_SetObject) setObject{};
    decltype(&NvAR_SetF32Array) setFloats{}; decltype(&NvAR_SetCudaStream) setStream{};
    decltype(&NvAR_CudaStreamCreate) streamCreate{}; decltype(&NvAR_CudaStreamDestroy) streamDestroy{};
    decltype(&NvAR_CudaStreamSynchronize) streamSync{};
    decltype(&NvCVImage_Alloc) alloc{}; decltype(&NvCVImage_Init) init{};
    decltype(&NvCVImage_Transfer) transfer{};
    int (*getDevice)(int*){}; int (*setDevice)(int){};
    using ErrorText = const char* (*)(NvCV_Status);
    ErrorText errorText{};
    HMODULE open(const fs::path& path) {
        if (!fs::is_regular_file(path)) throw std::runtime_error("Required NVIDIA runtime is missing: " + utf8(path));
        HMODULE module = LoadLibraryExW(path.c_str(), nullptr,
            LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32 | LOAD_LIBRARY_SEARCH_USER_DIRS);
        if (!module) throw std::runtime_error("Cannot load " + utf8(path.filename()) + " (Windows error " + std::to_string(GetLastError()) + ").");
        libraries.push_back(module); return module;
    }
    void close() noexcept {
        imageAllocate.store(nullptr); imageDeallocate.store(nullptr); imageLibrary.store(nullptr);
        for (auto i = libraries.rbegin(); i != libraries.rend(); ++i) FreeLibrary(*i);
        libraries.clear();
        for (auto directory : directories) RemoveDllDirectory(directory);
        directories.clear();
    }
    explicit Api(const fs::path& root) {
        try {
            for (const auto& path : {root / "bin", root / "features/nvarfaceboxdetection/bin",
                                    root / "features/nvarlandmarkdetection/bin", root / "features/nvargazeredirection/bin"}) {
                auto cookie = AddDllDirectory(path.c_str());
                if (!cookie) throw std::runtime_error("Cannot register NVIDIA runtime directory: " + utf8(path));
                directories.push_back(cookie);
            }
            auto cuda = open(root / "bin/cudart64_12.dll");
            getDevice = symbol<decltype(getDevice)>(cuda, "cudaGetDevice");
            setDevice = symbol<decltype(setDevice)>(cuda, "cudaSetDevice");
            auto image = open(root / "bin/NVCVImage.dll");
            alloc = symbol<decltype(alloc)>(image, "NvCVImage_Alloc");
            auto dealloc = symbol<decltype(&NvCVImage_Dealloc)>(image, "NvCVImage_Dealloc");
            init = symbol<decltype(init)>(image, "NvCVImage_Init");
            transfer = symbol<decltype(transfer)>(image, "NvCVImage_Transfer");
            errorText = symbol<ErrorText>(image, "NvCV_GetErrorStringFromCode");
            auto ar = open(root / "bin/nvARPose.dll");
#define BIND(member, apiName) member = symbol<decltype(member)>(ar, #apiName)
            BIND(create, NvAR_Create); BIND(destroy, NvAR_Destroy); BIND(load, NvAR_Load); BIND(run, NvAR_Run);
            BIND(setU32, NvAR_SetU32); BIND(setS32, NvAR_SetS32); BIND(setString, NvAR_SetString);
            BIND(setObject, NvAR_SetObject); BIND(setFloats, NvAR_SetF32Array); BIND(setStream, NvAR_SetCudaStream);
            BIND(streamCreate, NvAR_CudaStreamCreate); BIND(streamDestroy, NvAR_CudaStreamDestroy);
            BIND(streamSync, NvAR_CudaStreamSynchronize);
#undef BIND
            // Preload by absolute path; do not change the host's PATH or SetDllDirectory.
            open(root / "features/nvarfaceboxdetection/bin/nvARFaceBoxDetection.dll");
            open(root / "features/nvarlandmarkdetection/bin/nvARLandmarkDetection.dll");
            open(root / "features/nvargazeredirection/bin/nvARGazeRedirection.dll");
            imageAllocate.store(alloc); imageDeallocate.store(dealloc); imageLibrary.store(image);
        } catch (...) { close(); throw; }
    }
    ~Api() { close(); }
    void check(NvCV_Status status, const char* operation) const {
        if (status != NVCV_SUCCESS) throw std::runtime_error(std::string(operation) + ": " +
            (errorText ? errorText(status) : "NVIDIA SDK error") + " (" + std::to_string(int(status)) + ")");
    }
};
struct Engine {
    fs::path root; fourbyfour::Gpu gpu; fourbyfour::CudaContext context; Api api;
    NvAR_FeatureHandle feature{}; CUstream stream{};
    std::unique_ptr<NvCVImage> input, output, temporary;
    int width{}, height{}, eyeSize{};
    std::vector<NvAR_Point2f> landmarks, redirectedLandmarks;
    std::vector<float> confidence;
    float gaze[2]{}, translation[3]{};
    NvAR_Quaternion pose{}; NvAR_Point3f direction[2]{}; NvAR_Rect box{}; NvAR_BBoxes boxes{};
    explicit Engine(fs::path sdk) : root(std::move(sdk)), gpu(fourbyfour::selectGpu()), context(gpu.ordinal), api(root) {}
    void release() {
        fourbyfour::CudaContext::Guard guard(context);
        if (stream) api.streamSync(stream);
        if (feature) {const auto old=feature;feature=nullptr;try{api.destroy(old);}catch(...){}}
        temporary.reset(); output.reset(); input.reset();
        if (stream) { api.streamDestroy(stream); stream = nullptr; }
        width = height = eyeSize = 0;
    }
    ~Engine() { try { release(); } catch (...) {} }
    void configure(int w, int h, int size) {
        if (feature && width == w && height == h && eyeSize == size) return;
        release();
        fourbyfour::CudaContext::Guard guard(context);
        api.check(api.streamCreate(&stream), "Create CUDA stream");
        api.check(api.create(NvAR_Feature_GazeRedirection, &feature), "Create Eye Contact");
        const auto models = utf8(root / "bin/models");
        api.check(api.setString(feature, NvAR_Parameter_Config(ModelDir), models.c_str()), "Set model directory");
        api.check(api.setStream(feature, NvAR_Parameter_Config(CUDAStream), stream), "Set CUDA stream");
        api.check(api.setU32(feature, NvAR_Parameter_Config(Landmarks_Size), 126), "Set landmarks");
        api.check(api.setU32(feature, NvAR_Parameter_Config(Temporal), 0), "Disable temporal history");
        api.check(api.setU32(feature, NvAR_Parameter_Config(GazeRedirect), 1), "Enable redirection");
        api.check(api.setU32(feature, NvAR_Parameter_Config(EnableLookAway), 0), "Disable random look-away");
        api.check(api.setU32(feature, NvAR_Parameter_Config(EyeSizeSensitivity), size), "Set eye region size");
        api.check(api.load(feature), "Load Eye Contact models");
        input = std::make_unique<NvCVImage>(); output = std::make_unique<NvCVImage>();
        temporary = std::make_unique<NvCVImage>();
        api.check(api.alloc(input.get(), w, h, NVCV_BGR, NVCV_U8, NVCV_CHUNKY, NVCV_GPU, 1), "Allocate input image");
        api.check(api.alloc(output.get(), w, h, NVCV_BGR, NVCV_U8, NVCV_CHUNKY, NVCV_GPU, 1), "Allocate output image");
        api.check(api.setObject(feature, NvAR_Parameter_Input(Image), input.get(), sizeof(NvCVImage)), "Bind input image");
        api.check(api.setObject(feature, NvAR_Parameter_Output(Image), output.get(), sizeof(NvCVImage)), "Bind output image");
        api.check(api.setS32(feature, NvAR_Parameter_Input(Width), w), "Set width");
        api.check(api.setS32(feature, NvAR_Parameter_Input(Height), h), "Set height");
        landmarks.assign(126, {}); redirectedLandmarks.assign(24, {}); confidence.assign(126, 0);
        api.check(api.setObject(feature, NvAR_Parameter_Output(Landmarks), landmarks.data(), sizeof(NvAR_Point2f)), "Bind landmarks");
        api.check(api.setObject(feature, NvAR_Parameter_Output(GazeOutputLandmarks), redirectedLandmarks.data(), sizeof(NvAR_Point2f)), "Bind corrected landmarks");
        api.check(api.setFloats(feature, NvAR_Parameter_Output(LandmarksConfidence), confidence.data(), 126), "Bind confidence");
        api.check(api.setFloats(feature, NvAR_Parameter_Output(OutputGazeVector), gaze, 2), "Bind gaze");
        api.check(api.setFloats(feature, NvAR_Parameter_Output(OutputHeadTranslation), translation, 3), "Bind translation");
        api.check(api.setObject(feature, NvAR_Parameter_Output(HeadPose), &pose, sizeof(pose)), "Bind pose");
        api.check(api.setObject(feature, NvAR_Parameter_Output(GazeDirection), direction, sizeof(NvAR_Point3f)), "Bind gaze direction");
        boxes = {&box, 0, 1};
        api.check(api.setObject(feature, NvAR_Parameter_Output(BoundingBoxes), &boxes, sizeof(boxes)), "Bind face boxes");
        width = w; height = h; eyeSize = size;
    }
    std::vector<std::uint8_t> process(std::span<const std::uint8_t> bgr) {
        fourbyfour::CudaContext::Guard guard(context);
        std::vector<std::uint8_t> result(bgr.begin(), bgr.end());
        NvCVImage hostIn, hostOut;
        api.check(api.init(&hostIn, width, height, width * 3, const_cast<std::uint8_t*>(bgr.data()), NVCV_BGR, NVCV_U8, NVCV_CHUNKY, NVCV_CPU), "Describe source image");
        api.check(api.init(&hostOut, width, height, width * 3, result.data(), NVCV_BGR, NVCV_U8, NVCV_CHUNKY, NVCV_CPU), "Describe result image");
        api.check(api.transfer(&hostIn, input.get(), 1, stream, temporary.get()), "Upload image");
        api.check(api.transfer(input.get(), output.get(), 1, stream, nullptr), "Initialize output image");
        api.check(api.run(feature), "Run Eye Contact");
        api.check(api.transfer(output.get(), &hostOut, 1, stream, temporary.get()), "Download corrected image");
        api.check(api.streamSync(stream), "Wait for GPU");
        return result;
    }
};
std::mutex engineMutex;
std::unique_ptr<Engine> engine;
}

extern "C" NvCV_Status NvCV_API NvCVImage_Alloc(NvCVImage* image, unsigned w, unsigned h,
    NvCVImage_PixelFormat format, NvCVImage_ComponentType type, unsigned layout, unsigned space, unsigned alignment) {
    if (auto function = imageAllocate.load()) return function(image, w, h, format, type, layout, space, alignment);
    std::memset(image, 0, sizeof(*image)); return NVCV_ERR_LIBRARY;
}
extern "C" void NvCV_API NvCVImage_Dealloc(NvCVImage* image) {
    if (auto function = imageDeallocate.load())try{function(image);}catch(...){}
}
extern "C" void NvCV_API NvCVImage_InitView(NvCVImage* sub, NvCVImage* full, int x, int y, unsigned w, unsigned h) {
    auto module = imageLibrary.load();
    auto function = module ? reinterpret_cast<decltype(&NvCVImage_InitView)>(GetProcAddress(module, "NvCVImage_InitView")) : nullptr;
    if (function) function(sub, full, x, y, w, h);
}
extern "C" NvCV_Status NvCV_API NvCVImage_Transfer(const NvCVImage* src, NvCVImage* dst, float scale, CUstream stream, NvCVImage* temporary) {
    auto module = imageLibrary.load();
    auto function = module ? reinterpret_cast<decltype(&NvCVImage_Transfer)>(GetProcAddress(module, "NvCVImage_Transfer")) : nullptr;
    return function ? function(src, dst, scale, stream, temporary) : NVCV_ERR_LIBRARY;
}
extern "C" NvCV_Status NvCV_API NvCVImage_TransferRect(const NvCVImage* src, const NvCVRect2i* rect,
    NvCVImage* dst, const NvCVPoint2i* point, float scale, CUstream stream, NvCVImage* temporary) {
    auto module = imageLibrary.load();
    auto function = module ? reinterpret_cast<decltype(&NvCVImage_TransferRect)>(GetProcAddress(module, "NvCVImage_TransferRect")) : nullptr;
    return function ? function(src, rect, dst, point, scale, stream, temporary) : NVCV_ERR_LIBRARY;
}

namespace fourbyfour {
fs::path runtimeRoot() {
    const auto configured = environment(L"FOURBYFOUR_AR_SDK_ROOT");
    if (!configured.empty()) return fs::absolute(fs::path(configured));
    const auto directory = moduleDirectory();
    std::wstring value(32768, L'\0');
    const DWORD n = GetPrivateProfileStringW(L"NVIDIA", L"ARSDK_ROOT", L"", value.data(),
        static_cast<DWORD>(value.size()), (directory / "4x4EyeContact.ini").c_str());
    if (n) { value.resize(n); return fs::absolute(fs::path(value)); }
    return directory / "runtime/ARSDK";
}
std::vector<std::uint8_t> correctEyes(std::span<const std::uint8_t> bgr, int width, int height,
                                    const EyeSettings& settings, const fs::path& sdk) {
    if (width <= 0 || height <= 0 || width > 32768 || height > 32768 ||
        bgr.size() != static_cast<std::size_t>(width) * height * 3 || settings.eyeSize < 2 || settings.eyeSize > 5)
        throw std::invalid_argument("Invalid Eye Contact frame or eye-region size.");
    std::lock_guard lock(engineMutex);
    const auto root = sdk.empty() ? runtimeRoot() : fs::absolute(sdk);
    try {
        if (!engine || engine->root != root) { engine.reset(); engine = std::make_unique<Engine>(root); }
        engine->configure(width, height, settings.eyeSize);
        return engine->process(bgr);
    } catch (...) { engine.reset(); throw; }
}
void shutdownEyeContact() { std::lock_guard lock(engineMutex); engine.reset(); }
}
