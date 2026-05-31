// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2020-2026 Dayo Akanji
// SPDX-FileCopyrightText: 2021 Joe van Tunen
// SPDX-FileCopyrightText: 2012-2020 Roderick W. Smith
// SPDX-FileCopyrightText: 2006-2010 Christoph Pfisterer

#include "global.h"
#include "config.h"
#include "linux.h"
#include "scan.h"
#include "lib.h"
#include "menu.h"
#include "mystrings.h"

static CHAR16 *FlavourTail(IN CHAR16 *Name)
{
    CHAR16 *p = Name;

    if (Name == NULL) {
        return NULL;
    }
    while (*p != L'\0' && *p != L'-') {
        p++;
    }
    return (*p == L'-') ? (p + 1) : Name;
}

CHAR16 *FindInitrd(IN CHAR16 *LoaderPath, IN MERIDIAN_VOLUME *Volume)
{
#if MERIDIAN_DEBUG > 0
    CHAR16 *VolName;
#endif

    UINTN TempCount;
    UINTN SharedChars;
    UINTN MaxSharedChars;
    CHAR16 *Path;
    CHAR16 *OurPath;
    CHAR16 *FileName;
    CHAR16 *InitrdName;
    CHAR16 *KernFlavour;
    CHAR16 *InitrdBase;
    CHAR16 *KernelVersion;
    CHAR16 *InitrdVersion;
    BOOLEAN CheckIter;
    STRING_LIST *InitrdNames;
    STRING_LIST *FinalInitrdName;
    STRING_LIST *MaxSharedInitrd;
    STRING_LIST *CurrentInitrdName;
    EFI_FILE_INFO *DirEntry;
    MERIDIAN_DIR_ITER DirIter;

    OurPath = (LoaderPath[0] == L'\\') ? StrDuplicate(LoaderPath) : PoolPrint(L"\\%s", LoaderPath);

#if MERIDIAN_DEBUG > 0
    DEBUG_LOG(1, LOG_LINE_NORMAL, L"Locate/Match Linux Initrd File For Loader:- '%s'", OurPath);
#endif

    LOG_SEP(L"X");
    LOG_INCREMENT();
    FileName = Basename(OurPath);

    KernelVersion = FindNumbers(FileName);

    Path = FindPath(OurPath);

    if (StrLen(Path) == 0) {
        MergeStrings(&Path, L"\\", 0);
    }

#if MERIDIAN_DEBUG > 0
    VolName = Volume->VolName;
    DEBUG_LOG(1, LOG_THREE_STAR_MID, L"Path                  : %s",
              (Path != NULL) ? Path : L"NULL");
    DEBUG_LOG(1, LOG_THREE_STAR_MID, L"Volume                : %s",
              (VolName != NULL) ? VolName : L"NULL");
    DEBUG_LOG(1, LOG_THREE_STAR_MID, L"FileName              : %s",
              (FileName != NULL) ? FileName : L"NULL");
    DEBUG_LOG(1, LOG_THREE_STAR_MID, L"Kernel Version String : %s",
              (KernelVersion != NULL) ? KernelVersion : L"NULL");
#endif

    DirIterOpen(Volume->RootDir, Path, &DirIter);

    TempCount = StrLen(Path);
    if (TempCount > 0) {
        if (Path[TempCount - 1] != L'\\') {
            MergeStrings(&Path, L"\\", 0);
        }
    }

    InitrdNames = FinalInitrdName = CurrentInitrdName = NULL;
    while (1) {
        CheckIter = DirIterNext(&DirIter, 2, L"init*,booster*", &DirEntry);
        if (!CheckIter)
            break;

        InitrdVersion = FindNumbers(DirEntry->FileName);

#if MERIDIAN_DEBUG > 0
        DEBUG_LOG(1, LOG_LINE_NORMAL,
                  L"Validate 'KernelVersion = %s' on 'DirEntry = %s' with 'InitrdVersion = %s'",
                  (KernelVersion != NULL) ? KernelVersion : L"NULL",
                  (DirEntry->FileName != NULL) ? DirEntry->FileName : L"NULL",
                  (InitrdVersion != NULL) ? InitrdVersion : L"NULL");
#endif

        LOG_SEP(L"X");

        if ((KernelVersion != NULL && MrdStrEqualsCI(InitrdVersion, KernelVersion)) ||
            (KernelVersion == NULL && InitrdVersion == NULL)) {
            CurrentInitrdName = AllocateZeroPool(sizeof(STRING_LIST));

            if (InitrdNames == NULL) {
                InitrdNames = FinalInitrdName = CurrentInitrdName;
            }

            if (CurrentInitrdName != NULL) {
                CurrentInitrdName->Value = PoolPrint(L"%s%s", Path, DirEntry->FileName);

                if (CurrentInitrdName != FinalInitrdName) {
                    FinalInitrdName->Next = CurrentInitrdName;
                    FinalInitrdName = CurrentInitrdName;
                }
            }
        }
        MRD_FREE_POOL(InitrdVersion);
        MRD_FREE_POOL(DirEntry);

        LOG_SEP(L"X");
    }

    InitrdName = NULL;
    if (InitrdNames != NULL) {
        if (InitrdNames->Next == NULL) {
            InitrdName = StrDuplicate(InitrdNames->Value);
        }
        else {
            MaxSharedChars = 0;
            MaxSharedInitrd = CurrentInitrdName = InitrdNames;

            KernFlavour = FlavourTail(FileName);

            while (CurrentInitrdName != NULL) {
                LOG_SEP(L"X");

                InitrdBase = Basename(CurrentInitrdName->Value);

                SharedChars = MrdStrCommonPrefixLen(KernFlavour, FlavourTail(InitrdBase));

                if ((SharedChars > MaxSharedChars) ||
                    (SharedChars == MaxSharedChars &&
                     StrLen(CurrentInitrdName->Value) < StrLen(MaxSharedInitrd->Value))) {
                    MaxSharedChars = SharedChars;
                    MaxSharedInitrd = CurrentInitrdName;
                }

                MRD_FREE_POOL(InitrdBase);
                CurrentInitrdName = CurrentInitrdName->Next;
            }

            if (MaxSharedInitrd != NULL) {
                InitrdName = StrDuplicate(MaxSharedInitrd->Value);
            }
        }
    }

    DeleteStringList(InitrdNames);

    MRD_FREE_POOL(Path);
    MRD_FREE_POOL(OurPath);
    MRD_FREE_POOL(FileName);
    MRD_FREE_POOL(KernelVersion);

#if MERIDIAN_DEBUG > 0
    DEBUG_LOG(1, LOG_THREE_STAR_MID, L"Identified Linux Initrd File:- '%s'",
              (InitrdName != NULL) ? InitrdName : L"NONE");
#endif

    LOG_DECREMENT();
    LOG_SEP(L"X");

    return InitrdName;
}

