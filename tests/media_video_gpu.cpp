// Local media regression harness. Inputs and model outputs remain outside source control.
#include <windows.h>
#include "AEConfig.h"
#include "AE_Effect.h"
#include "AE_EffectCB.h"
#include "AE_EffectCBSuites.h"
#include "SPBasic.h"
#include "PrSDKAESupport.h"
#include "core/frame.h"
#include <array>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <mutex>
#include <vector>

using Main=PF_Err (*)(PF_Cmd,PF_InData*,PF_OutData*,PF_ParamDef*[],PF_LayerDef*,void*);
thread_local std::array<PF_ParamDef,19> controls;
thread_local PrPixelFormat format;
PF_PixelFormatSuite1 pixels{};
PF_Err clear(PF_ProgPtr){return 0;}
PF_Err addFormat(PF_ProgPtr,PrPixelFormat){return 0;}
PF_Err getFormat(PF_EffectWorld*,PrPixelFormat* value){*value=format;return 0;}
SPErr acquire(const char* name,int32 version,const void** result){
    if(!std::strcmp(name,kPFPixelFormatSuite)&&version==kPFPixelFormatSuiteVersion1){*result=&pixels;return 0;}return 999;
}
SPErr release(const char*,int32){return 0;}
PF_Err add(PF_ProgPtr,PF_ParamIndex,PF_ParamDef* value){controls.at(value->uu.id)=*value;return 0;}
PF_Err abortRender(PF_ProgPtr){return 0;}
PF_Err checkout(PF_ProgPtr,PF_ParamIndex index,A_long,A_long,A_u_long,PF_ParamDef* value){*value=controls.at(index);return 0;}
PF_Err checkin(PF_ProgPtr,PF_ParamDef*){return 0;}

