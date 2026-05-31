// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2020-2025 Dayo Akanji
// SPDX-FileCopyrightText: 2015 Roderick W. Smith
// SPDX-FileCopyrightText: 2019 vit9696

#include "global.h"
#include "config.h"
#include "lib.h"
#include "apple.h"
#include "mystrings.h"
#include "screenmgt.h"
#include "display.h"
#include <Library/SafeIntLib.h>

CHAR16 *gCsrStatus = NULL;
BOOLEAN MuteLogger = FALSE;
BOOLEAN NormaliseCall = FALSE;
EFI_GUID AppleBootGuid = APPLE_BOOT_VARIABLE_GUID;
EFI_GUID gAppleFramebufferInfoProtocolGuid = APPLE_FRAMEBUFFER_INFO_PROTOCOL_GUID;

EFI_GUID ApplePathPropertiesGuid = {
    0x4d1ede05, 0x38c7, 0x4a6a, {0x9c, 0xc6, 0x4b, 0xcc, 0xa8, 0xb3, 0x8c, 0x14}};

EFI_STATUS GetCsrStatus(IN OUT UINT32 *CsrStatus)
{
    EFI_STATUS Status;
    UINTN CsrLength;
    UINT32 *ReturnValue;

    MRD_FREE_POOL(gCsrStatus);

    ReturnValue = NULL;
    Status = EfivarGetRaw(&AppleBootGuid, L"csr-active-config", (VOID **)&ReturnValue, &CsrLength);
    if (EFI_ERROR(Status)) {
        if (Status != EFI_NOT_FOUND) {
            gCsrStatus = StrDuplicate(L"CSR Retrieval Error");
        }
        else {

            gCsrStatus = StrDuplicate(L"CSR Enabled (Cleared/Empty)");
            *CsrStatus = SIP_ENABLED_EX;

            if (!NormaliseCall) {

                Status = EFI_SUCCESS;
            }
        }

        return Status;
    }

    if (CsrLength != sizeof(UINT32)) {
        gCsrStatus = StrDuplicate(L"CSR Storage Error");

        return EFI_BAD_BUFFER_SIZE;
    }

    *CsrStatus = *ReturnValue;
    RecordgCsrStatus(*CsrStatus, FALSE);

    return Status;
}

