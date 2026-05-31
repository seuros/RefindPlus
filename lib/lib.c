// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2020-2025 Dayo Akanji
// SPDX-FileCopyrightText: 2021 Joe van Tunen
// SPDX-FileCopyrightText: 2012-2021 Roderick W. Smith
// SPDX-FileCopyrightText: 2006-2009 Christoph Pfisterer
// SPDX-FileCopyrightText: 2019 vit9696

#include "global.h"
#include "gpt.h"
#include "lib.h"
#include "scan.h"
#include "apple.h"
#include "config.h"
#include "screenmgt.h"
#include "mystrings.h"
#include "RemovableMedia.h"

#define LibLocateHandle gBS->LocateHandleBuffer
#define BlockIoProtocol gEfiBlockIoProtocolGuid
#define LibOpenRoot EfiLibOpenRoot
EFI_DEVICE_PATH_PROTOCOL EndDevicePath[] = {
    {END_DEVICE_PATH_TYPE, END_ENTIRE_DEVICE_PATH_SUBTYPE, {END_DEVICE_PATH_LENGTH, 0}}};

#if defined(EFIX64)
EFI_GUID gRootGuid = {0x4f68bce3, 0xe8cd, 0x4db1, {0x96, 0xe7, 0xfb, 0xca, 0xf9, 0x84, 0xb7, 0x09}};
#elif defined(EFIAARCH64)
EFI_GUID gRootGuid = {0xb921b045, 0x1df0, 0x41c3, {0xaf, 0x44, 0x4c, 0x6f, 0x28, 0x0d, 0x3f, 0xae}};
#else
EFI_GUID gRootGuid = {0x69dad710, 0x2ce4, 0x4e3c, {0xb1, 0x6c, 0x21, 0xa1, 0xd4, 0x9a, 0xbe, 0xd3}};
#endif

#define UNINIT_VOLUMES(x, y)                                                                       \
    do {                                                                                           \
        for (UINTN i = 0; i < y; i++) {                                                            \
            UninitVolume(&x[i]);                                                                   \
        }                                                                                          \
    } while (0)
#define REINIT_VOLUMES(x, y)                                                                       \
    do {                                                                                           \
        for (UINTN i = 0; i < y; i++) {                                                            \
            ReinitVolume(&x[i]);                                                                   \
        }                                                                                          \
    } while (0)

EFI_HANDLE SelfImageHandle = NULL;

EFI_LOADED_IMAGE_PROTOCOL *SelfLoadedImage = NULL;

CHAR16 *SelfDirPath = NULL;
CHAR16 *SelfBinaryDirPath = NULL;
CHAR16 *SelfBaseName = NULL;
CHAR16 *SelfToolPath = NULL;

EFI_FILE_PROTOCOL *SelfRootDir = NULL;
EFI_FILE_PROTOCOL *SelfDir = NULL;
EFI_FILE_PROTOCOL *gVarsDir = NULL;

MERIDIAN_VOLUME *SelfVolume = NULL;
MERIDIAN_VOLUME **Volumes = NULL;
MERIDIAN_VOLUME **RecoveryVolumesAPFS = NULL;
MERIDIAN_VOLUME **RecoveryVolumesHFS = NULL;
MERIDIAN_VOLUME **SkipApfsVolumes = NULL;
MERIDIAN_VOLUME **PreBootVolumes = NULL;
MERIDIAN_VOLUME **SystemVolumes = NULL;
MERIDIAN_VOLUME **DataVolumes = NULL;

UINTN RecoveryVolumesAPFSCount = 0;
UINTN RecoveryVolumesHFSCount = 0;
UINTN SkipApfsVolumesCount = 0;
UINTN PreBootVolumesCount = 0;
UINTN SystemVolumesCount = 0;
UINTN DataVolumesCount = 0;
UINTN VolumesCount = 0;

UINT64 MeridianReadOnly = EFI_FILE_MODE_READ;
UINT64 MeridianReadWrite = EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE;
UINT64 MeridianReadWriteCreate = EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE;

