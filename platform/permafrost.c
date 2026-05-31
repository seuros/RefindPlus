// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#include "cpu.h"
#include "global.h"
#include "lib.h"
#include "sysinfo.h"
#include "meridian_funcs.h"
#include "apple/smc.h"
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Protocol/MpService.h>

#if MERIDIAN_DEBUG > 0

#define PF_MSR_POWER_CTL 0x1FC
#define PF_BD_PROCHOT 0x01ULL
#define PF_MSR_THERM_STATUS 0x19C
#define PF_THERM_STATUS_NOW 0x01ULL

#define PF_SENT_LCD 0x82
#define PF_SENT_HDD 0x81
#define PF_SENT_OTHR 0x80

#ifndef PF_ALLOW_MSR
#define PF_ALLOW_MSR 0
#endif

#ifndef PF_TEST_FORCE
#define PF_TEST_FORCE 0
#endif
#if PF_TEST_FORCE
#if !defined(PF_TEST_FORCE_ACK) || (PF_TEST_FORCE_ACK != 0x50464C41)
#error "PF_TEST_FORCE is LAB ONLY -- set PF_TEST_FORCE_ACK=0x50464C41 to confirm"
#endif
#endif

typedef enum
{
    PF_KEY_OK = 0,
    PF_KEY_FAIL
} PF_KEY_STATE;

static PF_KEY_STATE PfReadKey(CHAR8 a, CHAR8 b, CHAR8 c, CHAR8 d, UINT8 *Buf, UINT8 Len)
{
    CHAR8 Key[4] = {a, b, c, d};
    SetMem(Buf, Len, 0);
    return MrdAppleSmcReadKey(Key, Buf, Len) ? PF_KEY_OK : PF_KEY_FAIL;
}

static BOOLEAN PfIsGenuineIntel(VOID) { return MeridianGetCpuInfo()->IsIntel; }
static BOOLEAN PfCpuSupportsThermalMsrs(VOID) { return MeridianGetCpuInfo()->HasThermalMsrs; }

#if PF_ALLOW_MSR

static VOID EFIAPI PfApClearBdProchot(IN OUT VOID *Buffer)
{
    UINT64 Msr = AsmReadMsr64(PF_MSR_POWER_CTL);
    (VOID) Buffer;
    if (Msr & PF_BD_PROCHOT) {
        AsmWriteMsr64(PF_MSR_POWER_CTL, Msr & ~PF_BD_PROCHOT);
    }
}
#endif

