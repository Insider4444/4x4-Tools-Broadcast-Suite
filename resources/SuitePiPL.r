#include "AEConfig.h"
#include "AE_EffectVers.h"
#include "EffectSpec.h"
resource 'PiPL' (16000) {
    {
        Kind { AEEffect },
        Name { FOUR_EFFECT_NAME },
        Category { "4x4Tools" },
        CodeWin64X86 { "EffectMain" },
        AE_PiPL_Version { 2, 0 },
        AE_Effect_Spec_Version { PF_PLUG_IN_VERSION, PF_PLUG_IN_SUBVERS },
        AE_Effect_Version { FOUR_VERSION },
        AE_Effect_Info_Flags { 0 },
        AE_Effect_Global_OutFlags { FOUR_FLAGS },
        AE_Effect_Global_OutFlags_2 { FOUR_FLAGS2 },
        AE_Effect_Match_Name { FOUR_MATCH_NAME },
        AE_Reserved_Info { 8 },
        AE_Effect_Support_URL { "" }
    }
};
