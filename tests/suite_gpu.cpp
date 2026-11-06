#include <windows.h>
#include "AEConfig.h"
#include "AE_Effect.h"
#include "AE_EffectCB.h"
#include "AE_EffectCBSuites.h"
#include "SPBasic.h"
#include "PrSDKAESupport.h"
#include "core/frame.h"
#include "nvidia/audio_effects.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace fourbyfour;
using Main=PF_Err (*)(PF_Cmd,PF_InData*,PF_OutData*,PF_ParamDef*[],PF_LayerDef*,void*);
namespace {
std::array<PF_ParamDef,20> controls{};
PF_EffectWorld *sourceWorld{},*outputWorld{};
PF_PixelFormatSuite1 prPixels{};PF_WorldSuite2 aeWorld{};
PrPixelFormat prFormat{};PF_PixelFormat aeFormat{};
int acquisitions{},checkins{};
void require(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
PF_Err clear(PF_ProgPtr) {return 0;}
PF_Err addFormat(PF_ProgPtr,PrPixelFormat f) {return f==PrPixelFormat_BGRA_4444_8u||f==PrPixelFormat_BGRA_4444_32f?0:999;}
PF_Err prPixel(PF_EffectWorld*,PrPixelFormat* f) {*f=prFormat;return 0;}
PF_Err aePixel(const PF_EffectWorld*,PF_PixelFormat* f) {*f=aeFormat;return 0;}
SPErr acquire(const char* name,int32 version,const void** result) {
    if(!std::strcmp(name,kPFPixelFormatSuite)&&version==kPFPixelFormatSuiteVersion1) {*result=&prPixels;return 0;}
    if(!std::strcmp(name,kPFWorldSuite)&&version==kPFWorldSuiteVersion2) {*result=&aeWorld;return 0;}
    return 999;
}
SPErr release(const char*,int32) {return 0;}
PF_Err add(PF_ProgPtr,PF_ParamIndex index,PF_ParamDef* p) {if(index!=-1||p->uu.id<1||p->uu.id>=20) return 999;controls[p->uu.id]=*p;return 0;}
PF_Err abortRender(PF_ProgPtr) {return 0;}
PF_Err checkout(PF_ProgPtr,PF_ParamIndex index,A_long,A_long,A_u_long,PF_ParamDef* value) {
    if(index<0||index>=20) return 999;*value=controls[index];++acquisitions;return 0;
}
PF_Err checkin(PF_ProgPtr,PF_ParamDef*) {++checkins;return 0;}
PF_Err checkoutLayer(PF_ProgPtr,PF_ParamIndex index,A_long,const PF_RenderRequest* request,A_long,A_long,A_u_long,PF_CheckoutResult* result) {
    if(index!=0||request->rect.right<sourceWorld->width||request->rect.bottom<sourceWorld->height||!request->preserve_rgb_of_zero_alpha) return 999;
    result->result_rect={0,0,sourceWorld->width,sourceWorld->height};result->max_result_rect=result->result_rect;return 0;
}
PF_Err checkoutPixels(PF_ProgPtr,A_long id,PF_EffectWorld** value) {if(id<1||id>5) return 999;*value=sourceWorld;++acquisitions;return 0;}
PF_Err checkinPixels(PF_ProgPtr,A_long) {++checkins;return 0;}
PF_Err checkoutOutput(PF_ProgPtr,PF_EffectWorld** value) {*value=outputWorld;return 0;}
}
int wmain(int argc,wchar_t** argv) {
    std::vector<HMODULE> modules;
    try {
        require(argc>=4&&argc<=6,"SuiteGpuTests BIN_DIR input.ppm ARTIFACT_DIR [effect] [8bit]");
        std::ifstream file(std::filesystem::path(argv[2]),std::ios::binary);std::string magic;int w{},h{},range{};file>>magic>>w>>h>>range;file.get();
        require(magic=="P6"&&w>0&&h>0&&range==255,"Invalid face sample");
        std::vector<unsigned char> rgb(static_cast<std::size_t>(w)*h*3);require(bool(file.read(reinterpret_cast<char*>(rgb.data()),rgb.size())),"Truncated face sample");
        prPixels.ClearSupportedPixelFormats=clear;prPixels.AddSupportedPixelFormat=addFormat;prPixels.GetPixelFormat=prPixel;aeWorld.PF_GetPixelFormat=aePixel;
        SPBasicSuite basic{};basic.AcquireSuite=acquire;basic.ReleaseSuite=release;
        for(const auto* name:{L"EyeContact",L"BackgroundRemove",L"BackgroundBlur",L"StudioLight",L"VideoDenoise",L"AutoFrame",L"DetailRestore"}) {
            if(argc>=5&&std::wstring(argv[4])!=name) continue;
            const auto path=std::filesystem::path(argv[1])/(std::wstring(L"4x4")+name+L".aex");
            auto module=LoadLibraryExW(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);require(module,"Cannot load Adobe video effect");modules.push_back(module);
            auto main=reinterpret_cast<Main>(GetProcAddress(module,"EffectMain"));require(main,"Missing effect export");
            const bool removal=std::wstring(name)==L"BackgroundRemove",framing=std::wstring(name)==L"AutoFrame",eye=std::wstring(name)==L"EyeContact";
            std::vector<float> ae8Input;
            std::vector<std::uint8_t> ae8Output;
            for(const auto layout:{PixelLayout::argb8,PixelLayout::argb16,PixelLayout::argb32f,PixelLayout::bgra8,PixelLayout::bgra32f}) {
                if(argc>=6&&std::wstring(argv[5])==L"8bit"&&layout!=PixelLayout::argb8) continue;
                if(eye&&layout==PixelLayout::argb32f) continue;
                const bool pr=layout==PixelLayout::bgra8||layout==PixelLayout::bgra32f,smart=layout==PixelLayout::argb32f;
                const int bpp=static_cast<int>(pixelBytes(layout)),stride=w*bpp+32;
                std::vector<float> rgba(static_cast<std::size_t>(w)*h*4);
                for(std::size_t i=0;i<rgb.size()/3;++i) {for(int c=0;c<3;++c) rgba[i*4+c]=rgb[i*3+c]/255.f;rgba[i*4+3]=1;}
                std::vector<unsigned char> src(stride*h,0xef),dst(stride*h,0xcd);
                Frame input{src.data(),w,h,stride,layout};fromRgbaFloat(input,rgba);
                if(layout==PixelLayout::argb8) ae8Input=toBgrFloat(input);
                if(layout==PixelLayout::bgra8) require(ae8Input==toBgrFloat(input),"Host channel layouts changed the model input");
                PF_InData in{};PF_OutData out{};in.effect_ref=reinterpret_cast<PF_ProgPtr>(module);in.width=w;in.height=h;in.current_time=100;in.time_step=1;in.time_scale=30;in.total_time=300;
                in.appl_id=pr?'PrMr':'FXTC';in.inter.add_param=add;in.inter.abort=abortRender;in.inter.checkout_param=checkout;in.inter.checkin_param=checkin;in.pica_basicP=&basic;
                prFormat=layout==PixelLayout::bgra32f?PrPixelFormat_BGRA_4444_32f:PrPixelFormat_BGRA_4444_8u;
                aeFormat=smart?PF_PixelFormat_ARGB128:layout==PixelLayout::argb16?PF_PixelFormat_ARGB64:PF_PixelFormat_ARGB32;
                controls={};require(!main(PF_Cmd_GLOBAL_SETUP,&in,&out,nullptr,nullptr,nullptr),"Global setup failed");
                require(!main(PF_Cmd_PARAMS_SETUP,&in,&out,nullptr,nullptr,nullptr),"Parameter setup failed");
                if(!eye)controls[3].u.pd.value=1;
                controls[eye?4:16].u.bd.value=1;
                auto& world=controls[0].u.ld;world.width=w;world.height=h;world.rowbytes=stride;world.data=reinterpret_cast<PF_Pixel*>(src.data());world.world_flags=layout==PixelLayout::argb16?PF_WorldFlag_DEEP:0;
                PF_EffectWorld target=world;target.data=reinterpret_cast<PF_Pixel*>(dst.data());sourceWorld=&world;outputWorld=&target;
                std::array<PF_ParamDef*,20> params{};for(int i=0;i<20;++i) params[i]=&controls[i];
                acquisitions=checkins=0;PF_Err error{};
                auto render=[&] {
                    if(smart) {
                        PF_PreRenderInput input{};PF_PreRenderOutput output{};PF_PreRenderCallbacks cb{};cb.checkout_layer=checkoutLayer;
                        PF_PreRenderExtra pre{&input,&output,&cb};
                        auto e=main(PF_Cmd_SMART_PRE_RENDER,&in,&out,nullptr,nullptr,&pre);if(e) return e;
                        require(output.flags&PF_RenderOutputFlag_RETURNS_EXTRA_PIXELS,"SmartFX did not request full model context");
                        PF_SmartRenderCallbacks scb{};scb.checkout_layer_pixels=checkoutPixels;scb.checkin_layer_pixels=checkinPixels;scb.checkout_output=checkoutOutput;
                        PF_SmartRenderInput si{};PF_SmartRenderExtra se{&si,&scb};return main(PF_Cmd_SMART_RENDER,&in,&out,nullptr,nullptr,&se);
                    }
                    return main(PF_Cmd_RENDER,&in,&out,params.data(),&target,nullptr);
                };
                error=render();if(error) throw std::runtime_error(out.return_msg);
                require(acquisitions==checkins,"Adobe input/parameter checkouts were not balanced");
                std::size_t changed=0;
                for(int y=0;y<h;++y) for(int x=0;x<w;++x) {
                    const auto i=static_cast<std::size_t>(y)*stride+x*bpp;
                    if(x<w/2) require(!std::memcmp(src.data()+i,dst.data()+i,bpp),"Split comparison changed the original side");
                    else if(std::memcmp(src.data()+i,dst.data()+i,bpp)) ++changed;
                    if(!removal&&!framing) {const int a=pr?bpp*3/4:0;require(!std::memcmp(src.data()+i+a,dst.data()+i+a,bpp/4),"Color effect changed source alpha");}
                }
                for(int y=0;y<h;++y) for(int x=w*bpp;x<stride;++x) require(dst[static_cast<std::size_t>(y)*stride+x]==0xcd,"Effect overwrote row padding");
                require(changed>0,"Effect produced no visible change on its test sample");
                const auto pixels=toBgr({dst.data(),w,h,stride,layout});
                if(layout==PixelLayout::argb8) ae8Output=pixels;
                if(layout==PixelLayout::bgra8) {
                    int maximum=0;for(std::size_t i=0;i<pixels.size();++i) maximum=std::max(maximum,std::abs(int(pixels[i])-int(ae8Output[i])));
                    std::wcout<<name<<L" AE/Premiere 8-bit maximum difference="<<maximum<<L"\n";
                    require(maximum==0,"The same 8-bit image rendered differently in AE and Premiere");
                }
                const auto first=dst;error=render();if(error) throw std::runtime_error(out.return_msg);
                require(first==dst,"Quality output changes with previous render history");
                const auto original=src;
                std::vector<float> black(rgba.size(),0.f);for(std::size_t i=3;i<black.size();i+=4) black[i]=1;
                fromRgbaFloat(input,black);error=render();if(error) throw std::runtime_error(out.return_msg);
                src=original;error=render();if(error) throw std::runtime_error(out.return_msg);
                require(first==dst,"A different clip contaminated the quality render history");
                if(std::wstring(name)==L"DetailRestore"&&layout==PixelLayout::bgra32f) {
                    // Keep AR and all six VFX models loaded while AFX creates a
                    // session. Different SDK DLL families must coexist safely
                    // inside the same Adobe process.
                    AudioEngine voice({AudioEffect::noise,1,-1});
                    std::vector<float> audioIn(voice.inputFrames(),.05f),audioOut(voice.outputFrames());
                    const float* sources[]{audioIn.data()};float* targets[]{audioOut.data()};voice.run(sources,targets);
                    error=render();if(error) throw std::runtime_error(out.return_msg);
                    require(first==dst,"Concurrent audio runtime changed video output");
                    std::cout<<"AR, six VFX sessions and AFX coexistence passed.\n";
                }
                if(layout==PixelLayout::argb8) {
                    const auto result=toRgbaFloat({dst.data(),w,h,stride,layout});
                    std::ofstream image(std::filesystem::path(argv[3])/(std::wstring(name)+L"-adobe.rgba"),std::ios::binary);
                    image.write(reinterpret_cast<const char*>(result.data()),result.size()*sizeof(float));
                }
                std::wcout<<name<<L" "<<(pr?L"Premiere ":L"AE ")<<bpp*8<<L" bits/pixel: "<<changed<<L" changed pixels, exact comparison and repeat output.\n";
            }
        }
        for(auto module:modules) {auto main=reinterpret_cast<Main>(GetProcAddress(module,"EffectMain"));PF_InData in{};PF_OutData out{};main(PF_Cmd_GLOBAL_SETDOWN,&in,&out,nullptr,nullptr,nullptr);FreeLibrary(module);}return 0;
    } catch(const std::exception& e) {for(auto module:modules) FreeLibrary(module);std::cerr<<e.what()<<'\n';return 1;}
}
