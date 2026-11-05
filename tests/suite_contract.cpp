#include <windows.h>
#include "AEConfig.h"
#include "AE_Effect.h"
#include "AE_EffectCB.h"
#include <array>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <vector>
using Main=PF_Err (*)(PF_Cmd,PF_InData*,PF_OutData*,PF_ParamDef*[],PF_LayerDef*,void*);
static std::array<PF_ParamDef,19> controls{};
static PF_Err add(PF_ProgPtr,PF_ParamIndex index,PF_ParamDef* p) {if(index!=-1||p->uu.id<1||p->uu.id>=19) return 999;controls[p->uu.id]=*p;return 0;}
static PF_Err abortRender(PF_ProgPtr) {return 0;}
void require(bool result,const char* text) {if(!result) throw std::runtime_error(text);}
int wmain(int argc,wchar_t** argv) {
    try {
        require(argc==2,"Build directory required");
        for(const auto* name:{L"BackgroundRemove",L"BackgroundBlur",L"StudioLight",L"VideoDenoise",L"AutoFrame",L"DetailRestore",L"NoiseClean",L"RoomEchoClean",L"VoiceClean",L"StudioVoicePreview",L"StudioVoiceHQ",L"VoiceRestore"}) {
            const bool audio=std::wstring(name)==L"NoiseClean"||std::wstring(name)==L"RoomEchoClean"||std::wstring(name)==L"VoiceClean"||std::wstring(name)==L"StudioVoicePreview"||std::wstring(name)==L"StudioVoiceHQ"||std::wstring(name)==L"VoiceRestore";
            const auto path=std::filesystem::path(argv[1])/(std::wstring(L"4x4")+name+L".aex");
            auto module=LoadLibraryExW(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
            require(module!=nullptr,"Effect cannot load without GPU initialization");
            try {
                auto main=reinterpret_cast<Main>(GetProcAddress(module,"EffectMain"));require(main&&GetProcAddress(module,"PluginDataEntryFunction2"),"Missing Adobe exports");
                require(FindResourceW(module,MAKEINTRESOURCEW(16000),L"PiPL")!=nullptr,"Missing PiPL");
                PF_InData in{};PF_OutData out{};in.inter.add_param=add;in.inter.abort=abortRender;
                controls={};require(!main(PF_Cmd_GLOBAL_SETUP,&in,&out,nullptr,nullptr,nullptr),"Global setup failed");
                require(out.my_version==PF_VERSION(0,2,1,PF_Stage_DEVELOP,1),"Wrong effect version");
                require(audio?bool(out.out_flags&PF_OutFlag_AUDIO_EFFECT_ONLY):bool(out.out_flags2&PF_OutFlag2_SUPPORTS_SMART_RENDER),"Incorrect Adobe capability flags");
                require(!main(PF_Cmd_PARAMS_SETUP,&in,&out,nullptr,nullptr,nullptr)&&out.num_params==(audio?7:19),"Wrong parameter contract");
                std::array<PF_ParamDef*,19> params{};for(int i=0;i<19;++i) params[i]=&controls[i];
                require(controls[1].u.bd.value&&controls[2].u.fs_d.value==100,"Effect and mix defaults are not enabled");
                if(!audio)require(controls[3].u.pd.value==2,"New video instances must default to interactive processing");
                controls[1].u.bd.value=0;
                if(audio) {
                    std::array<float,18> source{},destination{};for(int i=0;i<18;++i) source[i]=i*.0173f;
                    in.src_snd={{48000,2,PF_SIGNED_FLOAT,PF_SSS_4},9,source.data()};out.dest_snd=in.src_snd;out.dest_snd.dataP=destination.data();in.dur_sampL=9;
                    require(!main(PF_Cmd_AUDIO_RENDER,&in,&out,params.data(),nullptr,nullptr),"AE audio bypass failed");
                    require(!std::memcmp(source.data(),destination.data(),sizeof(source)),"AE audio bypass changes samples");
                } else for(int bytes:{4,8}) {
                    const int w=13,h=5,stride=w*bytes+12;
                    std::vector<unsigned char> source(stride*h),destination(stride*h,0xcd);
                    for(std::size_t i=0;i<source.size();++i) source[i]=static_cast<unsigned char>(i*17);
                    auto& world=controls[0].u.ld;world.width=w;world.height=h;world.rowbytes=stride;world.data=reinterpret_cast<PF_Pixel*>(source.data());world.world_flags=bytes==8?PF_WorldFlag_DEEP:0;
                    auto target=world;target.data=reinterpret_cast<PF_Pixel*>(destination.data());
                    require(!main(PF_Cmd_RENDER,&in,&out,params.data(),&target,nullptr),"Video bypass failed");
                    for(int y=0;y<h;++y) {require(!std::memcmp(source.data()+y*stride,destination.data()+y*stride,w*bytes),"Video bypass changed precision/alpha");for(int x=w*bytes;x<stride;++x) require(destination[y*stride+x]==0xcd,"Video overwrote row padding");}
                }
                main(PF_Cmd_GLOBAL_SETDOWN,&in,&out,nullptr,nullptr,nullptr);
            } catch(...) {FreeLibrary(module);throw;}
            FreeLibrary(module);
        }
        std::cout<<"Twelve new Adobe effects: exports, PiPL, capabilities, parameters and exact bypass passed.\n";return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