static VOID AddMenuEntrySpacer(IN OUT MERIDIAN_MENU_SCREEN **Screen)
{
    MERIDIAN_MENU_ENTRY *MenuEntrySpacer;

    if (Screen == NULL || *Screen == NULL) {

        return;
    }

    MenuEntrySpacer = AllocateZeroPool(sizeof(MERIDIAN_MENU_ENTRY));
    if (MenuEntrySpacer == NULL) {

        return;
    }

    MenuEntrySpacer->Title = StrDuplicate(GEN_TAG);
    MenuEntrySpacer->Tag = TAG_SPACER;
    AddMenuEntry(*Screen, MenuEntrySpacer);
}

CHAR16 *AddInitrdToOptions(CHAR16 *Options, CHAR16 *InitrdPath)
{
    CHAR16 *NewOptions;
    CHAR16 *InitrdVersion;

    LOG_SEP(L"X");
    LOG_INCREMENT();
    if (Options == NULL) {
        NewOptions = NULL;
    }
    else {
        NewOptions = StrDuplicate(Options);
    }

    if (InitrdPath == NULL) {
        LOG_DECREMENT();
        LOG_SEP(L"X");

        return NewOptions;
    }

    if (NewOptions != NULL && MrdStrIncludesCI(NewOptions, L"%v")) {
        InitrdVersion = FindNumbers(InitrdPath);

        ReplaceSubstring(&NewOptions, L"%v", InitrdVersion);

        MRD_FREE_POOL(InitrdVersion);
    }
    else {
        if (NewOptions == NULL || !MrdStrIncludesCI(NewOptions, L"initrd=")) {
            MergeStrings(&NewOptions, L"initrd=", L' ');

            MergeStrings(&NewOptions, InitrdPath, 0);
        }
    }

    LOG_DECREMENT();
    LOG_SEP(L"X");

    return NewOptions;
}

