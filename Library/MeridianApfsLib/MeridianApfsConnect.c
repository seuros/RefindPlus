// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2020-2026 Dayo Akanji
// SPDX-FileCopyrightText: 2019-2021 Acidanthera (OpenCore OcApfsLib, BSD-3-Clause)

#include "MeridianApfsInternal.h"
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include "MeridianApfsLib.h"
#include <Library/UefiBootManagerLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include "ApfsUnsupportedBds.h"
#include <Protocol/DevicePath.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleFileSystem.h>

extern BOOLEAN AppleFirmware;

extern CHAR16 * MrdStrFind (IN CHAR16 *String, IN CHAR16 *StrCharSet);

extern VOID MrdConsoleCaptureBegin(VOID);
extern VOID MrdConsoleCaptureEnd(VOID);

LIST_ENTRY               mApfsPrivateDataList = INITIALIZE_LIST_HEAD_VARIABLE (mApfsPrivateDataList);

static
EFI_STATUS ApfsRegisterPartition (
    IN  EFI_HANDLE               Handle,
    IN  EFI_BLOCK_IO_PROTOCOL   *BlockIo,
    IN  APFS_NX_SUPERBLOCK      *SuperBlock,
    OUT APFS_PRIVATE_DATA      **PrivateDataPointer
) {
    EFI_STATUS            Status;
    APFS_PRIVATE_DATA    *PrivateData;

    PrivateData = AllocateZeroPool (sizeof (*PrivateData));
    if (PrivateData == NULL) {
        return EFI_OUT_OF_RESOURCES;
    }

    PrivateData->Signature = APFS_PRIVATE_DATA_SIGNATURE;
    PrivateData->LocationInfo.ControllerHandle = Handle;
    CopyGuid (&PrivateData->LocationInfo.ContainerUuid, &SuperBlock->Uuid);

    PrivateData->BlockIo = BlockIo;
    PrivateData->ApfsBlockSize = SuperBlock->BlockSize;
    PrivateData->LbaMultiplier = PrivateData->ApfsBlockSize / PrivateData->BlockIo->Media->BlockSize;
    PrivateData->EfiJumpStart  = SuperBlock->EfiJumpStart;
    InternalApfsInitFusionData (SuperBlock, PrivateData);

    Status = gBS->InstallMultipleProtocolInterfaces(&PrivateData->LocationInfo.ControllerHandle, &gApfsEfiBootRecordInfoProtocolGuid, &PrivateData->LocationInfo, NULL);
    if (EFI_ERROR(Status)) {
        FreePool (PrivateData);
        return Status;
    }

    InsertTailList (&mApfsPrivateDataList, &PrivateData->Link);
    *PrivateDataPointer = PrivateData;

    return EFI_SUCCESS;
}

static
EFI_STATUS ApfsStartDriver (
    IN APFS_PRIVATE_DATA  *PrivateData,
    IN VOID               *DriverBuffer,
    IN UINTN               DriverSize
) {
    EFI_STATUS                  Status;
    EFI_HANDLE                  ImageHandle;
    EFI_DEVICE_PATH_PROTOCOL   *DevicePath;

    Status = gBS->HandleProtocol(PrivateData->LocationInfo.ControllerHandle, &gEfiDevicePathProtocolGuid, (VOID **) &DevicePath);

    if (EFI_ERROR(Status)) {
        DevicePath = NULL;
    }

    ImageHandle = NULL;
    Status = gBS->LoadImage(FALSE, gImageHandle, DevicePath, DriverBuffer, DriverSize, &ImageHandle);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    MrdConsoleCaptureBegin();

    Status = gBS->StartImage(ImageHandle, NULL, NULL);
    if (EFI_ERROR(Status)) {
        MrdConsoleCaptureEnd();
        gBS->UnloadImage (ImageHandle);

        return Status;
    }

    gBS->DisconnectController (
        PrivateData->LocationInfo.ControllerHandle,
        NULL, NULL
    );

    if (!AppleFirmware) {

        EfiBootManagerConnectAll();
    }
    else {

        gBS->ConnectController(PrivateData->LocationInfo.ControllerHandle, NULL, NULL, TRUE);
    }

    MrdConsoleCaptureEnd();

    return EFI_SUCCESS;
}

static
EFI_STATUS ApfsConnectDevice (
    IN EFI_HANDLE              Handle,
    IN EFI_BLOCK_IO_PROTOCOL  *BlockIo
) {
    EFI_STATUS            Status;
    APFS_NX_SUPERBLOCK   *SuperBlock;
    APFS_PRIVATE_DATA    *PrivateData;
    VOID                 *DriverBuffer;
    UINTN                 DriverSize;

    Status = InternalApfsReadSuperBlock (BlockIo, &SuperBlock);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Status = ApfsRegisterPartition (Handle, BlockIo, SuperBlock, &PrivateData);
    FreePool (SuperBlock);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    if (!PrivateData->CanLoadDriver) {
        return EFI_NOT_READY;
    }

    Status = InternalApfsReadDriver (PrivateData, &DriverSize, &DriverBuffer);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Status = ApfsStartDriver (PrivateData, DriverBuffer, DriverSize);
    FreePool (DriverBuffer);

    return Status;
}

EFI_STATUS MeridianApfsConnectHandle (
    IN EFI_HANDLE  Handle
) {
    EFI_STATUS              Status;
    VOID                   *TempProtocol;
    EFI_BLOCK_IO_PROTOCOL  *BlockIo;

    Status = gBS->HandleProtocol(Handle, &gEfiSimpleFileSystemProtocolGuid, &TempProtocol);
    if (!EFI_ERROR(Status)) {
        return (AppleFirmware) ? EFI_ALREADY_STARTED : EFI_NO_MAPPING;
    }

    Status = gBS->HandleProtocol(Handle, &gEfiBlockIoProtocolGuid, (VOID **) &BlockIo);
    if (EFI_ERROR(Status)) {
        return EFI_UNSUPPORTED;
    }

    if (BlockIo->Media == NULL || !BlockIo->Media->LogicalPartition) {
        return EFI_UNSUPPORTED;
    }

    if (BlockIo->Media->BlockSize == 0 ||
        (BlockIo->Media->BlockSize & (BlockIo->Media->BlockSize - 1)) != 0
    ) {
        return EFI_UNSUPPORTED;
    }

    Status = gBS->HandleProtocol(Handle, &gApfsUnsupportedBdsProtocolGuid, &TempProtocol);
    if (!EFI_ERROR(Status)) {
        return EFI_UNSUPPORTED;
    }

    Status = gBS->HandleProtocol(Handle, &gApfsEfiBootRecordInfoProtocolGuid, &TempProtocol);
    if (!EFI_ERROR(Status)) {
        return EFI_UNSUPPORTED;
    }

    return ApfsConnectDevice (Handle, BlockIo);
}
