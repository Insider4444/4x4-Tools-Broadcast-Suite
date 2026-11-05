#include "core/audio_pipeline.h"
#include "ui/processing_panel.h"
#include "public.sdk/source/common/pluginview.h"
#include "public.sdk/source/vst/vstaudioeffect.h"
#include "public.sdk/source/vst/vsteditcontroller.h"
#include "public.sdk/source/main/pluginfactory.h"
#include "base/source/fstreamer.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"
#include "pluginterfaces/vst/ivstmessage.h"
#include <atomic>
#include <cstring>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>

using namespace Steinberg;
using namespace Steinberg::Vst;
namespace {
enum Id : ParamID { Mix=100,Gain=101,Profile=102,Bypass=103,Quality=104 };
const std::array<FUID,6> processorIds{{
    {0x34425834,0x4E4F4953,0x45414658,0x00020001},{0x34425834,0x4543484F,0x41414658,0x00020001},
    {0x34425834,0x434C4541,0x4E414658,0x00020001},{0x34425834,0x53545544,0x4C4C4658,0x00020001},
    {0x34425834,0x53545544,0x48514658,0x00020001},{0x34425834,0x52455354,0x4F414658,0x00020001}}};
const std::array<FUID,6> controllerIds{{
    {0x34425834,0x4E4F4953,0x45414354,0x00020001},{0x34425834,0x4543484F,0x41414354,0x00020001},
    {0x34425834,0x434C4541,0x4E414354,0x00020001},{0x34425834,0x53545544,0x4C4C4354,0x00020001},
    {0x34425834,0x53545544,0x48514354,0x00020001},{0x34425834,0x52455354,0x4F414354,0x00020001}}};
struct State {
    double mix{1},gain{2./3.}; int32 profile{},bypass{},quality{};
    bool read(IBStream* stream) {
        if(!stream) return false; IBStreamer reader(stream,kLittleEndian);int32 version{};
        if(!reader.readInt32(version)||(version!=1&&version!=2)||!reader.readDouble(mix)||!reader.readDouble(gain)||!reader.readInt32(profile)||!reader.readInt32(bypass)) return false;
        if(version==1)quality=2; // preserve the original HQ choice in existing projects
        else if(!reader.readInt32(quality)||quality<0||quality>2)return false;
        if(!std::isfinite(mix)||!std::isfinite(gain)||profile<0||profile>6) return false;
        mix=std::clamp(mix,0.,1.);gain=std::clamp(gain,0.,1.);bypass=bypass?1:0;return true;
    }
    bool write(IBStream* stream) const {
        if(!stream) return false; IBStreamer writer(stream,kLittleEndian);
        return writer.writeInt32(2)&&writer.writeDouble(mix)&&writer.writeDouble(gain)&&writer.writeInt32(profile)&&writer.writeInt32(bypass)&&writer.writeInt32(quality);
    }
};
class VoiceView final:public CPluginView {
    EditController* owner;
    fourbyfour::ui::Controls controls;
    HWND window{};
public:
    VoiceView(EditController* controller,fourbyfour::ui::Controls settings):owner(controller),controls(std::move(settings)) {owner->addRef();rect={0,0,600,488};}
    ~VoiceView() override {if(window)DestroyWindow(window);owner->release();}
    tresult PLUGIN_API isPlatformTypeSupported(FIDString type) override {return type&&!std::strcmp(type,kPlatformTypeHWND)?kResultTrue:kResultFalse;}
    tresult PLUGIN_API attached(void* parent,FIDString type) override {
        if(isPlatformTypeSupported(type)!=kResultTrue||!parent)return kResultFalse;
        const auto result=CPluginView::attached(parent,type);if(result!=kResultTrue)return result;
        window=fourbyfour::ui::createPanel(static_cast<HWND>(parent),controls);return window?kResultTrue:kResultFalse;
    }
    tresult PLUGIN_API removed() override {if(window){DestroyWindow(window);window=nullptr;}return CPluginView::removed();}
};
template<int Kind> class Controller final: public EditController {
    bool restoring{};
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
        if constexpr(Kind==4) {
            auto* quality=new StringListParameter(STR16("Processing"),Quality,nullptr,0);
            quality->appendString(STR16("Auto: Preview / Offline HQ"));quality->appendString(STR16("Interactive Preview"));quality->appendString(STR16("Highest Quality: Render"));parameters.addParameter(quality);
        }
        return kResultOk;
    }
    tresult PLUGIN_API setComponentState(IBStream* stream) override {
        State state;if(!state.read(stream)) return kResultFalse;
        restoring=true;
        setParamNormalized(Mix,state.mix);setParamNormalized(Gain,state.gain);setParamNormalized(Bypass,state.bypass?1:0);
        if constexpr(Kind==3) setParamNormalized(Profile,state.profile/6.);
        if constexpr(Kind==4) setParamNormalized(Quality,state.quality/2.);
        restoring=false;
        return kResultOk;
    }
    tresult PLUGIN_API setParamNormalized(ParamID id,ParamValue value) override {
        const auto previous=getParamNormalized(id);
        const auto result=EditController::setParamNormalized(id,value);
        if constexpr(Kind==4)if(result==kResultOk&&id==Quality&&!restoring&&previous!=getParamNormalized(id)) {
            if(auto* message=allocateMessage()) {
                message->setMessageID("4x4ToolsQuality");message->getAttributes()->setInt("quality",static_cast<int64>(std::lround(getParamNormalized(id)*2)));sendMessage(message);message->release();
            }
            if(componentHandler)componentHandler->restartComponent(kReloadComponent|kLatencyChanged);
        }
        return result;
    }
    IPlugView* PLUGIN_API createView(FIDString name) override {
        if(!name||std::strcmp(name,ViewType::kEditor))return nullptr;
        fourbyfour::ui::Controls controls;
        controls.effect=7+Kind;controls.voice=true;controls.qualityChoice=Kind==4;
        controls.microphoneChoice=Kind==3;controls.microphone=static_cast<int>(std::lround(getParamNormalized(Profile)*6));
        controls.quality=static_cast<int>(std::lround(getParamNormalized(Quality)*2));
        controls.mix=getParamNormalized(Mix);controls.gainDb=getParamNormalized(Gain)*36-24;controls.bypass=getParamNormalized(Bypass)>=.5;
        controls.changed=[this](int control,double value) {
            const auto id=control==0?Quality:control==1?Mix:control==2?Gain:control==4?Profile:Bypass;
            const double normalized=control==0?value/2:control==2?(value+24)/36:control==4?value/6:value;
            setParamNormalized(id,normalized);beginEdit(id);performEdit(id,normalized);endEdit(id);
        };
        controls.synchronize=[this](fourbyfour::ui::Controls& current){
            current.quality=static_cast<int>(std::lround(getParamNormalized(Quality)*2));
            current.microphone=static_cast<int>(std::lround(getParamNormalized(Profile)*6));
            current.mix=getParamNormalized(Mix);current.gainDb=getParamNormalized(Gain)*36-24;current.bypass=getParamNormalized(Bypass)>=.5;
        };
        return new VoiceView(this,std::move(controls));
    }
};
template<int Kind> class Processor final: public AudioEffect {
    State state;
    std::unique_ptr<fourbyfour::AudioPipeline> pipeline;
    int channels{2},rate{},latency{};
    int64 nextSample{}; bool haveTime{};
    std::atomic<int> qualityChoice{0};
    fourbyfour::AudioEffect selectedEffect{static_cast<fourbyfour::AudioEffect>(Kind)};
    fourbyfour::AudioEffect requestedEffect(int mode) const {
        if constexpr(Kind==4) {
            const auto quality=qualityChoice.load();
            return quality==2||(quality==0&&mode==kOffline)?fourbyfour::AudioEffect::studioQuality:fourbyfour::AudioEffect::studioPreview;
        }
        return static_cast<fourbyfour::AudioEffect>(Kind);
    }
    std::unique_ptr<fourbyfour::AudioPipeline> makePipeline(int sampleRate) {
        return std::make_unique<fourbyfour::AudioPipeline>(fourbyfour::AudioSettings{selectedEffect,channels,state.profile-1,7+Kind},sampleRate);
    }
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
            const auto nextEffect=requestedEffect(setup.processMode);
            if(!pipeline||nextRate!=rate||selectedEffect!=nextEffect) {
                pipeline.reset();selectedEffect=nextEffect;pipeline=makePipeline(nextRate);
                rate=nextRate;latency=pipeline->latency();
            }
            return AudioEffect::setupProcessing(setup);
        } catch(const std::exception& e) { std::fprintf(stderr,"4x4Tools audio initialization: %s\n",e.what());return kResultFalse; }
    }
    uint32 PLUGIN_API getLatencySamples() override { return static_cast<uint32>(latency); }
    uint32 PLUGIN_API getTailSamples() override { return static_cast<uint32>(latency); }
    tresult PLUGIN_API setActive(TBool active) override {
        try {
            if(!active)pipeline.reset();
            if(active&&selectedEffect!=requestedEffect(processSetup.processMode)){pipeline.reset();selectedEffect=requestedEffect(processSetup.processMode);}
            if(active&&!pipeline&&rate>=8000) {
                pipeline=makePipeline(rate);
                latency=pipeline->latency();
            }
            if(active&&pipeline) {pipeline->reset();pipeline->profile(state.profile-1);} haveTime=false;
        }
        catch(...) { return kResultFalse; }
        return AudioEffect::setActive(active);
    }
    tresult PLUGIN_API setProcessing(TBool processing) override {
        try {
            if(!processing)pipeline.reset();
            else {
                if(!pipeline&&rate>=8000) {
                    pipeline=makePipeline(rate);
                    latency=pipeline->latency();
                }
                if(pipeline)pipeline->reset();
            }
            haveTime=false;
        }
        catch(...) { return kResultFalse; }
        return AudioEffect::setProcessing(processing);
    }
    tresult PLUGIN_API setState(IBStream* stream) override {
        State next;if(!next.read(stream)) return kResultFalse;
        try {if(pipeline) pipeline->profile(next.profile-1);state=next;qualityChoice.store(next.quality);return kResultOk;} catch(...) {return kResultFalse;}
    }
    tresult PLUGIN_API getState(IBStream* stream) override {auto copy=state;copy.quality=qualityChoice.load();return copy.write(stream)?kResultOk:kResultFalse;}
    tresult PLUGIN_API notify(IMessage* message) override {
        if constexpr(Kind==4)if(message&&message->getMessageID()&&!std::strcmp(message->getMessageID(),"4x4ToolsQuality")) {
            int64 quality{};if(message->getAttributes()->getInt("quality",quality)==kResultOk&&quality>=0&&quality<=2){qualityChoice.store(static_cast<int>(quality));return kResultOk;}
            return kResultFalse;
        }
        return AudioEffect::notify(message);
    }
    struct Event { int32 offset;ParamID id;ParamValue value; };
    void apply(const Event& event) {
        const double value=std::isfinite(event.value)?std::clamp(event.value,0.,1.):0.;
        if(event.id==Mix) state.mix=value;
        else if(event.id==Gain) state.gain=value;
        else if(event.id==Bypass) state.bypass=value>=.5;
        else if(event.id==Quality&&Kind==4) qualityChoice.store(static_cast<int>(std::lround(value*2)));
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
    DEF_CLASS2(INLINE_UID_FROM_FUID(processorIds[KIND]),PClassInfo::kManyInstances,kVstAudioEffectClass,NAME,0,"Fx|Restoration","0.2.1",kVstVersionString,Processor<KIND>::create) \
    DEF_CLASS2(INLINE_UID_FROM_FUID(controllerIds[KIND]),PClassInfo::kManyInstances,kVstComponentControllerClass,NAME " Controller",0,"","0.2.1",kVstVersionString,Controller<KIND>::create)
FOUR_AUDIO_CLASS(0,"4x4 Noise Clean")
FOUR_AUDIO_CLASS(1,"4x4 Room Echo Clean")
FOUR_AUDIO_CLASS(2,"4x4 Voice Clean")
FOUR_AUDIO_CLASS(3,"4x4 Studio Voice Preview")
FOUR_AUDIO_CLASS(4,"4x4 Studio Voice HQ")
FOUR_AUDIO_CLASS(5,"4x4 Voice Restore")
END_FACTORY
