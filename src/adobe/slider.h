#pragma once
// The SDK's X macro clears def, including a caller's PF_PUI_INVISIBLE flag.
// Callers initialize def explicitly; preserve their effect-specific UI flags.
#define FOUR_ADD_FLOAT_SLIDERX(NAME,VMIN,VMAX,SMIN,SMAX,DFLT,PREC,DISP,FLAGS,ID) \
    do { def.flags=(FLAGS); \
        PF_ADD_FLOAT_SLIDER(NAME,VMIN,VMAX,SMIN,SMAX,AEFX_DEFAULT_CURVE_TOLERANCE,DFLT,PREC,DISP,false,ID); \
    } while(0)