VOID RecordgCsrStatus(UINT32 CsrStatus, BOOLEAN ShowResult)
{
    CHAR16 *MsgStr;

    MRD_FREE_POOL(gCsrStatus);

    switch (CsrStatus) {

    case SIP_ENABLED_EX:
        gCsrStatus = StrDuplicate(L"0x0000 - SIP/SSV Enabled (Cleared/Empty)");

        break;

    case SIP_ENABLED:
        gCsrStatus = PoolPrint(L"0x%04x - SIP/SSV Enabled", CsrStatus);

        break;

    case SIP_ENABLED_A001:
    case SIP_ENABLED_A002:
        gCsrStatus = PoolPrint(L"0x%04x - SIP Enabled (Sans FileSys Limits)", CsrStatus);

        break;
    case SIP_ENABLED_B001:
    case SIP_ENABLED_B002:
        gCsrStatus = PoolPrint(L"0x%04x - SIP Enabled (Sans nvRAM Limits)", CsrStatus);

        break;
    case SIP_ENABLED_C001:
    case SIP_ENABLED_C002:
        gCsrStatus = PoolPrint(L"0x%04x - SIP Enabled (Sans Kext Limits)", CsrStatus);

        break;
    case SIP_ENABLED_AB01:
    case SIP_ENABLED_AB02:
        gCsrStatus = PoolPrint(L"0x%04x - SIP Enabled (Sans FileSys/nvRAM Limits)", CsrStatus);

        break;
    case SIP_ENABLED_AC01:
    case SIP_ENABLED_AC02:
        gCsrStatus = PoolPrint(L"0x%04x - SIP Enabled (Sans FileSys/Kext Limits)", CsrStatus);

        break;
    case SIP_ENABLED_BC01:
    case SIP_ENABLED_BC02:
        gCsrStatus = PoolPrint(L"0x%04x - SIP Enabled (Sans nvRAM/Kext Limits)", CsrStatus);

        break;
    case SIP_ENABLED_ABC1:
    case SIP_ENABLED_ABC2:
        gCsrStatus = PoolPrint(L"0x%04x - SIP Enabled (Sans FileSys/nvRAM/Kext Limits)", CsrStatus);

        break;

    case SIP_DISABLED:
    case SIP_DISABLED_B:
    case SIP_DISABLED_EX:
    case SIP_DISABLED_DBG:
    case SIP_DISABLED_KEXT:
    case SIP_DISABLED_EXTRA:
        gCsrStatus = PoolPrint(L"0x%04x - SIP Disabled", CsrStatus);

        break;

    case SSV_DISABLED:
    case SSV_DISABLED_B:
    case SSV_DISABLED_EX:
        gCsrStatus = PoolPrint(L"0x%04x - SIP/SSV Disabled", CsrStatus);

        break;

    case SSV_DISABLED_ANY:
    case SSV_DISABLED_KEXT:
    case SSV_DISABLED_ANY_EX:
        gCsrStatus = PoolPrint(L"0x%04x - SIP/SSV Semi Disabled (Known Custom Setting)", CsrStatus);

        break;

    case SSV_DISABLED_WIDE_OPEN:
    case CSR_MAX_LEGAL_VALUE:
        gCsrStatus = PoolPrint(L"0x%04x - SIP/SSV Totally Disabled", CsrStatus);

        break;

    default:
        gCsrStatus =
            PoolPrint(L"0x%04x - SIP/SSV Semi Disabled (Unknown Custom Setting)", CsrStatus);
    }

    if (ShowResult) {
        MsgStr = PoolPrint(L"Changed %sCSR Setting", (NormaliseCall) ? L"and Normalised " : L"");

#if MERIDIAN_DEBUG > 0
        INFO_LOG("%s    * %s", OffsetNext, MsgStr);
        INFO_LOG("\n\n");
#endif

        egDisplayMessage(MsgStr, &BGColorBase, CENTER, 2, L"PauseSeconds");
    }
}

EFI_STATUS FlagNoCSR(VOID)
{
    MRD_FREE_POOL(gCsrStatus);
    gCsrStatus = StrDuplicate(L"CSR Values *NOT* Configured");

    return EFI_NOT_READY;
}