BOOLEAN FoundExternalDisk = FALSE;
BOOLEAN DoneHeadings = FALSE;
BOOLEAN SkipSpacing = FALSE;
BOOLEAN UseButJoin = FALSE;
BOOLEAN SelfVolSet = FALSE;
BOOLEAN SelfVolRun = FALSE;
BOOLEAN MediaCheck = FALSE;
BOOLEAN SingleAPFS = TRUE;
BOOLEAN ValidAPFS = TRUE;
BOOLEAN ScanMBR = FALSE;

#if MERIDIAN_DEBUG > 0
BOOLEAN FirstVolume = TRUE;
BOOLEAN ScannedOnce = FALSE;
BOOLEAN FoundMBR = FALSE;
#endif

EFI_GUID GuidESP = ESP_GUID_VALUE;
EFI_GUID GuidHFS = HFS_GUID_VALUE;
EFI_GUID GuidAPFS = APFS_GUID_VALUE;
EFI_GUID GuidNull = NULL_GUID_VALUE;
EFI_GUID GuidSwap = SWAP_GUID_VALUE;
EFI_GUID GuidHome = HOME_GUID_VALUE;
EFI_GUID GuidLuks = LUKS_GUID_VALUE;
EFI_GUID GuidLinux = LINUX_GUID_VALUE;
EFI_GUID GuidBasicData = BASIC_DATA_GUID_VALUE;
EFI_GUID GuidApplTvRec = APPLE_TV_RECOVERY_GUID;
EFI_GUID GuidFlagAPFS = APFS_FINGER_PRINT_GUID;
EFI_GUID GuidMacRaidOn = MAC_RAID_ON_GUID_VALUE;
EFI_GUID GuidMacRaidOff = MAC_RAID_OFF_GUID_VALUE;
EFI_GUID GuidReservedMS = MSFT_RESERVED_GUID_VALUE;
EFI_GUID GuidWindowsRE = WIN_RECOVERY_ENV_GUID_VALUE;
EFI_GUID GuidRecoveryHD = MAC_RECOVERY_HD_GUID_VALUE;
EFI_GUID GuidContainHFS = CONTAINER_HFS_GUID_VALUE;

extern EFI_GUID MeridianGuid;
extern BOOLEAN ScanningLoaders;

static VOID ResolveAssetsDir(VOID)
{
    CHAR16 *Home;
    EFI_STATUS Status;
    EFI_FILE_PROTOCOL *NewSelfDir;

    if (SelfRootDir == NULL || SelfDirPath == NULL) {
        return;
    }

    Home = MERIDIAN_ESP_HOME;

    if (MrdStrEqualsCI(SelfDirPath, Home)) {
        return;
    }

    NewSelfDir = NULL;
    Status = SelfRootDir->Open(SelfRootDir, &NewSelfDir, Home, MeridianReadOnly, 0);
    if (EFI_ERROR(Status) || NewSelfDir == NULL) {
        return;
    }

    if (SelfDir != NULL && SelfDir != SelfRootDir) {
        SelfDir->Close(SelfDir);
    }
    SelfDir = NewSelfDir;

    MRD_FREE_POOL(SelfDirPath);
    SelfDirPath = StrDuplicate(Home);

    MRD_FREE_POOL(SelfToolPath);
    SelfToolPath = PoolPrint(L"%s\\%s", SelfDirPath, MERIDIAN_ESP_TOOLS_DIR);

#if MERIDIAN_DEBUG > 0
    INFO_LOG("INFO: Home anchored at '%s' (decree: config always in Meridian)\n\n", SelfDirPath);
#endif
}

static EFI_STATUS FinishInitMeridianLib(VOID)
{
#if MERIDIAN_DEBUG > 0
    BOOLEAN CheckMute = FALSE;
#endif

    EFI_STATUS Status;

    if (SelfVolume != NULL && SelfVolume->DeviceHandle != SelfLoadedImage->DeviceHandle) {
        SelfLoadedImage->DeviceHandle = SelfVolume->DeviceHandle;
    }

    if (SelfRootDir == NULL) {
        SelfRootDir = LibOpenRoot(SelfLoadedImage->DeviceHandle);
    }

    if (SelfRootDir == NULL) {
        Status = EFI_INVALID_PARAMETER;
    }
    else {

        ResolveAssetsDir();

        Status = SelfRootDir->Open(SelfRootDir, &SelfDir, SelfDirPath, MeridianReadOnly, 0);
        if (!EFI_ERROR(Status)) {
#if MERIDIAN_DEBUG > 0
            MRD_MUTELOGGER_SET;
#endif
            Status = FindVarsDir();
#if MERIDIAN_DEBUG > 0
            MRD_MUTELOGGER_OFF;
#endif
        }
    }

    if (EFI_ERROR(Status)) {
        CheckError(Status, L"While (Re)Opening Installation Volume/Directory");
    }

    return Status;
}