VOID PermafrostTame(VOID)
{
    EFI_STATUS Status;
    APPLE_SMC_BACKEND SmcBe;
    UINT8 Spht[2], Msts[1], Msal[1], One[1], Lcd[2];
    PF_KEY_STATE SphtSt, MstsSt, MsalSt, BatpSt, BbadSt;
    BOOLEAN SmcProchot, CpuProchot, Headless, BattAbsent, BattBad, Intel;
    const CHAR8 *Cause;
    const CHAR8 *LcdKeys[4] = {"TL0P", "TL0p", "TL1P", "TL1p"};
    const CHAR8 *HddKeys[4] = {"TH0P", "TH0F", "TH1P", "TH0R"};
    UINTN i, Off, Cap, Len;
    BOOLEAN Truncated;
    CHAR8 Line[224];
    CHAR8 *Buf;
    UINT64 PowerCtl, ThermStatus;
    BOOLEAN HavePowerCtl, ThermNow;

    if (MeridianFirmwareVendor != FW_VENDOR_APPLE) {
        return;
    }

    Cap = 6144;
    Off = 0;
    Truncated = FALSE;
    Buf = AllocateZeroPool(Cap);

#define PF_EMIT()                                                                                  \
    do {                                                                                           \
        Print(L"%a", Line);                                                                        \
        Len = AsciiStrLen(Line);                                                                   \
        if (Buf != NULL && Off + Len + 1 < Cap) {                                                  \
            AsciiStrCpyS(Buf + Off, Cap - Off, Line);                                              \
            Off += Len;                                                                            \
        }                                                                                          \
        else {                                                                                     \
            Truncated = TRUE;                                                                      \
        }                                                                                          \
    } while (0)

    SmcBe = MrdAppleSmcBackend();

    AsciiSPrint(Line, sizeof(Line), "[PF] Permafrost -- SMC thermal recon%a%a  backend=%a\n",
                PF_ALLOW_MSR ? " [MSR WRITE ARMED]" : " [READ-ONLY]",
                PF_TEST_FORCE ? " [LAB TEST-FORCE ACTIVE]" : "",
                SmcBe == APPLE_SMC_PROTOCOL ? "apple-smc-protocol"
                : SmcBe == APPLE_SMC_PORT   ? "port-0x300"
                : SmcBe == APPLE_SMC_MMIO   ? "mmio-t2"
                                            : "NONE");
    PF_EMIT();

    if (SmcBe == APPLE_SMC_NONE) {
        AsciiSPrint(Line, sizeof(Line),
                    "[PF] no SMC backend (T2/MMIO without protocol, or non-Apple SMC) "
                    "-- aborting\n");
        PF_EMIT();
        goto done;
    }

    if (SmcBe == APPLE_SMC_PROTOCOL) {
        UINT8 Nk[4];
        if (PfReadKey('#', 'K', 'E', 'Y', Nk, 4) == PF_KEY_OK) {
            AsciiSPrint(Line, sizeof(Line),
                        "[PF] protocol #KEY raw=%02x%02x%02x%02x (expect big-endian "
                        "count; verifies byte order)\n",
                        Nk[0], Nk[1], Nk[2], Nk[3]);
            PF_EMIT();
        }
    }

    SphtSt = PfReadKey('S', 'P', 'H', 'T', Spht, 2);
    MstsSt = PfReadKey('M', 'S', 'T', 'S', Msts, 1);
    MsalSt = PfReadKey('M', 'S', 'A', 'L', Msal, 1);

    SmcProchot = (SphtSt == PF_KEY_OK && Spht[0] != 0);
    CpuProchot = (SphtSt == PF_KEY_OK && Spht[1] != 0);

    if (SphtSt == PF_KEY_OK) {
        AsciiSPrint(Line, sizeof(Line), "[PF] SPHT=%02x%02x (smc-prochot=%a cpu-prochot=%a)\n",
                    Spht[0], Spht[1], SmcProchot ? "YES" : "no", CpuProchot ? "YES" : "no");
    }
    else {
        AsciiSPrint(Line, sizeof(Line), "[PF] SPHT=n/a (read failed)\n");
    }
    PF_EMIT();

    if (MstsSt == PF_KEY_OK && MsalSt == PF_KEY_OK) {
        AsciiSPrint(Line, sizeof(Line), "[PF] MSTS=%02x MSAL=%02x (logged only)\n", Msts[0],
                    Msal[0]);
    }
    else {
        AsciiSPrint(Line, sizeof(Line), "[PF] MSTS=%a MSAL=%a (logged only)\n",
                    MstsSt == PF_KEY_OK ? "ok" : "n/a", MsalSt == PF_KEY_OK ? "ok" : "n/a");
    }
    PF_EMIT();

    Headless = FALSE;
    for (i = 0; i < 4; i++) {
        PF_KEY_STATE St =
            PfReadKey(LcdKeys[i][0], LcdKeys[i][1], LcdKeys[i][2], LcdKeys[i][3], Lcd, 2);
        if (St != PF_KEY_OK) {
            AsciiSPrint(Line, sizeof(Line), "[PF] %a=n/a\n", LcdKeys[i]);
        }
        else {
            BOOLEAN Dead = ((Lcd[0] == PF_SENT_LCD || Lcd[0] == PF_SENT_OTHR) && Lcd[1] == 0x00);
            if (Dead)
                Headless = TRUE;
            AsciiSPrint(Line, sizeof(Line), "[PF] %a=%02x%02x %a\n", LcdKeys[i], Lcd[0], Lcd[1],
                        Dead ? "DISCONNECTED" : "live");
        }
        PF_EMIT();
    }

    for (i = 0; i < 4; i++) {
        PF_KEY_STATE St =
            PfReadKey(HddKeys[i][0], HddKeys[i][1], HddKeys[i][2], HddKeys[i][3], Lcd, 2);
        if (St != PF_KEY_OK) {
            AsciiSPrint(Line, sizeof(Line), "[PF] %a=n/a\n", HddKeys[i]);
        }
        else {
            BOOLEAN Dead = (Lcd[0] == PF_SENT_HDD && Lcd[1] == 0x00);
            AsciiSPrint(Line, sizeof(Line), "[PF] %a=%02x%02x %a\n", HddKeys[i], Lcd[0], Lcd[1],
                        Dead ? "DEAD (fan-max; OS-governor only)" : "live");
        }
        PF_EMIT();
    }

    BatpSt = PfReadKey('B', 'A', 'T', 'P', One, 1);
    BattAbsent = (BatpSt == PF_KEY_OK && One[0] == 0);
    BbadSt = PfReadKey('B', 'B', 'A', 'D', One, 1);
    BattBad = (BbadSt == PF_KEY_OK && One[0] == 1);
    AsciiSPrint(Line, sizeof(Line), "[PF] BATP=%a BBAD=%a\n",
                BatpSt != PF_KEY_OK ? "n/a" : (BattAbsent ? "absent" : "present"),
                BbadSt != PF_KEY_OK ? "n/a" : (BattBad ? "bad" : "ok"));
    PF_EMIT();

    Intel = PfCpuSupportsThermalMsrs();
    HavePowerCtl = FALSE;
    ThermNow = FALSE;
    PowerCtl = 0;
    ThermStatus = 0;
    if (Intel) {
        PowerCtl = AsmReadMsr64(PF_MSR_POWER_CTL);
        HavePowerCtl = TRUE;
        ThermStatus = AsmReadMsr64(PF_MSR_THERM_STATUS);
        ThermNow = (ThermStatus & PF_THERM_STATUS_NOW) != 0;
        AsciiSPrint(Line, sizeof(Line),
                    "[PF] MSR 0x1FC=%016lx BD_PROCHOT=%a  IA32_THERM_STATUS=%016lx "
                    "tcc-now=%a\n",
                    PowerCtl, (PowerCtl & PF_BD_PROCHOT) ? "set" : "clear", ThermStatus,
                    ThermNow ? "YES" : "no");
    }
    else {
        AsciiSPrint(Line, sizeof(Line),
                    "[PF] CPU lacks supported IA32_POWER_CTL/THERM_STATUS "
                    "(non-Intel or pre-Nehalem) -- skipping all MSR access\n");
    }
    PF_EMIT();

    Cause = NULL;
    if (Headless)
        Cause = "headless (LCD sentinel)";
    else if (BattAbsent)
        Cause = "battery absent";
    else if (BattBad)
        Cause = "battery bad";

#if PF_TEST_FORCE
    if (Cause == NULL) {
        Cause = "TEST-FORCE (lab; no natural trigger)";
    }
    AsciiSPrint(Line, sizeof(Line),
                "[PF] *** PF_TEST_FORCE: bypassing PROCHOT/cause gate (lab) ***\n");
    PF_EMIT();
#else
    if (!SmcProchot) {
        AsciiSPrint(Line, sizeof(Line), "[PF] SMC not asserting PROCHOT -- nothing to do%a\n",
                    (CpuProchot || (MsalSt == PF_KEY_OK && (Msal[0] & 0x40)))
                        ? " (note: a sticky CPU/MSAL bit is set but SMC pin is clear)"
                        : "");
        PF_EMIT();
        goto done;
    }

    if (Cause == NULL) {
        AsciiSPrint(Line, sizeof(Line),
                    "[PF] PROCHOT asserted but NO benign cause -- NOT overriding "
                    "(possible real thermal event)\n");
        PF_EMIT();
        goto done;
    }
#endif
    if (!Intel) {
        AsciiSPrint(Line, sizeof(Line),
                    "[PF] cause=%a but CPU lacks supported MSRs -- cannot use lever\n", Cause);
        PF_EMIT();
        goto done;
    }
    if (ThermNow) {
        AsciiSPrint(Line, sizeof(Line),
                    "[PF] cause=%a BUT IA32_THERM_STATUS shows active TCC -- REFUSING "
                    "to mask PROCHOT (real heat)\n",
                    Cause);
        PF_EMIT();
        goto done;
    }
    if (HavePowerCtl && (PowerCtl & PF_BD_PROCHOT) == 0) {
        AsciiSPrint(Line, sizeof(Line),
                    "[PF] cause=%a, BD_PROCHOT already clear -- nothing to do\n", Cause);
        PF_EMIT();
        goto done;
    }

#if PF_ALLOW_MSR
    {
        EFI_MP_SERVICES_PROTOCOL *Mp = NULL;
        EFI_GUID MpGuid = EFI_MP_SERVICES_PROTOCOL_GUID;
        UINTN NumCpu = 1, NumEnabled = 1, *Failed = NULL;
        const CHAR8 *Coverage;
        EFI_STATUS ApStatus;

        PfApClearBdProchot(NULL);
        Coverage = "bsp-only";

        Status = gBS->LocateProtocol(&MpGuid, NULL, (VOID **)&Mp);
        if (!EFI_ERROR(Status) && Mp != NULL &&
            !EFI_ERROR(Mp->GetNumberOfProcessors(Mp, &NumCpu, &NumEnabled))) {
            if (NumEnabled > 1) {
                ApStatus =
                    Mp->StartupAllAPs(Mp, PfApClearBdProchot, TRUE, NULL, 1000000, NULL, &Failed);
                Coverage = EFI_ERROR(ApStatus) ? "DEGRADED (AP failures)" : "all-cpus";
                if (Failed != NULL) {
                    MRD_FREE_POOL(Failed);
                }
            }
            else {
                Coverage = "single-cpu";
            }
        }

        PowerCtl = AsmReadMsr64(PF_MSR_POWER_CTL);
        AsciiSPrint(Line, sizeof(Line),
                    "[PF] *** cleared BD_PROCHOT (cause=%a, cpus=%lu/%lu %a) -> "
                    "MSR 0x1FC=%016lx (%a) ***\n",
                    Cause, (UINT64)NumEnabled, (UINT64)NumCpu, Coverage, PowerCtl,
                    (PowerCtl & PF_BD_PROCHOT) ? "FAILED, still set" : "throttle masked");
        PF_EMIT();
    }
#else
    AsciiSPrint(Line, sizeof(Line),
                "[PF] WOULD clear BD_PROCHOT (cause=%a) -- build -DPF_ALLOW_MSR=1 to "
                "apply\n",
                Cause);
    PF_EMIT();
#endif

done:
    if (Buf != NULL && Off > 0) {
        Status = MrdSaveFile(NULL, L"meridian-permafrost.txt", (UINT8 *)Buf, Off);
        Print(L"[PF] ---- %a \\meridian-permafrost.txt (%lu bytes)%a ----\n",
              EFI_ERROR(Status) ? "FAILED to write" : "wrote", (UINT64)Off,
              Truncated ? " [TRUNCATED]" : "");
    }
    else {
        Print(L"[PF] ---- nothing to write ----\n");
    }
    MRD_FREE_POOL(Buf);
    gBS->Stall(3000000);

#undef PF_EMIT
}

#else
VOID PermafrostTame(VOID) {}
#endif
