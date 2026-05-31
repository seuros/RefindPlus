// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2020-2026 Dayo Akanji
// SPDX-FileCopyrightText: 2012-2023 Roderick W. Smith

#include "screenmgt.h"
#include "install.h"
#include "global.h"
#include "lib.h"
#include "scan.h"
#include "menu.h"
#include "mystrings.h"
#include "launch_efi.h"
#include "Handle.h"

typedef struct _esp_list
{
    MERIDIAN_VOLUME *Volume;
    struct _esp_list *NextESP;
} ESP_LIST;

static VOID DeleteESPList(ESP_LIST *AllESPs)
{
    ESP_LIST *Temp;

#if MERIDIAN_DEBUG > 0
    DEBUG_LOG(1, LOG_LINE_NORMAL, L"Delete List of ESPs");
#endif

    while (AllESPs != NULL) {
        Temp = AllESPs;
        AllESPs = AllESPs->NextESP;
        MRD_FREE_POOL(Temp);
    }
}

static ESP_LIST *FindAllESPs(VOID)
{
    ESP_LIST *AllESPs;
    ESP_LIST *NewESP;
    UINTN VolumeIndex;

#if MERIDIAN_DEBUG > 0
    DEBUG_LOG(1, LOG_LINE_NORMAL, L"Search for ESPs");
#endif

    AllESPs = NULL;
    for (VolumeIndex = 0; VolumeIndex < VolumesCount; VolumeIndex++) {
        if (Volumes[VolumeIndex]->DiskKind == DISK_KIND_INTERNAL &&
            GuidsAreEqual(&(Volumes[VolumeIndex]->PartTypeGuid), &GuidESP) &&
            !GuidsAreEqual(&(Volumes[VolumeIndex]->PartGuid), &SelfVolume->PartGuid)) {
            NewESP = AllocateZeroPool(sizeof(ESP_LIST));
            if (NewESP == NULL) {
                DeleteESPList(AllESPs);
                break;
            }

            NewESP->Volume = Volumes[VolumeIndex];
            NewESP->NextESP = AllESPs;
            AllESPs = NewESP;
        }
    }

    return AllESPs;
}

static MERIDIAN_VOLUME *PickOneESP(ESP_LIST *AllESPs)
{
    UINTN i;
    CHAR16 *FsName;
    CHAR16 *VolName;
    CHAR16 *GuidStr;
    CHAR16 *PartName;
    ESP_LIST *CurrentESP;
    MERIDIAN_VOLUME *ChosenVolume;
    MERIDIAN_MENU_ENTRY *MenuEntryItem;
    MERIDIAN_MENU_SCREEN *InstallMenu;

    INTN DefaultEntry;
    UINTN MenuExit;
    MENU_STYLE_FUNC Style;
    MERIDIAN_MENU_ENTRY *ChosenOption;

#if MERIDIAN_DEBUG > 0
    DEBUG_LOG(1, LOG_LINE_NORMAL, L"Prompt User to Select an ESP for Installation");
#endif

    if (AllESPs == NULL) {
        DisplaySimpleMessage(L"No Eligible ESPs Found", NULL);

#if MERIDIAN_DEBUG > 0
        DEBUG_LOG(1, LOG_LINE_NORMAL, L"No Eligible ESPs Found");
#endif

        return NULL;
    }

    InstallMenu = AllocateZeroPool(sizeof(MERIDIAN_MENU_SCREEN));
    if (InstallMenu == NULL) {

        return NULL;
    }

    InstallMenu->Title = StrDuplicate(L"Install Meridian");
    InstallMenu->Hint1 = StrDuplicate(L"Select a destination and press 'Enter' or");
    InstallMenu->Hint2 = StrDuplicate(RETURN_MAIN_SCREEN_HINT);

    AddMenuInfoLine(InstallMenu, L"Select a Partition and Press 'Enter' to Install Meridian",
                    FALSE);

    i = 1;
    CurrentESP = AllESPs;
    while (CurrentESP != NULL) {
        MenuEntryItem = AllocateZeroPool(sizeof(MERIDIAN_MENU_ENTRY));
        if (MenuEntryItem == NULL) {
            FreeMenuScreen(&InstallMenu);

            return NULL;
        }

        GuidStr = GuidAsString(&(CurrentESP->Volume->PartGuid));
        PartName = CurrentESP->Volume->PartName;
        FsName = CurrentESP->Volume->FsName;
        VolName = CurrentESP->Volume->VolName;

        if (PartName != NULL && (StrLen(PartName) > 0) && FsName != NULL && (StrLen(FsName) > 0) &&
            !MrdStrEqualsCI(FsName, PartName)) {
            MenuEntryItem->Title = PoolPrint(L"%s - '%s', AKA '%s'", GuidStr, PartName, FsName);
        }
        else if (FsName != NULL && (StrLen(FsName) > 0)) {
            MenuEntryItem->Title = PoolPrint(L"%s - '%s'", GuidStr, FsName);
        }
        else if (PartName != NULL && (StrLen(PartName) > 0)) {
            MenuEntryItem->Title = PoolPrint(L"%s - '%s'", GuidStr, PartName);
        }
        else if (VolName != NULL && (StrLen(VolName) > 0)) {
            MenuEntryItem->Title = PoolPrint(L"%s - '%s'", GuidStr, VolName);
        }
        else {
            MenuEntryItem->Title = PoolPrint(L"%s - No Name", GuidStr);
        }

#if MERIDIAN_DEBUG > 0
        DEBUG_LOG(1, LOG_LINE_NORMAL, L"Append '%s' to UI List of ESPs");
#endif

        MenuEntryItem->Tag = TAG_RETURN;
        MenuEntryItem->Row = i++;

        AddMenuEntry(InstallMenu, MenuEntryItem);

        CurrentESP = CurrentESP->NextESP;

        MRD_FREE_POOL(GuidStr);
    }

    do {
        ChosenVolume = NULL;
        if (!GetMenuEntryReturn(&InstallMenu)) {
            break;
        }

        DefaultEntry = 9999;
        Style = NULL;
        MenuExit = DrawMenuScreen(InstallMenu, Style, &DefaultEntry, &ChosenOption);

#if MERIDIAN_DEBUG > 0
        LogExit(MenuExit, __func__, ChosenOption->Title);
#endif

        if (ChosenOption->Tag == TAG_RETURN) {
            break;
        }

        if (MenuExit == MENU_EXIT_ENTER) {
            CHAR16 *Temp;
            CurrentESP = AllESPs;
            while (CurrentESP != NULL) {
                Temp = GuidAsString(&(CurrentESP->Volume->PartGuid));
                if (MrdStrFind(ChosenOption->Title, Temp)) {
                    ChosenVolume = CurrentESP->Volume;
                }
                CurrentESP = CurrentESP->NextESP;
                MRD_FREE_POOL(Temp);
            }
        }
    } while (0);

    FreeMenuScreen(&InstallMenu);

    return ChosenVolume;
}

