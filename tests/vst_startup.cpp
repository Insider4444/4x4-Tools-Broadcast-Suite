#include <windows.h>
#include "pluginterfaces/base/ipluginbase.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"
#include "public.sdk/source/common/memorystream.h"
#include "base/source/fstreamer.h"
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace Steinberg;
using namespace Steinberg::Vst;
void require(bool condition,const char* message) {if(!condition)throw std::runtime_error(message);}
void noModels() {
    require(!GetModuleHandleW(L"NVAudioEffects.dll"),"Host probing loaded the NVIDIA audio runtime");
    require(!GetModuleHandleW(L"nvcuda.dll"),"Host probing initialized CUDA");
}
void checkVoiceState(IComponent* component,bool dialogue) {
    MemoryStream stream;require(component->getState(&stream)==kResultOk,"Cannot save voice state");
    stream.seek(0,IBStream::kIBSeekSet,nullptr);IBStreamer reader(&stream,kLittleEndian);
    int32 version{},profile{},bypass{},quality{},channels{},match{};double mix{},gain{},strength{};
    require(reader.readInt32(version)&&version==4&&reader.readDouble(mix)&&reader.readDouble(gain)&&reader.readInt32(profile)&&reader.readInt32(bypass)&&reader.readInt32(quality)&&reader.readInt32(channels)&&reader.readInt32(match)&&reader.readDouble(strength)&&strength==1,"Voice state is truncated or changed suppression");
    require(channels==(dialogue?1:0)&&match==(dialogue?1:0),"Voice routing/level state did not persist");
}
int wmain(int argc,wchar_t** argv) {
    try {
        require(argc==2,"Pass the audio VST3 binary path");
        // The scanner must work even on a build agent without NVIDIA models.
        // This override is process-local and never changes the user's config.
        const auto missing=std::filesystem::absolute(L"missing-startup-test-runtime");
        SetEnvironmentVariableW(L"FOURBYFOUR_AFX_SDK_ROOT",missing.c_str());
        const auto library=LoadLibraryExW(std::filesystem::absolute(argv[1]).c_str(),nullptr,
            LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
        require(library!=nullptr,"Cannot load the audio plugin");
        const auto init=reinterpret_cast<bool(*)()>(GetProcAddress(library,"InitDll"));
        const auto exit=reinterpret_cast<bool(*)()>(GetProcAddress(library,"ExitDll"));
        const auto getFactory=reinterpret_cast<IPluginFactory*(PLUGIN_API*)()>(GetProcAddress(library,"GetPluginFactory"));
        require(getFactory&&(!init||init()),"Plugin entry failed");
        auto* factory=getFactory();require(factory!=nullptr,"No plugin factory");
        int processors=0,configurations=0;
        const auto started=std::chrono::steady_clock::now();
        for(int32 index=0;index<factory->countClasses();++index) {
            PClassInfo info{};require(factory->getClassInfo(index,&info)==kResultOk,"No class metadata");
            if(std::strcmp(info.category,kVstAudioEffectClass))continue;
            IComponent* component{};
            require(factory->createInstance(info.cid,IComponent::iid,reinterpret_cast<void**>(&component))==kResultOk,"Cannot create processor");
            require(component->initialize(nullptr)==kResultOk,"Processor initialization failed");
            const bool studio=!std::strcmp(info.name,"4x4 Studio Voice Preview")||!std::strcmp(info.name,"4x4 Studio Voice HQ");
            checkVoiceState(component,studio);
            // A version-two project must keep independent channels and its
            // original volume instead of silently adopting new defaults.
            MemoryStream legacy;IBStreamer writer(&legacy,kLittleEndian);
            writer.writeInt32(2);writer.writeDouble(1);writer.writeDouble(2./3.);writer.writeInt32(0);writer.writeInt32(0);writer.writeInt32(0);
            legacy.seek(0,IBStream::kIBSeekSet,nullptr);require(component->setState(&legacy)==kResultOk,"Legacy voice state no longer opens");checkVoiceState(component,false);
            IAudioProcessor* processor{};
            require(component->queryInterface(IAudioProcessor::iid,reinterpret_cast<void**>(&processor))==kResultOk,"No audio processor interface");
            if(!std::strcmp(info.name,"4x4 Studio Voice HQ")) {
                ParameterChanges quality(1);int32 qi{},pi{};quality.addParameterData(104,qi)->addPoint(0,1,pi);
                ProcessData flush;flush.inputParameterChanges=&quality;require(processor->process(flush)==kResultOk,"HQ state flush failed");
            }
            for(const int mode:{kRealtime,kPrefetch,kOffline})for(const int rate:{44100,48000,96000})for(const auto arrangement:{SpeakerArr::kMono,SpeakerArr::kStereo}) {
                auto inputArrangement=arrangement,outputArrangement=arrangement;
                require(processor->setBusArrangements(&inputArrangement,1,&outputArrangement,1)==kResultOk,"Channel configuration failed");
                ProcessSetup setup{mode,kSample32,512,double(rate)};
                require(processor->setupProcessing(setup)==kResultOk,"Host setup requires GPU models");
                const auto latency=processor->getLatencySamples();require(latency>0&&latency<rate*7u,"Bad advertised latency");
                if(mode!=kOffline)require(latency<rate,"A live effect selected a multi-second HQ model");
                require(component->setActive(true)==kResultOk,"Activation requires GPU models");
                require(processor->setProcessing(true)==kResultOk,"Processing activation requires GPU models");
                noModels();
                ProcessData data{};data.processMode=mode;data.symbolicSampleSize=kSample32;
                ParameterChanges initial(1);int32 initialQueue{},initialPoint{};
                initial.addParameterData(103,initialQueue)->addPoint(0,0,initialPoint);data.inputParameterChanges=&initial;
                require(processor->process(data)==kResultOk,"Parameter-only flush failed");
                data.inputParameterChanges=nullptr;
                if(mode==kRealtime) {
                    const int channels=SpeakerArr::getChannelCount(arrangement);
                    std::array<std::array<float,512>,2> input{},output{};
                    float* inputs[]{input[0].data(),input[1].data()},*outputs[]{output[0].data(),output[1].data()};
                    AudioBusBuffers in,out;in.numChannels=out.numChannels=channels;in.channelBuffers32=inputs;out.channelBuffers32=outputs;
                    data.numInputs=data.numOutputs=1;data.inputs=&in;data.outputs=&out;data.numSamples=512;
                    for(int pass=0;pass<4;++pass)require(processor->process(data)==kResultOk,"Silent host probe failed");
                    noModels();
                    ParameterChanges changes(1);int32 queueIndex{},pointIndex{};
                    changes.addParameterData(103,queueIndex)->addPoint(0,1,pointIndex);data.inputParameterChanges=&changes;
                    const int total=static_cast<int>(latency)+1024;
                    int offset=0;
                    while(offset<total) {
                        data.numSamples=std::min(512,total-offset);
                        for(int c=0;c<channels;++c)for(int i=0;i<data.numSamples;++i)input[c][i]=float((offset+i)%101+1)*(c?-.002f:.001f);
                        require(processor->process(data)==kResultOk,"Bypassed processing failed");
                        data.inputParameterChanges=nullptr;
                        for(int c=0;c<channels;++c)for(int i=0;i<data.numSamples;++i) {
                            const int source=offset+i-static_cast<int>(latency);
                            const float expected=source<0?0.f:float(source%101+1)*(c?-.002f:.001f);
                            require(std::abs(output[c][i]-expected)<1.e-7f,"Bypass changed audio or its advertised timing");
                        }
                        offset+=data.numSamples;
                    }
                    noModels();
                }
                require(processor->setProcessing(false)==kResultOk,"Processing stop failed");
                require(component->setActive(false)==kResultOk,"Deactivation failed");
                noModels();++configurations;
            }
            processor->release();require(component->terminate()==kResultOk,"Processor termination failed");component->release();++processors;
        }
        factory->release();if(exit)require(exit(),"Plugin exit failed");FreeLibrary(library);
        require(processors==6,"Expected all six audio effects");
        const auto elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count();
        std::cout<<processors<<" effects, "<<configurations<<" format/activation cycles, silence and exact delayed bypass passed with no NVIDIA runtime ("<<elapsed<<" s).\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