CHAR16 *GetMainLinuxOptions(IN CHAR16 *LoaderPath, IN MERIDIAN_VOLUME *Volume)
{
#if MERIDIAN_DEBUG > 0
    BOOLEAN CheckMute = FALSE;
#endif

    CHAR16 *Options;
    CHAR16 *InitrdName;
    CHAR16 *FullOptions;
    CHAR16 *KernelVersion;

    LOG_SEP(L"X");
    LOG_INCREMENT();
    Options = GetFirstOptionsFromFile(LoaderPath, Volume);

#if MERIDIAN_DEBUG > 0
    MRD_MUTELOGGER_SET;
#endif
    InitrdName = FindInitrd(LoaderPath, Volume);
#if MERIDIAN_DEBUG > 0
    MRD_MUTELOGGER_OFF;
#endif

    if (InitrdName != NULL) {
        KernelVersion = FindNumbers(InitrdName);

        if (Options != NULL) {
            ReplaceSubstring(&Options, KERNEL_VERSION, KernelVersion);
        }
        MRD_FREE_POOL(KernelVersion);
    }

    FullOptions = NULL;
    if (InitrdName != NULL || Options != NULL) {
        FullOptions = AddInitrdToOptions(Options, InitrdName);
    }

    MRD_FREE_POOL(Options);
    MRD_FREE_POOL(InitrdName);

    LOG_DECREMENT();
    LOG_SEP(L"X");

    return FullOptions;
}

static VOID ParseReleaseFile(CHAR16 **OSSplashHint, MERIDIAN_VOLUME *Volume, CHAR16 *FileName,
                             BOOLEAN FirstOnly)
{
    EFI_STATUS Status;
    UINTN FileSize;
    UINTN TokenCount;
    CHAR16 **TokenList;
    CHAR16 *TempName;
    BOOLEAN Depart;
    MERIDIAN_FILE *File;

    if (Volume == NULL || FileName == NULL || !FileExists(Volume->RootDir, FileName)) {
        return;
    }

    File = AllocateZeroPool(sizeof(MERIDIAN_FILE));
    if (File == NULL) {
        return;
    }

    FileSize = 0;
    TempName = NULL;
    Depart = FALSE;

    Status = MeridianReadFile(Volume->RootDir, FileName, File, &FileSize);
    if (!EFI_ERROR(Status)) {
        while (1) {
            TokenCount = ReadTokenLine(File, &TokenList);
            if (TokenCount == 0) {

                Depart = TRUE;
            }
            else {
                if (TokenCount > 1 &&
                    (MrdStrEqualsCI(TokenList[0], L"ID") || MrdStrEqualsCI(TokenList[0], L"NAME") ||
                     MrdStrEqualsCI(TokenList[0], L"DISTRIB_ID"))) {
                    if (FirstOnly && (MrdStrEqualsCI(TokenList[0], L"ID") ||
                                      MrdStrEqualsCI(TokenList[0], L"DISTRIB_ID"))) {

                        Depart = TRUE;
                    }

                    MRD_FREE_POOL(TempName);
                    TempName = StrDuplicate(TokenList[1]);
                    MergeUniqueWords(OSSplashHint, TempName, L',');
                }
            }

            FreeTokenLine(&TokenList, &TokenCount);

            if (Depart)
                break;
        }
    }

    MRD_FREE_FILE(File);

    if (!FirstOnly) {
        ToLower(*OSSplashHint);
        MRD_FREE_POOL(TempName);

        return;
    }

    if (TempName == NULL) {
        return;
    }

    if (TempName[0] >= L'a' && TempName[0] <= L'z') {
        TempName[0] = TempName[0] - L'a' + L'A';
    }

    MRD_FREE_POOL(*OSSplashHint);
    *OSSplashHint = TempName;
}

