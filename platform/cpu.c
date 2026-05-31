// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#include "cpu.h"
#include <Library/BaseLib.h>
#if defined(EFIX64)
#include <cpuid.h>
#endif

static MeridianCpuInfo gCache;
static BOOLEAN gCached = FALSE;

#if defined(EFIX64)
static VOID Populate(VOID)
{
    UINT32 Eax = 0, Ebx = 0, Ecx = 0, Edx = 0;
    UINT32 RawFamily, RawModel, ExtFamily, ExtModel;

    AsmCpuid(0, NULL, &Ebx, &Ecx, &Edx);
    *(UINT32 *)(gCache.Vendor + 0) = Ebx;
    *(UINT32 *)(gCache.Vendor + 4) = Edx;
    *(UINT32 *)(gCache.Vendor + 8) = Ecx;
    gCache.Vendor[12] = '\0';

    gCache.IsIntel =
        (Ebx == signature_INTEL_ebx && Edx == signature_INTEL_edx && Ecx == signature_INTEL_ecx);
    gCache.IsAmd =
        (Ebx == signature_AMD_ebx && Edx == signature_AMD_edx && Ecx == signature_AMD_ecx);

    AsmCpuid(1, &Eax, NULL, &Ecx, NULL);
    gCache.Stepping = Eax & 0xF;
    RawFamily = (Eax >> 8) & 0xF;
    RawModel = (Eax >> 4) & 0xF;
    ExtFamily = (Eax >> 20) & 0xFF;
    ExtModel = (Eax >> 16) & 0xF;

    gCache.Family = (RawFamily == 0xF) ? RawFamily + ExtFamily : RawFamily;

    gCache.Model = ((RawFamily == 0xF) || (gCache.IsIntel && RawFamily == 6))
                       ? (ExtModel << 4) | RawModel
                       : RawModel;

    if (gCache.IsIntel)
        gCache.Uarch = MeridianIntelUarch(gCache.Family, gCache.Model, gCache.Stepping);
    else if (gCache.IsAmd)
        gCache.Uarch = MeridianAmdUarch(gCache.Family, gCache.Model);
    else
        gCache.Uarch = MERIDIAN_UARCH_UNKNOWN;

    gCache.HasThermalMsrs =
        gCache.IsIntel && gCache.Family == 6 && gCache.Model >= 0x1A;

    gCache.IsHaswell = (gCache.Uarch == MERIDIAN_UARCH_INTEL_HASWELL);
    gCache.HasRdrand = (Ecx & BIT30) != 0;

    gCached = TRUE;
}
#else
static VOID Populate(VOID)
{
    AsciiStrCpyS(gCache.Vendor, sizeof(gCache.Vendor), "AArch64");
    gCache.Uarch = MERIDIAN_UARCH_UNKNOWN;
    gCached = TRUE;
}
#endif

const MeridianCpuInfo *MeridianGetCpuInfo(VOID)
{
    if (!gCached)
        Populate();
    return &gCache;
}

const CHAR8 *MeridianUarchName(MeridianUarch Uarch)
{
    switch (Uarch) {
    case MERIDIAN_UARCH_INTEL_CORE2:
        return "Intel Core2";
    case MERIDIAN_UARCH_INTEL_NEHALEM:
        return "Intel Nehalem";
    case MERIDIAN_UARCH_INTEL_WESTMERE:
        return "Intel Westmere";
    case MERIDIAN_UARCH_INTEL_SANDY_BRIDGE:
        return "Intel Sandy Bridge";
    case MERIDIAN_UARCH_INTEL_IVY_BRIDGE:
        return "Intel Ivy Bridge";
    case MERIDIAN_UARCH_INTEL_HASWELL:
        return "Intel Haswell";
    case MERIDIAN_UARCH_INTEL_BROADWELL:
        return "Intel Broadwell";
    case MERIDIAN_UARCH_INTEL_SKYLAKE:
        return "Intel Skylake";
    case MERIDIAN_UARCH_INTEL_KABY_LAKE:
        return "Intel Kaby Lake";
    case MERIDIAN_UARCH_INTEL_COFFEE_LAKE:
        return "Intel Coffee Lake";
    case MERIDIAN_UARCH_INTEL_CANNON_LAKE:
        return "Intel Cannon Lake";
    case MERIDIAN_UARCH_INTEL_ICE_LAKE:
        return "Intel Ice Lake";
    case MERIDIAN_UARCH_INTEL_COMET_LAKE:
        return "Intel Comet Lake";
    case MERIDIAN_UARCH_INTEL_ROCKET_LAKE:
        return "Intel Rocket Lake";
    case MERIDIAN_UARCH_INTEL_TIGER_LAKE:
        return "Intel Tiger Lake";
    case MERIDIAN_UARCH_INTEL_ALDER_LAKE:
        return "Intel Alder Lake";
    case MERIDIAN_UARCH_INTEL_RAPTOR_LAKE:
        return "Intel Raptor Lake";
    case MERIDIAN_UARCH_INTEL_METEOR_LAKE:
        return "Intel Meteor Lake";
    case MERIDIAN_UARCH_INTEL_UNKNOWN:
        return "Intel (unknown uarch)";
    case MERIDIAN_UARCH_AMD_K8:
        return "AMD K8";
    case MERIDIAN_UARCH_AMD_K10:
        return "AMD K10";
    case MERIDIAN_UARCH_AMD_BULLDOZER:
        return "AMD Bulldozer";
    case MERIDIAN_UARCH_AMD_EXCAVATOR:
        return "AMD Excavator";
    case MERIDIAN_UARCH_AMD_ZEN:
        return "AMD Zen";
    case MERIDIAN_UARCH_AMD_ZEN_PLUS:
        return "AMD Zen+";
    case MERIDIAN_UARCH_AMD_ZEN2:
        return "AMD Zen 2";
    case MERIDIAN_UARCH_AMD_ZEN3:
        return "AMD Zen 3";
    case MERIDIAN_UARCH_AMD_ZEN4:
        return "AMD Zen 4";
    case MERIDIAN_UARCH_AMD_ZEN5:
        return "AMD Zen 5";
    case MERIDIAN_UARCH_AMD_UNKNOWN:
        return "AMD (unknown uarch)";
    default:
        return "Unknown";
    }
}