VOID UninitVolume(IN OUT MERIDIAN_VOLUME **Volume)
{
    if (Volume == NULL || *Volume == NULL) {
        return;
    }

    if ((*Volume)->RootDir != NULL) {
        (*Volume)->RootDir->Close((*Volume)->RootDir);
        (*Volume)->RootDir = NULL;
    }

    (*Volume)->BlockIO = NULL;
    (*Volume)->DeviceHandle = NULL;
    (*Volume)->WholeDiskBlockIO = NULL;
}

static VOID UninitVolumes(VOID)
{
    UNINIT_VOLUMES(RecoveryVolumesAPFS, RecoveryVolumesAPFSCount);
    UNINIT_VOLUMES(RecoveryVolumesHFS, RecoveryVolumesHFSCount);
    UNINIT_VOLUMES(SkipApfsVolumes, SkipApfsVolumesCount);
    UNINIT_VOLUMES(PreBootVolumes, PreBootVolumesCount);
    UNINIT_VOLUMES(SystemVolumes, SystemVolumesCount);
    UNINIT_VOLUMES(DataVolumes, DataVolumesCount);
    UNINIT_VOLUMES(Volumes, VolumesCount);
    UninitVolume(&SelfVolume);
}

VOID ReinitVolume(IN OUT MERIDIAN_VOLUME **Volume)
{
    EFI_STATUS Status;
    CHAR16 *ErrStr;
    EFI_HANDLE DeviceHandle;
    EFI_HANDLE WholeDiskHandle;
    EFI_DEVICE_PATH_PROTOCOL *RemainingDevicePath;

    if (Volume == NULL || *Volume == NULL) {
        return;
    }

    if ((*Volume)->DevicePath != NULL) {
        RemainingDevicePath = (*Volume)->DevicePath;
        Status = gBS->LocateDevicePath(&BlockIoProtocol, &RemainingDevicePath, &DeviceHandle);
        if (!EFI_ERROR(Status)) {
            (*Volume)->DeviceHandle = DeviceHandle;
            (*Volume)->RootDir = LibOpenRoot((*Volume)->DeviceHandle);
        }
        else {
            ErrStr = PoolPrint(L"from LocateDevicePath for DeviceHandle in ReinitVolume:- '%s'",
                               ((*Volume)->VolName != NULL) ? (*Volume)->VolName : L"Unnamed");
            CheckError(Status, ErrStr);
            MRD_FREE_POOL(ErrStr);
        }
    }

    if ((*Volume)->WholeDiskDevicePath != NULL) {
        RemainingDevicePath = (*Volume)->WholeDiskDevicePath;
        Status = gBS->LocateDevicePath(&BlockIoProtocol, &RemainingDevicePath, &WholeDiskHandle);
        if (!EFI_ERROR(Status)) {
            Status = gBS->HandleProtocol(WholeDiskHandle, &BlockIoProtocol,
                                         (VOID **)&(*Volume)->WholeDiskBlockIO);
            if (EFI_ERROR(Status)) {
                (*Volume)->WholeDiskBlockIO = NULL;
            }
        }
    }

    if ((*Volume)->DeviceHandle != NULL) {
        Status = gBS->HandleProtocol((*Volume)->DeviceHandle, &BlockIoProtocol,
                                     (VOID **)&((*Volume)->BlockIO));
        if (EFI_ERROR(Status)) {
            (*Volume)->BlockIO = NULL;
        }
    }
}

