#include "AEConfig.h"
#include "AE_Effect.h"
#include "AE_EffectCB.h"
#include "AE_EffectCBSuites.h"
#include "AE_EffectSuites.h"
#include "AE_Macros.h"
#include "Param_Utils.h"
#include "entry.h"
#include "SPBasic.h"
#include "PrSDKAESupport.h"
#include "EffectSpec.h"
#include "core/frame.h"
#include "core/image_ops.h"
#include "nvidia/video_effects.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <stdexcept>

using namespace fourbyfour;
namespace {
enum Param { Input,Enabled,Amount,Quality,KeepChair,Edge,Feather,ShowMask,Blur,Azimuth,Elevation,Exposure,Warmth,Zoom,Horizontal,Vertical,Compare,NoiseModel,Count };
constexpr bool removal=FOUR_VIDEO_KIND==0, blur=FOUR_VIDEO_KIND==1, relight=FOUR_VIDEO_KIND==2,
    denoise=FOUR_VIDEO_KIND==3, framing=FOUR_VIDEO_KIND==4, restoration=FOUR_VIDEO_KIND==5;
constexpr bool segmented=removal||blur||relight||framing;
bool premiere(PF_InData* in) { return in->appl_id=='PrMr'; }
template<class T> class Suite {
    SPBasicSuite* basic; const char* name; int version;
public:
    const T* p{};
    Suite(PF_InData* in,const char* n,int v):basic(in->pica_basicP),name(n),version(v) {
        if(!basic||basic->AcquireSuite(name,version,reinterpret_cast<const void**>(&p))||!p) throw std::runtime_error("Adobe image API is unavailable.");
    }
    ~Suite(){ if(p) basic->ReleaseSuite(name,version); }
};
PixelLayout layout(PF_InData* in,PF_EffectWorld* world,bool smart=false) {
    if(premiere(in)) {
        Suite<PF_PixelFormatSuite1> suite(in,kPFPixelFormatSuite,kPFPixelFormatSuiteVersion1);
        PrPixelFormat format{}; if(suite.p->GetPixelFormat(world,&format)) throw std::runtime_error("Unknown Premiere pixel format.");
        if(format==PrPixelFormat_BGRA_4444_8u) return PixelLayout::bgra8;
        if(format==PrPixelFormat_BGRA_4444_32f) return PixelLayout::bgra32f;
        throw std::runtime_error("Unsupported Premiere pixel format.");
    }
    if(smart) {
        Suite<PF_WorldSuite2> suite(in,kPFWorldSuite,kPFWorldSuiteVersion2);
        PF_PixelFormat format{}; if(suite.p->PF_GetPixelFormat(world,&format)) throw std::runtime_error("Unknown After Effects pixel format.");
        if(format==PF_PixelFormat_ARGB128) return PixelLayout::argb32f;
        if(format==PF_PixelFormat_ARGB64) return PixelLayout::argb16;
        if(format==PF_PixelFormat_ARGB32) return PixelLayout::argb8;
        throw std::runtime_error("Unsupported After Effects pixel format.");
    }
    return PF_WORLD_IS_DEEP(world)?PixelLayout::argb16:PixelLayout::argb8;
}
Frame frame(PF_InData* in,PF_EffectWorld& world,bool smart) { return {world.data,world.width,world.height,world.rowbytes,layout(in,&world,smart)}; }
PF_Err fail(PF_OutData* out,const char* message) {
    if(out) { std::snprintf(out->return_msg,sizeof(out->return_msg),"%s: %.200s",FOUR_EFFECT_NAME,message); out->out_flags|=PF_OutFlag_DISPLAY_ERROR_MESSAGE; }
    return PF_Err_INTERNAL_STRUCT_DAMAGED;
}
PF_Err parameters(PF_InData* in_data,PF_OutData* out_data) {
    PF_ParamDef def{};
    PF_ADD_CHECKBOX("Effect","Enabled",TRUE,0,Enabled);
    def={}; PF_ADD_FLOAT_SLIDERX("Mix",0,100,0,100,100,1,PF_ValueDisplayFlag_PERCENT,0,Amount);
    def={}; PF_ADD_POPUP("Processing Quality",2,1,"Highest Quality|Faster Preview",Quality);
    def={}; if(!segmented) def.ui_flags=PF_PUI_INVISIBLE;
    PF_ADD_CHECKBOX("Subject","Keep chair",FALSE,0,KeepChair);
    def={}; if(!segmented) def.ui_flags=PF_PUI_INVISIBLE;
    PF_ADD_FLOAT_SLIDERX("Matte Edge Bias",-100,100,-50,50,0,1,PF_ValueDisplayFlag_PERCENT,0,Edge);
    def={}; if(!segmented) def.ui_flags=PF_PUI_INVISIBLE;
    PF_ADD_FLOAT_SLIDERX("Edge Refinement",0,6,0,3,.75,2,PF_ValueDisplayFlag_NONE,0,Feather);
    def={}; if(!segmented) def.ui_flags=PF_PUI_INVISIBLE;
    PF_ADD_CHECKBOX("Matte View","Show confidence matte",FALSE,0,ShowMask);
    def={}; if(!blur) def.ui_flags=PF_PUI_INVISIBLE;
    PF_ADD_FLOAT_SLIDERX("Background Blur",0,100,0,100,50,1,PF_ValueDisplayFlag_PERCENT,0,Blur);
    def={}; if(!relight) def.ui_flags=PF_PUI_INVISIBLE;
    PF_ADD_FLOAT_SLIDERX("Light Direction",-180,180,-180,180,-35,1,PF_ValueDisplayFlag_NONE,0,Azimuth);
    def={}; if(!relight) def.ui_flags=PF_PUI_INVISIBLE;
    PF_ADD_FLOAT_SLIDERX("Light Elevation",-80,80,-60,60,25,1,PF_ValueDisplayFlag_NONE,0,Elevation);
    def={}; if(!relight) def.ui_flags=PF_PUI_INVISIBLE;
    PF_ADD_FLOAT_SLIDERX("Light Exposure",-4,4,-2,2,0,2,PF_ValueDisplayFlag_NONE,0,Exposure);
    def={}; if(!relight) def.ui_flags=PF_PUI_INVISIBLE;
    PF_ADD_FLOAT_SLIDERX("Light Warmth",-100,100,-100,100,0,1,PF_ValueDisplayFlag_NONE,0,Warmth);
    def={}; if(!framing) def.ui_flags=PF_PUI_INVISIBLE;
    PF_ADD_FLOAT_SLIDERX("Framing Zoom",100,400,100,200,125,1,PF_ValueDisplayFlag_PERCENT,0,Zoom);
    def={}; if(!framing) def.ui_flags=PF_PUI_INVISIBLE;
    PF_ADD_FLOAT_SLIDERX("Horizontal Trim",-50,50,-25,25,0,1,PF_ValueDisplayFlag_PERCENT,0,Horizontal);
    def={}; if(!framing) def.ui_flags=PF_PUI_INVISIBLE;
    PF_ADD_FLOAT_SLIDERX("Vertical Trim",-50,50,-25,25,0,1,PF_ValueDisplayFlag_PERCENT,0,Vertical);
    def={}; PF_ADD_CHECKBOX("Compare","Original left / effect right",FALSE,0,Compare);
    def={}; if(!denoise) def.ui_flags=PF_PUI_INVISIBLE;
    PF_ADD_POPUP("Noise Cleanup Model",2,1,"Preserve Fine Detail|Heavy Noise",NoiseModel);
    out_data->num_params=Count; return PF_Err_NONE;
}
VideoResult model(std::span<const float> proxy,int w,int h,VideoEffect effect,const VideoSettings& s,std::span<const std::vector<float>> past = {}) {
    if(denoise&&(w>1920||h>1080)) {
        // Full-resolution overlap tiles keep 4K detail instead of reducing a
        // whole frame to the webcam model's 1080p limit.
        const int tw=std::min(w,1920),th=std::min(h,1080),overlap=64;
        std::vector<int> xs{0},ys{0};
        while(xs.back()+tw<w) xs.push_back(std::min(xs.back()+tw-overlap*2,w-tw));
        while(ys.back()+th<h) ys.push_back(std::min(ys.back()+th-overlap*2,h-th));
        VideoResult result{w,h};result.bgr.assign(static_cast<std::size_t>(w)*h*3,0);
        std::vector<float> weight(static_cast<std::size_t>(w)*h,0),tile(static_cast<std::size_t>(tw)*th*3);
        for(const int top:ys) for(const int left:xs) {
            for(int y=0;y<th;++y) std::copy_n(proxy.data()+(static_cast<std::size_t>(top+y)*w+left)*3,tw*3,tile.data()+static_cast<std::size_t>(y)*tw*3);
            std::vector<std::vector<float>> window;
            for(const auto& frame:past) {
                auto& previous=window.emplace_back(static_cast<std::size_t>(tw)*th*3);
                for(int y=0;y<th;++y) std::copy_n(frame.data()+(static_cast<std::size_t>(top+y)*w+left)*3,tw*3,previous.data()+static_cast<std::size_t>(y)*tw*3);
            }
            window.push_back(tile);
            const auto processed=processVideoWindow(effect,window,tw,th,s);
            for(int y=0;y<th;++y) for(int x=0;x<tw;++x) {
                float wx=1,wy=1;
                if(left>0) wx=std::min(wx,(x+.5f)/overlap);
                if(left+tw<w) wx=std::min(wx,(tw-x-.5f)/overlap);
                if(top>0) wy=std::min(wy,(y+.5f)/overlap);
                if(top+th<h) wy=std::min(wy,(th-y-.5f)/overlap);
                const float amount=std::clamp(wx*wy,0.f,1.f);
                const auto to=static_cast<std::size_t>(top+y)*w+left+x,from=static_cast<std::size_t>(y)*tw+x;
                weight[to]+=amount;
                for(int c=0;c<3;++c) result.bgr[to*3+c]+=amount*processed.bgr[from*3+c];
            }
        }
        for(std::size_t i=0;i<weight.size();++i) for(int c=0;c<3;++c) result.bgr[i*3+c]/=weight[i];
        return result;
    }
    // The person model requires >=512x288. Keep aspect ratio and resize back
    // rather than stretching a portrait or an Adobe half-resolution preview.
    int mw=w,mh=h;
    const int minWidth=segmented?512:denoise?144:1,minHeight=segmented?288:denoise?80:1;
    if(w<minWidth||h<minHeight) {
        const double scale=std::max(double(minWidth)/w,double(minHeight)/h);
        mw=static_cast<int>(std::ceil(w*scale)); mh=static_cast<int>(std::ceil(h*scale));
    }
    auto resized=resample(proxy,w,h,3,mw,mh);
    std::vector<std::vector<float>> window;
    for(const auto& frame:past) window.push_back(resample(frame,w,h,3,mw,mh));
    window.push_back(std::move(resized));
    auto result=processVideoWindow(effect,window,mw,mh,s);
    if(mw!=w||mh!=h) {
        if(!result.bgr.empty()) result.bgr=resample(result.bgr,mw,mh,3,w,h);
        if(!result.matte.empty()) result.matte=resample(result.matte,mw,mh,1,w,h);
        result.width=w; result.height=h;
    }
    return result;
}
PF_Err render(PF_InData* in,PF_OutData*,PF_ParamDef* params[],PF_EffectWorld& source,PF_EffectWorld& destination,bool smart=false,
              std::span<const Frame> context = {}) {
    if(!source.width||!source.height||!destination.width||!destination.height) return PF_Err_NONE;
    auto input=frame(in,source,smart), output=frame(in,destination,smart);
    if(input.width!=output.width||input.height!=output.height||input.layout!=output.layout)
        throw std::runtime_error("Adobe supplied inconsistent full-frame image bounds.");
    auto proxy=toBgrFloat(input);
    const float mix=params[Enabled]->u.bd.value?static_cast<float>(params[Amount]->u.fs_d.value/100):0.f;
    const bool compare=params[Compare]->u.bd.value!=0;
    if(mix<=0) { applyFloatDelta(input,output,proxy,proxy,0); return PF_Err_NONE; }
    // U8 models see a quantized proxy. Compare their output against that exact
    // proxy so unchanged pixels retain the host's original 16/32-bit precision.
    if(!denoise) for(auto& value:proxy) value=std::round(value*255.f)/255.f;
    if(PF_ABORT(in)) return PF_Interrupt_CANCEL;
    VideoSettings settings;
    settings.contextId=reinterpret_cast<std::uintptr_t>(in->effect_ref);settings.time=in->current_time;settings.timeStep=in->time_step;
    settings.preview=params[Quality]->u.pd.value==2; settings.keepChair=params[KeepChair]->u.bd.value!=0;
    settings.strength=blur?static_cast<float>(params[Blur]->u.fs_d.value/100):denoise?(params[NoiseModel]->u.pd.value==2?1.f:0.f):1.f;
    settings.lightAzimuth=static_cast<float>(params[Azimuth]->u.fs_d.value);
    settings.lightElevation=static_cast<float>(params[Elevation]->u.fs_d.value);
    settings.lightExposure=static_cast<float>(params[Exposure]->u.fs_d.value);
    settings.lightWarmth=static_cast<float>(params[Warmth]->u.fs_d.value);
    constexpr auto effect=removal||framing?VideoEffect::matte:blur?VideoEffect::blur:relight?VideoEffect::relight:denoise?VideoEffect::denoise:VideoEffect::superResolution;
    std::vector<std::vector<float>> past;
    if(!settings.preview&&!restoration) for(std::size_t i=0;i<std::min<std::size_t>(context.size(),2);++i) {
        if(context[i].pixels&&context[i].width==input.width&&context[i].height==input.height) {
            auto p=toBgrFloat(context[i]);if(!denoise) for(auto& v:p) v=std::round(v*255.f)/255.f;
            past.push_back(std::move(p));
        }
    }
    auto processed=model(proxy,input.width,input.height,effect,settings,past);
    if(PF_ABORT(in)) return PF_Interrupt_CANCEL;
    if(segmented) refineMatte(processed.matte,proxy,input.width,input.height,
        static_cast<float>(params[Edge]->u.fs_d.value/100),settings.preview?0.f:static_cast<float>(params[Feather]->u.fs_d.value));
    if(segmented&&params[ShowMask]->u.bd.value) { showMatte(output,processed.matte); return PF_Err_NONE; }
    if(removal) { applyFloatDelta(input,output,proxy,proxy,mix,compare,processed.matte); return PF_Err_NONE; }
    if(framing) {
        auto center=subjectCenter(processed.matte,input.width,input.height);
        auto rgba=toRgbaFloat(input);
        if(!center.found) { applyFloatDelta(input,output,proxy,proxy,0); return PF_Err_NONE; }
        if(!settings.preview) {
            float x=center.centerX*4,y=center.centerY*4,total=4;
            for(const auto& adjacent:context) if(adjacent.pixels&&adjacent.width==input.width&&adjacent.height==input.height) {
                auto p=toBgrFloat(adjacent);
                const auto neighbor=model(p,input.width,input.height,VideoEffect::matte,settings);
                const auto candidate=subjectCenter(neighbor.matte,input.width,input.height);
                if(candidate.found) {x+=candidate.centerX;y+=candidate.centerY;++total;}
            }
            center.centerX=x/total;center.centerY=y/total;
        }
        const float zoom=1.f+mix*static_cast<float>(params[Zoom]->u.fs_d.value/100-1);
        const float cw=input.width/zoom,ch=input.height/zoom;
        const float cx=(.5f+mix*(center.centerX-.5f)+static_cast<float>(params[Horizontal]->u.fs_d.value/100))*input.width;
        // Place the upper-subject centroid at 38% of the output frame.
        const float cy=center.centerY*input.height+(.5f-.38f)*ch+static_cast<float>(params[Vertical]->u.fs_d.value/100)*input.height;
        const float left=std::clamp(cx-cw*.5f,0.f,input.width-cw), top=std::clamp(cy-ch*.5f,0.f,input.height-ch);
        auto transformed=resample(rgba,input.width,input.height,4,input.width,input.height,left,top,cw,ch);
        if(compare) for(int y=0;y<input.height;++y) for(int x=0;x<input.width/2;++x) for(int c=0;c<4;++c) {
            const auto i=(static_cast<std::size_t>(y)*input.width+x)*4+c; transformed[i]=rgba[i];
        }
        fromRgbaFloat(output,transformed); return PF_Err_NONE;
    }
    if(relight) for(std::size_t i=0;i<processed.matte.size();++i) for(int c=0;c<3;++c)
        processed.bgr[i*3+c]=proxy[i*3+c]+processed.matte[i]*(processed.bgr[i*3+c]-proxy[i*3+c]);
    applyFloatDelta(input,output,proxy,processed.bgr,mix,compare); return PF_Err_NONE;
}
PF_Err preRender(PF_InData* in,PF_PreRenderExtra* extra) {
    if(!extra||!extra->input||!extra->output||!extra->cb) return PF_Err_BAD_CALLBACK_PARAM;
    auto request=extra->input->output_request;
    request.rect={std::numeric_limits<A_long>::min()/4,std::numeric_limits<A_long>::min()/4,
                  std::numeric_limits<A_long>::max()/4,std::numeric_limits<A_long>::max()/4};
    request.preserve_rgb_of_zero_alpha=TRUE; request.channel_mask=PF_ChannelMask_ARGB;
    PF_CheckoutResult result{};
    const auto error=extra->cb->checkout_layer(in->effect_ref,0,1,&request,in->current_time,in->time_step,in->time_scale,&result);
    if(error) return error;
    extra->output->result_rect=result.result_rect; extra->output->max_result_rect=result.max_result_rect;
    extra->output->flags|=PF_RenderOutputFlag_RETURNS_EXTRA_PIXELS;
    if(!restoration&&in->time_step>0) {
        PF_ParamDef quality{};
        auto e=PF_CHECKOUT_PARAM(in,Quality,in->current_time,in->time_step,in->time_scale,&quality);
        if(e) return e;
        const bool high=quality.u.pd.value==1;PF_CHECKIN_PARAM(in,&quality);
        if(high) for(int i=0;i<(framing?4:2);++i) {
            const int delta=i<2?i-2:i-1;
            const auto time=std::clamp<std::int64_t>(std::int64_t(in->current_time)+std::int64_t(delta)*in->time_step,0,
                in->total_time>0?std::int64_t(in->total_time):std::numeric_limits<A_long>::max());
            PF_CheckoutResult adjacent{};
            e=extra->cb->checkout_layer(in->effect_ref,0,i+2,&request,static_cast<A_long>(time),in->time_step,in->time_scale,&adjacent);
            if(e) return e;
        }
    }
    return PF_Err_NONE;
}
PF_Err smartRender(PF_InData* in,PF_OutData* out,PF_SmartRenderExtra* extra) {
    if(!extra||!extra->cb) return PF_Err_BAD_CALLBACK_PARAM;
    std::array<PF_ParamDef,Count> values{}; std::array<PF_ParamDef*,Count> params{};
    int checked=0; bool layerChecked=false;int adjacentChecked=0;
    auto cleanup=[&] {for(int i=0;i<adjacentChecked;++i) extra->cb->checkin_layer_pixels(in->effect_ref,i+2); if(layerChecked) extra->cb->checkin_layer_pixels(in->effect_ref,1); for(int i=1;i<=checked;++i) PF_CHECKIN_PARAM(in,&values[i]); };
    try {
        for(int i=1;i<Count;++i) {
            const auto error=PF_CHECKOUT_PARAM(in,i,in->current_time,in->time_step,in->time_scale,&values[i]);
            if(error) { cleanup(); return error; } checked=i; params[i]=&values[i];
        }
        PF_EffectWorld *source{},*destination{};
        auto error=extra->cb->checkout_layer_pixels(in->effect_ref,1,&source);
        if(!error) { layerChecked=true; error=extra->cb->checkout_output(in->effect_ref,&destination); }
        std::array<Frame,4> context{};
        if(!error&&!restoration&&in->time_step>0&&params[Quality]->u.pd.value==1) for(int i=0;i<(framing?4:2);++i) {
            PF_EffectWorld* world{};error=extra->cb->checkout_layer_pixels(in->effect_ref,i+2,&world);
            if(error) break;++adjacentChecked;
            if(world&&world->data&&world->width&&world->height) context[i]=frame(in,*world,true);
        }
        if(!error&&source&&destination) error=render(in,out,params.data(),*source,*destination,true,context);
        cleanup(); return error;
    } catch(...) { cleanup(); throw; }
}
PF_Err legacyRender(PF_InData* in,PF_OutData* out,PF_ParamDef* params[],PF_EffectWorld& result) {
    std::array<PF_ParamDef,4> adjacent{};std::array<bool,4> checked{};std::array<Frame,4> context{};
    auto cleanup=[&] {for(int i=0;i<4;++i) if(checked[i]) PF_CHECKIN_PARAM(in,&adjacent[i]);};
    try {
        if(!restoration&&params[Quality]->u.pd.value==1&&in->time_step>0&&in->inter.checkout_param) for(int i=0;i<(framing?4:2);++i) {
            const int delta=i<2?i-2:i-1;
            const auto time=std::clamp<std::int64_t>(std::int64_t(in->current_time)+std::int64_t(delta)*in->time_step,0,
                in->total_time>0?std::int64_t(in->total_time):std::numeric_limits<A_long>::max());
            if(!PF_CHECKOUT_PARAM(in,0,static_cast<A_long>(time),in->time_step,in->time_scale,&adjacent[i])) {
                checked[i]=true;auto& world=adjacent[i].u.ld;
                if(world.data&&world.width&&world.height) context[i]=frame(in,world,false);
            }
        }
        const auto error=render(in,out,params,params[0]->u.ld,result,false,context);cleanup();return error;
    } catch(...) {cleanup();throw;}
}
}
extern "C" DllExport PF_Err EffectMain(PF_Cmd command,PF_InData* in,PF_OutData* out,PF_ParamDef* params[],PF_LayerDef* result,void* extra) {
    try {
        switch(command) {
        case PF_Cmd_ABOUT:
            if(out) std::snprintf(out->return_msg,sizeof(out->return_msg),"%s 0.2.0\r4x4Tools | NVIDIA Maxine\rHighest quality by default. Local SDR model processing.",FOUR_EFFECT_NAME);
            break;
        case PF_Cmd_GLOBAL_SETUP:
            if(!in||!out) return PF_Err_BAD_CALLBACK_PARAM;
            out->my_version=FOUR_VERSION; out->out_flags=FOUR_FLAGS; out->out_flags2=FOUR_FLAGS2;
            if(premiere(in)) {
                Suite<PF_PixelFormatSuite1> suite(in,kPFPixelFormatSuite,kPFPixelFormatSuiteVersion1);
                auto error=suite.p->ClearSupportedPixelFormats(in->effect_ref);
                if(!error) error=suite.p->AddSupportedPixelFormat(in->effect_ref,PrPixelFormat_BGRA_4444_32f);
                if(!error) error=suite.p->AddSupportedPixelFormat(in->effect_ref,PrPixelFormat_BGRA_4444_8u);
                if(error) return error;
            } break;
        case PF_Cmd_PARAMS_SETUP: return parameters(in,out);
        case PF_Cmd_RENDER:
            if(!in||!params||!params[0]||!result) return PF_Err_BAD_CALLBACK_PARAM;
            return legacyRender(in,out,params,*result);
        case PF_Cmd_SMART_PRE_RENDER: return preRender(in,static_cast<PF_PreRenderExtra*>(extra));
        case PF_Cmd_SMART_RENDER: return smartRender(in,out,static_cast<PF_SmartRenderExtra*>(extra));
        case PF_Cmd_GLOBAL_SETDOWN: shutdownVideo(); break;
        default: break;
        }
        return PF_Err_NONE;
    } catch(const std::bad_alloc&) { return PF_Err_OUT_OF_MEMORY; }
    catch(const std::exception& e) { return fail(out,e.what()); }
    catch(...) { return fail(out,"Unexpected processing error."); }
}
extern "C" DllExport PF_Err PluginDataEntryFunction2(PF_PluginDataPtr inPtr,PF_PluginDataCB2 callback,SPBasicSuite*,const char*,const char*) {
    if(!callback) return PF_Err_BAD_CALLBACK_PARAM;
    PF_Err result=PF_Err_NONE;
    PF_REGISTER_EFFECT_EXT2(inPtr,callback,FOUR_EFFECT_NAME,FOUR_MATCH_NAME,"4x4Tools",AE_RESERVED_INFO,"EffectMain","");
    return result;
}