int wmain(int argc,wchar_t** argv){
    HMODULE module{};
    try{
        if(argc<5)throw std::runtime_error("MediaVideoGpu BIN_DIR frame.ppm output.ppm Effect [quality 1|2] [frames] [host-contexts 0|1] [float 0|1] [repeat-time 0|1]");
        const int quality=argc>5?std::stoi(argv[5]):1,count=argc>6?std::stoi(argv[6]):3;
        const bool contexts=argc>7&&std::stoi(argv[7]),floating=argc>8&&std::stoi(argv[8]);
        const bool repeatTime=argc>9&&std::stoi(argv[9]);
        const bool parallel=argc>10&&std::stoi(argv[10]);
        const std::filesystem::path path=std::filesystem::absolute(argv[1])/(std::wstring(L"4x4")+argv[4]+L".aex");
        module=LoadLibraryExW(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
        if(!module)throw std::runtime_error("Cannot load the effect");
        auto main=reinterpret_cast<Main>(GetProcAddress(module,"EffectMain"));if(!main)throw std::runtime_error("Missing effect entry point");
        std::ifstream file(std::filesystem::path(argv[2]),std::ios::binary);std::string magic;int w{},h{},range{};file>>magic>>w>>h>>range;file.get();
        if(magic!="P6"||w<1||h<1||range!=255)throw std::runtime_error("Expected an RGB P6 frame");
        std::vector<unsigned char> rgb(static_cast<std::size_t>(w)*h*3);
        if(!file.read(reinterpret_cast<char*>(rgb.data()),rgb.size()))throw std::runtime_error("Truncated frame");
        pixels.ClearSupportedPixelFormats=clear;pixels.AddSupportedPixelFormat=addFormat;pixels.GetPixelFormat=getFormat;
        SPBasicSuite basic{};basic.AcquireSuite=acquire;basic.ReleaseSuite=release;
        PF_InData setup{};PF_OutData setupOut{};setup.appl_id='PrMr';setup.pica_basicP=&basic;
        if(main(PF_Cmd_GLOBAL_SETUP,&setup,&setupOut,nullptr,nullptr,nullptr))throw std::runtime_error(setupOut.return_msg);
        auto driver=LoadLibraryExW(L"nvcuda.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
        auto init=reinterpret_cast<int(*)(unsigned)>(GetProcAddress(driver,"cuInit"));
        auto create=reinterpret_cast<int(*)(void**,unsigned,int)>(GetProcAddress(driver,"cuCtxCreate_v2"));
        auto set=reinterpret_cast<int(*)(void*)>(GetProcAddress(driver,"cuCtxSetCurrent"));
        auto get=reinterpret_cast<int(*)(void**)>(GetProcAddress(driver,"cuCtxGetCurrent"));
        auto destroy=reinterpret_cast<int(*)(void*)>(GetProcAddress(driver,"cuCtxDestroy_v2"));
        std::array<void*,2> host{};
        if(contexts){if(!init||!create||!set||!get||!destroy||init(0)||create(&host[0],0,0)||create(&host[1],0,0))throw std::runtime_error("Cannot create host CUDA contexts");set(nullptr);}
        std::vector<unsigned char> result;std::exception_ptr failure;std::mutex capture;
        std::vector<std::thread> workers;
        for(int index=0;index<count;++index){
            auto run=[&,index]{try{
                if(contexts)set(host[index%2]);
                PF_InData in{};PF_OutData out{};in.appl_id='PrMr';in.pica_basicP=&basic;
                in.width=w;in.height=h;in.current_time=repeatTime?0:index;in.time_step=1;in.time_scale=30;in.total_time=180;
                in.effect_ref=reinterpret_cast<PF_ProgPtr>(module);in.inter.add_param=add;in.inter.abort=abortRender;
                in.inter.checkout_param=checkout;in.inter.checkin_param=checkin;
                controls={};if(main(PF_Cmd_PARAMS_SETUP,&in,&out,nullptr,nullptr,nullptr))throw std::runtime_error(out.return_msg);
                if(std::wstring(argv[4])!=L"EyeContact")controls[3].u.pd.value=quality;
                const int stride=w*(floating?16:4)+32;
                std::vector<unsigned char> source(static_cast<std::size_t>(stride)*h),destination(source.size(),0xcd);
                fourbyfour::Frame frame{source.data(),w,h,stride,floating?fourbyfour::PixelLayout::bgra32f:fourbyfour::PixelLayout::bgra8};
                std::vector<float> rgba(static_cast<std::size_t>(w)*h*4);
                for(std::size_t i=0;i<rgb.size()/3;++i){for(int c=0;c<3;++c)rgba[i*4+c]=rgb[i*3+c]/255.f;rgba[i*4+3]=1;}
                fourbyfour::fromRgbaFloat(frame,rgba);format=floating?PrPixelFormat_BGRA_4444_32f:PrPixelFormat_BGRA_4444_8u;
                auto& world=controls[0].u.ld;world.width=w;world.height=h;world.rowbytes=stride;world.data=reinterpret_cast<PF_Pixel*>(source.data());
                auto output=world;output.data=reinterpret_cast<PF_Pixel*>(destination.data());
                std::array<PF_ParamDef*,19> params{};for(int i=0;i<19;++i)params[i]=&controls[i];
                const auto start=std::chrono::steady_clock::now();const auto error=main(PF_Cmd_RENDER,&in,&out,params.data(),&output,nullptr);
                const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
                if(error)throw std::runtime_error(out.return_msg);
                if(contexts){void* current{};if(get(&current)||current!=host[index%2])throw std::runtime_error("Effect replaced the host CUDA context");}
                auto next=fourbyfour::toBgr({destination.data(),w,h,stride,frame.layout});
                PF_InData unused=in;unused.effect_ref=reinterpret_cast<PF_ProgPtr>(std::uintptr_t(1));
                if(main(PF_Cmd_SEQUENCE_SETDOWN,&unused,&out,nullptr,nullptr,nullptr))throw std::runtime_error("Unrelated instance teardown failed");
                std::lock_guard guard(capture);
                if(repeatTime&&!result.empty()&&next!=result)throw std::runtime_error("Cached frame pixels changed");
                result=std::move(next);
                std::cout<<"frame="<<index<<" ms="<<ms<<" fps="<<1000/ms<<" size="<<w<<'x'<<h<<" host_context="<<(contexts?index%2:-1)<<"\n"<<std::flush;
                if(contexts)set(nullptr);
            }catch(...){std::lock_guard guard(capture);failure=std::current_exception();}};
            if(parallel)workers.emplace_back(run);
            else {std::thread thread(run);thread.join();if(failure)std::rethrow_exception(failure);}
        }
        for(auto& worker:workers)worker.join();if(failure)std::rethrow_exception(failure);
        if(contexts)set(host[0]);
        main(PF_Cmd_GLOBAL_SETDOWN,&setup,&setupOut,nullptr,nullptr,nullptr);FreeLibrary(module);module=nullptr;
        if(contexts){set(nullptr);for(auto value:host)destroy(value);}
        std::ofstream output(std::filesystem::path(argv[3]),std::ios::binary);output<<"P6\n"<<w<<' '<<h<<"\n255\n";
        for(std::size_t i=0;i<result.size();i+=3)std::swap(result[i],result[i+2]);
        output.write(reinterpret_cast<const char*>(result.data()),result.size());return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';if(module)FreeLibrary(module);return 1;}
}