/*
 * Derived from the OpenCore Project.
 * Copyright (C) 2019, vit9696. All rights reserved.
 * Modifications copyright (c) 2024 Dayo Akanji (sf.net/u/dakanji/profile).
 *
 * Licensed and made available under the terms and conditions of the BSD
 * License which accompanies this distribution. The full text of the license
 * may be found at http://opensource.org/licenses/bsd-license.php
 *
 * THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
 * WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
 */
CHAR16 *MeridianGetBootPathName(IN EFI_DEVICE_PATH_PROTOCOL *DevicePath)
{
    UINTN Len;
    CHAR16 *FilePathName;
    CHAR16 *PathName;
    CHAR16 *Slash;

    if (DevicePathType(DevicePath) != MEDIA_DEVICE_PATH ||
        DevicePathSubType(DevicePath) != MEDIA_FILEPATH_DP) {
        PathName = AllocateZeroPool(sizeof(L"\\"));
        if (PathName != NULL) {
            StrCpyS(PathName, sizeof(L"\\"), L"\\");
        }

        return PathName;
    }

    PathName = ConvertDevicePathToText(DevicePath, FALSE, FALSE);
    if (PathName == NULL) {
        return NULL;
    }

    Slash = MrdStrFind(PathName, L"\\");

    if (Slash != NULL) {
        Len = StrLen(PathName);

        FilePathName = &PathName[Len - 1];

        while (*FilePathName != L'\\') {
            *FilePathName = L'\0';
            --FilePathName;
        }
    }

    return PathName;
}

VOID CleanUpPathNameSlashes(IN OUT CHAR16 *PathName)
{
    UINTN Dest;
    UINTN Source;

    if (PathName == NULL || PathName[0] == '\0') {
        return;
    }

    Source = Dest = 0;
    while (PathName[Source] != '\0') {
        if ((PathName[Source] == L'/') || (PathName[Source] == L'\\')) {
            if (Dest == 0) {

                Source++;
            }
            else {
                PathName[Dest] = L'\\';
                do {

                    Source++;
                } while ((PathName[Source] == L'/') || (PathName[Source] == L'\\'));

                Dest++;
            }
        }
        else {

            PathName[Dest] = PathName[Source];
            Source++;
            Dest++;
        }
    }

    if ((Dest > 0) && (PathName[Dest - 1] == L'\\')) {
        Dest--;
    }

    PathName[Dest] = L'\0';
    if (PathName[0] == L'\0') {
        PathName[0] = L'\\';
        PathName[1] = L'\0';
    }
}

CHAR16 *SplitDeviceString(IN OUT CHAR16 *InString)
{
    INTN i;
    CHAR16 *FileName;
    BOOLEAN Found;

    FileName = NULL;
    if (InString != NULL) {
        Found = FALSE;
        i = StrLen(InString) - 1;
        while ((i >= 0) && (!Found)) {
            if (InString[i] == L')') {
                Found = TRUE;
                FileName = StrDuplicate(&InString[i + 1]);
                CleanUpPathNameSlashes(FileName);
                InString[i + 1] = '\0';
            }
            i--;
        }

        if (FileName == NULL) {
            FileName = StrDuplicate(InString);
        }
    }

    return FileName;
}

EFI_STATUS InitMeridianLib(IN EFI_HANDLE ImageHandle)
{
    EFI_STATUS Status;
    CHAR16 *Temp;
    CHAR16 *DevicePathAsString;

    SelfImageHandle = ImageHandle;
    Status = gBS->HandleProtocol(SelfImageHandle, &LoadedImageProtocol, (VOID **)&SelfLoadedImage);
    if (EFI_ERROR(Status)) {
        CheckFatalError(Status, L"While Getting 'Self' LoadedImageProtocol Handle");

        return EFI_LOAD_ERROR;
    }

    DevicePathAsString = DevicePathToStr(SelfLoadedImage->FilePath);
    GlobalConfig.SelfDevicePath = FileDevicePath(SelfLoadedImage->DeviceHandle, DevicePathAsString);

    CleanUpPathNameSlashes(DevicePathAsString);
    Temp = SplitDeviceString(DevicePathAsString);

    MRD_FREE_POOL(SelfDirPath);
    SelfDirPath = FindPath(Temp);

    MRD_FREE_POOL(SelfBinaryDirPath);
    SelfBinaryDirPath = StrDuplicate(SelfDirPath);

    MRD_FREE_POOL(SelfBaseName);
    SelfBaseName = Basename(Temp);

    MRD_FREE_POOL(SelfToolPath);
    SelfToolPath = PoolPrint(L"%s\\%s", SelfDirPath, MERIDIAN_ESP_TOOLS_DIR);

    MRD_FREE_POOL(DevicePathAsString);
    MRD_FREE_POOL(Temp);

    Status = FinishInitMeridianLib();

    return Status;
}

