// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2020-2026 Dayo Akanji
// SPDX-FileCopyrightText: 2012-2023 Roderick W. Smith
// SPDX-FileCopyrightText: 2006-2010 Christoph Pfisterer

#include "global.h"
#include "config.h"
#include "lib.h"
#include "mok.h"
#include "menu.h"
#include "scan.h"
#include "linux.h"
#include "loop_timeout.h"
#include "apple.h"
#include "install.h"
#include "screenmgt.h"
#include "mystrings.h"
#include "launch_efi.h"
#include "driver_support.h"
#include "security_policy.h"

struct LOADER_LIST
{
    CHAR16 *FileName;
    EFI_TIME TimeStamp;
    struct LOADER_LIST *NextEntry;
};

static INTN TimeComp(IN EFI_TIME *Time1, IN EFI_TIME *Time2)
{
    if (Time1->Year != Time2->Year)
        return (Time1->Year > Time2->Year) ? 1 : -1;
    if (Time1->Month != Time2->Month)
        return (Time1->Month > Time2->Month) ? 1 : -1;
    if (Time1->Day != Time2->Day)
        return (Time1->Day > Time2->Day) ? 1 : -1;
    if (Time1->Hour != Time2->Hour)
        return (Time1->Hour > Time2->Hour) ? 1 : -1;
    if (Time1->Minute != Time2->Minute)
        return (Time1->Minute > Time2->Minute) ? 1 : -1;
    if (Time1->Second != Time2->Second)
        return (Time1->Second > Time2->Second) ? 1 : -1;

    return 0;
}

static struct LOADER_LIST *AddLoaderListEntry(struct LOADER_LIST *LoaderList,
                                              struct LOADER_LIST *NewEntry)
{
    struct LOADER_LIST *LatestEntry;
    struct LOADER_LIST *CurrentEntry;
    struct LOADER_LIST *PrevEntry;
    BOOLEAN LinuxRescue;
    BOOLEAN NewerOrEqual;
    INTN TimeCmpResult;

    NewEntry->NextEntry = NULL;
    if (LoaderList == NULL) {
        return NewEntry;
    }

    LinuxRescue = MrdStrIncludesCI(NewEntry->FileName, L"vmlinuz-0-rescue") ? TRUE : FALSE;

    if (LinuxRescue) {

        CurrentEntry = LoaderList;
        while (CurrentEntry->NextEntry != NULL) {
            CurrentEntry = CurrentEntry->NextEntry;
        }
        CurrentEntry->NextEntry = NewEntry;
        NewEntry->NextEntry = NULL;

        return LoaderList;
    }

    PrevEntry = NULL;
    LatestEntry = CurrentEntry = LoaderList;

    while (CurrentEntry != NULL) {

        if (MrdStrIncludesCI(CurrentEntry->FileName, L"vmlinuz-0-rescue")) {
            PrevEntry = CurrentEntry;
            CurrentEntry = CurrentEntry->NextEntry;

            continue;
        }

        TimeCmpResult = TimeComp(&(NewEntry->TimeStamp), &(CurrentEntry->TimeStamp));

        NewerOrEqual = (TimeCmpResult > 0 || TimeCmpResult == 0);
        if (NewerOrEqual)
            break;

        PrevEntry = CurrentEntry;
        CurrentEntry = CurrentEntry->NextEntry;
    }

    NewEntry->NextEntry = CurrentEntry;

    if (PrevEntry == NULL) {
        LatestEntry = NewEntry;
    }
    else {
        PrevEntry->NextEntry = NewEntry;
    }

    return LatestEntry;
}

static VOID CleanUpLoaderList(struct LOADER_LIST *LoaderList)
{
    struct LOADER_LIST *Temp;

    if (LoaderList == NULL) {
        return;
    }

    while (LoaderList != NULL) {
        Temp = LoaderList;
        LoaderList = LoaderList->NextEntry;
        MRD_FREE_POOL(Temp->FileName);
        MRD_FREE_POOL(Temp);
    }
}

