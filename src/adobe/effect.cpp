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
#include "metadata.h"
#include "core/frame.h"
#include "nvidia/eye_contact.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <stdexcept>

using namespace fourbyfour;
enum Parameter { Input, Enabled, Strength, EyeSize, Compare, ParameterCount };
static_assert(FOUR_VERSION == PF_VERSION(0, 1, 0, PF_Stage_DEVELOP, 1));
static_assert(FOUR_FLAGS == PF_OutFlag_DEEP_COLOR_AWARE);
static_assert(FOUR_FLAGS2 == PF_OutFlag2_SUPPORTS_THREADED_RENDERING);
static bool premiere(const PF_InData* in) { return in && in->appl_id == 'PrMr'; }
class PixelSuite {
    SPBasicSuite* basic{};
public:
    const PF_PixelFormatSuite1* suite{};
    explicit PixelSuite(PF_InData* in) : basic(in->pica_basicP) {
        if (!basic || basic->AcquireSuite(kPFPixelFormatSuite, kPFPixelFormatSuiteVersion1,
                    reinterpret_cast<const void**>(&suite)) || !suite)
            throw std::runtime_error("Premiere's pixel-format API is unavailable.");
    }
    ~PixelSuite() { if (suite) basic->ReleaseSuite(kPFPixelFormatSuite, kPFPixelFormatSuiteVersion1); }
};
static PF_Err failure(PF_OutData* out, const char* message) {
    if (out) {
        std::snprintf(out->return_msg, sizeof(out->return_msg), "4x4 Eye Contact: %.220s", message);
        out->out_flags |= PF_OutFlag_DISPLAY_ERROR_MESSAGE;
    }
    return PF_Err_INTERNAL_STRUCT_DAMAGED;
}
static PF_Err parameters(PF_InData* in_data, PF_OutData* out_data) {
    PF_ParamDef def{};
    PF_ADD_CHECKBOX("Eye Contact", "Enabled", TRUE, 0, Enabled);
    std::memset(&def, 0, sizeof(def));
    PF_ADD_FLOAT_SLIDERX("Strength", 0, 100, 0, 100, 100, 1, PF_ValueDisplayFlag_PERCENT, 0, Strength);
    std::memset(&def, 0, sizeof(def));
    PF_ADD_SLIDER("Eye Region Size", 2, 5, 2, 5, 3, EyeSize);
    std::memset(&def, 0, sizeof(def));
    PF_ADD_CHECKBOX("Compare", "Original left / corrected right", FALSE, 0, Compare);
    out_data->num_params = ParameterCount;
    return PF_Err_NONE;
}
static PixelLayout layout(PF_InData* in, PF_EffectWorld* world) {
    if (!premiere(in)) return PF_WORLD_IS_DEEP(world) ? PixelLayout::argb16 : PixelLayout::argb8;
    PixelSuite pixel(in); PrPixelFormat format{};
    if (pixel.suite->GetPixelFormat(world, &format)) throw std::runtime_error("Could not identify Premiere's image format.");
    if (format == PrPixelFormat_BGRA_4444_8u) return PixelLayout::bgra8;
    if (format == PrPixelFormat_BGRA_4444_32f) return PixelLayout::bgra32f;
    throw std::runtime_error("Unsupported Premiere image format.");
}
static PF_Err render(PF_InData* in, PF_OutData* out, PF_ParamDef* params[], PF_LayerDef* result) {
    if (!in || !out || !params || !params[Input] || !params[Enabled] || !params[Strength] ||
        !params[EyeSize] || !params[Compare] || !result) return PF_Err_BAD_CALLBACK_PARAM;
    auto& source = params[Input]->u.ld;
    if (source.width == 0 || source.height == 0 || result->width == 0 || result->height == 0) return PF_Err_NONE;
    const auto sourceLayout = layout(in, &source);
    if (sourceLayout != layout(in, result)) return failure(out, "Input and output image formats differ.");
    Frame input{source.data, source.width, source.height, source.rowbytes, sourceLayout};
    Frame output{result->data, result->width, result->height, result->rowbytes, sourceLayout};
    auto proxy = toBgr(input);
    if (PF_ABORT(in)) return PF_Interrupt_CANCEL;
    const float strength = params[Enabled]->u.bd.value ? static_cast<float>(params[Strength]->u.fs_d.value / 100.0) : 0;
    const auto corrected = strength > 0 ? correctEyes(proxy, input.width, input.height, {params[EyeSize]->u.sd.value}) : proxy;
    if (PF_ABORT(in)) return PF_Interrupt_CANCEL;
    applyBgrDelta(input, output, proxy, corrected, strength, params[Compare]->u.bd.value != 0);
    return PF_Err_NONE;
}
extern "C" DllExport PF_Err EffectMain(PF_Cmd command, PF_InData* in_data, PF_OutData* out_data,
    PF_ParamDef* params[], PF_LayerDef* output, void*) {
    try {
        switch (command) {
        case PF_Cmd_ABOUT:
            if (!out_data) return PF_Err_BAD_CALLBACK_PARAM;
            std::strcpy(out_data->return_msg, "4x4 Eye Contact 0.1.0-dev.1\rNVIDIA-accelerated gaze correction.\rDevelopment preview; single subject; SDR model processing.");
            break;
        case PF_Cmd_GLOBAL_SETUP:
            if (!out_data || !in_data) return PF_Err_BAD_CALLBACK_PARAM;
            out_data->my_version = FOUR_VERSION; out_data->out_flags = FOUR_FLAGS; out_data->out_flags2 = FOUR_FLAGS2;
            if (premiere(in_data)) {
                PixelSuite pixel(in_data);
                auto error = pixel.suite->ClearSupportedPixelFormats(in_data->effect_ref);
                if (!error) error = pixel.suite->AddSupportedPixelFormat(in_data->effect_ref, PrPixelFormat_BGRA_4444_32f);
                if (!error) error = pixel.suite->AddSupportedPixelFormat(in_data->effect_ref, PrPixelFormat_BGRA_4444_8u);
                if (error) return error;
            }
            break;
        case PF_Cmd_PARAMS_SETUP: return parameters(in_data, out_data);
        case PF_Cmd_RENDER: return render(in_data, out_data, params, output);
        case PF_Cmd_GLOBAL_SETDOWN: shutdownEyeContact(); break;
        default: break;
        }
        return PF_Err_NONE;
    } catch (const std::bad_alloc&) { return PF_Err_OUT_OF_MEMORY; }
    catch (const std::exception& error) { return failure(out_data, error.what()); }
    catch (...) { return failure(out_data, "Unexpected processing error."); }
}
extern "C" DllExport PF_Err PluginDataEntryFunction2(PF_PluginDataPtr inPtr, PF_PluginDataCB2 callback,
    SPBasicSuite*, const char*, const char*) {
    if (!callback) return PF_Err_BAD_CALLBACK_PARAM;
    PF_Err result = PF_Err_NONE;
    PF_REGISTER_EFFECT_EXT2(inPtr, callback, FOUR_EFFECT_NAME, FOUR_MATCH_NAME, FOUR_CATEGORY, AE_RESERVED_INFO, "EffectMain", "");
    return result;
}
