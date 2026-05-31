// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#include "dp.h"
#include "ssdt_proof.h"

#include <Library/UefiLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>

#ifndef DP_ALLOW_REENABLE
#define DP_ALLOW_REENABLE 0
#endif

#ifndef DP_ACPI_SELFTEST
#define DP_ACPI_SELFTEST 0
#endif

#define DP_REPORT_CAP 8192

#define DP_REPORT_FILE L"darkpassenger-app.txt"

const DARK_PASSENGER *const gDpRegistry[] = {
    &gDpLpssLynxPoint,
};
const UINTN gDpRegistryCount = ARRAY_SIZE(gDpRegistry);

EFI_STATUS
EFIAPI
UefiMain(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable)
{
    DP_CTX Ctx;
    UINTN i, Applied;

    (VOID) ImageHandle;
    (VOID) SystemTable;

    ZeroMem(&Ctx, sizeof(Ctx));
    Ctx.Cap = DP_REPORT_CAP;
    Ctx.Buf = AllocateZeroPool(Ctx.Cap);
    Ctx.AllowActivate = (DP_ALLOW_REENABLE != 0);

    DpEmit(&Ctx, "[DP] The Dark Passenger -- vendor-lockout recon (%a)\n",
           Ctx.AllowActivate ? "ARMED: writes enabled" : "READ-ONLY");

    DpDumpAcpi(&Ctx);

#if DP_ACPI_SELFTEST
    if (Ctx.AllowActivate) {
        DpEmit(&Ctx, "[DP] ACPI self-test: injecting inert DPT0 SSDT\n");
        DpInstallAcpiTable(&Ctx, gSsdtProof, sizeof(gSsdtProof));
    }
#endif

    Applied = 0;
    for (i = 0; i < gDpRegistryCount; i++) {
        const DARK_PASSENGER *Dp = gDpRegistry[i];

        Ctx.Lpc = NULL;
        Ctx.RcbaBase = 0;

        if (Dp->Applies == NULL || !Dp->Applies(&Ctx)) {
            continue;
        }
        Applied++;
        DpEmit(&Ctx, "[DP] == passenger '%a' applies: %a ==\n", Dp->Name, Dp->Desc);

        if (Dp->Recon != NULL) {
            Dp->Recon(&Ctx);
        }
        if (Ctx.AllowActivate && Dp->Activate != NULL) {
            Dp->Activate(&Ctx);
        }

        if (Ctx.AllowActivate && Dp->Describe != NULL) {
            Dp->Describe(&Ctx);
        }
    }

    if (Applied == 0) {
        DpEmit(&Ctx, "[DP] no applicable passengers on this machine\n");
    }

    if (Ctx.Buf != NULL && Ctx.Off > 0) {
        EFI_STATUS Status = DpSaveFile(DP_REPORT_FILE, (UINT8 *)Ctx.Buf, Ctx.Off);
        Print(L"[DP] ---- report (%d bytes): %s ----\n", Ctx.Off,
              EFI_ERROR(Status) ? L"console only (no writable FS)" : DP_REPORT_FILE);
    }

    FreePool(Ctx.Buf);
    return EFI_SUCCESS;
}
