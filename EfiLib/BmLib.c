// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2020-2026 Dayo Akanji
// SPDX-FileCopyrightText: Intel Corporation

#include "Platform.h"
#include <Library/FileHandleLib.h>

#include "meridian_funcs.h"

EFI_STATUS EfiLibLocateProtocol (
    IN  EFI_GUID  *ProtocolGuid,
    OUT VOID     **Interface
) {
    EFI_STATUS  Status;

    Status = gBS->LocateProtocol(ProtocolGuid, NULL, (VOID **) Interface);

    return Status;
}

EFI_FILE_HANDLE EfiLibOpenRoot (
    IN EFI_HANDLE DeviceHandle
) {
    EFI_STATUS                       Status;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *Volume;
    EFI_FILE_HANDLE                  File;

    Status = gBS->HandleProtocol(DeviceHandle, &gEfiSimpleFileSystemProtocolGuid, (VOID **) &Volume);
    if (EFI_ERROR(Status)) {
        return NULL;
    }

    File = NULL;
    Status = Volume->OpenVolume(Volume, &File);
    if (EFI_ERROR(Status)) {
        #if MERIDIAN_DEBUG > 0
        CheckError (
            Status,
            L"in Meridian:- 'File Handle to Root Directory'"
        );
        #endif

        return NULL;
    }

    return File;
}

CHAR16 * EfiStrDuplicate (
    IN CHAR16   *Src
) {
    if (Src == NULL) {
        return NULL;
    }

    return AllocateCopyPool (StrSize (Src), Src);
}

EFI_FILE_INFO * EfiLibFileInfo (
    IN EFI_FILE_HANDLE      FHand
) {
    return FileHandleGetInfo (FHand);
}

EFI_FILE_SYSTEM_INFO * EfiLibFileSystemInfo (
    IN EFI_FILE_HANDLE      FHand
) {
    EFI_STATUS            Status;
    EFI_FILE_SYSTEM_INFO *FileSystemInfo = NULL;
    UINTN                 Size = 0;

    Status = FHand->GetInfo (
        FHand, &gEfiFileSystemInfoGuid,
        &Size, FileSystemInfo
    );
    if (Status == EFI_BUFFER_TOO_SMALL) {
        FileSystemInfo = AllocateZeroPool (Size);
        Status = FHand->GetInfo (
            FHand, &gEfiFileSystemInfoGuid,
            &Size, FileSystemInfo
        );
    }

    return EFI_ERROR(Status) ? NULL : FileSystemInfo;
}

VOID * EfiReallocatePool (
    IN VOID  *OldPool,
    IN UINTN  OldSize,
    IN UINTN  NewSize
) {
    VOID  *NewPool;

    NewPool = NULL;
    if (NewSize != 0) {
        NewPool = AllocateZeroPool (NewSize);
    }

    if (OldPool != NULL) {
        if (NewPool != NULL) {
            gBS->CopyMem(NewPool, OldPool, OldSize < NewSize ? OldSize : NewSize);
        }

        MRD_FREE_POOL(OldPool);
    }

    return NewPool;
}