BOOLEAN ShouldScan(MERIDIAN_VOLUME *Volume, CHAR16 *Path)
{
    UINTN i;
    CHAR16 *VolName;
    CHAR16 *PathCopy;
    CHAR16 *DontScanDir;
    CHAR16 *VentoyName;
    BOOLEAN ScanIt;

    if (MrdStrStartsWithCI(L"EFI\\APPLE", Path) || MrdStrStartsWithCI(L"\\EFI\\APPLE", Path)) {
        return FALSE;
    }

    if ((MrdStrEqualsCI(Path, SelfDirPath) ||
         (SelfBinaryDirPath != NULL && MrdStrEqualsCI(Path, SelfBinaryDirPath))) &&
        Volume->DeviceHandle == SelfVolume->DeviceHandle) {
        return FALSE;
    }

    if (MrdStrFind(Path, L"\\memtest")) {
        return FALSE;
    }

    ScanIt = VolumeScanAllowed(Volume, FALSE, FALSE);
    if (!ScanIt) {

        if (GlobalConfig.HandleVentoy) {
            if (MrdStrEqualsCI(Path, L"EFI\\BOOT") &&
                FileExists(Volume->RootDir, FALLBACK_FULLNAME)) {
                i = 0;
                while (!ScanIt) {
                    VentoyName = FindCommaDelimited(VENTOY_NAMES, i++);
                    if (VentoyName == NULL)
                        break;

                    if (MrdStrStartsWithCI(VentoyName, Volume->VolName) ||
                        MrdStrStartsWithCI(VentoyName, Volume->FsName) ||
                        MrdStrStartsWithCI(VentoyName, Volume->PartName)) {
                        ScanIt = TRUE;
                    }
                    MRD_FREE_POOL(VentoyName);
                }
            }
        }

        return ScanIt;
    }

    VolName = NULL;
    PathCopy = StrDuplicate(Path);
    if (SplitVolumeAndFilename(&PathCopy, &VolName)) {
        if (VolName) {
            if (!MrdStrEqualsCI(VolName, Volume->FsName) &&
                !MrdStrEqualsCI(VolName, Volume->PartName)) {
                ScanIt = FALSE;
            }
            MRD_FREE_POOL(VolName);
        }
    }
    MRD_FREE_POOL(PathCopy);

    if (!ScanIt) {
        return FALSE;
    }

    i = 0;
    while (1) {
        DontScanDir = FindCommaDelimited(GlobalConfig.DontScanDirs, i++);
        if (DontScanDir == NULL)
            break;

        SplitVolumeAndFilename(&DontScanDir, &VolName);
        CleanUpPathNameSlashes(DontScanDir);
        if (VolName == NULL) {
            if (MrdStrEqualsCI(DontScanDir, Path)) {
                ScanIt = FALSE;
            }
        }
        else {
            if (MrdStrEqualsCI(DontScanDir, Path) && VolumeMatchesDescription(Volume, VolName)) {
                ScanIt = FALSE;
            }
        }

        MRD_FREE_POOL(VolName);
        MRD_FREE_POOL(DontScanDir);

        if (!ScanIt)
            break;
    }

    return ScanIt;
}

BOOLEAN DuplicatesFallback(IN MERIDIAN_VOLUME *Volume, IN CHAR16 *FileName)
{
    EFI_STATUS Status;
    EFI_FILE_HANDLE FileHandle;
    EFI_FILE_HANDLE FallbackHandle;
    EFI_FILE_INFO *FileInfo;
    EFI_FILE_INFO *FallbackInfo;
    CHAR8 *FileContents;
    CHAR8 *FallbackContents;
    UINTN FileSize;
    UINTN FallbackSize;
    BOOLEAN AreIdentical;

    if (!FileExists(Volume->RootDir, FileName) || !FileExists(Volume->RootDir, FALLBACK_FULLNAME)) {
        return FALSE;
    }

    CleanUpPathNameSlashes(FileName);

    if (MrdStrEqualsCI(FileName, FALLBACK_FULLNAME)) {

        return FALSE;
    }

    Status = Volume->RootDir->Open(Volume->RootDir, &FileHandle, FileName, MeridianReadOnly, 0);
    if (EFI_ERROR(Status)) {
        return FALSE;
    }

    FileInfo = LibFileInfo(FileHandle);
    FileSize = FileInfo->FileSize;
    MRD_FREE_POOL(FileInfo);

    Status = Volume->RootDir->Open(Volume->RootDir, &FallbackHandle, FALLBACK_FULLNAME,
                                   MeridianReadOnly, 0);
    if (EFI_ERROR(Status)) {
        FileHandle->Close(FileHandle);
        return FALSE;
    }

    FallbackInfo = LibFileInfo(FallbackHandle);
    FallbackSize = FallbackInfo->FileSize;
    MRD_FREE_POOL(FallbackInfo);

    AreIdentical = FALSE;
    if (FallbackSize == FileSize) {

        FileContents = AllocatePool(FileSize);
        FallbackContents = AllocatePool(FallbackSize);
        if (FileContents != NULL && FallbackContents != NULL) {
            Status = FileHandle->Read(FileHandle, &FileSize, FileContents);
            if (!EFI_ERROR(Status)) {
                Status = FallbackHandle->Read(FallbackHandle, &FallbackSize, FallbackContents);
            }
            if (!EFI_ERROR(Status)) {
                AreIdentical = (CompareMem(FileContents, FallbackContents, FileSize) == 0);
            }
        }

        MRD_FREE_POOL(FileContents);
        MRD_FREE_POOL(FallbackContents);
    }

    FileHandle->Close(FallbackHandle);
    FileHandle->Close(FileHandle);

    return AreIdentical;
}