VOID ReinitVolumes(VOID)
{
    REINIT_VOLUMES(RecoveryVolumesAPFS, RecoveryVolumesAPFSCount);
    REINIT_VOLUMES(RecoveryVolumesHFS, RecoveryVolumesHFSCount);
    REINIT_VOLUMES(SkipApfsVolumes, SkipApfsVolumesCount);
    REINIT_VOLUMES(PreBootVolumes, PreBootVolumesCount);
    REINIT_VOLUMES(SystemVolumes, SystemVolumesCount);
    REINIT_VOLUMES(DataVolumes, DataVolumesCount);
    REINIT_VOLUMES(Volumes, VolumesCount);
    ReinitVolume(&SelfVolume);
}

VOID UninitMeridianLib(VOID)
{

    if (SelfRootDir == SelfVolume->RootDir) {
        SelfRootDir = NULL;
    }

    UninitVolumes();

    if (SelfDir != NULL) {
        SelfDir->Close(SelfDir);
        SelfDir = NULL;
    }

    if (SelfRootDir != NULL) {
        SelfRootDir->Close(SelfRootDir);
        SelfRootDir = NULL;
    }

    if (gVarsDir != NULL) {
        gVarsDir->Close(gVarsDir);
        gVarsDir = NULL;
    }
}

EFI_STATUS ReinitMeridianLib(VOID)
{
    EFI_STATUS Status;

    ReinitVolumes();
    Status = FinishInitMeridianLib();

    return Status;
}

EFI_STATUS FindVarsDir(VOID)
{
    EFI_STATUS Status;
    CHAR16 *VarsFolder;
    EFI_FILE_HANDLE EspRootDir;

    if (gVarsDir != NULL) {
        return EFI_SUCCESS;
    }

    VarsFolder = L"vars";
    Status =
        SelfDir->Open(SelfDir, &gVarsDir, VarsFolder, MeridianReadWriteCreate, EFI_FILE_DIRECTORY);

#if MERIDIAN_DEBUG > 0
    DEBUG_LOG(1, LOG_BLANK_LINE_SEP, L"X");
    DEBUG_LOG(1, LOG_LINE_NORMAL,
              L"Locate/Create %s for Meridian-specific Items ... In Installation Folder:- '%r'",
              NVRAM_EMULATED, Status);
#endif

    if (!EFI_ERROR(Status)) {
        return Status;
    }

    Status = MrdFindESP(&EspRootDir);
    if (!EFI_ERROR(Status)) {
        Status = EspRootDir->Open(EspRootDir, &gVarsDir, VarsFolder, MeridianReadWriteCreate,
                                  EFI_FILE_DIRECTORY);
    }

#if MERIDIAN_DEBUG > 0
    DEBUG_LOG(1, LOG_LINE_NORMAL,
              L"Locate/Create %s for Meridian-specific Items ... In First Available ESP:- '%r'",
              NVRAM_EMULATED, Status);

    if (EFI_ERROR(Status)) {
        DEBUG_LOG(1, LOG_THREE_STAR_MID, L"Could *NOT* Locate/Create %s Store", NVRAM_EMULATED);
    }
#endif

    return Status;
}

static VOID EfivarSetResult(EFI_STATUS Status, VOID *TmpBuffer, UINTN BufferSize,
                            VOID **VariableData, UINTN *VariableSize)
{
    if (!EFI_ERROR(Status)) {
        *VariableData = TmpBuffer;

        if (VariableSize != NULL) {
            *VariableSize = (BufferSize != 0) ? BufferSize : 0;
        }
    }
    else {
        if (VariableSize != NULL) {
            *VariableSize = 0;
        }
        *VariableData = NULL;
        MRD_FREE_POOL(TmpBuffer);
    }
}

