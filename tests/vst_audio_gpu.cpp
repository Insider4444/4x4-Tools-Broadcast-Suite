#include <windows.h>
#include "core/audio_pipeline.h"
#include "core/processing_status.h"
#include "pluginterfaces/base/ipluginbase.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace Steinberg;
using namespace Steinberg::Vst;
using Clock=std::chrono::steady_clock;
void require(bool condition,const char* message) {if(!condition)throw std::runtime_error(message);}
struct Instance {
    IComponent* component{};IAudioProcessor* processor{};
    Instance(IPluginFactory* factory,const TUID id,int mode,bool explicitHq=false) {
        try {
            require(factory->createInstance(id,IComponent::iid,reinterpret_cast<void**>(&component))==kResultOk,"Cannot create processor");
            require(component->initialize(nullptr)==kResultOk,"Initialization failed");
            require(component->queryInterface(IAudioProcessor::iid,reinterpret_cast<void**>(&processor))==kResultOk,"No processor");
            if(explicitHq){ParameterChanges quality(1);int32 qi{},pi{};quality.addParameterData(104,qi)->addPoint(0,1,pi);ProcessData flush;flush.inputParameterChanges=&quality;require(processor->process(flush)==kResultOk,"HQ state flush failed");}
            ProcessSetup setup{mode,kSample32,512,48000};
            require(processor->setupProcessing(setup)==kResultOk,"Setup failed");
            require(component->setActive(true)==kResultOk,"Activation failed");
            require(processor->setProcessing(true)==kResultOk,"Processing activation failed");
        }catch(...){clear();throw;}
    }
    void clear() {
        if(processor)processor->setProcessing(false);
        if(component){component->setActive(false);component->terminate();}
        if(processor)processor->release();if(component)component->release();processor=nullptr;component=nullptr;
    }
    ~Instance(){clear();}
};
struct Buffers {
    std::array<std::array<float,512>,2> source{},target{};
    float* in[2]{source[0].data(),source[1].data()},*out[2]{target[0].data(),target[1].data()};
    AudioBusBuffers input,output;ProcessData data;
    explicit Buffers(int mode) {
        input.numChannels=output.numChannels=2;input.channelBuffers32=in;output.channelBuffers32=out;
        data.processMode=mode;data.symbolicSampleSize=kSample32;data.numInputs=data.numOutputs=1;data.inputs=&input;data.outputs=&output;
    }
};
int wmain(int argc,wchar_t** argv) {
    try {
        require(argc==4,"VstAudioGpuTests VST_BINARY AFX_RUNTIME mono.f32");
        const auto root=std::filesystem::absolute(argv[2]);SetEnvironmentVariableW(L"FOURBYFOUR_AFX_SDK_ROOT",root.c_str());
        std::ifstream file(std::filesystem::path(argv[3]),std::ios::binary|std::ios::ate);
        const auto bytes=file.tellg();require(bytes>0&&bytes%4==0,"Invalid float audio");file.seekg(0);
        std::vector<float> mono(static_cast<std::size_t>(bytes)/4);require(bool(file.read(reinterpret_cast<char*>(mono.data()),bytes)),"Truncated audio");
        mono.resize(std::min<std::size_t>(mono.size(),48000*2+127));
        const auto module=LoadLibraryExW(std::filesystem::absolute(argv[1]).c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
        require(module!=nullptr,"Cannot load plugin");
        const auto init=reinterpret_cast<bool(*)()>(GetProcAddress(module,"InitDll")),exit=reinterpret_cast<bool(*)()>(GetProcAddress(module,"ExitDll"));
        const auto getFactory=reinterpret_cast<IPluginFactory*(PLUGIN_API*)()>(GetProcAddress(module,"GetPluginFactory"));
        require(getFactory&&(!init||init()),"Plugin entry failed");auto* factory=getFactory();require(factory!=nullptr,"No factory");
        int kind=0;
        for(int32 index=0;index<factory->countClasses();++index) {
            PClassInfo info{};require(factory->getClassInfo(index,&info)==kResultOk,"No class info");
            if(std::strcmp(info.category,kVstAudioEffectClass))continue;
            std::vector<float> input(mono.size()*2);
            for(std::size_t i=0;i<mono.size();++i)input[i*2]=mono[i];
            float maximum=0,right=0,prefetchError=0;
            for(int mode:{kOffline,kPrefetch}) {
                const auto selected=kind==4&&mode==kPrefetch?fourbyfour::AudioEffect::studioPreview:static_cast<fourbyfour::AudioEffect>(kind);
                const auto reference=fourbyfour::processAudio(input,48000,{selected,2,-1},1,0,root);
                // Realtime <-> prefetch changes are legal without setupProcessing.
                Instance effect(factory,info.cid,mode==kPrefetch?kRealtime:kOffline);Buffers buffers(mode);
                const auto latency=effect.processor->getLatencySamples();
                const auto total=mono.size()+latency;const std::array<int,6> blocks{1,13,57,441,512,127};std::size_t pass=0;
                for(std::size_t offset=0;offset<total;) {
                    const int count=static_cast<int>(std::min<std::size_t>(blocks[pass++%blocks.size()],total-offset));buffers.data.numSamples=count;
                    for(int i=0;i<count;++i){buffers.source[0][i]=offset+i<mono.size()?mono[offset+i]:0;buffers.source[1][i]=0;}
                    require(effect.processor->process(buffers.data)==kResultOk,"Offline VST processing failed");
                    for(int i=0;i<count;++i)if(offset+i>=latency) {
                        const auto sample=offset+i-latency;
                        require(std::isfinite(buffers.target[0][i])&&std::isfinite(buffers.target[1][i]),"Non-finite offline output");
                        auto& error=mode==kPrefetch?prefetchError:maximum;
                        error=std::max(error,std::abs(buffers.target[0][i]-reference[sample*2]));
                        right=std::max(right,std::abs(buffers.target[1][i]));
                    }
                    offset+=count;
                }
            }
            require(maximum<2.e-5f&&prefetchError<2.e-5f,"VST rendering changed the reference audio");require(right==0,"Offline speech crossed into a silent channel");
            double firstMs{},worstMs{};float change=0;bool inferred=false;
            {
                Instance effect(factory,info.cid,kRealtime,kind==4);Buffers buffers(kRealtime);buffers.data.numSamples=512;
                const auto latency=effect.processor->getLatencySamples();const auto deadline=Clock::now()+std::chrono::seconds(20);
                for(std::size_t offset=0;Clock::now()<deadline;offset+=512) {
                    for(int i=0;i<512;++i){buffers.source[0][i]=mono[(offset+i)%mono.size()];buffers.source[1][i]=0;}
                    const auto before=Clock::now();require(effect.processor->process(buffers.data)==kResultOk,"Realtime VST processing failed");
                    const double ms=std::chrono::duration<double,std::milli>(Clock::now()-before).count();if(!offset)firstMs=ms;worstMs=std::max(worstMs,ms);
                    for(int i=0;i<512;++i) {
                        require(std::isfinite(buffers.target[0][i])&&buffers.target[1][i]==0,"Realtime output is invalid or crosses channels");
                        const float dry=offset+i<latency?0:mono[(offset+i-latency)%mono.size()];
                        change=std::max(change,std::abs(buffers.target[0][i]-dry));
                    }
                    fourbyfour::processing::Status status;
                    if(fourbyfour::processing::read(7+kind,status)&&!std::wcscmp(status.phase,L"Audio block ready"))inferred=true;
                    if(inferred&&change>1.e-4f)break;
                    Sleep(2);
                }
                require(firstMs<100,"The first playback callback blocked on model loading");
                require(inferred&&change>1.e-4f,"Background model never reached processed audio");
            }
            std::cout<<info.name<<": offline_error="<<maximum<<" prefetch_error="<<prefetchError<<" silent_channel="<<right<<" first_playback_ms="<<firstMs<<" worst_callback_ms="<<worstMs<<" neural_output=confirmed\n"<<std::flush;
            ++kind;
        }
        factory->release();if(exit)require(exit(),"Plugin exit failed");FreeLibrary(module);require(kind==6,"Missing audio effect");
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