static EFI_STATUS CreateDirectories(EFI_FILE_PROTOCOL *BaseDir)
{
    EFI_STATUS Status;
    UINTN i;
    CHAR16 *FileName;
    EFI_FILE_PROTOCOL *TheDir;

    i = 0;
    TheDir = NULL;
    FileName = NULL;
    Status = EFI_SUCCESS;
    while (1) {
        FileName = FindCommaDelimited(INST_DIRECTORIES, i++);
        if (FileName == NULL)
            break;

        BaseDir->Open(BaseDir, &TheDir, FileName, MeridianReadWriteCreate, EFI_FILE_DIRECTORY);

        Status = TheDir->Close(TheDir);
        MRD_FREE_POOL(FileName);
        MRD_FREE_POOL(TheDir);
        if (EFI_ERROR(Status))
            break;
    }

    return Status;
}

static EFI_STATUS CopyOneFile(EFI_FILE_PROTOCOL *SourceDir, CHAR16 *SourceName,
                              EFI_FILE_PROTOCOL *DestDir, CHAR16 *DestName)
{
    EFI_STATUS Status;
    UINTN *Buffer;
    UINTN FileSize;
    EFI_FILE_INFO *FileInfo;
    EFI_FILE_PROTOCOL *SourceFile;
    EFI_FILE_PROTOCOL *DestFile;

    SourceFile = NULL;
    Status = SourceDir->Open(SourceDir, &SourceFile, SourceName, MeridianReadOnly, 0);
    if (EFI_ERROR(Status)) {
#if MERIDIAN_DEBUG > 0
        DEBUG_LOG(1, LOG_LINE_NORMAL, L"Error:- '%r' When Opening SourceDir in 'CopyOneFile'",
                  Status);
#endif

        return Status;
    }

    FileInfo = LibFileInfo(SourceFile);
    if (FileInfo == NULL) {
        SourceFile->Close(SourceFile);

        return EFI_NO_RESPONSE;
    }

    FileSize = FileInfo->FileSize;
    MRD_FREE_POOL(FileInfo);

    Buffer = AllocateZeroPool(FileSize);
    if (Buffer == NULL) {

        return EFI_OUT_OF_RESOURCES;
    }

    Status = SourceFile->Read(SourceFile, &FileSize, Buffer);
    if (EFI_ERROR(Status)) {
#if MERIDIAN_DEBUG > 0
        DEBUG_LOG(1, LOG_LINE_NORMAL, L"Error:- '%r' When Reading SourceFile in 'CopyOneFile'",
                  Status);
#endif

        return Status;
    }

    SourceFile->Close(SourceFile);

    DestFile = NULL;
    Status = DestDir->Open(DestDir, &DestFile, DestName, MeridianReadWriteCreate, 0);
    if (EFI_ERROR(Status)) {
#if MERIDIAN_DEBUG > 0
        DEBUG_LOG(1, LOG_LINE_NORMAL, L"Error:- '%r' When Opening DestDir in 'CopyOneFile'",
                  Status);
#endif

        return Status;
    }

    Status = DestFile->Write(DestFile, &FileSize, Buffer);
    if (EFI_ERROR(Status)) {
#if MERIDIAN_DEBUG > 0
        DEBUG_LOG(1, LOG_LINE_NORMAL, L"Error:- '%r' When Writing to DestDir in 'CopyOneFile'",
                  Status);
#endif

        return Status;
    }

    Status = DestFile->Close(DestFile);

    MRD_FREE_POOL(SourceFile);
    MRD_FREE_POOL(DestFile);
    MRD_FREE_POOL(Buffer);

#if MERIDIAN_DEBUG > 0
    if (EFI_ERROR(Status)) {
        DEBUG_LOG(1, LOG_LINE_NORMAL, L"Error:- '%r' When Closing DestDir in 'CopyOneFile'",
                  Status);
    }
#endif

    return Status;
}

