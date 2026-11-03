#include "AEConfig.h"
#include "AE_Effect.h"
#include "AE_EffectCB.h"
#include "AE_Macros.h"
#include "Param_Utils.h"
#include "entry.h"
#include "EffectSpec.h"
#include "core/audio_pipeline.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <deque>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>

using namespace fourbyfour;
namespace {
enum Param { Input,Enabled,Mix,Gain,Profile,Count };
constexpr auto kind=static_cast<AudioEffect>(FOUR_VIDEO_KIND);
struct Range { A_long first{},last{},window{}; };
Range sourceRange(PF_InData* in) {
    const double rate=in->src_snd.fi.rateF;
    if(!std::isfinite(rate)||rate<8000||rate>192000||in->start_sampL<0||in->dur_sampL<0) throw std::runtime_error("Invalid Adobe audio time range.");
    const std::int64_t window=static_cast<std::int64_t>(std::llround(rate*6));
    const std::int64_t start=in->start_sampL,end=start+in->dur_sampL;
    const auto first=std::max<std::int64_t>(0,(start/window)*window-window);
    auto last=((end+window-1)/window)*window+window;
    if(in->total_sampL>0) last=std::min<std::int64_t>(last,in->total_sampL);
    if(last>std::numeric_limits<A_long>::max()) throw std::runtime_error("Adobe audio range is too long.");
    return {static_cast<A_long>(first),static_cast<A_long>(last),static_cast<A_long>(window)};
}
std::mutex mutex;
std::unique_ptr<AudioPipeline> pipeline;
int pipelineRate{},pipelineChannels{},pipelineProfile{-99};
struct Cache { std::uint64_t hash{};std::vector<float> samples; };
std::deque<Cache> cache;
std::uint64_t hashInput(std::span<const float> data,int rate,int channels,int profile,A_long windowOffset) {
    std::uint64_t result=14695981039346656037ull;
    auto append=[&](const void* pointer,std::size_t bytes) {
        const auto* p=static_cast<const unsigned char*>(pointer);
        for(std::size_t i=0;i<bytes;++i) {result^=p[i];result*=1099511628211ull;}
    };
    append(data.data(),data.size_bytes());append(&rate,sizeof(rate));append(&channels,sizeof(channels));append(&profile,sizeof(profile));append(&windowOffset,sizeof(windowOffset));
    return result;
}
std::vector<float> window(std::span<const float> data,int rate,int channels,int profile,A_long crop,A_long length) {
    const auto hash=hashInput(data,rate,channels,profile,crop);
    for(const auto& item:cache) if(item.hash==hash&&item.samples.size()==static_cast<std::size_t>(length)*channels) return item.samples;
    if(!pipeline||pipelineRate!=rate||pipelineChannels!=channels||pipelineProfile!=profile) {
        pipeline.reset();cache.clear();
        pipeline=std::make_unique<AudioPipeline>(AudioSettings{kind,channels,profile},rate);
        pipelineRate=rate;pipelineChannels=channels;pipelineProfile=profile;
    } else pipeline->reset();
    constexpr int block=2048;
    std::array<std::array<float,block>,2> input{},output{};
    const float* sources[]{input[0].data(),input[1].data()};float* destinations[]{output[0].data(),output[1].data()};
    std::vector<float> result(static_cast<std::size_t>(length)*channels);
    const auto first=static_cast<std::size_t>(crop)+pipeline->latency(),end=first+length;
    for(std::size_t offset=0;offset<end;offset+=block) {
        const int n=static_cast<int>(std::min<std::size_t>(block,end-offset));
        for(int i=0;i<n;++i) for(int c=0;c<channels;++c) input[c][i]=(offset+i)*channels+c<data.size()?data[(offset+i)*channels+c]:0.f;
        pipeline->process(sources,destinations,n);
        for(int i=0;i<n;++i) if(offset+i>=first) for(int c=0;c<channels;++c) result[(offset+i-first)*channels+c]=output[c][i];
    }
    cache.push_front({hash,result});while(cache.size()>3) cache.pop_back();
    return result;
}
PF_Err render(PF_InData* in,PF_OutData* out,PF_ParamDef* params[]) {
    if(!in||!out||!params||!params[Enabled]||!params[Mix]||!params[Gain]||!params[Profile]) return PF_Err_BAD_CALLBACK_PARAM;
    const auto& src=in->src_snd;auto& dst=out->dest_snd;
    const int channels=src.fi.num_channels,rate=static_cast<int>(std::llround(src.fi.rateF));
    if(channels<1||channels>2||src.fi.format!=PF_SIGNED_FLOAT||src.fi.sample_size!=4||!src.dataP||!dst.dataP||src.num_samples<0||dst.num_samples<0||
       dst.fi.num_channels!=channels||dst.fi.sample_size!=4||dst.fi.format!=PF_SIGNED_FLOAT||std::abs(dst.fi.rateF-src.fi.rateF)>.001)
        throw std::runtime_error("After Effects must supply matching mono/stereo float audio.");
    const float mix=params[Enabled]->u.bd.value?static_cast<float>(params[Mix]->u.fs_d.value/100):0.f;
    const float gain=std::pow(10.f,static_cast<float>(params[Gain]->u.fs_d.value/20));
    const auto range=sourceRange(in);
    const bool bypass=mix<=0;
    const auto sourceStart=bypass?in->start_sampL:range.first;
    const auto* source=static_cast<const float*>(src.dataP);auto* destination=static_cast<float*>(dst.dataP);
    const auto outputFrames=std::min(dst.num_samples,in->dur_sampL);
    std::fill_n(destination,static_cast<std::size_t>(dst.num_samples)*channels,0.f);
    if(bypass) {
        const auto count=static_cast<std::size_t>(std::min(src.num_samples,outputFrames))*channels;
        if(gain==1) std::memcpy(destination,source,count*sizeof(float));
        else for(std::size_t i=0;i<count;++i) destination[i]=source[i]*gain;
        return PF_Err_NONE;
    }
    std::lock_guard lock(mutex);
    const int profile=std::clamp(params[Profile]->u.pd.value-2,-1,5);
    for(A_long offset=0;offset<outputFrames;) {
        if(PF_ABORT(in)) return PF_Interrupt_CANCEL;
        const A_long absolute=in->start_sampL+offset;
        const A_long blockStart=(absolute/range.window)*range.window;
        const A_long contextStart=std::max<A_long>(0,blockStart-range.window);
        const A_long contextEnd=std::min<A_long>(sourceStart+src.num_samples,blockStart+range.window*2);
        const auto first=std::clamp<A_long>(contextStart-sourceStart,0,src.num_samples);
        const auto last=std::clamp<A_long>(contextEnd-sourceStart,first,src.num_samples);
        const auto available=std::span(source+static_cast<std::size_t>(first)*channels,static_cast<std::size_t>(last-first)*channels);
        const auto processed=window(available,rate,channels,profile,blockStart-contextStart,range.window);
        const A_long within=absolute-blockStart,count=std::min(outputFrames-offset,range.window-within);
        for(A_long i=0;i<count;++i) for(int c=0;c<channels;++c) {
            const auto target=static_cast<std::size_t>(offset+i)*channels+c;
            const auto dryFrame=absolute+i-sourceStart;
            const float dry=dryFrame>=0&&dryFrame<src.num_samples?source[static_cast<std::size_t>(dryFrame)*channels+c]:0.f;
            const float wet=processed[(static_cast<std::size_t>(within)+i)*channels+c];
            destination[target]=(dry+mix*(wet-dry))*gain;
        }
        offset+=count;
    }
    return PF_Err_NONE;
}
PF_Err parameters(PF_InData* in_data,PF_OutData* out_data) {
    PF_ParamDef def{};PF_ADD_CHECKBOX("Voice Effect","Enabled",true,0,Enabled);
    def={};PF_ADD_FLOAT_SLIDERX("Mix",0,100,0,100,100,1,PF_ValueDisplayFlag_PERCENT,PF_ParamFlag_CANNOT_TIME_VARY,Mix);
    def={};PF_ADD_FLOAT_SLIDERX("Output Gain",-24,12,-12,6,0,2,PF_ValueDisplayFlag_NONE,PF_ParamFlag_CANNOT_TIME_VARY,Gain);
    def={};if(kind!=AudioEffect::studioPreview) def.ui_flags=PF_PUI_INVISIBLE;
    def.flags=PF_ParamFlag_CANNOT_TIME_VARY;
    PF_ADD_POPUP("Microphone Character",7,1,"Natural|Bright|Vibrant|Flat|Full|Thin|Warm",Profile);
    out_data->num_params=Count;return PF_Err_NONE;
}
}
extern "C" DllExport PF_Err EffectMain(PF_Cmd command,PF_InData* in,PF_OutData* out,PF_ParamDef* params[],PF_LayerDef*,void*) {
    try {
        switch(command) {
        case PF_Cmd_ABOUT:
            if(out) std::snprintf(out->return_msg,sizeof(out->return_msg),"%s 0.2.0-preview.1\r4x4Tools | NVIDIA Maxine\rNative After Effects audio; deterministic analysis windows.",FOUR_EFFECT_NAME);break;
        case PF_Cmd_GLOBAL_SETUP:
            if(!in||!out) return PF_Err_BAD_CALLBACK_PARAM;
            out->my_version=FOUR_VERSION;out->out_flags=FOUR_FLAGS;out->out_flags2=FOUR_FLAGS2;break;
        case PF_Cmd_PARAMS_SETUP:return parameters(in,out);
        case PF_Cmd_AUDIO_SETUP:
            if(!in||!out||!params) return PF_Err_BAD_CALLBACK_PARAM;
            if(params[Enabled]->u.bd.value&&params[Mix]->u.fs_d.value>0) {
                const auto range=sourceRange(in);out->start_sampL=range.first;out->dur_sampL=range.last-range.first;
            } else {out->start_sampL=in->start_sampL;out->dur_sampL=in->dur_sampL;}break;
        case PF_Cmd_AUDIO_RENDER:return render(in,out,params);
        case PF_Cmd_GLOBAL_SETDOWN:{std::lock_guard lock(mutex);cache.clear();pipeline.reset();break;}
        default:break;
        }
        return PF_Err_NONE;
    } catch(const std::bad_alloc&) {return PF_Err_OUT_OF_MEMORY;}
    catch(const std::exception& e) {
        if(out) {std::snprintf(out->return_msg,sizeof(out->return_msg),"%s: %.190s",FOUR_EFFECT_NAME,e.what());out->out_flags|=PF_OutFlag_DISPLAY_ERROR_MESSAGE;}
        return PF_Err_INTERNAL_STRUCT_DAMAGED;
    } catch(...) {return PF_Err_INTERNAL_STRUCT_DAMAGED;}
}
extern "C" DllExport PF_Err PluginDataEntryFunction2(PF_PluginDataPtr inPtr,PF_PluginDataCB2 callback,SPBasicSuite*,const char*,const char*) {
    if(!callback) return PF_Err_BAD_CALLBACK_PARAM;PF_Err result=PF_Err_NONE;
    PF_REGISTER_EFFECT_EXT2(inPtr,callback,FOUR_EFFECT_NAME,FOUR_MATCH_NAME,"4x4Tools",AE_RESERVED_INFO,"EffectMain","");return result;
}