VOID RotateCsrValue(BOOLEAN UnsetDynamic)
{
    EFI_STATUS Status;
    UINT32 TargetCsr;
    UINT32 CurrentValue;
    UINT32_LIST *ListItem;

#if MERIDIAN_DEBUG > 0
    DEBUG_LOG(1, LOG_THREE_STAR_SEP, L"Rotate CSR");
#endif

    if (GlobalConfig.CsrValues == NULL) {
        FlagNoCSR();

#if MERIDIAN_DEBUG > 0
        DEBUG_LOG(1, LOG_LINE_NORMAL, L"%s", gCsrStatus);
#endif

        egDisplayMessage(gCsrStatus, &BGColorBase, CENTER, 4, L"PauseSeconds");

        return;
    }

    Status = GetCsrStatus(&CurrentValue);
    if (EFI_ERROR(Status)) {
        MRD_FREE_POOL(gCsrStatus);
        gCsrStatus = StrDuplicate(L"Could *NOT* Retrieve SIP/SSV Status");

#if MERIDIAN_DEBUG > 0
        DEBUG_LOG(1, LOG_LINE_NORMAL, L"%s", gCsrStatus);
#endif

        egDisplayMessage(gCsrStatus, &BGColorWarn, CENTER, 4, L"PauseSeconds");

        return;
    }

    ListItem = GlobalConfig.CsrValues;
    while ((ListItem != NULL) && (ListItem->Value != CurrentValue)) {
        ListItem = ListItem->Next;
    }

    TargetCsr = (ListItem == NULL || ListItem->Next == NULL) ? GlobalConfig.CsrValues->Value
                                                             : ListItem->Next->Value;

#if MERIDIAN_DEBUG > 0
    if (TargetCsr == 0) {

        DEBUG_LOG(1, LOG_LINE_NORMAL, L"Clear SIP to 'NULL' from '0x%04x'", CurrentValue);
    }
    else if (CurrentValue == 0) {
        DEBUG_LOG(1, LOG_LINE_NORMAL, L"Change SIP to '0x%04x' from 'NULL'", TargetCsr);
    }
    else {
        DEBUG_LOG(1, LOG_LINE_NORMAL, L"Change SIP to '0x%04x' from '0x%04x'", TargetCsr,
                  CurrentValue);
    }
#endif

    Status =
        (TargetCsr != 0)
            ? EfivarSetRaw(&AppleBootGuid, L"csr-active-config", &TargetCsr, sizeof(UINT32), TRUE)
            : gRT->SetVariable(L"csr-active-config", &AppleBootGuid, AccessFlagsFull, 0, NULL);
    if (EFI_ERROR(Status)) {
        MRD_FREE_POOL(gCsrStatus);
        gCsrStatus = StrDuplicate(L"Error While Setting SIP/SSV");

#if MERIDIAN_DEBUG > 0
        DEBUG_LOG(1, LOG_LINE_NORMAL, L"%s", gCsrStatus);
#endif

        egDisplayMessage(gCsrStatus, &BGColorFail, CENTER, 4, L"PauseSeconds");

        return;
    }

    if (UnsetDynamic || !GlobalConfig.NormaliseCSR) {
        NormaliseCall = FALSE;
    }
    else {
        if ((TargetCsr & CSR_ALLOW_APPLE_INTERNAL) != 0) {
            TargetCsr &= ~CSR_ALLOW_APPLE_INTERNAL;
            NormaliseCall = TRUE;
        }
    }

    RecordgCsrStatus(TargetCsr, UnsetDynamic);

#if MERIDIAN_DEBUG > 0
    DEBUG_LOG(1, LOG_LINE_NORMAL, L"Successfully Set SIP/SSV:- '0x%04x'", TargetCsr);
    if (NormaliseCall) {
        DEBUG_LOG(1, LOG_LINE_NORMAL, L"NOTE: Normalised SIP/SSV");
    }
#endif

    NormaliseCall = FALSE;

    if (UnsetDynamic) {

        GlobalConfig.DynamicCSR = 0;
    }
}

EFI_STATUS NormaliseCSR(VOID)
{
#if MERIDIAN_DEBUG > 0
    BOOLEAN CheckMute = FALSE;
#endif

    EFI_STATUS Status;
    UINT32 OurCSR;

    NormaliseCall = TRUE;

#if MERIDIAN_DEBUG > 0
    MRD_MUTELOGGER_SET;
#endif
    Status = GetCsrStatus(&OurCSR);
#if MERIDIAN_DEBUG > 0
    MRD_MUTELOGGER_OFF;
#endif

    if (EFI_ERROR(Status)) {
        if (Status == EFI_NOT_FOUND) {

            Status = EFI_ALREADY_STARTED;
        }

        return Status;
    }

    if ((OurCSR & CSR_ALLOW_APPLE_INTERNAL) == 0) {

        return EFI_ALREADY_STARTED;
    }

    OurCSR &= ~CSR_ALLOW_APPLE_INTERNAL;
    RecordgCsrStatus(OurCSR, FALSE);

    EfivarSetRaw(&AppleBootGuid, L"csr-active-config", &OurCSR, sizeof(UINT32), TRUE);

    NormaliseCall = FALSE;

    return EFI_SUCCESS;
}

#define EFI_APPLE_SET_OS_PROTOCOL_GUID                                                             \
    {0xc5c5da95, 0x7d5c, 0x45e6, {0xb2, 0xf1, 0x3f, 0xd5, 0x2b, 0xb1, 0x00, 0x77}}

