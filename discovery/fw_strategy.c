// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#include "global.h"
#include "sysinfo.h"
#include "fw_strategy.h"

CONST FW_STRATEGY *gFwStrategy = &gFwStrategyUefi2;

static BOOLEAN RuntimeServicesHaveQvi(VOID)
{
    UINTN NeedSize;

    NeedSize =
        MRD_OFFSET_OF(EFI_RUNTIME_SERVICES, QueryVariableInfo) + sizeof(gRT->QueryVariableInfo);

    return (gRT->Hdr.HeaderSize >= NeedSize);
}

VOID SelectFwStrategy(VOID)
{
    UINT16 MajST;
    UINT16 MajBS;
    UINT16 MajRT;

    if (MeridianFirmwareVendor == FW_VENDOR_APPLE) {
        gFwStrategy = RuntimeServicesHaveQvi() ? &gFwStrategyApple : &gFwStrategyAppleLegacy;
        return;
    }

    MajST = (UINT16)(gST->Hdr.Revision >> 16U);
    MajBS = (UINT16)(gBS->Hdr.Revision >> 16U);
    MajRT = (UINT16)(gRT->Hdr.Revision >> 16U);

    if (MajST < 2 || MajBS < 2 || MajRT < 2) {
        gFwStrategy = &gFwStrategyUefi1;
        return;
    }

    gFwStrategy = &gFwStrategyUefi2;
}
