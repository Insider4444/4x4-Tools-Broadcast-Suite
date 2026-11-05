#include <windows.h>
#include "AEConfig.h"
#include "AE_Effect.h"
#include "AE_EffectCB.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
using Main=PF_Err (*)(PF_Cmd,PF_InData*,PF_OutData*,PF_ParamDef*[],PF_LayerDef*,void*);
namespace {
std::array<PF_ParamDef,7> controls{};
void require(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
PF_Err add(PF_ProgPtr,PF_ParamIndex index,PF_ParamDef* value) {if(index!=-1||value->uu.id<1||value->uu.id>=7) return 999;controls[value->uu.id]=*value;return 0;}
PF_Err abortRender(PF_ProgPtr) {return 0;}
}
int wmain(int argc,wchar_t** argv) {
    HMODULE module{};
    try {
        require(argc==3,"AdobeAudioGpuTests BIN_DIR mono.f32");
        std::ifstream file(std::filesystem::path(argv[2]),std::ios::binary|std::ios::ate);const auto bytes=file.tellg();file.seekg(0);
        require(bytes>0&&bytes%4==0,"Invalid speech input");std::vector<float> mono(static_cast<std::size_t>(bytes)/4);
        file.read(reinterpret_cast<char*>(mono.data()),bytes);const int count=static_cast<int>(mono.size());
        std::vector<float> stereo(mono.size()*2);for(std::size_t i=0;i<mono.size();++i) stereo[i*2]=mono[i];
        for(const auto* name:{L"NoiseClean",L"RoomEchoClean",L"VoiceClean",L"StudioVoicePreview",L"StudioVoiceHQ",L"VoiceRestore"}) {
            const auto path=std::filesystem::path(argv[1])/(std::wstring(L"4x4")+name+L".aex");
            module=LoadLibraryExW(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);require(module,"Cannot load AE audio effect");
            auto main=reinterpret_cast<Main>(GetProcAddress(module,"EffectMain"));require(main,"Missing AE audio export");
            PF_InData in{};PF_OutData out{};in.appl_id='FXTC';in.inter.add_param=add;in.inter.abort=abortRender;
            in.total_sampL=count;in.src_snd.fi={48000,2,PF_SIGNED_FLOAT,PF_SSS_4};
            require(!main(PF_Cmd_GLOBAL_SETUP,&in,&out,nullptr,nullptr,nullptr),"Audio setup failed");
            controls={};require(!main(PF_Cmd_PARAMS_SETUP,&in,&out,nullptr,nullptr,nullptr),"Audio parameter setup failed");
            std::array<PF_ParamDef*,7> params{};for(int i=0;i<7;++i) params[i]=&controls[i];
            auto render=[&](int start,int frames) {
                in.start_sampL=start;in.dur_sampL=frames;
                auto error=main(PF_Cmd_AUDIO_SETUP,&in,&out,params.data(),nullptr,nullptr);if(error) throw std::runtime_error(out.return_msg);
                const auto inputFirst=out.start_sampL,inputFrames=out.dur_sampL;
                require(inputFirst>=0&&inputFirst<=start&&inputFirst+inputFrames>=std::min(start+frames,count),"AUDIO_SETUP requested insufficient context");
                in.src_snd.num_samples=inputFrames;in.src_snd.dataP=stereo.data()+static_cast<std::size_t>(inputFirst)*2;
                std::vector<float> result(static_cast<std::size_t>(frames)*2+16,12345.f);
                out.dest_snd={in.src_snd.fi,frames,result.data()};
                error=main(PF_Cmd_AUDIO_RENDER,&in,&out,params.data(),nullptr,nullptr);if(error) throw std::runtime_error(out.return_msg);
                for(std::size_t i=static_cast<std::size_t>(frames)*2;i<result.size();++i) require(result[i]==12345.f,"AE audio overwrote its output buffer");
                result.resize(static_cast<std::size_t>(frames)*2);
                for(std::size_t i=0;i<result.size();++i) {require(std::isfinite(result[i]),"Non-finite AE audio sample");if(i%2) require(result[i]==0,"AE audio introduced sound into a silent channel");}
                return result;
            };
            const int frames=std::min(count,48000*6+10017);
            const auto whole=render(0,frames),a=render(0,7001),b=render(7001,frames-7001);
            require(std::equal(a.begin(),a.end(),whole.begin())&&std::equal(b.begin(),b.end(),whole.begin()+a.size()),"AE render request boundaries change the audio");
            double energy=0;for(float value:whole) energy+=double(value)*value;require(energy>.001,"AE voice output is silent");
            std::wcout<<name<<L": stereo audio, context checkout, split-request consistency, silence isolation and buffer bounds passed.\n";
            main(PF_Cmd_GLOBAL_SETDOWN,&in,&out,nullptr,nullptr,nullptr);FreeLibrary(module);module=nullptr;
        }
        return 0;
    } catch(const std::exception& e) {if(module) FreeLibrary(module);std::cerr<<e.what()<<'\n';return 1;}
}
