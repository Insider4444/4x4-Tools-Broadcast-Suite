#include "audio_effects.h"
#include "gpu.h"
#include "runtime_path.h"
#include "cuda_context.h"
#include "core/processing_status.h"
#include "nvAudioEffects.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <mutex>
#include <stdexcept>

namespace fs=std::filesystem;
using fourbyfour::runtime::symbol;
using fourbyfour::runtime::utf8;
namespace {
int anchor;
struct Api {
    fourbyfour::runtime::Libraries libraries;
    decltype(&NvAFX_CreateEffect) create{}; decltype(&NvAFX_DestroyEffect) destroy{};
    decltype(&NvAFX_SetU32) setU32{}; decltype(&NvAFX_SetFloat) setFloat{}; decltype(&NvAFX_SetString) setString{};
    decltype(&NvAFX_GetU32) getU32{};
    decltype(&NvAFX_Load) load{}; decltype(&NvAFX_Run) run{}; decltype(&NvAFX_Reset) reset{};
    decltype(&NvAFX_GetSupportedDevices) supported{};
    explicit Api(const fs::path& root) {
        libraries.add(root/"bin");
        for(const auto* path:{"bin/external/cuda/bin","bin/external/nvtrt/bin","bin/external/openssl/bin"}) libraries.add(root/path);
        for(const auto* feature:{"nvafxdenoiser","nvafxdereverb","nvafxdereverbdenoiser","nvafxstudiovoicelowlatency","nvafxstudiovoicehighquality","nvafxsuperres"})
            libraries.add(root/"features"/feature/"bin");
        auto audio=libraries.open(root/"bin/NVAudioEffects.dll");
#define BIND(member,name) member=symbol<decltype(member)>(audio,#name)
        BIND(create,NvAFX_CreateEffect); BIND(destroy,NvAFX_DestroyEffect); BIND(setU32,NvAFX_SetU32);
        BIND(setFloat,NvAFX_SetFloat); BIND(setString,NvAFX_SetString); BIND(getU32,NvAFX_GetU32);
        BIND(load,NvAFX_Load); BIND(run,NvAFX_Run); BIND(reset,NvAFX_Reset); BIND(supported,NvAFX_GetSupportedDevices);
#undef BIND
        for(const auto* feature:{"nvafxdenoiser","nvafxdereverb","nvafxdereverbdenoiser","nvafxstudiovoicelowlatency","nvafxstudiovoicehighquality","nvafxsuperres"})
            libraries.open(root/"features"/feature/"bin"/(std::string(feature)+".dll"));
    }
    void check(NvAFX_Status status,const char* action) const {
        if(status!=NVAFX_STATUS_SUCCESS) throw std::runtime_error(std::string(action)+" failed (NVIDIA audio status "+std::to_string(int(status))+").");
    }
};
std::mutex apiMutex;
std::map<fs::path,std::weak_ptr<Api>> apis;
std::shared_ptr<Api> acquire(const fs::path& path) {
    std::lock_guard lock(apiMutex);
    if(auto result=apis[path].lock()) return result;
    auto result=std::make_shared<Api>(path); apis[path]=result; return result;
}
struct Description { const char* selector; const char* feature; const char* model; int inputRate; };
constexpr std::array<Description,6> descriptions{{
    {"denoiser","nvafxdenoiser","denoiser_v2_48k.trtpkg",48000},
    {"dereverb","nvafxdereverb","dereverb_48k.trtpkg",48000},
    {"dereverb_denoiser","nvafxdereverbdenoiser","dereverb_denoiser_48k.trtpkg",48000},
    {"studio_voice_low_latency","nvafxstudiovoicelowlatency","studio_voice_low_latency_48k.trtpkg",48000},
    {"studio_voice_high_quality","nvafxstudiovoicehighquality","studio_voice_high_quality_48k.trtpkg",48000},
    {"superres","nvafxsuperres","superres_16kto48k.trtpkg",16000}
}};
}
namespace fourbyfour {
fs::path audioRuntimeRoot() { return runtime::root(&anchor,L"AFXSDK_ROOT",L"FOURBYFOUR_AFX_SDK_ROOT",L"AudioFX"); }
struct AudioEngine::Impl {
    fs::path root; Gpu gpu; CudaContext context;
    std::shared_ptr<Api> api;
    AudioSettings settings;
    std::vector<NvAFX_Handle> handles;
    std::vector<bool> receivedSignal;
    unsigned inRate{},outRate{},inFrames{},outFrames{};
    std::mutex mutex;
    int statusId()const{return settings.statusEffect>=0?settings.statusEffect:processing::AudioBase+static_cast<int>(settings.effect);}
    Impl(const AudioSettings& s,const fs::path& sdk):root(sdk.empty()?audioRuntimeRoot():fs::absolute(sdk)),gpu(selectGpu()),context(gpu.ordinal),api(acquire(root)),settings(s) {
        CudaContext::Guard guard(context);
        processing::Status pending;
        if(!processing::read(statusId(),pending)||!pending.active)
            processing::begin(statusId(),s.effect==AudioEffect::studioQuality?L"Highest Quality · six-second audio blocks":L"Interactive speech processing");
        processing::report(statusId(),L"Loading NVIDIA voice models…");
        if(s.channels<1||s.channels>2||static_cast<unsigned>(s.effect)>=descriptions.size()||s.micProfile < -1||s.micProfile>5||!std::isfinite(s.intensity)||s.intensity<0||s.intensity>1)
            throw std::invalid_argument("Audio effects support mono/stereo and microphone profiles -1 through 5.");
        receivedSignal.assign(s.channels,false);
        const auto& desc=descriptions[static_cast<unsigned>(s.effect)];
        const std::string architecture=gpu.major==7?"turing":gpu.major==12?"blackwell":gpu.minor==9?"ada":"ampere";
        const auto model=root/"features"/desc.feature/"models"/architecture/desc.model;
        if(!fs::is_regular_file(model)) throw std::runtime_error("Missing audio model: "+utf8(model));
        try {
            for(int c=0;c<s.channels;++c) {
                if(processing::cancelled(statusId()))throw std::runtime_error("Voice model preparation cancelled.");
                NvAFX_Handle handle{}; api->check(api->create(desc.selector,&handle),"Create voice effect");
                handles.push_back(handle);
                api->check(api->setU32(handle,NVAFX_PARAM_USER_CUDA_CONTEXT,1),"Share the managed Adobe processing context");
                api->check(api->setString(handle,NVAFX_PARAM_MODEL_PATH,utf8(model).c_str()),"Set voice model");
                api->check(api->setU32(handle,NVAFX_PARAM_INPUT_SAMPLE_RATE,desc.inputRate),"Set voice input rate");
                api->check(api->setU32(handle,NVAFX_PARAM_OUTPUT_SAMPLE_RATE,48000),"Set voice output rate");
                if(s.effect==AudioEffect::noise) api->check(api->setU32(handle,NVAFX_PARAM_EFFECT_VERSION,2),"Enable denoiser v2");
                const auto intensity=api->setFloat(handle,NVAFX_PARAM_INTENSITY_RATIO,s.intensity);
                if(intensity!=NVAFX_STATUS_SUCCESS&&(intensity!=NVAFX_STATUS_INVALID_PARAM||s.intensity!=1.f))api->check(intensity,"Set voice intensity");
                if(s.effect==AudioEffect::studioPreview) {
                    const auto profiles=root/"features"/desc.feature/"models"/architecture/"studio_voice_mic_profiles_48k.trtpkg";
                    api->check(api->setString(handle,"mic_profile_model",utf8(profiles).c_str()),"Set microphone profiles");
                }
                int count{}; const auto probe=api->supported(handle,&count,nullptr);
                if(probe!=NVAFX_STATUS_OUTPUT_BUFFER_TOO_SMALL&&probe!=NVAFX_STATUS_SUCCESS) api->check(probe,"Query supported audio GPUs");
                if(count<1||count>128) throw std::runtime_error("No compatible audio GPU was found.");
                std::vector<int> supported(static_cast<std::size_t>(count));
                api->check(api->supported(handle,&count,supported.data()),"Query supported audio GPUs");
                if(std::find(supported.begin(),supported.end(),gpu.ordinal)==supported.end()) throw std::runtime_error("This GPU is not supported by the voice model.");
                processing::report(statusId(),s.channels==2?(c?L"Loading right-channel model…":L"Loading left-channel model…"):L"Loading mono voice model…",c,s.channels);
                api->check(api->load(handle),"Load voice model");
                if(processing::cancelled(statusId()))throw std::runtime_error("Voice model preparation cancelled after the current model step.");
                unsigned inCh{},outCh{},ir{},orr{},inf{},outf{};
                api->check(api->getU32(handle,NVAFX_PARAM_NUM_INPUT_CHANNELS,&inCh),"Query voice channels");
                api->check(api->getU32(handle,NVAFX_PARAM_NUM_OUTPUT_CHANNELS,&outCh),"Query voice channels");
                api->check(api->getU32(handle,NVAFX_PARAM_INPUT_SAMPLE_RATE,&ir),"Query voice sample rate");
                api->check(api->getU32(handle,NVAFX_PARAM_OUTPUT_SAMPLE_RATE,&orr),"Query voice sample rate");
                api->check(api->getU32(handle,NVAFX_PARAM_NUM_SAMPLES_PER_INPUT_FRAME,&inf),"Query voice block size");
                api->check(api->getU32(handle,NVAFX_PARAM_NUM_SAMPLES_PER_OUTPUT_FRAME,&outf),"Query voice block size");
                if(inCh!=1||outCh!=1||ir!=desc.inputRate||orr!=48000||inf==0||outf==0||inf>48000*8||outf>48000*8||
                   (c&&(ir!=inRate||orr!=outRate||inf!=inFrames||outf!=outFrames))) throw std::runtime_error("Unexpected voice model I/O contract.");
                inRate=ir;outRate=orr;inFrames=inf;outFrames=outf;
            }
            profile(s.micProfile);
            processing::finish(statusId(),L"Voice model ready");
        } catch(...) { clear(); throw; }
    }
    void profile(int id) {
        if(settings.effect!=AudioEffect::studioPreview) return;
        for(auto handle:handles) api->check(api->setU32(handle,"mic_profile_id",static_cast<unsigned>(id)),"Set microphone character");
    }
    void clear() noexcept {for(auto h:handles)try{api->destroy(h);}catch(...){}handles.clear();}
    ~Impl() {try{CudaContext::Guard guard(context);clear();processing::finish(statusId(),L"Inactive · GPU models released");}catch(...) {}}
};
AudioEngine::AudioEngine(const AudioSettings& s,const fs::path& root):impl(std::make_unique<Impl>(s,root)) {}
AudioEngine::~AudioEngine()=default;
int AudioEngine::inputRate() const { return impl->inRate; } int AudioEngine::outputRate() const { return impl->outRate; }
int AudioEngine::inputFrames() const { return impl->inFrames; } int AudioEngine::outputFrames() const { return impl->outFrames; }
int AudioEngine::channels() const { return impl->settings.channels; }
void AudioEngine::reset() {
    std::lock_guard lock(impl->mutex);
    CudaContext::Guard guard(impl->context);
    for(auto handle:impl->handles) impl->api->check(impl->api->reset(handle),"Reset voice history");
    std::fill(impl->receivedSignal.begin(),impl->receivedSignal.end(),false);
}
void AudioEngine::profile(int id) {
    if(id < -1||id>5) throw std::invalid_argument("Invalid microphone profile.");
    std::lock_guard lock(impl->mutex);CudaContext::Guard guard(impl->context); impl->profile(id);
}
void AudioEngine::intensity(float value) {
    if(!std::isfinite(value)||value<0||value>1)throw std::invalid_argument("Invalid voice suppression strength.");
    if(value==impl->settings.intensity)return;
    std::lock_guard lock(impl->mutex);CudaContext::Guard guard(impl->context);
    for(auto handle:impl->handles)impl->api->check(impl->api->setFloat(handle,NVAFX_PARAM_INTENSITY_RATIO,value),"Set voice suppression strength");
    impl->settings.intensity=value;
}
void AudioEngine::run(std::span<const float* const> input,std::span<float* const> output) {
    if(input.size()!=impl->handles.size()||output.size()!=impl->handles.size()) throw std::invalid_argument("Invalid audio channel count.");
    std::lock_guard lock(impl->mutex);
    CudaContext::Guard guard(impl->context);
    processing::begin(impl->statusId(),impl->settings.effect==AudioEffect::studioQuality?L"Highest Quality · six-second audio blocks":L"Interactive speech processing");
    bool processed=false;
    for(std::size_t c=0;c<input.size();++c) {
        if(!input[c]||!output[c]) throw std::invalid_argument("Missing audio buffer.");
        const float* source=input[c]; float* destination=output[c];
        impl->receivedSignal[c]=impl->receivedSignal[c]||std::any_of(source,source+impl->inFrames,[](float value){return value!=0.f;});
        if(!impl->receivedSignal[c]){std::fill_n(destination,impl->outFrames,0.f);continue;}
        processing::report(impl->statusId(),impl->settings.effect==AudioEffect::studioQuality?L"Processing six-second HQ block…":L"Processing speech block…",static_cast<int>(c),static_cast<int>(input.size()));
        impl->api->check(impl->api->run(impl->handles[c],&source,&destination,impl->inFrames,1),"Process voice block");
        processed=true;
        if(processing::cancelled(impl->statusId())){processing::finish(impl->statusId(),L"Cancelled");throw std::runtime_error("Voice processing cancelled after the current model step.");}
        // Neural models can emit a tiny noise floor for all-zero input. Keep a
        // channel with no signal history exactly silent, without gating breaths
        // or quiet tails in channels that have actually received audio.
        if(!impl->receivedSignal[c]) std::fill_n(destination,impl->outFrames,0.f);
        for(unsigned i=0;i<impl->outFrames;++i) if(!std::isfinite(destination[i])) throw std::runtime_error("Voice model produced a non-finite sample.");
    }
    processing::finish(impl->statusId(),processed?L"Processed audio · NVIDIA effect applied":L"Silent input · no model run");
}
}