typedef struct EfiAppleSetOsInterface
{
    UINT64 Version;
    EFI_STATUS EFIAPI (*SetOsVersion)(IN CHAR8 *Version);
    EFI_STATUS EFIAPI (*SetOsVendor)(IN CHAR8 *Vendor);
} EfiAppleSetOsInterface;

EFI_STATUS SetAppleOSInfo(VOID)
{
    EFI_STATUS Status;
    EFI_GUID apple_set_os_guid = EFI_APPLE_SET_OS_PROTOCOL_GUID;
    CHAR16 *AppleVersionOS;
    CHAR8 *MacVersionStr;
    EfiAppleSetOsInterface *SetOurOS;

    if (!AppleFirmware) {

        return EFI_NOT_STARTED;
    }

    SetOurOS = NULL;

    Status = gBS->LocateProtocol(&apple_set_os_guid, NULL, (VOID **)&SetOurOS);
    if (EFI_ERROR(Status) || SetOurOS == NULL || SetOurOS->Version == 0) {

        return EFI_SUCCESS;
    }

    AppleVersionOS = StrDuplicate(L"Mac OS X");
    if (AppleVersionOS == NULL) {

        return EFI_OUT_OF_RESOURCES;
    }

    MergeStrings(&AppleVersionOS, GlobalConfig.SpoofOSXVersion, ' ');

    MacVersionStr = AllocateZeroPool((StrLen(AppleVersionOS) + 1) * sizeof(CHAR8));
    if (MacVersionStr == NULL) {
        MRD_FREE_POOL(AppleVersionOS);

        return EFI_OUT_OF_RESOURCES;
    }

#if MERIDIAN_DEBUG > 0
    DEBUG_LOG(1, LOG_LINE_NORMAL, L"Set Mac OS Information:- '%s'", AppleVersionOS);
#endif

    UnicodeStrToAsciiStrS(AppleVersionOS, MacVersionStr, StrLen(AppleVersionOS) + 1);

    Status = SetOurOS->SetOsVersion(MacVersionStr);
    if (!EFI_ERROR(Status) && SetOurOS->Version >= 2) {
        SetOurOS->SetOsVendor((CHAR8 *)"Apple Inc.");
    }

    MRD_FREE_POOL(MacVersionStr);
    MRD_FREE_POOL(AppleVersionOS);

    return Status;
}

/*
 * Derived from the OpenCore Project.
 * Copyright (C) 2019, vit9696. All rights reserved.
 * Modifications copyright (c) 2021-2024 Dayo Akanji (sf.net/u/dakanji/profile).
 *
 * Licensed and made available under the terms and conditions of the BSD
 * License which accompanies this distribution. The full text of the license
 * may be found at http://opensource.org/licenses/bsd-license.php
 *
 * THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
 * WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
 */
static VOID ZapBootFlag(CHAR16 *VariableName)
{
    EFI_STATUS Status;
    UINTN BufferSize;
    VOID *TmpBuffer;

    TmpBuffer = NULL;
    BufferSize = 0;
    Status = gRT->GetVariable(VariableName, &AppleBootGuid, NULL, &BufferSize, TmpBuffer);
    if (Status == EFI_BUFFER_TOO_SMALL || BufferSize != 0) {
        gRT->SetVariable(VariableName, &AppleBootGuid, AccessFlagsFull, 0, NULL);
    }

    MRD_FREE_POOL(TmpBuffer);
}

static VOID *MeridianGetFileInfo(IN EFI_FILE_PROTOCOL *File, IN EFI_GUID *InformationType,
                                 IN UINTN MinFileInfoSize, OUT UINTN *RealFileInfoSize OPTIONAL)
{
    EFI_STATUS Status;
    UINTN FileInfoSize;
    VOID *FileInfoBuffer;

    FileInfoSize = 0;
    FileInfoBuffer = NULL;

    Status = File->GetInfo(File, InformationType, &FileInfoSize, NULL);
    if (Status != EFI_BUFFER_TOO_SMALL || FileInfoSize < MinFileInfoSize) {

        return NULL;
    }

    if (CompareGuid(InformationType, &gEfiFileInfoGuid) &&
        EFI_ERROR(SafeUintnAdd(FileInfoSize, sizeof(CHAR16), &FileInfoSize))) {

        return NULL;
    }

    FileInfoBuffer = AllocateZeroPool(FileInfoSize);
    if (FileInfoBuffer == NULL) {

        return NULL;
    }

    Status = File->GetInfo(File, InformationType, &FileInfoSize, FileInfoBuffer);
    if (EFI_ERROR(Status)) {
        MRD_FREE_POOL(FileInfoBuffer);

        return NULL;
    }

    if (RealFileInfoSize != NULL) {
        *RealFileInfoSize = FileInfoSize;
    }

    return FileInfoBuffer;
}