EFI_STATUS EfivarGetRaw(IN EFI_GUID *VendorGUID, IN CHAR16 *VariableName, OUT VOID **VariableData,
                        OUT UINTN *VariableSize OPTIONAL)
{
#if MERIDIAN_DEBUG > 0
    CHAR16 *MsgStr;
    BOOLEAN HybridLogger = FALSE;
#endif

    EFI_STATUS Status;
    UINTN BufferSize;
    VOID *TmpBuffer;
    BOOLEAN TypeMeridian;

#if MERIDIAN_DEBUG > 0
    MRD_HYBRIDLOGGER_SET;
#endif

    BufferSize = 0;
    TmpBuffer = NULL;
    TypeMeridian = GuidsAreEqual(VendorGUID, &MeridianGuid);

    if (TypeMeridian) {

        Status = FindVarsDir();
        if (!EFI_ERROR(Status)) {
            Status = MrdLoadFile(gVarsDir, VariableName, (UINT8 **)&TmpBuffer, &BufferSize);
        }

        EfivarSetResult(Status, TmpBuffer, BufferSize, VariableData, VariableSize);

#if MERIDIAN_DEBUG > 0
        if (EFI_ERROR(Status) && Status != EFI_NOT_FOUND) {
            MsgStr = PoolPrint(L"%s %s:- '%r ... %s'", NVRAM_LOG_GET, NVRAM_EMULATED, Status,
                               VariableName);
            DEBUG_LOG(1, LOG_THREE_STAR_MID, L"%s", MsgStr);
            INFO_LOG("\n");
            INFO_LOG("** WARN: %s", MsgStr);
            INFO_LOG("\n");
            MRD_FREE_POOL(MsgStr);
        }
#endif
    }
    else {

        Status = gRT->GetVariable(VariableName, VendorGUID, NULL, &BufferSize, TmpBuffer);

        if (Status != EFI_BUFFER_TOO_SMALL) {
            Status = EFI_NOT_FOUND;
        }
        else {
            TmpBuffer = AllocateZeroPool(BufferSize);
            if (TmpBuffer == NULL) {
                Status = EFI_OUT_OF_RESOURCES;
            }
            else {

                Status = gRT->GetVariable(VariableName, VendorGUID, NULL, &BufferSize, TmpBuffer);
            }
        }

#if MERIDIAN_DEBUG > 0
        DEBUG_LOG(1, LOG_THREE_STAR_MID, L"%s %s:- '%r ... %s'", NVRAM_LOG_GET, NVRAM_HARDWARE,
                  Status, VariableName);
#endif

        EfivarSetResult(Status, TmpBuffer, BufferSize, VariableData, VariableSize);
    }

#if MERIDIAN_DEBUG > 0
    MRD_HYBRIDLOGGER_OFF;
#endif

    return Status;
}

