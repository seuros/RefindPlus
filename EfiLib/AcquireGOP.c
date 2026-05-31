// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2020-2024 Dayo Akanji
// SPDX-FileCopyrightText: 2021 Joe van Tunen
// SPDX-FileCopyrightText: Intel Corporation

#include "Platform.h"
#include "lib.h"
#include <Library/HandleParsingLib.h>

static
EFI_STATUS ReloadOptionROM (
    IN       VOID    *RomBar,
    IN       UINT64   RomSize,
    IN const CHAR16  *FileName
) {
    VOID                          *ImageBuffer;
    VOID                          *DecompressedImageBuffer;
    UINTN                          ImageIndex;
    UINTN                          RomBarOffset;
    UINT8                         *Scratch;
    UINT16                         ImageOffset;
    UINT32                         ImageSize;
    UINT32                         ScratchSize;
    UINT32                         ImageLength;
    UINT32                         DestinationSize;
    UINT32                         InitializationSize;
    CHAR16                        *RomFileName;
    BOOLEAN                        LoadROM;
    EFI_STATUS                     Status;
    EFI_STATUS                     ReturnStatus;
    EFI_HANDLE                     ImageHandle;
    PCI_DATA_STRUCTURE            *Pcir;
    EFI_DECOMPRESS_PROTOCOL       *Decompress;
    EFI_DEVICE_PATH_PROTOCOL      *FilePath;
    EFI_PCI_EXPANSION_ROM_HEADER  *EfiRomHeader;

    ImageIndex   = 0;
    ReturnStatus = Status = EFI_NOT_FOUND;
    RomBarOffset = (UINTN) RomBar;

    do {
        LoadROM      = FALSE;
        EfiRomHeader = (EFI_PCI_EXPANSION_ROM_HEADER *) (UINTN) RomBarOffset;

        if (EfiRomHeader->Signature != PCI_EXPANSION_ROM_HEADER_SIGNATURE) {
            return EFI_VOLUME_CORRUPTED;
        }

        if ((EfiRomHeader->PcirOffset == 0)     ||
            (EfiRomHeader->PcirOffset & 3) != 0 ||
            (
                RomBarOffset                  -
                (UINTN) RomBar                +
                EfiRomHeader->PcirOffset      +
                sizeof (PCI_DATA_STRUCTURE)
            ) > RomSize
        ) {
            break;
        }

        Pcir = (PCI_DATA_STRUCTURE *) (UINTN) (RomBarOffset + EfiRomHeader->PcirOffset);

        if (Pcir->Signature != PCI_DATA_STRUCTURE_SIGNATURE) {
            break;
        }

        ImageSize = Pcir->ImageLength * 512;

        if ((RomBarOffset - (UINTN) RomBar + ImageSize) > RomSize) {
            break;
        }

        if (Pcir->CodeType == PCI_CODE_TYPE_EFI_IMAGE &&
            EfiRomHeader->EfiSignature  == EFI_PCI_EXPANSION_ROM_HEADER_EFISIGNATURE &&
            (
                EfiRomHeader->EfiSubsystem == EFI_IMAGE_SUBSYSTEM_EFI_BOOT_SERVICE_DRIVER ||
                EfiRomHeader->EfiSubsystem == EFI_IMAGE_SUBSYSTEM_EFI_RUNTIME_DRIVER
            )
        ) {
            ImageOffset        = EfiRomHeader->EfiImageHeaderOffset;
            InitializationSize = EfiRomHeader->InitializationSize * 512;

            if (InitializationSize <= ImageSize && ImageOffset < InitializationSize) {
                ImageBuffer             = (VOID *) (UINTN) (RomBarOffset + ImageOffset);
                ImageLength             = InitializationSize - ImageOffset;
                DecompressedImageBuffer = NULL;
                DestinationSize         = 0;

                if (EfiRomHeader->CompressionType != EFI_PCI_EXPANSION_ROM_HEADER_COMPRESSED) {

                    Status = EFI_SUCCESS;
                }
                else {

                    Status = gBS->LocateProtocol(&gEfiDecompressProtocolGuid, NULL, (VOID **) &Decompress);
                    if (!EFI_ERROR(Status)) {
                        Status = Decompress->GetInfo(Decompress, ImageBuffer, ImageLength, &DestinationSize, &ScratchSize);
                        if (!EFI_ERROR(Status)) {
                            DecompressedImageBuffer = AllocateZeroPool (DestinationSize);
                            if (DecompressedImageBuffer == NULL) {
                                MRD_FREE_POOL(ImageBuffer);

                                return EFI_OUT_OF_RESOURCES;
                            }

                            if (ImageBuffer != NULL) {
                                Scratch = AllocateZeroPool (ScratchSize);
                                if (Scratch == NULL) {
                                    MRD_FREE_POOL(ImageBuffer);
                                    MRD_FREE_POOL(DecompressedImageBuffer);

                                    return EFI_OUT_OF_RESOURCES;
                                }

                                Status = Decompress->Decompress(Decompress, ImageBuffer, ImageLength, DecompressedImageBuffer, DestinationSize, Scratch, ScratchSize);
                                if (!EFI_ERROR(Status)) {
                                    LoadROM = TRUE;
                                }

                                MRD_FREE_POOL(Scratch);
                            }
                        }
                    }
                }

                if (LoadROM) {
                    MRD_FREE_POOL(ImageBuffer);
                    ImageBuffer = DecompressedImageBuffer;
                    ImageLength = DestinationSize;
                }

                if (!EFI_ERROR(Status)) {
                    RomFileName = PoolPrint (L"%s[%d]", FileName, ImageIndex);
                    FilePath = FileDevicePath(NULL, RomFileName);
                    Status = gBS->LoadImage(TRUE, gImageHandle, FilePath, ImageBuffer, ImageLength, &ImageHandle);
                    if (EFI_ERROR(Status)) {
                        if (Status == EFI_SECURITY_VIOLATION) {
                            gBS->UnloadImage(ImageHandle);
                        }
                    }
                    else {
                        Status = gBS->StartImage(ImageHandle, NULL, NULL);
                    }

                     MRD_FREE_POOL(RomFileName);
                }

                MRD_FREE_POOL(ImageBuffer);
            }
        }

        RomBarOffset = RomBarOffset + ImageSize;
        ImageIndex++;

        if (EFI_ERROR(ReturnStatus)) {
            ReturnStatus = Status;
        }
    } while (
        (Pcir->Indicator & 0x80) == 0x00 &&
        (RomBarOffset - (UINTN) RomBar) < RomSize
    );

    return ReturnStatus;
}