static EFI_STATUS
MeridianGetApfsSpecialFileInfo(IN EFI_FILE_PROTOCOL *Root,
                               IN OUT APFS_INFO_VOLUME **VolumeInfo OPTIONAL,
                               IN OUT APFS_INFO_CONTAINER **ContainerInfo OPTIONAL)
{
    EFI_GUID AppleApfsVolumeInfoGuid = GUID_APFS_INFO_VOLUME;
    EFI_GUID AppleApfsContainerInfoGuid = GUID_APFS_INFO_CONTAINER;

    if (ContainerInfo == NULL && VolumeInfo == NULL) {

        return EFI_INVALID_PARAMETER;
    }

    if (VolumeInfo != NULL) {
        *VolumeInfo =
            MeridianGetFileInfo(Root, &AppleApfsVolumeInfoGuid, sizeof(**VolumeInfo), NULL);
        if (*VolumeInfo == NULL) {

            return EFI_NOT_FOUND;
        }
    }

    if (ContainerInfo != NULL) {
        *ContainerInfo =
            MeridianGetFileInfo(Root, &AppleApfsContainerInfoGuid, sizeof(**ContainerInfo), NULL);
        if (*ContainerInfo == NULL) {

            return EFI_NOT_FOUND;
        }
    }

    return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI MeridianAppleFramebufferGetInfo(
    IN APPLE_FRAMEBUFFER_INFO_PROTOCOL *This, OUT EFI_PHYSICAL_ADDRESS *FramebufferBase,
    OUT UINT32 *FramebufferSize, OUT UINT32 *ScreenRowBytes, OUT UINT32 *ScreenWidth,
    OUT UINT32 *ScreenHeight, OUT UINT32 *ScreenDepth)
{
    EFI_STATUS Status;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *GraphicsOutput;
    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *Mode;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info;

    if (!AppleFirmware) {
        return EFI_UNSUPPORTED;
    }

    if (This == NULL || FramebufferBase == NULL || FramebufferSize == 0 || ScreenRowBytes == 0 ||
        ScreenWidth == 0 || ScreenHeight == 0 || ScreenDepth == 0) {
        return EFI_INVALID_PARAMETER;
    }

    Status = gBS->HandleProtocol(gST->ConsoleOutHandle, &gEfiGraphicsOutputProtocolGuid,
                                 (VOID **)&GraphicsOutput);
    if (EFI_ERROR(Status) || GraphicsOutput->Mode->Info == NULL) {
        return EFI_UNSUPPORTED;
    }

    Mode = GraphicsOutput->Mode;
    Info = Mode->Info;

    *FramebufferBase = Mode->FrameBufferBase;
    *FramebufferSize = (UINT32)Mode->FrameBufferSize;
    *ScreenRowBytes = (UINT32)(Info->PixelsPerScanLine * sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL));
    *ScreenWidth = Info->HorizontalResolution;
    *ScreenHeight = Info->VerticalResolution;
    *ScreenDepth = DEFAULT_COLOUR_DEPTH;

    return EFI_SUCCESS;
}