static EFI_STATUS CopyDrivers(EFI_FILE_PROTOCOL *SourceDirPtr, CHAR16 *SourceDirName,
                              EFI_FILE_PROTOCOL *DestDirPtr, CHAR16 *DestDirName)
{
    EFI_STATUS Status;
    EFI_STATUS WorstStatus;
    UINTN i;
    CHAR16 *DestFileName;
    CHAR16 *SourceFileName;
    CHAR16 *DriverName;
    BOOLEAN DriverCopied[NUM_FS_TYPES];

#if MERIDIAN_DEBUG > 0
    DEBUG_LOG(1, LOG_LINE_NORMAL, L"Scan %d Volumes for Identifiable Filesystems", VolumesCount);
#endif

    DriverCopied[FS_TYPE_UNKNOWN] = FALSE;
    DriverCopied[FS_TYPE_WHOLEDISK] = FALSE;
    DriverCopied[FS_TYPE_FAT12] = FALSE;
    DriverCopied[FS_TYPE_FAT16] = FALSE;
    DriverCopied[FS_TYPE_FAT32] = FALSE;
    DriverCopied[FS_TYPE_EXFAT] = FALSE;
    DriverCopied[FS_TYPE_NTFS] = FALSE;
    DriverCopied[FS_TYPE_EXT2] = FALSE;
    DriverCopied[FS_TYPE_EXT3] = FALSE;
    DriverCopied[FS_TYPE_EXT4] = FALSE;
    DriverCopied[FS_TYPE_HFSPLUS] = FALSE;
    DriverCopied[FS_TYPE_BTRFS] = FALSE;
    DriverCopied[FS_TYPE_XFS] = FALSE;
    DriverCopied[FS_TYPE_JFS] = FALSE;
    DriverCopied[FS_TYPE_ISO9660] = FALSE;
    DriverCopied[FS_TYPE_APFS] = FALSE;
    DriverCopied[FS_TYPE_UFS] = FALSE;
    DriverCopied[FS_TYPE_BCACHEFS] = FALSE;

    WorstStatus = EFI_SUCCESS;

    for (i = 0; i < VolumesCount; i++) {
#if MERIDIAN_DEBUG > 0
        DEBUG_LOG(1, LOG_LINE_NORMAL, L"Look for Driver for Volume %02d:- '%s'", i,
                  Volumes[i]->VolName);
#endif

        DriverName = NULL;
        switch (Volumes[i]->FSType) {
        case FS_TYPE_BTRFS:
            if (!DriverCopied[FS_TYPE_BTRFS]) {
                DriverName = L"btrfs";
                DriverCopied[FS_TYPE_BTRFS] = TRUE;
            }

            break;
        case FS_TYPE_BCACHEFS:
            if (!DriverCopied[FS_TYPE_BCACHEFS]) {
                DriverName = L"bcachefs";
                DriverCopied[FS_TYPE_BCACHEFS] = TRUE;
            }

            break;
        case FS_TYPE_EXT2:
            if (!DriverCopied[FS_TYPE_EXT2]) {
                DriverName = L"ext2";
                DriverCopied[FS_TYPE_EXT2] = TRUE;
                DriverCopied[FS_TYPE_EXT3] = TRUE;
            }

            break;
        case FS_TYPE_EXT3:
            if (!DriverCopied[FS_TYPE_EXT3]) {
                DriverName = L"ext2";
                DriverCopied[FS_TYPE_EXT2] = TRUE;
                DriverCopied[FS_TYPE_EXT3] = TRUE;
            }

            break;
        case FS_TYPE_EXT4:
            if (!DriverCopied[FS_TYPE_EXT4]) {
                DriverName = L"ext4";
                DriverCopied[FS_TYPE_EXT4] = TRUE;
            }

            break;
        case FS_TYPE_HFSPLUS:
            if (!DriverCopied[FS_TYPE_HFSPLUS] && (!AppleFirmware)) {
                DriverName = L"hfs";
                DriverCopied[FS_TYPE_HFSPLUS] = TRUE;
            }

            break;
        case FS_TYPE_UFS:
            if (!DriverCopied[FS_TYPE_UFS]) {
                DriverName = L"ufs";
                DriverCopied[FS_TYPE_UFS] = TRUE;
            }

            break;
        default:
            DriverName = NULL;
        }

        if (DriverName != NULL) {
            SourceFileName =
                PoolPrint(L"%s\\%s%s", SourceDirName, DriverName, INST_PLATFORM_EXTENSION);
            DestFileName = PoolPrint(L"%s\\%s%s", DestDirName, DriverName, INST_PLATFORM_EXTENSION);

#if MERIDIAN_DEBUG > 0
            DEBUG_LOG(1, LOG_LINE_NORMAL, L"Copy Driver for %s", DriverName);
#endif

            Status = CopyOneFile(SourceDirPtr, SourceFileName, DestDirPtr, DestFileName);
            if (EFI_ERROR(Status)) {
                WorstStatus = Status;
            }

            MRD_FREE_POOL(SourceFileName);
            MRD_FREE_POOL(DestFileName);
        }
    }

    return WorstStatus;
}

