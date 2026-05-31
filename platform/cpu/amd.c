// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#include "../cpu.h"

#define AMD_F15_EXCAVATOR_MODEL_MIN 0x60

#define AMD_ZEN1_NAPLES 0x01
#define AMD_ZEN1_SUMMIT_RIDGE 0x01
#define AMD_ZEN1_RAVEN_RIDGE 0x11

#define AMD_ZEN_PLUS_PINNACLE_RIDGE 0x08
#define AMD_ZEN_PLUS_PICASSO 0x18
#define AMD_ZEN_PLUS_DALI 0x20

#define AMD_ZEN2_ROME 0x31
#define AMD_ZEN2_RENOIR 0x60
#define AMD_ZEN2_MATISSE 0x71

#define AMD_ZEN3_VERMEER 0x21
#define AMD_ZEN3_CEZANNE 0x50
#define AMD_ZEN3_REMBRANDT 0x44

#define AMD_ZEN4_RAPHAEL 0x61
#define AMD_ZEN4_GENOA 0x11
#define AMD_ZEN4_PHOENIX 0x74
#define AMD_ZEN4_HAWK_POINT 0x78

MeridianUarch MeridianAmdUarch(UINT32 Family, UINT32 Model)
{
    switch (Family) {
    case 0x0F:
        return MERIDIAN_UARCH_AMD_K8;
    case 0x10:
        return MERIDIAN_UARCH_AMD_K10;
    case 0x15:
        return (Model >= AMD_F15_EXCAVATOR_MODEL_MIN) ? MERIDIAN_UARCH_AMD_EXCAVATOR
                                                      : MERIDIAN_UARCH_AMD_BULLDOZER;
    case 0x17:
        if (Model == AMD_ZEN1_NAPLES || Model == AMD_ZEN1_RAVEN_RIDGE)
            return MERIDIAN_UARCH_AMD_ZEN;
        if (Model == AMD_ZEN_PLUS_PINNACLE_RIDGE || Model == AMD_ZEN_PLUS_PICASSO ||
            Model == AMD_ZEN_PLUS_DALI)
            return MERIDIAN_UARCH_AMD_ZEN_PLUS;
        return MERIDIAN_UARCH_AMD_ZEN2;
    case 0x19:
        if (Model == AMD_ZEN4_GENOA || Model == AMD_ZEN4_RAPHAEL || Model == AMD_ZEN4_PHOENIX ||
            Model == AMD_ZEN4_HAWK_POINT)
            return MERIDIAN_UARCH_AMD_ZEN4;
        return MERIDIAN_UARCH_AMD_ZEN3;
    case 0x1A:
        return MERIDIAN_UARCH_AMD_ZEN5;
    default:
        return MERIDIAN_UARCH_AMD_UNKNOWN;
    }
}