EFI_STATUS EfivarSetRaw(IN EFI_GUID *VendorGUID, IN CHAR16 *VariableName, IN VOID *VariableData,
                        IN UINTN VariableSize, IN BOOLEAN Persistent)
{
#if MERIDIAN_DEBUG > 0
    BOOLEAN CheckMute = FALSE;
    BOOLEAN HybridLogger = FALSE;
#endif

    EFI_STATUS Status;
    UINT32 OurAccessFlag;
    VOID *OldBuf;
    UINTN OldSize;
    BOOLEAN SettingMatch;

#if MERIDIAN_DEBUG > 0
    MRD_HYBRIDLOGGER_SET;
#endif

    if (VariableSize > 0 && VariableData != NULL) {
#if MERIDIAN_DEBUG > 0
        MRD_MUTELOGGER_SET;
#endif
        Status = EfivarGetRaw(VendorGUID, VariableName, &OldBuf, &OldSize);
#if MERIDIAN_DEBUG > 0
        MRD_MUTELOGGER_OFF;
#endif

        if (!EFI_ERROR(Status)) {

            SettingMatch =
                (VariableSize == OldSize && CompareMem(VariableData, OldBuf, VariableSize) == 0);

            if (SettingMatch) {

#if MERIDIAN_DEBUG > 0
                MRD_HYBRIDLOGGER_OFF;
#endif

                MRD_FREE_POOL(OldBuf);

                return EFI_ALREADY_STARTED;
            }
        }
        MRD_FREE_POOL(OldBuf);
    }

    if (GuidsAreEqual(VendorGUID, &MeridianGuid)) {

        Status = FindVarsDir();
        if (!EFI_ERROR(Status)) {

            MrdSaveFile(gVarsDir, VariableName, NULL, 0);

            Status = MrdSaveFile(gVarsDir, VariableName, (UINT8 *)VariableData, VariableSize);
        }

#if MERIDIAN_DEBUG > 0
        if (VariableData != NULL && VariableSize != 0 && StrLen(VariableData) > 0) {
            DEBUG_LOG(1, LOG_THREE_STAR_MID, L"%s %s:- '%r ... %s'", NVRAM_LOG_SET, NVRAM_EMULATED,
                      Status, VariableName);

            if (EFI_ERROR(Status)) {
                INFO_LOG("** WARN: Could *NOT* Save to %s:- '%s'", NVRAM_EMULATED, VariableName);
                INFO_LOG("\n\n");
            }
        }
#endif
    }
    else {

        OurAccessFlag = EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS;
        if (Persistent) {
            OurAccessFlag |= EFI_VARIABLE_NON_VOLATILE;
        }
        Status =
            gRT->SetVariable(VariableName, VendorGUID, OurAccessFlag, VariableSize, VariableData);

#if MERIDIAN_DEBUG > 0
        if (VariableData != NULL && VariableSize != 0 && StrLen(VariableData) > 0) {
            DEBUG_LOG(1, LOG_THREE_STAR_MID, L"%s %s:- '%r ... %s'%s", NVRAM_LOG_SET,
                      NVRAM_HARDWARE, Status, VariableName, (!Persistent) ? L" ::: Volatile" : L"");
        }
#endif
    }

#if MERIDIAN_DEBUG > 0
    MRD_HYBRIDLOGGER_OFF;
#endif

    return Status;
}

VOID AddListElement(IN OUT VOID ***ListPtr, IN OUT UINTN *ElementCount, IN VOID *NewElement)
{
    VOID *TmpListPtr;
    const UINTN AllocatePointer = sizeof(VOID *) * (*ElementCount + 16);
    const UINTN ElementPointer = sizeof(VOID *) * (*ElementCount);
    BOOLEAN Abort;

    Abort = FALSE;
    if (*ListPtr == NULL) {
        TmpListPtr = AllocatePool(AllocatePointer);
        if (TmpListPtr == NULL) {
            Abort = TRUE;
        }
        else {
            *ListPtr = TmpListPtr;
        }
    }
    else if ((*ElementCount & 15) == 0) {
        if (*ElementCount == 0) {
            MRD_SOFT_FREE(*ListPtr);

            TmpListPtr = AllocatePool(AllocatePointer);
        }
        else {
            TmpListPtr = EfiReallocatePool(*ListPtr, ElementPointer, AllocatePointer);
        }

        if (TmpListPtr == NULL) {
            Abort = TRUE;
        }
        else {
            *ListPtr = TmpListPtr;
        }
    }

    if (!Abort) {
        (*ListPtr)[*ElementCount] = NewElement;
        (*ElementCount)++;
    }
}

VOID FreeList(IN OUT VOID ***ListPtr, IN OUT UINTN *ElementCount)
{
    UINTN i;

    if ((*ElementCount > 0) && (**ListPtr != NULL)) {
        for (i = 0; i < *ElementCount; i++) {

            MRD_FREE_POOL((*ListPtr)[i]);
        }
        MRD_FREE_POOL(*ListPtr);
    }
}

BOOLEAN FindVolume(IN MERIDIAN_VOLUME **Volume, IN CHAR16 *Identifier)
{
    UINTN i;
    BOOLEAN Found;

    if (Identifier == NULL) {
        return FALSE;
    }

    i = 0;
    Found = FALSE;
    while (!Found && VolumesCount > i) {
        Found = VolumeMatchesDescription(Volumes[i], Identifier);
        if (Found) {
            *Volume = Volumes[i];
        }
        i++;
    }

    return Found;
}