static EFI_STATUS CopyFiles(EFI_FILE_PROTOCOL *TargetDir)
{
    EFI_STATUS Status;
    EFI_STATUS WorstStatus;
    CHAR16 *SourceFile;
    CHAR16 *SourceDir;
    CHAR16 *MeridianName;
    CHAR16 *TargetDriversDir;
    CHAR16 *SourceDriversDir;
    MERIDIAN_VOLUME *SourceVolume;

    SourceFile = NULL;
    SourceVolume = NULL;

    FindVolumeAndFilename(GlobalConfig.SelfDevicePath, &SourceVolume, &SourceFile);
    SourceDir = FindPath(SourceFile);

    MeridianName = PoolPrint(L"EFI\\Meridian\\%s", INST_MERIDIAN_NAME);
    Status = WorstStatus = CopyOneFile(SourceVolume->RootDir, SourceFile, TargetDir, MeridianName);
    MRD_FREE_POOL(SourceFile);
    MRD_FREE_POOL(MeridianName);
    if (EFI_ERROR(Status)) {
#if MERIDIAN_DEBUG > 0
        DEBUG_LOG(1, LOG_LINE_NORMAL,
                  L"Error When Copying Meridian Binary ... Installation has Failed!!");
#endif

        MRD_FREE_POOL(SourceDir);

        return EFI_ABORTED;
    }

    SourceFile = PoolPrint(L"%s\\config.conf", SourceDir);
    if (FileExists(SourceVolume->RootDir, SourceFile) &&
        !FileExists(TargetDir, L"\\EFI\\Meridian\\config.conf")) {

        Status = CopyOneFile(SourceVolume->RootDir, SourceFile, TargetDir,
                             L"EFI\\Meridian\\config.conf");
        if (EFI_ERROR(Status)) {
#if MERIDIAN_DEBUG > 0
            DEBUG_LOG(1, LOG_LINE_NORMAL, L"Error When Copying Config File:- '%d'", Status);
#endif

            WorstStatus = Status;
        }
    }
    MRD_FREE_POOL(SourceFile);

    SourceDriversDir = PoolPrint(L"%s\\%s", SourceDir, INST_DRIVERS_SUBDIR);
    TargetDriversDir = PoolPrint(L"EFI\\Meridian\\%s", INST_DRIVERS_SUBDIR);

    Status = CopyDrivers(SourceVolume->RootDir, SourceDriversDir, TargetDir, TargetDriversDir);

    if (EFI_ERROR(Status)) {
#if MERIDIAN_DEBUG > 0
        DEBUG_LOG(1, LOG_LINE_NORMAL, L"Error When Copying Drivers:- '%d'", Status);
#endif

        WorstStatus = Status;
    }

    MRD_FREE_POOL(SourceDir);
    MRD_FREE_POOL(SourceDriversDir);
    MRD_FREE_POOL(TargetDriversDir);

    return WorstStatus;
}

static VOID CreateFallbackCSV(EFI_FILE_PROTOCOL *TargetDir)
{
    EFI_STATUS Status;
    UINTN FileSize;
    CHAR16 *Contents;
    EFI_FILE_PROTOCOL *FilePtr;

    Status = TargetDir->Open(TargetDir, &FilePtr, L"\\EFI\\Meridian\\BOOT.CSV",
                             MeridianReadWriteCreate, 0);

    if (!EFI_ERROR(Status)) {
        Contents = PoolPrint(L"%s, Meridian Boot Manager,,This is the Boot Entry for Meridian\n",
                             INST_MERIDIAN_NAME);

        if (Contents != NULL) {
            FileSize = StrSize(Contents);
            Status = FilePtr->Write(FilePtr, &FileSize, Contents);

            if (!EFI_ERROR(Status)) {
                FilePtr->Close(FilePtr);
            }
            MRD_FREE_POOL(FilePtr);
        }

        MRD_FREE_POOL(Contents);
    }

#if MERIDIAN_DEBUG > 0
    if (EFI_ERROR(Status)) {
        DEBUG_LOG(1, LOG_LINE_NORMAL, L"Error When Writing 'BOOT.CSV' File:- '%r'", Status);
    }
#endif
}

static BOOLEAN CopyMeridianFiles(EFI_FILE_PROTOCOL *TargetDir)
{
    EFI_STATUS Status;
    EFI_STATUS Status2;

    Status = EFI_SUCCESS;

    if (!EFI_ERROR(Status)) {
        Status = CreateDirectories(TargetDir);

#if MERIDIAN_DEBUG > 0
        if (EFI_ERROR(Status)) {
            DEBUG_LOG(1, LOG_LINE_NORMAL, L"Error When Creating Target Directory");
        }
#endif
    }
    if (!EFI_ERROR(Status)) {

        Status2 = CopyFiles(TargetDir);
        if (EFI_ERROR(Status2)) {
            if (Status2 == EFI_ABORTED) {
                Status = EFI_ABORTED;
            }
            else {
                DisplaySimpleMessage(L"Warning", L"Error When Copying Some Files");
            }
        }
    }
    CreateFallbackCSV(TargetDir);

    return Status;
}