EFI_STATUS ReissueGOP (VOID) {
    UINTN                 Index;
    UINTN                 HandleIndex;
    UINTN                 HandleArrayCount;
    UINTN                 BindingHandleCount;
    CHAR16               *RomFileName;
    EFI_HANDLE           *HandleArray;
    EFI_HANDLE           *BindingHandleBuffer;
    EFI_STATUS            ReturnStatus;
    EFI_STATUS            Status;
    EFI_PCI_IO_PROTOCOL  *PciIo;

    HandleArrayCount = 0;
    HandleArray = NULL;
    Status = gBS->LocateHandleBuffer(ByProtocol, &gEfiPciIoProtocolGuid, NULL, &HandleArrayCount, &HandleArray);
    if (EFI_ERROR(Status)) {

        return EFI_PROTOCOL_ERROR;
    }

    ReturnStatus = EFI_LOAD_ERROR;
    for (Index = 0; Index < HandleArrayCount; Index++) {
        Status = gBS->HandleProtocol(HandleArray[Index], &gEfiPciIoProtocolGuid, (void **) &PciIo);
        if (EFI_ERROR(Status)) {
            if (EFI_ERROR(ReturnStatus)) {
                ReturnStatus = Status;
            }

            continue;
        }

        if (PciIo->RomImage == NULL || PciIo->RomSize == 0) {
            if (EFI_ERROR(ReturnStatus)) {
                ReturnStatus = EFI_NOT_FOUND;
            }

            continue;
        }

        BindingHandleCount = 0;
        BindingHandleBuffer = NULL;
        PARSE_HANDLE_DATABASE_UEFI_DRIVERS(HandleArray[Index], &BindingHandleCount, &BindingHandleBuffer);
        if (BindingHandleCount != 0) {
            MRD_FREE_POOL(BindingHandleBuffer);

            if (EFI_ERROR(ReturnStatus)) {
                ReturnStatus = EFI_NO_MAPPING;
            }

            continue;
        }

        HandleIndex = ConvertHandleToHandleIndex (HandleArray[Index]);
        RomFileName = PoolPrint (L"Handle%X", HandleIndex);

        Status = ReloadOptionROM (
            PciIo->RomImage,
            PciIo->RomSize,
            (const CHAR16 *) RomFileName
        );
        if (EFI_ERROR(ReturnStatus)) {
            ReturnStatus = Status;
        }

        MRD_FREE_POOL(RomFileName);
        MRD_FREE_POOL(BindingHandleBuffer);
    }

    MRD_FREE_POOL(HandleArray);

    return ReturnStatus;
}

EFI_STATUS AcquireGOP (VOID) {
    UINTN                 Index;
    UINTN                 HandleArrayCount;
    UINTN                 BindingHandleCount;
    BOOLEAN               FirstLoop;
    EFI_HANDLE           *HandleArray;
    EFI_HANDLE           *BindingHandleBuffer;
    EFI_STATUS            ReturnStatus;
    EFI_STATUS            Status;
    EFI_PCI_IO_PROTOCOL  *PciIo;

    HandleArrayCount = 0;
    HandleArray = NULL;
    Status = gBS->LocateHandleBuffer(ByProtocol, &gEfiPciIoProtocolGuid, NULL, &HandleArrayCount, &HandleArray);
    if (EFI_ERROR(Status)) {

        return EFI_PROTOCOL_ERROR;
    }

    FirstLoop = TRUE;
    BindingHandleBuffer = NULL;
    ReturnStatus = EFI_LOAD_ERROR;
    for (Index = 0; Index < HandleArrayCount; Index++) {
        do {
            if (FirstLoop == TRUE) {

                BindingHandleBuffer = NULL;
            }

            Status = gBS->HandleProtocol(HandleArray[Index], &gEfiPciIoProtocolGuid, (void **) &PciIo);
            if (EFI_ERROR(Status)) {
                break;
            }

            if (PciIo->RomImage == NULL || PciIo->RomSize == 0) {
                if (EFI_ERROR(ReturnStatus)) {
                    ReturnStatus = EFI_NOT_FOUND;
                }

                break;
            }

            BindingHandleCount = 0;
            PARSE_HANDLE_DATABASE_UEFI_DRIVERS(HandleArray[Index], &BindingHandleCount, &BindingHandleBuffer);
            if (BindingHandleCount != 0) {
                if (EFI_ERROR(ReturnStatus)) {
                    ReturnStatus = EFI_NO_MAPPING;
                }

                break;
            }

            ReturnStatus = EFI_SUCCESS;
        } while (0);

        FirstLoop = FALSE;

        MRD_FREE_POOL(BindingHandleBuffer);

        if (!EFI_ERROR(ReturnStatus)) {
            break;
        }
    }

    MRD_FREE_POOL(HandleArray);

    return ReturnStatus;
}