BOOLEAN VolumeMatchesDescription(IN MERIDIAN_VOLUME *Volume, IN CHAR16 *Description)
{
    CHAR16 *FilteredDescription;
    EFI_GUID TargetVolGuid = NULL_GUID_VALUE;

    if (Volume == NULL || Description == NULL) {
        return FALSE;
    }

    FilteredDescription = GetSubStrAfter(DEFAULT_STRING_DELIM, Description);

    if (!IsGuid(FilteredDescription)) {
        return (MrdStrEqualsCI(FilteredDescription, Volume->VolName) ||
                MrdStrEqualsCI(FilteredDescription, Volume->FsName) ||
                MrdStrEqualsCI(FilteredDescription, Volume->PartName));
    }

    TargetVolGuid = StringAsGuid(FilteredDescription);
    FilteredDescription = NULL;

    return GuidsAreEqual(&TargetVolGuid, &(Volume->PartGuid));
}

BOOLEAN FilenameIn(IN MERIDIAN_VOLUME *Volume, IN CHAR16 *Directory, IN CHAR16 *Filename,
                   IN CHAR16 *List)
{
    UINTN i;
    CHAR16 *AnElement;
    CHAR16 *OneElement;
    CHAR16 *TargetPath;
    CHAR16 *TargetVolName;
    CHAR16 *TargetFilename;
    BOOLEAN Found;

    if (Filename == NULL || List == NULL) {
        return FALSE;
    }

    TargetPath = TargetVolName = TargetFilename = NULL;

    i = 0;
    Found = FALSE;
    while (!Found) {
        OneElement = FindCommaDelimited(List, i++);
        if (OneElement == NULL)
            break;

        AnElement = GetSubStrAfter(DEFAULT_STRING_DELIM, OneElement);
        SplitPathName(AnElement, &TargetVolName, &TargetPath, &TargetFilename);
        MRD_FREE_POOL(OneElement);

        if (TargetPath == NULL && TargetVolName == NULL && TargetFilename == NULL) {
            continue;
        }

        if (MrdStrEqualsCI(TargetPath, Directory) && MrdStrEqualsCI(TargetFilename, Filename) &&
            VolumeMatchesDescription(Volume, TargetVolName)) {
            Found = TRUE;
        }

        MRD_FREE_POOL(TargetPath);
        MRD_FREE_POOL(TargetVolName);
        MRD_FREE_POOL(TargetFilename);
    }

    return Found;
}

BOOLEAN EjectMedia(VOID)
{
    EFI_STATUS Status;
    UINTN HandleIndex;
    EFI_GUID AppleRemovableMediaGuid = APPLE_REMOVABLE_MEDIA_PROTOCOL_GUID;
    UINTN HandleCount;
    UINTN Ejected;
    EFI_HANDLE *Handles;
    EFI_HANDLE Handle;
    APPLE_REMOVABLE_MEDIA_PROTOCOL *Ejectable;

    HandleCount = 0;
    Status = LibLocateHandle(ByProtocol, &AppleRemovableMediaGuid, NULL, &HandleCount, &Handles);
    if (EFI_ERROR(Status) || HandleCount == 0) {

        return FALSE;
    }

    Ejected = 0;
    for (HandleIndex = 0; HandleIndex < HandleCount; HandleIndex++) {
        Handle = Handles[HandleIndex];
        Status = gBS->HandleProtocol(Handle, &AppleRemovableMediaGuid, (VOID **)&Ejectable);
        if (EFI_ERROR(Status)) {
            continue;
        }

        Status = Ejectable->Eject(Ejectable);
        if (!EFI_ERROR(Status)) {
            Ejected++;
        }
    }
    MRD_FREE_POOL(Handles);

    return (Ejected > 0);
}

BOOLEAN GuidsAreEqual(IN EFI_GUID *Guid1, IN EFI_GUID *Guid2)
{
    return (CompareMem(Guid1, Guid2, 16) == 0);
}

VOID EraseUint32List(IN UINT32_LIST **TheList)
{
    UINT32_LIST *NextItem;

    while (*TheList != NULL) {
        NextItem = (*TheList)->Next;
        MRD_FREE_POOL(*TheList);
        *TheList = NextItem;
    }
}