UINTN FindBootNum(EFI_DEVICE_PATH_PROTOCOL *Entry, UINTN Size, BOOLEAN *AlreadyExists)
{
    EFI_STATUS Status;
    UINTN Index;
    UINTN VarSize;
    CHAR16 *VarName;
    CHAR16 *Contents;

    *AlreadyExists = FALSE;
    Contents = NULL;

    Index = 80;
    do {
        if (Index > 89) {
            break;
        }

        VarName = PoolPrint(L"Boot%04x", Index++);
        Status = EfivarGetRaw(&GlobalGuid, VarName, (VOID **)&Contents, &VarSize);
        if (!EFI_ERROR(Status) && VarSize == Size && CompareMem(Contents, Entry, VarSize) == 0) {
            *AlreadyExists = TRUE;
        }

        MRD_FREE_POOL(VarName);
    } while (!(*AlreadyExists));

    if (*AlreadyExists == TRUE) {
        return (Index - 1);
    }

    Index = 0;
    do {
        VarName = PoolPrint(L"Boot%04x", Index++);
        Status = EfivarGetRaw(&GlobalGuid, VarName, (VOID **)&Contents, &VarSize);
        if (!EFI_ERROR(Status) && VarSize == Size && CompareMem(Contents, Entry, VarSize) == 0) {
            *AlreadyExists = TRUE;
        }

        MRD_FREE_POOL(VarName);
    } while (!EFI_ERROR(Status) && !(*AlreadyExists));

    if (Index > 0x10000) {

        Index = 0x10000;
    }

    return (Index - 1);
}

static EFI_STATUS SetBootDefault(UINTN BootNum)
{
    EFI_STATUS Status;
    UINTN i, j;
    UINTN VarSize;
    UINTN ListSize;
    UINT16 *BootOrder;
    UINT16 *NewBootOrder;
    BOOLEAN IsAlreadyFirst;

    Status = EfivarGetRaw(&GlobalGuid, L"BootOrder", (VOID **)&BootOrder, &VarSize);
    if (!EFI_ERROR(Status)) {
        IsAlreadyFirst = FALSE;
        ListSize = VarSize / sizeof(UINT16);
        for (i = 0; i < ListSize; i++) {
            if (BootOrder[i] == BootNum) {
                if (i == 0) {
                    IsAlreadyFirst = TRUE;
                }
            }
        }

        if (!IsAlreadyFirst) {
            NewBootOrder = AllocateZeroPool((ListSize + 1) * sizeof(UINT16));
            if (NewBootOrder != NULL) {
                NewBootOrder[0] = BootNum;

                j = 1;
                for (i = 0; i < ListSize; i++) {
                    if (BootOrder[i] != BootNum) {
                        NewBootOrder[j++] = BootOrder[i];
                    }
                }

                Status =
                    EfivarSetRaw(&GlobalGuid, L"BootOrder", NewBootOrder, j * sizeof(UINT16), TRUE);

                MRD_FREE_POOL(NewBootOrder);
            }
        }
        MRD_FREE_POOL(BootOrder);
    }

    return Status;
}

static EFI_STATUS CreateNvramEntry(IN EFI_HANDLE DeviceHandle, IN CHAR16 *ProgLabel,
                                   IN CHAR16 *LoaderPath, IN BOOLEAN MakeDefault)
{
    EFI_STATUS Status;
    CHAR16 *VarName;
    UINTN BootNum;
    UINTN EntrySize;
    UINT16 EfiBootNum;
    BOOLEAN AlreadyExists;
    EFI_DEVICE_PATH_PROTOCOL *DevicePath;

    Status =
        ConstructBootEntry(DeviceHandle, LoaderPath, ProgLabel, (CHAR8 **)&DevicePath, &EntrySize);

    if (EFI_ERROR(Status)) {
        BootNum = 0;
        AlreadyExists = TRUE;
    }
    else {
        AlreadyExists = FALSE;
        BootNum = FindBootNum(DevicePath, EntrySize, &AlreadyExists);
    }

    if (!EFI_ERROR(Status) && !AlreadyExists) {
        VarName = PoolPrint(L"Boot%04x", BootNum);
        Status =
            SetHardwareNvramVariable(VarName, &GlobalGuid, AccessFlagsFull, EntrySize, DevicePath);
        MRD_FREE_POOL(VarName);
    }
    MRD_FREE_POOL(DevicePath);

    if (!EFI_ERROR(Status)) {

        MeridianStall(25);

        if (MakeDefault) {
            Status = SetBootDefault(BootNum);
        }
        else {
            VarName = PoolPrint(L"%d", BootNum);
            EfiBootNum = StrToHex(VarName, 0, 16);
            Status = SetHardwareNvramVariable(L"BootNext", &GlobalGuid, AccessFlagsFull,
                                              sizeof(UINT16), &EfiBootNum);
            MRD_FREE_POOL(VarName);
        }
    }

    return Status;
}