EFI_STATUS MeridianGetApfsVolumeInfo(IN EFI_HANDLE Device, OUT EFI_GUID *ContainerGuid OPTIONAL,
                                     OUT EFI_GUID *VolumeGuid OPTIONAL,
                                     OUT APFS_VOLUME_ROLE *VolumeRole OPTIONAL)
{
    EFI_STATUS Status;
    EFI_FILE_PROTOCOL *Root;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem;
    APFS_INFO_CONTAINER *ApfsContainerInfo;
    APFS_INFO_VOLUME *ApfsVolumeInfo;

    if (ContainerGuid == NULL && VolumeGuid == NULL && VolumeRole == NULL) {

        return EFI_INVALID_PARAMETER;
    }

    Root = NULL;

    Status = gBS->HandleProtocol(Device, &gEfiSimpleFileSystemProtocolGuid, (VOID **)&FileSystem);
    if (EFI_ERROR(Status)) {

        return Status;
    }

    Status = FileSystem->OpenVolume(FileSystem, &Root);
    if (EFI_ERROR(Status)) {

        return Status;
    }

    Status = MeridianGetApfsSpecialFileInfo(Root, &ApfsVolumeInfo, &ApfsContainerInfo);
    Root->Close(Root);
    if (EFI_ERROR(Status)) {

        return EFI_NOT_FOUND;
    }

    if (VolumeGuid != NULL) {
        CopyGuid(VolumeGuid, &ApfsVolumeInfo->Uuid);
    }

    if (VolumeRole != NULL) {
        *VolumeRole = ApfsVolumeInfo->Role;
    }

    if (ContainerGuid != NULL) {
        CopyGuid(ContainerGuid, &ApfsContainerInfo->Uuid);
    }

    MRD_FREE_POOL(ApfsVolumeInfo);
    MRD_FREE_POOL(ApfsContainerInfo);

    return EFI_SUCCESS;
}

VOID MeridianAppleFbInfoInstallProtocol(VOID)
{
#if MERIDIAN_DEBUG > 0
    CHAR16 *MsgStr;
#endif

    EFI_STATUS Status;
    APPLE_FRAMEBUFFER_INFO_PROTOCOL *Protocol;

    static APPLE_FRAMEBUFFER_INFO_PROTOCOL OurAppleFramebufferInfo = {
        MeridianAppleFramebufferGetInfo};

#if MERIDIAN_DEBUG > 0
    MsgStr = L"Update Base Apple Framebuffer";
    DEBUG_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
    INFO_LOG("%s:", MsgStr);
#endif

    do {
        if (!GlobalConfig.SetAppleFB) {
#if MERIDIAN_DEBUG > 0
            Status = EFI_NOT_STARTED;
#endif

            break;
        }

        Status = gBS->LocateProtocol(&gAppleFramebufferInfoProtocolGuid, NULL, (VOID *)&Protocol);
        if (!EFI_ERROR(Status)) {
#if MERIDIAN_DEBUG > 0
            Status = EFI_ALREADY_STARTED;
#endif

            break;
        }

#if MERIDIAN_DEBUG > 0
        MsgStr = L"Get Old Apple Framebuffer";
        DEBUG_LOG(1, LOG_LINE_NORMAL, L"%s:- '%r'", MsgStr, Status);
        INFO_LOG("%s  - %s ... %r", OffsetNext, MsgStr, Status);
#endif

        UninitMeridianLib();
#if MERIDIAN_DEBUG > 0

        Status =
#endif
            gBS->InstallMultipleProtocolInterfaces(&gImageHandle,
                                                   &gAppleFramebufferInfoProtocolGuid,
                                                   (VOID *)&OurAppleFramebufferInfo, NULL);
        ReinitMeridianLib();
    } while (0);

#if MERIDIAN_DEBUG > 0
    MsgStr = L"Set New Apple Framebuffer";
    DEBUG_LOG(1, LOG_LINE_NORMAL, L"%s:- '%r'", MsgStr, Status);
    INFO_LOG("%s  - %s ... %r", OffsetNext, MsgStr, Status);
    INFO_LOG("\n\n");
#endif
}

VOID ClearRecoveryBootFlags(VOID)
{
    ZapBootFlag(L"recovery-boot-mode");
    ZapBootFlag(L"internet-recovery-mode");
    ZapBootFlag(L"RecoveryBootInitiator");
}