VOID GuessLinuxDistribution(CHAR16 **OSSplashHint, MERIDIAN_VOLUME *Volume, CHAR16 *LoaderPath,
                            BOOLEAN FirstOnly)
{
    UINTN i;
    CHAR16 *LinuxName;
    CHAR16 *ShowName;
    BOOLEAN Found;

    LOG_SEP(L"X");
    LOG_INCREMENT();

    ParseReleaseFile(OSSplashHint, Volume, L"etc\\os-release", FirstOnly);

    if (!FirstOnly || *OSSplashHint == NULL) {
        ParseReleaseFile(OSSplashHint, Volume, L"etc\\lsb-release", FirstOnly);
    }

    DeleteItemFromCsvList(L"os", OSSplashHint);
    DeleteItemFromCsvList(L"gnu", OSSplashHint);
    DeleteItemFromCsvList(L"linux", OSSplashHint);

    if (FirstOnly && *OSSplashHint != NULL) {
        LOG_DECREMENT();
        LOG_SEP(L"X");

        return;
    }

    if (MrdStrIncludesCI(LoaderPath, L".fc")) {
        if (FirstOnly) {
            *OSSplashHint = StrDuplicate(L"Fedora");
        }
        else {
            MergeUniqueStrings(OSSplashHint, L"fedora", L',');
        }
    }
    else if (MrdStrIncludesCI(LoaderPath, L".el")) {
        if (FirstOnly) {
            *OSSplashHint = StrDuplicate(L"RedHat");
        }
        else {
            MergeUniqueStrings(OSSplashHint, L"redhat", L',');
        }
    }
    else {
        Found = FALSE;

        i = 0;
        while (!Found) {
            LinuxName = FindCommaDelimited(MAIN_LINUX_DISTROS, i++);
            if (LinuxName == NULL)
                break;

            ShowName = GetShowName(LinuxName);
            if (MrdStrIncludesCI(LoaderPath, ShowName)) {
                Found = TRUE;

                if (FirstOnly) {
                    *OSSplashHint = StrDuplicate(ShowName);
                }
                else {
                    MergeUniqueStrings(OSSplashHint, ShowName, L',');
                }
            }

            if (!Found && MrdStrIncludesCI(LoaderPath, LinuxName)) {
                Found = TRUE;

                if (FirstOnly) {
                    *OSSplashHint = StrDuplicate(LinuxName);
                }
                else {
                    MergeUniqueStrings(OSSplashHint, LinuxName, L',');
                }
            }

            MRD_FREE_POOL(LinuxName);
        }
    }

    LOG_DECREMENT();
    LOG_SEP(L"X");
}

