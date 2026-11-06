#pragma once
#include "core/audio_pipeline.h"
#include "core/processing_status.h"
#include <windows.h>
#include <atomic>
#include <cstdio>
#include <exception>
#include <stdexcept>
#include <thread>

namespace fourbyfour {
// One owner, one processing thread. Construction/activation is GPU-free. A
// nonzero, non-bypassed process request wakes the loader. Publication is release
// / acquire; the worker never touches the pipeline again until deactivation,
// which the VST contract places after the last process call.
class AudioWarmup {
    enum class Stage { idle,requested,loading,ready,failed };
    std::atomic<Stage> stage{Stage::idle};
    std::atomic<bool> stopping{};
    HANDLE event{},completed{};
    std::thread worker;
    std::unique_ptr<AudioPipeline> pipeline;
    std::exception_ptr failure;
public:
    AudioWarmup(AudioSettings settings,int rate) {
        event=CreateEventW(nullptr,FALSE,FALSE,nullptr);
        if(!event)throw std::runtime_error("Cannot create the voice preparation event.");
        completed=CreateEventW(nullptr,TRUE,FALSE,nullptr);
        if(!completed){CloseHandle(event);throw std::runtime_error("Cannot create the voice completion event.");}
        try {
            worker=std::thread([this,settings,rate] {
                if(WaitForSingleObject(event,INFINITE)!=WAIT_OBJECT_0||stopping.load())return;
                stage.store(Stage::loading,std::memory_order_release);
                processing::begin(settings.statusEffect,L"Preparing voice in the background");
                processing::report(settings.statusEffect,L"Loading NVIDIA voice runtime… audio passes through");
                try {
                    pipeline=std::make_unique<AudioPipeline>(settings,rate);
                    if(processing::cancelled(settings.statusEffect))throw std::runtime_error("Voice preparation cancelled.");
                    if(!stopping.load())stage.store(Stage::ready,std::memory_order_release);
                } catch(const std::exception& error) {
                    failure=std::current_exception();
                    std::fprintf(stderr,"4x4Tools voice preparation: %s\n",error.what());
                    pipeline.reset();
                    processing::finish(settings.statusEffect,processing::cancelled(settings.statusEffect)?L"Cancelled · audio passes through":L"Model load failed · audio passes through");
                    stage.store(Stage::failed,std::memory_order_release);
                } catch(...) {
                    failure=std::current_exception();
                    pipeline.reset();
                    processing::finish(settings.statusEffect,L"Model load failed · audio passes through");
                    stage.store(Stage::failed,std::memory_order_release);
                }
                SetEvent(completed);
                // Keep ownership on this thread, including vendor destruction.
                // Never detach: Adobe must be able to unload the DLL safely.
                if(!stopping.load())WaitForSingleObject(event,INFINITE);
                pipeline.reset();
            });
        } catch(...) {CloseHandle(completed);CloseHandle(event);event=nullptr;throw;}
    }
    ~AudioWarmup() {
        stopping.store(true);SetEvent(event);
        if(worker.joinable())worker.join();
        CloseHandle(completed);CloseHandle(event);
    }
    AudioWarmup(const AudioWarmup&)=delete;
    AudioWarmup& operator=(const AudioWarmup&)=delete;
    void request() noexcept {
        auto expected=Stage::idle;
        if(stage.compare_exchange_strong(expected,Stage::requested,std::memory_order_acq_rel))SetEvent(event);
    }
    AudioPipeline* ready() const noexcept {
        return stage.load(std::memory_order_acquire)==Stage::ready?pipeline.get():nullptr;
    }
    // Only non-realtime rendering may wait. Prefetch and export must never
    // silently save the temporary dry signal used while live playback warms.
    AudioPipeline* waitUntilReady() {
        request();
        if(WaitForSingleObject(completed,INFINITE)!=WAIT_OBJECT_0)throw std::runtime_error("Voice preparation wait failed.");
        if(auto* result=ready())return result;
        if(failure)std::rethrow_exception(failure);
        throw std::runtime_error("Voice preparation was cancelled.");
    }
};
}