static BOOLEAN BufferHasSavedConfig(IN UINT8 *Buffer, IN UINTN BufferSize)
{
    static CONST UINT8 Needle[] = {'s', 0, 'a', 0, 'v', 0, 'e', 0, 'd', 0, '-', 0,
                                   'c', 0, 'o', 0, 'n', 0, 'f', 0, 'i', 0, 'g', 0};
    UINTN i;
    UINTN j;
    UINTN NeedleLen = sizeof(Needle);

    if (Buffer == NULL || BufferSize < NeedleLen) {
        return FALSE;
    }

    for (i = 0; i + NeedleLen <= BufferSize; i++) {
        for (j = 0; j < NeedleLen; j++) {
            if (Buffer[i + j] != Needle[j]) {
                break;
            }
        }
        if (j == NeedleLen) {
            return TRUE;
        }
    }

    return FALSE;
}

VOID HandleAppleGfxRestore(VOID)
{
    EFI_STATUS DeleteStatus;
    EFI_STATUS GuardStatus;
    EFI_STATUS Status;
    VOID *PathProps;
    UINTN PathPropsSize;
    UINTN VarSize;
    UINT8 GuardByte;
    UINT32 HealCount;
    BOOLEAN GuardSet;
    BOOLEAN GuardSaved;
    BOOLEAN HasSavedConfig;
    BOOLEAN PoisonCleared;

#if MERIDIAN_DEBUG > 0
    CHAR16 *MsgStr;
#endif

    if (!AppleFirmware) {
        return;
    }

    PathProps = NULL;
    PathPropsSize = 0;
    Status = EfivarGetRaw(&ApplePathPropertiesGuid, L"AAPL,PathProperties0000", &PathProps,
                          &PathPropsSize);
    HasSavedConfig =
        (!EFI_ERROR(Status)) ? BufferHasSavedConfig((UINT8 *)PathProps, PathPropsSize) : FALSE;
    MRD_FREE_POOL(PathProps);

    GuardByte = 0;
    VarSize = sizeof(GuardByte);
    Status = gRT->GetVariable(L"MeridianGfxRestorePass", &MeridianGuid, NULL, &VarSize, &GuardByte);
    GuardSet = (!EFI_ERROR(Status));

    if (!HasSavedConfig) {

        if (GuardSet) {
            HealCount = 0;
            VarSize = sizeof(HealCount);
            gRT->GetVariable(L"MeridianGfxRestoreCount", &MeridianGuid, NULL, &VarSize, &HealCount);

#if MERIDIAN_DEBUG > 0
            MsgStr = PoolPrint(L"Apple GFX Restore Fix:- 'Self-Heal Complete (Count:- %d) ... "
                               L"Resumed Normal Boot'",
                               HealCount);
            DEBUG_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
            INFO_LOG("INFO: %s", MsgStr);
            INFO_LOG("\n\n");
            MRD_FREE_POOL(MsgStr);
#endif

            gRT->SetVariable(L"MeridianGfxRestorePass", &MeridianGuid, AccessFlagsFull, 0, NULL);
        }

        return;
    }

    if (GuardSet) {

#if MERIDIAN_DEBUG > 0
        MsgStr = L"Apple GFX Restore Fix:- 'Still Present After Reset ... Abort to Avoid Loop'";
        DEBUG_LOG(1, LOG_LINE_NORMAL, L"%s!!", MsgStr);
        INFO_LOG("** WARN: %s", MsgStr);
        INFO_LOG("\n\n");
#endif

        gRT->SetVariable(L"MeridianGfxRestorePass", &MeridianGuid, AccessFlagsFull, 0, NULL);

        return;
    }

    GuardByte = 1;
    GuardStatus = gRT->SetVariable(L"MeridianGfxRestorePass", &MeridianGuid, AccessFlagsFull,
                                   sizeof(GuardByte), &GuardByte);
    GuardSaved = (!EFI_ERROR(GuardStatus));

    DeleteStatus = gRT->SetVariable(L"AAPL,PathProperties0000", &ApplePathPropertiesGuid,
                                    AccessFlagsFull, 0, NULL);

    PoisonCleared = FALSE;
    if (!EFI_ERROR(DeleteStatus) || !GuardSaved) {
        PathProps = NULL;
        PathPropsSize = 0;
        Status = EfivarGetRaw(&ApplePathPropertiesGuid, L"AAPL,PathProperties0000", &PathProps,
                              &PathPropsSize);
        if (Status == EFI_NOT_FOUND) {
            PoisonCleared = TRUE;
        }
        else if (!EFI_ERROR(Status)) {
            PoisonCleared = !BufferHasSavedConfig((UINT8 *)PathProps, PathPropsSize);
        }
        MRD_FREE_POOL(PathProps);
    }

    if (!GuardSaved && !PoisonCleared) {
#if MERIDIAN_DEBUG > 0
        MsgStr = PoolPrint(L"Apple GFX Restore Fix:- 'Guard/Removal Failed ... Abort Reset' "
                           L"(Guard:- %r, Delete:- %r, Verify:- %r)",
                           GuardStatus, DeleteStatus, Status);
        DEBUG_LOG(1, LOG_LINE_NORMAL, L"%s!!", MsgStr);
        INFO_LOG("** WARN: %s", MsgStr);
        INFO_LOG("\n\n");
        MRD_FREE_POOL(MsgStr);
#endif

        return;
    }

#if MERIDIAN_DEBUG > 0
    MsgStr = L"Apple GFX Restore Fix:- 'Mac OS saved-config Detected ... Clear and Warm Reset'";
    DEBUG_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
    INFO_LOG("INFO: %s", MsgStr);
    INFO_LOG("\n\n");
#endif

    HealCount = 0;
    VarSize = sizeof(HealCount);
    gRT->GetVariable(L"MeridianGfxRestoreCount", &MeridianGuid, NULL, &VarSize, &HealCount);
    HealCount++;
    gRT->SetVariable(L"MeridianGfxRestoreCount", &MeridianGuid, AccessFlagsFull, sizeof(HealCount),
                     &HealCount);

    gRT->ResetSystem(EfiResetWarm, EFI_SUCCESS, 0, NULL);

}