EFI_STATUS ConstructBootEntry(EFI_HANDLE *TargetVolume, CHAR16 *Loader, CHAR16 *Label,
                              CHAR8 **Entry, UINTN *Size)
{
    EFI_STATUS Status;
    UINTN DestSize;
    UINTN DevPathSize;
    CHAR8 *Working;
    EFI_DEVICE_PATH_PROTOCOL *DevicePath;

    DevicePath = FileDevicePath(TargetVolume, Loader);
    DevPathSize = DevicePathSize(DevicePath);

    *Size = sizeof(UINT32) + sizeof(UINT16) + StrSize(Label) + DevPathSize;
    *Entry = Working = AllocateZeroPool(*Size);

    if (DevicePath == NULL || *Entry == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
    }
    else {
        Status = EFI_SUCCESS;

        *(UINT32 *)Working = LOAD_OPTION_ACTIVE;
        Working += sizeof(UINT32);

        *(UINT16 *)Working = DevPathSize;
        Working += sizeof(UINT16);

        DestSize = (*Size - ((UINTN)Working - (UINTN)*Entry)) / sizeof(CHAR16);
        StrCpyS((CHAR16 *)Working, DestSize, Label);
        Working += StrSize(Label);

        gBS->CopyMem(Working, DevicePath, DevPathSize);

    }
    MRD_FREE_POOL(DevicePath);

    return Status;
}

VOID InstallMeridian(VOID)
{
    EFI_STATUS Status;
    BOOLEAN BadTag;
    CHAR16 *MsgStr;
    CHAR16 *ProgName;
    ESP_LIST *AllESPs;
    MERIDIAN_VOLUME *SelectedESP;

#if MERIDIAN_DEBUG > 0
    DEBUG_LOG(1, LOG_LINE_NORMAL, L"Install Meridian to an ESP");
#endif

    do {
        AllESPs = FindAllESPs();
        if (AllESPs == NULL) {
            BadTag = TRUE;
        }
        else {
            SelectedESP = PickOneESP(AllESPs);
            BadTag = (SelectedESP == NULL);
        }

        if (BadTag) {
            Status = EFI_NOT_READY;

            break;
        }

        Status = CopyMeridianFiles(SelectedESP->RootDir);
        if (!EFI_ERROR(Status)) {
            ProgName = PoolPrint(L"\\EFI\\Meridian\\%s", INST_MERIDIAN_NAME);
            Status = CreateNvramEntry(SelectedESP->DeviceHandle, L"Meridian Boot Manager", ProgName,
                                      TRUE);

            MRD_FREE_POOL(ProgName);
        }
    } while (0);

    if (EFI_ERROR(Status)) {
        MsgStr = L"Problems Encountered During Installation!!";
        DisplaySimpleMessage(L"Warning", MsgStr);
    }
    else {
        MsgStr = L"Meridian Successfully Installed";
        DisplaySimpleMessage(MsgStr, NULL);
    }

#if MERIDIAN_DEBUG > 0
    DEBUG_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
    if (!EFI_ERROR(Status)) {
        INFO_LOG("%s    * %s", OffsetNext, MsgStr);
    }
    else {
        INFO_LOG("\n\n");
        INFO_LOG("WARN: %s", MsgStr);
    }
    INFO_LOG("\n\n");
#endif

    DeleteESPList(AllESPs);
}

BOOT_ENTRY_LIST *FindBootOrderEntries(VOID)
{
    EFI_STATUS Status;
    UINTN i;
    UINTN VarSize;
    UINTN ListSize;
    UINT16 *BootOrder;
    CHAR16 *VarName;
    CHAR16 *Contents;
    BOOT_ENTRY_LIST *L;
    BOOT_ENTRY_LIST *ListEnd;
    BOOT_ENTRY_LIST *ListStart;

#if MERIDIAN_DEBUG > 0
    DEBUG_LOG(1, LOG_LINE_NORMAL, L"Fetch BootOrder Variables:");
#endif

    BootOrder = NULL;
    Status = EfivarGetRaw(&GlobalGuid, L"BootOrder", (VOID **)&BootOrder, &VarSize);
    if (EFI_ERROR(Status)) {
        return NULL;
    }

    Contents = NULL;
    ListStart = ListEnd = NULL;
    ListSize = VarSize / sizeof(UINT16);

    for (i = 0; i < ListSize; i++) {
        VarName = PoolPrint(L"Boot%04x", BootOrder[i]);
        if (VarName == NULL) {
            break;
        }

        Status = EfivarGetRaw(&GlobalGuid, VarName, (VOID **)&Contents, &VarSize);

        if (!EFI_ERROR(Status)) {
            L = AllocateZeroPool(sizeof(BOOT_ENTRY_LIST));
            if (L != NULL) {
                L->BootEntry.BootNum = BootOrder[i];
                L->BootEntry.Options = (UINT32)Contents[0];
                L->BootEntry.Size = (UINT16)Contents[2];
                L->BootEntry.Label = StrDuplicate((CHAR16 *)&(Contents[3]));
                L->BootEntry.DevPath = AllocatePool(L->BootEntry.Size);
                gBS->CopyMem(L->BootEntry.DevPath,
                             (EFI_DEVICE_PATH *)&Contents[3 + StrSize(L->BootEntry.Label) / 2],
                             L->BootEntry.Size);
                L->NextBootEntry = NULL;

                if (ListStart == NULL) {
                    ListStart = L;
                }
                else {
                    ListEnd->NextBootEntry = L;
                }
                ListEnd = L;
            }
        }

        MRD_FREE_POOL(VarName);
        MRD_FREE_POOL(Contents);
    }

    MRD_FREE_POOL(BootOrder);

    return ListStart;
}

