#include "core/audio_pipeline.h"
#include "public.sdk/source/vst/vstaudioeffect.h"
#include "public.sdk/source/vst/vsteditcontroller.h"
#include "public.sdk/source/main/pluginfactory.h"
#include "base/source/fstreamer.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>

using namespace Steinberg;
using namespace Steinberg::Vst;
namespace {
enum Id : ParamID { Mix=100,Gain=101,Profile=102,Bypass=103 };
const std::array<FUID,6> processorIds{{
    {0x34425834,0x4E4F4953,0x45414658,0x00020001},{0x34425834,0x4543484F,0x41414658,0x00020001},
    {0x34425834,0x434C4541,0x4E414658,0x00020001},{0x34425834,0x53545544,0x4C4C4658,0x00020001},
    {0x34425834,0x53545544,0x48514658,0x00020001},{0x34425834,0x52455354,0x4F414658,0x00020001}}};
const std::array<FUID,6> controllerIds{{
    {0x34425834,0x4E4F4953,0x45414354,0x00020001},{0x34425834,0x4543484F,0x41414354,0x00020001},
    {0x34425834,0x434C4541,0x4E414354,0x00020001},{0x34425834,0x53545544,0x4C4C4354,0x00020001},
    {0x34425834,0x53545544,0x48514354,0x00020001},{0x34425834,0x52455354,0x4F414354,0x00020001}}};
struct State {
    double mix{1},gain{2./3.}; int32 profile{},bypass{};
    bool read(IBStream* stream) {
        if(!stream) return false; IBStreamer reader(stream,kLittleEndian);int32 version{};
        if(!reader.readInt32(version)||version!=1||!reader.readDouble(mix)||!reader.readDouble(gain)||!reader.readInt32(profile)||!reader.readInt32(bypass)) return false;
        if(!std::isfinite(mix)||!std::isfinite(gain)||profile<0||profile>6) return false;
        mix=std::clamp(mix,0.,1.);gain=std::clamp(gain,0.,1.);bypass=bypass?1:0;return true;
    }
    bool write(IBStream* stream) const {
        if(!stream) return false; IBStreamer writer(stream,kLittleEndian);
        return writer.writeInt32(1)&&writer.writeDouble(mix)&&writer.writeDouble(gain)&&writer.writeInt32(profile)&&writer.writeInt32(bypass);
    }
};
template<int Kind> class Controller final: public EditController {
public:
    static FUnknown* create(void*) { return static_cast<IEditController*>(new Controller); }
    tresult PLUGIN_API initialize(FUnknown* context) override {
        const auto error=EditController::initialize(context);if(error!=kResultOk) return error;
        parameters.addParameter(new RangeParameter(STR16("Mix"),Mix,STR16("%"),0,100,100));
        parameters.addParameter(new RangeParameter(STR16("Output Gain"),Gain,STR16("dB"),-24,12,0));
        if constexpr(Kind==3) {
            auto* p=new StringListParameter(STR16("Microphone Character"),Profile);
            for(const auto* text:{STR16("Natural"),STR16("Bright"),STR16("Vibrant"),STR16("Flat"),STR16("Full"),STR16("Thin"),STR16("Warm")}) p->appendString(text);
            parameters.addParameter(p);
        }
        parameters.addParameter(STR16("Bypass"),nullptr,1,0,ParameterInfo::kCanAutomate|ParameterInfo::kIsBypass,Bypass);
        return kResultOk;
    }
    tresult PLUGIN_API setComponentState(IBStream* stream) override {
        State state;if(!state.read(stream)) return kResultFalse;
        setParamNormalized(Mix,state.mix);setParamNormalized(Gain,state.gain);setParamNormalized(Bypass,state.bypass?1:0);
        if constexpr(Kind==3) setParamNormalized(Profile,state.profile/6.);
        return kResultOk;
    }
};
template<int Kind> class Processor final: public AudioEffect {
    State state;
    std::unique_ptr<fourbyfour::AudioPipeline> pipeline;
    int channels{2},rate{},latency{};
    int64 nextSample{}; bool haveTime{};
public:
    Processor() { setControllerClass(controllerIds[Kind]); }
    static FUnknown* create(void*) { return static_cast<IAudioProcessor*>(new Processor); }
    tresult PLUGIN_API initialize(FUnknown* context) override {
        const auto error=AudioEffect::initialize(context);if(error!=kResultOk) return error;
        addAudioInput(STR16("Voice Input"),SpeakerArr::kStereo);
        addAudioOutput(STR16("Voice Output"),SpeakerArr::kStereo);return kResultOk;
    }
    tresult PLUGIN_API terminate() override { pipeline.reset();return AudioEffect::terminate(); }
    tresult PLUGIN_API setBusArrangements(SpeakerArrangement* in,int32 ni,SpeakerArrangement* out,int32 no) override {
        if(ni!=1||no!=1||!in||!out||in[0]!=out[0]||(in[0]!=SpeakerArr::kMono&&in[0]!=SpeakerArr::kStereo)) return kResultFalse;
        const auto error=AudioEffect::setBusArrangements(in,ni,out,no);
        if(error==kResultOk) { const int count=SpeakerArr::getChannelCount(in[0]);if(count!=channels) {channels=count;pipeline.reset();} }
        return error;
    }
    tresult PLUGIN_API canProcessSampleSize(int32 size) override { return size==kSample32?kResultTrue:kResultFalse; }
    tresult PLUGIN_API setupProcessing(ProcessSetup& setup) override {
        if(setup.symbolicSampleSize!=kSample32||!std::isfinite(setup.sampleRate)||setup.sampleRate<8000||setup.sampleRate>192000||setup.maxSamplesPerBlock<1)
            return kResultFalse;
        try {
            const int nextRate=static_cast<int>(std::lround(setup.sampleRate));
            if(std::abs(nextRate-setup.sampleRate)>.001) return kResultFalse;
            if(!pipeline||nextRate!=rate) {
                pipeline=std::make_unique<fourbyfour::AudioPipeline>(fourbyfour::AudioSettings{static_cast<fourbyfour::AudioEffect>(Kind),channels,state.profile-1},nextRate);
                rate=nextRate;latency=pipeline->latency();
            }
            return AudioEffect::setupProcessing(setup);
        } catch(const std::exception& e) { std::fprintf(stderr,"4x4Tools audio initialization: %s\n",e.what());return kResultFalse; }
    }
    uint32 PLUGIN_API getLatencySamples() override { return static_cast<uint32>(latency); }
    uint32 PLUGIN_API getTailSamples() override { return static_cast<uint32>(latency); }
    tresult PLUGIN_API setActive(TBool active) override {
        try {
            if(active&&!pipeline&&rate>=8000) {
                pipeline=std::make_unique<fourbyfour::AudioPipeline>(fourbyfour::AudioSettings{static_cast<fourbyfour::AudioEffect>(Kind),channels,state.profile-1},rate);
                latency=pipeline->latency();
            }
            if(active&&pipeline) {pipeline->reset();pipeline->profile(state.profile-1);} haveTime=false;
        }
        catch(...) { return kResultFalse; }
        return AudioEffect::setActive(active);
    }
    tresult PLUGIN_API setProcessing(TBool processing) override {
        try { if(processing&&pipeline) pipeline->reset();haveTime=false; }
        catch(...) { return kResultFalse; }
        return AudioEffect::setProcessing(processing);
    }
    tresult PLUGIN_API setState(IBStream* stream) override {
        State next;if(!next.read(stream)) return kResultFalse;
        try {if(pipeline) pipeline->profile(next.profile-1);state=next;return kResultOk;} catch(...) {return kResultFalse;}
    }
    tresult PLUGIN_API getState(IBStream* stream) override { return state.write(stream)?kResultOk:kResultFalse; }
    struct Event { int32 offset;ParamID id;ParamValue value; };
    void apply(const Event& event) {
        const double value=std::isfinite(event.value)?std::clamp(event.value,0.,1.):0.;
        if(event.id==Mix) state.mix=value;
        else if(event.id==Gain) state.gain=value;
        else if(event.id==Bypass) state.bypass=value>=.5;
        else if constexpr(Kind==3) if(event.id==Profile) {
            const int profile=static_cast<int>(std::lround(value*6));
            if(state.profile!=profile) {state.profile=profile;if(pipeline) pipeline->profile(profile-1);}
        }
    }
    tresult PLUGIN_API process(ProcessData& data) override {
        try {
            std::vector<Event> events;
            if(data.inputParameterChanges) for(int32 i=0;i<data.inputParameterChanges->getParameterCount();++i) {
                auto* queue=data.inputParameterChanges->getParameterData(i);if(!queue) continue;
                for(int32 p=0;p<queue->getPointCount();++p) {
                    int32 offset{};ParamValue value{};
                    if(queue->getPoint(p,offset,value)==kResultTrue) events.push_back({std::clamp(offset,0,data.numSamples),queue->getParameterId(),value});
                }
            }
            std::stable_sort(events.begin(),events.end(),[](const auto& a,const auto& b){return a.offset<b.offset;});
            if(!data.numSamples||!data.numInputs||!data.numOutputs) {for(const auto& e:events) apply(e);return kResultOk;}
            if(data.numSamples<0||data.numInputs!=1||data.numOutputs!=1||data.inputs[0].numChannels!=channels||data.outputs[0].numChannels!=channels||!pipeline)
                return kResultFalse;
            if(data.processContext&&(data.processContext->state&ProcessContext::kPlaying)) {
                if(haveTime&&data.processContext->projectTimeSamples!=nextSample) pipeline->reset();
                nextSample=data.processContext->projectTimeSamples+data.numSamples;haveTime=true;
            }
            int32 position=0;
            auto render=[&](int32 count) {
                if(count<=0) return;
                const float* in[2]{};float* out[2]{};
                for(int c=0;c<channels;++c) {in[c]=data.inputs[0].channelBuffers32[c]+position;out[c]=data.outputs[0].channelBuffers32[c]+position;}
                pipeline->process(in,out,count,static_cast<float>(state.mix),static_cast<float>(state.gain*36-24),state.bypass!=0);position+=count;
            };
            for(const auto& e:events) {render(e.offset-position);apply(e);}
            render(data.numSamples-position);data.outputs[0].silenceFlags=0;return kResultOk;
        } catch(const std::exception& e) {
            std::fprintf(stderr,"4x4Tools audio processing: %s\n",e.what());return kResultFalse;
        } catch(...) {return kResultFalse;}
    }
};
}
BEGIN_FACTORY_DEF("4x4Tools","","")
#define FOUR_AUDIO_CLASS(KIND,NAME) \
    DEF_CLASS2(INLINE_UID_FROM_FUID(processorIds[KIND]),PClassInfo::kManyInstances,kVstAudioEffectClass,NAME,Vst::kDistributable,"Fx|Restoration","0.2.0",kVstVersionString,Processor<KIND>::create) \
    DEF_CLASS2(INLINE_UID_FROM_FUID(controllerIds[KIND]),PClassInfo::kManyInstances,kVstComponentControllerClass,NAME " Controller",0,"","0.2.0",kVstVersionString,Controller<KIND>::create)
FOUR_AUDIO_CLASS(0,"4x4 Noise Clean")
FOUR_AUDIO_CLASS(1,"4x4 Room Echo Clean")
FOUR_AUDIO_CLASS(2,"4x4 Voice Clean")
FOUR_AUDIO_CLASS(3,"4x4 Studio Voice Preview")
FOUR_AUDIO_CLASS(4,"4x4 Studio Voice HQ")
FOUR_AUDIO_CLASS(5,"4x4 Voice Restore")
END_FACTORY