static EFI_STATUS EFIAPI OpenProtocolEx(IN EFI_HANDLE Handle, IN EFI_GUID *Protocol,
                                        OUT VOID **Interface OPTIONAL, IN EFI_HANDLE AgentHandle,
                                        IN EFI_HANDLE ControllerHandle, IN UINT32 Attributes)
{
    EFI_STATUS Status;

    Status =
        OrigOpenProtocolBS(Handle, Protocol, Interface, AgentHandle, ControllerHandle, Attributes);

    if (!EFI_ERROR(Status) && Interface != NULL) {
        if (CompareGuid(&gEfiGraphicsOutputProtocolGuid, Protocol)) {
            Status = gBS->LocateProtocol(&gEfiGraphicsOutputProtocolGuid, NULL, Interface);
        }
        else if (CompareGuid(&gEfiUgaDrawProtocolGuid, Protocol)) {
            Status = gBS->LocateProtocol(&gEfiUgaDrawProtocolGuid, NULL, Interface);
        }
    }

    return Status;
}

EFI_STATUS EFIAPI HandleProtocolEx(IN EFI_HANDLE Handle, IN EFI_GUID *Protocol,
                                   OUT VOID **Interface)
{
    EFI_STATUS Status;

    Status = gBS->OpenProtocol(Handle, Protocol, Interface, gImageHandle, NULL,
                               EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);

    return Status;
}

VOID RemapOpenProtocol(VOID)
{
    if (GOPDraw == NULL && UGADraw == NULL) {

        return;
    }

    if (AppleFirmware && (!DevicePresence || AppleFramebuffers == 0)) {

        return;
    }

    gBS->OpenProtocol = OpenProtocolEx;
    gBS->Hdr.CRC32 = 0;
    gBS->CalculateCrc32(gBS, gBS->Hdr.HeaderSize, &gBS->Hdr.CRC32);
}
