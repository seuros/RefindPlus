// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2020-2026 Dayo Akanji
// SPDX-FileCopyrightText: 2019-2021 Acidanthera (OpenCore OcApfsLib, BSD-3-Clause)

#include "MeridianApfsInternal.h"
#include "MeridianApfsLib.h"
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Protocol/BlockIo.h>
#include <Protocol/PartitionInfo.h>

EFI_STATUS MeridianApfsConnectParentDevice (VOID) {
    EFI_STATUS        Status;
    EFI_STATUS        XStatus;
    UINTN             HandleCount;
    EFI_HANDLE       *HandleBuffer;
    UINTN             Index;

    HandleCount = 0;
    Status = gBS->LocateHandleBuffer(ByProtocol, &gEfiBlockIoProtocolGuid, NULL, &HandleCount, &HandleBuffer);
    if (EFI_ERROR(Status)) {
        return EFI_LOAD_ERROR;
    }

    Status = EFI_NOT_FOUND;
    for (Index = 0; Index < HandleCount; ++Index) {
        XStatus = MeridianApfsConnectHandle (HandleBuffer[Index]);
        if (XStatus == EFI_SUCCESS      ||
            XStatus == EFI_NO_MAPPING   ||
            XStatus == EFI_ALREADY_STARTED
        ) {
            if (EFI_ERROR(Status)) {
                Status = XStatus;
            }
        }
    }
    FreePool (HandleBuffer);

    return Status;
}

EFI_STATUS MeridianApfsConnectDevices (VOID) {
    EFI_STATUS   Status;
    VOID        *PartitionInfoInterface;

    gBS->LocateProtocol(&gEfiPartitionInfoProtocolGuid, NULL, &PartitionInfoInterface);
    Status = MeridianApfsConnectParentDevice();

    return Status;
}