VOID DeleteBootOrderEntries(BOOT_ENTRY_LIST *Entries)
{
    BOOT_ENTRY_LIST *Current;

    while (Entries != NULL) {
        Current = Entries;
        MRD_FREE_POOL(Current->BootEntry.Label);
        MRD_FREE_POOL(Current->BootEntry.DevPath);
        Entries = Entries->NextBootEntry;
        MRD_FREE_POOL(Current);
    }
}

static UINTN ConfirmBootOptionOperation(UINTN Operation, CHAR16 *BootOptionString)
{
    CHAR16 *CheckString;
    BOOLEAN RetVal;
    MERIDIAN_MENU_SCREEN *ConfirmBootOptionMenu;

    INTN DefaultEntry;
    UINTN MenuExit;
    MENU_STYLE_FUNC Style;
    MERIDIAN_MENU_ENTRY *ChosenOption;

    if (Operation != EFI_BOOT_OPTION_DELETE && Operation != EFI_BOOT_OPTION_MAKE_DEFAULT) {

        return EFI_BOOT_OPTION_DO_NOTHING;
    }

#if MERIDIAN_DEBUG > 0
    DEBUG_LOG(1, LOG_LINE_THIN_SEP, L"Prepare Menu Screen");
    DEBUG_LOG(1, LOG_LINE_NORMAL, L"Screen Title:- 'Confirm Boot Option Operation'");
#endif

    ConfirmBootOptionMenu = AllocateZeroPool(sizeof(MERIDIAN_MENU_SCREEN));
    if (ConfirmBootOptionMenu == NULL) {

        return EFI_BOOT_OPTION_DO_NOTHING;
    }

    ConfirmBootOptionMenu->Title = StrDuplicate(L"Confirm Boot Option Operation");
    ConfirmBootOptionMenu->Hint1 = StrDuplicate(SELECT_OPTION_HINT);
    ConfirmBootOptionMenu->Hint2 = StrDuplicate(RETURN_MAIN_SCREEN_HINT);

    AddMenuInfoLine(ConfirmBootOptionMenu, PoolPrint(L"%s", BootOptionString), TRUE);

    CheckString = NULL;
    if (Operation == EFI_BOOT_OPTION_MAKE_DEFAULT) {
        CheckString = L"Set This Boot Option as Default?";
    }
    else {
        if (Operation == EFI_BOOT_OPTION_DELETE) {
            CheckString = L"Delete This Boot Option?";
        }
    }
    AddMenuInfoLine(ConfirmBootOptionMenu, CheckString, FALSE);

    RetVal = GetMenuEntryYesNo(&ConfirmBootOptionMenu);
    if (!RetVal) {
        FreeMenuScreen(&ConfirmBootOptionMenu);

        return EFI_BOOT_OPTION_DO_NOTHING;
    }

    DefaultEntry = 9999;
    Style = NULL;
    MenuExit = DrawMenuScreen(ConfirmBootOptionMenu, Style, &DefaultEntry, &ChosenOption);

#if MERIDIAN_DEBUG > 0
    LogExit(MenuExit, __func__, ChosenOption->Title);
#endif

    if (MenuExit != MENU_EXIT_ENTER || ChosenOption->Tag != TAG_YES) {
        Operation = EFI_BOOT_OPTION_DO_NOTHING;
    }

    FreeMenuScreen(&ConfirmBootOptionMenu);

    return Operation;
}