static BOOLEAN IsSymbolicLink(MERIDIAN_VOLUME *Volume, CHAR16 *FullName, EFI_FILE_INFO *DirEntry)
{
    EFI_FILE_HANDLE FileHandle;
    EFI_FILE_INFO *FileInfo;
    EFI_STATUS Status;
    UINTN FileSize2;

    FileSize2 = 0;
    Status = Volume->RootDir->Open(Volume->RootDir, &FileHandle, FullName, MeridianReadOnly, 0);
    if (!EFI_ERROR(Status)) {
        FileInfo = LibFileInfo(FileHandle);
        if (FileInfo != NULL) {
            FileSize2 = FileInfo->FileSize;
        }
        MRD_FREE_POOL(FileInfo);
    }

    return (DirEntry->FileSize != FileSize2);
}

BOOLEAN ScanLoaderDir(IN MERIDIAN_VOLUME *Volume, IN CHAR16 *Path, IN CHAR16 *Pattern)
{
#if MERIDIAN_DEBUG > 0
    BOOLEAN FoundFlag;
    BOOLEAN CheckMute = FALSE;
#endif

    EFI_STATUS Status;
    MERIDIAN_DIR_ITER DirIter;
    EFI_FILE_INFO *DirEntry;
    CHAR16 *Message;
    CHAR16 *FullName;
    struct LOADER_LIST *NewLoader;
    struct LOADER_LIST *LoaderList;
    LOADER_ENTRY *FirstKernel;
    LOADER_ENTRY *LatestEntry;
    BOOLEAN CheckIter;
    BOOLEAN IsLinux;
    BOOLEAN InSelfPath;
    BOOLEAN SelfPathFlag;
    BOOLEAN ShouldScanThis;
    BOOLEAN IsFallbackLoader;
    BOOLEAN FallbackDuplicate;

#if MERIDIAN_DEBUG > 0
    MRD_MUTELOGGER_SET;
#endif
    ShouldScanThis = ShouldScan(Volume, Path);
#if MERIDIAN_DEBUG > 0
    MRD_MUTELOGGER_OFF;
#endif

    if (!ShouldScanThis) {
        return FALSE;
    }

#if MERIDIAN_DEBUG > 0
    LOG_SEP(L"X");
    LOG_INCREMENT();

    DEBUG_LOG(1, LOG_LINE_NORMAL, L"Scan for '%s' on Volume '%s' ... Location:- '%s'", Pattern,
              Volume->VolName, Path);
#endif

    InSelfPath = MrdStrEqualsCI(Path, SelfDirPath);
    SelfPathFlag = (!InSelfPath || SelfDirPath != NULL || Path != NULL ||
                    (InSelfPath && Volume->DeviceHandle != SelfVolume->DeviceHandle));

    if (!SelfPathFlag) {
        LOG_DECREMENT();
        LOG_SEP(L"X");

        return FALSE;
    }

    FallbackDuplicate = FALSE;
    LoaderList = NULL;

    DirIterOpen(Volume->RootDir, Path, &DirIter);

    while (1) {
        CheckIter = DirIterNext(&DirIter, 2, Pattern, &DirEntry);
        if (!CheckIter)
            break;

        LOG_SEP(L"X");
        do {
            FullName = StrDuplicate(Path);

            MergeStrings(&FullName, DirEntry->FileName, L'\\');

            CleanUpPathNameSlashes(FullName);

            if (!Volume->AllowSymlinks) {
                if (IsSymbolicLink(Volume, FullName, DirEntry)) {

                    break;
                }
            }

            IsFallbackLoader = MrdStrEqualsCI(DirEntry->FileName, FALLBACK_BASENAME);

#if MERIDIAN_DEBUG > 0
            MRD_MUTELOGGER_SET;
#endif
            ShouldScanThis = IsValidLoader(Volume->RootDir, FullName);
#if MERIDIAN_DEBUG > 0
            MRD_MUTELOGGER_OFF;
#endif

            if (!ShouldScanThis || DirEntry->FileName[0] == '.' || MrdStrFind(Path, L"\\memtest") ||
                IsListItem(Path, GlobalConfig.ToolLocations) ||
                IsBlsClaimedLinuxPath(Volume, FullName) ||
                (IsFallbackLoader && MrdStrEqualsCI(Path, L"EFI\\BOOT")) ||
                (!IsFallbackLoader && IsListItem(DirEntry->FileName, MEMTEST_FILES)) ||
                (IsListItem(DirEntry->FileName, GlobalConfig.DontScanFiles)) ||
                (FilenameIn(Volume, Path, DirEntry->FileName, GlobalConfig.DontScanFiles)) ||
                IsBsdAuxLoader(Volume, DirEntry->FileName) ||
                (IsListMatch(DirEntry->FileName, SKIPNAME_PATTERNS)) ||
                (

                    IsListMatch(DirEntry->FileName, SELF_LOADER_PATTERNS)) ||
                (

                    HasSignedCounterpart(Volume, FullName))) {

                break;
            }

            NewLoader = AllocateZeroPool(sizeof(struct LOADER_LIST));
            if (NewLoader != NULL) {
                NewLoader->FileName = StrDuplicate(FullName);
                NewLoader->TimeStamp = DirEntry->ModificationTime;
                LoaderList = AddLoaderListEntry(LoaderList, NewLoader);

                if (DuplicatesFallback(Volume, FullName)) {
                    FallbackDuplicate = TRUE;
                }
            }

            IsLinux = IsListItemSubstringIn(FullName, GlobalConfig.LinuxPrefixes);

            if (IsLinux) {
                if (GlobalConfig.ToolLocationsExtra == NULL) {
                    GlobalConfig.ToolLocationsExtra = StrDuplicate(Path);
                }
                else {
                    MergeUniqueStrings(&GlobalConfig.ToolLocationsExtra, Path, L',');
                }
            }
        } while (0);

        MRD_FREE_POOL(FullName);
        MRD_FREE_POOL(DirEntry);

        LOG_SEP(L"X");
    }

    Status = DirIterClose(&DirIter);

    if (LoaderList == NULL) {
#if MERIDIAN_DEBUG > 0
        FoundFlag = FALSE;
#endif
    }
    else {
        IsLinux = FALSE;
        NewLoader = LoaderList;
        FirstKernel = NULL;

#if MERIDIAN_DEBUG > 0
        FoundFlag = TRUE;
#endif

        LOG_SEP(L"X");
        while (NewLoader != NULL) {

            IsLinux = IsListItemSubstringIn(NewLoader->FileName, GlobalConfig.LinuxPrefixes);

            if (IsLinux && FirstKernel != NULL && GlobalConfig.FoldLinuxKernels) {
                AddKernelToSubmenu(FirstKernel, NewLoader->FileName, Volume);
            }
            else {
                DisplayLoader = TRUE;
                LatestEntry =
                    AddLoaderEntry(NewLoader->FileName, NULL, Volume,
                                   !(IsLinux && GlobalConfig.FoldLinuxKernels), TRUE, NULL);

                if (IsLinux && FirstKernel == NULL) {
                    FirstKernel = LatestEntry;
                }
            }
            NewLoader = NewLoader->NextEntry;

            LOG_SEP(L"X");
        }

        if (IsLinux && FirstKernel != NULL && GlobalConfig.FoldLinuxKernels) {
            GetMenuEntryReturn(&FirstKernel->me.SubScreen);
        }

        CleanUpLoaderList(LoaderList);
    }

    if (EFI_ERROR(Status) && Status != EFI_NOT_FOUND && Status != EFI_INVALID_PARAMETER) {
        if (Path != NULL) {
            Message =
                PoolPrint(L"While Scanning the '%s' Directory on '%s'", Path, Volume->VolName);
        }
        else {
            Message = PoolPrint(L"While Scanning the Root Directory on '%s'", Volume->VolName);
        }

        CheckError(Status, Message);

        MRD_FREE_POOL(Message);
    }

    LOG_DECREMENT();
    LOG_SEP(L"X");

#if MERIDIAN_DEBUG > 0
    if (FoundFlag) {
        DEBUG_LOG(1, LOG_BLANK_LINE_SEP, L"X");
    }
#endif

    return FallbackDuplicate;
}