VOID AddKernelToSubmenu(LOADER_ENTRY *TargetLoader, CHAR16 *FileName, MERIDIAN_VOLUME *Volume)
{
#if MERIDIAN_DEBUG > 0
    BOOLEAN CheckMute = FALSE;
#endif

    MERIDIAN_FILE *File;
    CHAR16 **TokenList = NULL;

    CHAR16 *Path = NULL;
    CHAR16 *VolName = NULL;
    CHAR16 *KernFile = NULL;
    CHAR16 *InitrdName;
    CHAR16 *ActualLoader;
    CHAR16 *KernelVersion;
    CHAR16 *BootTypeTag;
    CHAR16 *OutputData;
    MERIDIAN_MENU_SCREEN *SubScreen;
    LOADER_ENTRY *SubEntry;
    UINTN TokenCount;

#if MERIDIAN_DEBUG > 0
    DEBUG_LOG(1, LOG_STAR_HEAD_SEPX, L"Add Linux Kernel as SubMenu Entry");
#endif

    LOG_SEP(L"X");
    LOG_INCREMENT();
    File = ReadLinuxOptionsFile(TargetLoader->LoaderPath, Volume);

    if (File == NULL) {
        LOG_DECREMENT();
        LOG_SEP(L"X");

        return;
    }

    SubScreen = TargetLoader->me.SubScreen;

#if MERIDIAN_DEBUG > 0
    MRD_MUTELOGGER_SET;
#endif
    AddMenuEntrySpacer(&SubScreen);
#if MERIDIAN_DEBUG > 0
    MRD_MUTELOGGER_OFF;
#endif

    InitrdName = FindInitrd(FileName, Volume);

    KernelVersion = FindNumbers(FileName);

    ActualLoader = StrDuplicate(FileName);
    CleanUpPathNameSlashes(ActualLoader);

    Path = VolName = BootTypeTag = NULL;
    while (1) {
        TokenCount = ReadTokenLine(File, &TokenList);
        if (TokenCount < 2) {
            FreeTokenLine(&TokenList, &TokenCount);
            break;
        }

        LOG_SEP(L"X");
        ReplaceSubstring(&(TokenList[1]), KERNEL_VERSION, KernelVersion);

        SubEntry = CopyLoaderEntry(TargetLoader);

        if (SubEntry == NULL) {
            LOG_SEP(L"X");

            FreeTokenLine(&TokenList, &TokenCount);
            continue;
        }

        BootTypeTag = (TokenList[0] != NULL) ? CapitalisedCase(TokenList[0], TRUE)
                                             : StrDuplicate(L"Boot Linux");

        SplitPathName(FileName, &VolName, &Path, &KernFile);

        OutputData = PoolPrint(L"%s : %s", BootTypeTag, KernFile);

        LimitStringLength(OutputData, MAX_LINE_LENGTH);

        SubEntry->me.Title = OutputData;

        MRD_FREE_POOL(SubEntry->LoadOptions);
        SubEntry->LoadOptions = AddInitrdToOptions(TokenList[1], InitrdName);

        SubEntry->Volume = Volume;
        MRD_FREE_POOL(SubEntry->LoaderPath);
        SubEntry->LoaderPath = ActualLoader;
        SubEntry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_LINUX;
        AddMenuEntry(SubScreen, (MERIDIAN_MENU_ENTRY *)SubEntry);

        FreeTokenLine(&TokenList, &TokenCount);
        MRD_FREE_POOL(BootTypeTag);
        MRD_FREE_POOL(KernFile);
        MRD_FREE_POOL(VolName);
        MRD_FREE_POOL(Path);

        LOG_SEP(L"X");
    }

    MRD_FREE_POOL(KernelVersion);
    MRD_FREE_POOL(InitrdName);
    MRD_FREE_FILE(File);

#if MERIDIAN_DEBUG > 0
    DEBUG_LOG(1, LOG_THREE_STAR_END, L"Added Linux Kernel SubMenu Entry to %s",
              TargetLoader->Title);
#endif

    LOG_DECREMENT();
    LOG_SEP(L"X");
}

BOOLEAN HasSignedCounterpart(IN MERIDIAN_VOLUME *Volume, IN CHAR16 *FullName)
{
    CHAR16 *NewFile;
    BOOLEAN retval;

    NewFile = NULL;
    MergeStrings(&NewFile, FullName, 0);
    MergeStrings(&NewFile, L".efi.signed", 0);

    retval = FALSE;
    if (NewFile != NULL) {
        if (FileExists(Volume->RootDir, NewFile)) {
#if MERIDIAN_DEBUG > 0
            DEBUG_LOG(1, LOG_LINE_NORMAL, L"Found Signed Counterpart to '%s'", FullName);
#endif

            retval = TRUE;
        }
        MRD_FREE_POOL(NewFile);
    }

    return retval;
}