static UINTN PickOneBootOption(IN BOOT_ENTRY_LIST *Entries, IN OUT UINTN *BootOrderNum)
{
    UINTN Operation;
    CHAR16 *Filename;
    MERIDIAN_VOLUME *Volume;
    MERIDIAN_MENU_ENTRY *MenuEntryItem;
    MERIDIAN_MENU_SCREEN *PickBootOptionMenu;

    INTN DefaultEntry;
    UINTN MenuExit;
    MENU_STYLE_FUNC Style;
    MERIDIAN_MENU_ENTRY *ChosenOption;

    if (Entries == NULL) {
        DisplaySimpleMessage(L"Firmware BootOrder List is Empty", NULL);

        return EFI_BOOT_OPTION_DO_NOTHING;
    }

    PickBootOptionMenu = AllocateZeroPool(sizeof(MERIDIAN_MENU_SCREEN));
    if (PickBootOptionMenu == NULL) {

        return EFI_BOOT_OPTION_DO_NOTHING;
    }

    PickBootOptionMenu->Title = StrDuplicate(L"Manage BootOrder");
    PickBootOptionMenu->Hint1 =
        StrDuplicate(L"Select an option and press 'Enter' to make it the default. Press '-' or");
    PickBootOptionMenu->Hint2 = PoolPrint(L"'Delete' to delete it, or %s", RETURN_MAIN_SCREEN_HINT);

    AddMenuInfoLine(PickBootOptionMenu, L"Promote or Remove Firmware BootOrder Variables", FALSE);

    Volume = NULL;
    Filename = NULL;
    do {
        MenuEntryItem = AllocateZeroPool(sizeof(MERIDIAN_MENU_ENTRY));
        FindVolumeAndFilename(Entries->BootEntry.DevPath, &Volume, &Filename);
        if (Filename != NULL && StrLen(Filename) > 0) {
            if (Volume != NULL && Volume->VolName != NULL) {
                MenuEntryItem->Title =
                    PoolPrint(L"Boot%04x - %s - %s on %s", Entries->BootEntry.BootNum,
                              Entries->BootEntry.Label, Filename, Volume->VolName);
            }
            else {
                MenuEntryItem->Title = PoolPrint(L"Boot%04x - %s - %s", Entries->BootEntry.BootNum,
                                                 Entries->BootEntry.Label, Filename);
            }
        }
        else {
            MenuEntryItem->Title =
                PoolPrint(L"Boot%04x - %s", Entries->BootEntry.BootNum, Entries->BootEntry.Label);
        }

        MenuEntryItem->Row = Entries->BootEntry.BootNum;
        AddMenuEntry(PickBootOptionMenu, MenuEntryItem);

        MRD_SOFT_FREE(Volume);
        MRD_FREE_POOL(Filename);

        Entries = Entries->NextBootEntry;
    } while (Entries != NULL);

    do {
        Operation = EFI_BOOT_OPTION_DO_NOTHING;
        if (!GetMenuEntryReturn(&PickBootOptionMenu)) {
            break;
        }

        DefaultEntry = 9999;
        Style = NULL;
        MenuExit = DrawMenuScreen(PickBootOptionMenu, Style, &DefaultEntry, &ChosenOption);

#if MERIDIAN_DEBUG > 0
        LogExit(MenuExit, __func__, ChosenOption->Title);
#endif

        if (ChosenOption->Tag == TAG_RETURN) {
            break;
        }

        if (MenuExit == MENU_EXIT_ENTER) {
            Operation = EFI_BOOT_OPTION_MAKE_DEFAULT;
            *BootOrderNum = ChosenOption->Row;
        }
        else {
            if (MenuExit == MENU_EXIT_DELETE) {
                Operation = EFI_BOOT_OPTION_DELETE;
                *BootOrderNum = ChosenOption->Row;
            }
        }

        Operation = ConfirmBootOptionOperation(Operation, ChosenOption->Title);
    } while (0);

    FreeMenuScreen(&PickBootOptionMenu);

    return Operation;
}

static EFI_STATUS DeleteInvalidBootEntries(VOID)
{
    EFI_STATUS Status;
    UINTN i, j;
    UINTN VarSize;
    UINTN ListSize;
    CHAR8 *Contents;
    UINT16 *BootOrder;
    UINT16 *NewBootOrder;
    CHAR16 *VarName;

#if MERIDIAN_DEBUG > 0
    DEBUG_LOG(1, LOG_LINE_NORMAL, L"Delete Invalid Boot Entries from Internal BootOrder List");
#endif

    Status = EfivarGetRaw(&GlobalGuid, L"BootOrder", (VOID **)&BootOrder, &VarSize);
    if (!EFI_ERROR(Status)) {
        ListSize = VarSize / sizeof(UINT16);
        NewBootOrder = AllocateZeroPool(VarSize);

        j = 0;
        for (i = 0; i < ListSize; i++) {
            VarName = PoolPrint(L"Boot%04x", BootOrder[i]);
            Status = EfivarGetRaw(&GlobalGuid, VarName, (VOID **)&Contents, &VarSize);
            if (!EFI_ERROR(Status)) {
                NewBootOrder[j++] = BootOrder[i];
                MRD_FREE_POOL(Contents);
            }
            MRD_FREE_POOL(VarName);
        }

        Status = EfivarSetRaw(&GlobalGuid, L"BootOrder", NewBootOrder, j * sizeof(UINT16), TRUE);

        MRD_FREE_POOL(NewBootOrder);
        MRD_FREE_POOL(BootOrder);
    }

    return Status;
}

VOID ManageBootorder(VOID)
{
    UINTN BootNum;
    UINTN Operation;
    CHAR16 *Name;
    CHAR16 *Message;
    BOOT_ENTRY_LIST *Entries;

#if MERIDIAN_DEBUG > 0
    DEBUG_LOG(1, LOG_LINE_THIN_SEP, L"Prepare Menu Screen");
    DEBUG_LOG(1, LOG_LINE_NORMAL, L"Screen Title:- 'Manage BootOrder'");
#endif

    BootNum = 0;
    Entries = FindBootOrderEntries();
    Operation = PickOneBootOption(Entries, &BootNum);

    if (Operation == EFI_BOOT_OPTION_DELETE) {
        Name = PoolPrint(L"Boot%04x", BootNum);
        EfivarSetRaw(&GlobalGuid, Name, NULL, 0, TRUE);
        DeleteInvalidBootEntries();
        Message = PoolPrint(L"Boot%04x has been Deleted.", BootNum);
        DisplaySimpleMessage(Message, NULL);
        MRD_FREE_POOL(Message);
        MRD_FREE_POOL(Name);
    }

    if (Operation == EFI_BOOT_OPTION_MAKE_DEFAULT) {
        SetBootDefault(BootNum);
        Message = PoolPrint(L"Boot%04x is Now the Default EFI Boot Option.", BootNum);
        DisplaySimpleMessage(Message, NULL);
        MRD_FREE_POOL(Message);
    }

    DeleteBootOrderEntries(Entries);
}
