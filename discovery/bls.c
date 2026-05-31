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

#define BLS_ENTRIES_DIR L"loader\\entries"
#define BLS_MATCH_PATTERNS L"*.conf,*.CONF"

struct BLS_CLAIMED_LOADER
{
    MERIDIAN_VOLUME *Volume;
    CHAR16 *LoaderPath;
    struct BLS_CLAIMED_LOADER *NextEntry;
};

static struct BLS_CLAIMED_LOADER *BlsClaimedLoaders = NULL;

static BOOLEAN BlsIsWhitespace(IN CHAR16 Char) { return (Char == L' ' || Char == L'\t'); }

static CHAR16 *BlsTrimLeft(IN CHAR16 *Text)
{
    if (Text == NULL) {
        return NULL;
    }

    while (BlsIsWhitespace(*Text)) {
        Text++;
    }

    return Text;
}

static VOID BlsTrimRight(IN OUT CHAR16 *Text)
{
    UINTN Length;

    if (Text == NULL) {
        return;
    }

    Length = StrLen(Text);
    while (Length > 0 && BlsIsWhitespace(Text[Length - 1])) {
        Length--;
        Text[Length] = L'\0';
    }
}

static CHAR16 *BlsDuplicateValue(IN CHAR16 *Value)
{
    CHAR16 *ValueCopy;

    Value = BlsTrimLeft(Value);
    if (Value == NULL) {
        return NULL;
    }

    BlsTrimRight(Value);

    ValueCopy = (Value[0] != L'\0') ? StrDuplicate(Value) : NULL;

    return ValueCopy;
}

static CHAR16 *BlsNormalizePath(IN CHAR16 *Path, IN BOOLEAN AddLeadingSlash)
{
    CHAR16 *NormalPath;
    CHAR16 *TempPath;

    NormalPath = BlsDuplicateValue(Path);
    if (NormalPath == NULL) {
        return NULL;
    }

    CleanUpPathNameSlashes(NormalPath);

    if (AddLeadingSlash && NormalPath[0] != L'\\') {
        TempPath = PoolPrint(L"\\%s", NormalPath);
        MRD_FREE_POOL(NormalPath);
        NormalPath = TempPath;
    }

    return NormalPath;
}

VOID BlsAppendString(IN OUT STRING_LIST **List, IN OUT STRING_LIST **Last, IN CHAR16 *Value)
{
    STRING_LIST *NewItem;

    if (List == NULL || Last == NULL || Value == NULL) {
        return;
    }

    NewItem = AllocateZeroPool(sizeof(STRING_LIST));
    if (NewItem == NULL) {
        return;
    }

    NewItem->Value = StrDuplicate(Value);
    if (NewItem->Value == NULL) {
        MRD_FREE_POOL(NewItem);

        return;
    }

    if (*List == NULL) {
        *List = NewItem;
    }
    else {
        (*Last)->Next = NewItem;
    }

    *Last = NewItem;
}

static CHAR16 *BlsBuildLoadOptions(IN CHAR16 *Options, IN STRING_LIST *Initrds)
{
    CHAR16 *LoadOptions;
    STRING_LIST *Initrd;

    LoadOptions = (Options != NULL) ? StrDuplicate(Options) : NULL;

    Initrd = Initrds;
    while (Initrd != NULL) {
        if (Initrd->Value != NULL && Initrd->Value[0] != L'\0') {
            MergeStrings(&LoadOptions, L"initrd=", L' ');
            MergeStrings(&LoadOptions, Initrd->Value, 0);
        }

        Initrd = Initrd->Next;
    }

    return LoadOptions;
}

static BOOLEAN BlsIsSupportedArchitecture(IN CHAR16 *Architecture)
{
    if (Architecture == NULL || Architecture[0] == L'\0') {
        return TRUE;
    }

#if defined(EFIX64)
    return (MrdStrEqualsCI(Architecture, L"x64") || MrdStrEqualsCI(Architecture, L"x86_64"));
#elif defined(EFIAARCH64)
    return (MrdStrEqualsCI(Architecture, L"aa64") || MrdStrEqualsCI(Architecture, L"aarch64") ||
            MrdStrEqualsCI(Architecture, L"arm64"));
#else
    return TRUE;
#endif
}

static CHAR16 *BlsTitleFromFields(IN CHAR16 *Title, IN CHAR16 *Version, IN CHAR16 *EntryFileName)
{
    CHAR16 *BaseName;
    CHAR16 *EntryTitle;

    if (Title != NULL && Title[0] != L'\0') {
        return StrDuplicate(Title);
    }

    if (Version != NULL && Version[0] != L'\0') {
        return StrDuplicate(Version);
    }

    BaseName = Basename(EntryFileName);
    EntryTitle = StripSetExtension(L".conf", BaseName);
    MRD_FREE_POOL(BaseName);

    if (EntryTitle == NULL) {
        EntryTitle = StrDuplicate(L"Linux");
    }

    return EntryTitle;
}

static VOID AddBlsClaimedLinuxPath(IN MERIDIAN_VOLUME *Volume, IN CHAR16 *LoaderPath)
{
    CHAR16 *ClaimPath;
    struct BLS_CLAIMED_LOADER *Claim;
    struct BLS_CLAIMED_LOADER *NewClaim;

    if (Volume == NULL || LoaderPath == NULL) {
        return;
    }

    ClaimPath = StrDuplicate(LoaderPath);
    if (ClaimPath == NULL) {
        return;
    }

    CleanUpPathNameSlashes(ClaimPath);

    Claim = BlsClaimedLoaders;
    while (Claim != NULL) {
        if (Claim->Volume == Volume && MrdStrEqualsCI(Claim->LoaderPath, ClaimPath)) {
            MRD_FREE_POOL(ClaimPath);

            return;
        }

        Claim = Claim->NextEntry;
    }

    NewClaim = AllocateZeroPool(sizeof(struct BLS_CLAIMED_LOADER));
    if (NewClaim == NULL) {
        MRD_FREE_POOL(ClaimPath);

        return;
    }

    NewClaim->Volume = Volume;
    NewClaim->LoaderPath = ClaimPath;
    NewClaim->NextEntry = BlsClaimedLoaders;
    BlsClaimedLoaders = NewClaim;
}

VOID ClearBlsClaimedLoaders(VOID)
{
    struct BLS_CLAIMED_LOADER *Claim;
    struct BLS_CLAIMED_LOADER *NextClaim;

    Claim = BlsClaimedLoaders;
    while (Claim != NULL) {
        NextClaim = Claim->NextEntry;
        MRD_FREE_POOL(Claim->LoaderPath);
        MRD_FREE_POOL(Claim);
        Claim = NextClaim;
    }

    BlsClaimedLoaders = NULL;
}

BOOLEAN IsBlsClaimedLinuxPath(IN MERIDIAN_VOLUME *Volume, IN CHAR16 *LoaderPath)
{
    CHAR16 *TestPath;
    struct BLS_CLAIMED_LOADER *Claim;
    BOOLEAN IsClaimed;

    if (Volume == NULL || LoaderPath == NULL) {
        return FALSE;
    }

    TestPath = StrDuplicate(LoaderPath);
    if (TestPath == NULL) {
        return FALSE;
    }

    CleanUpPathNameSlashes(TestPath);

    IsClaimed = FALSE;
    Claim = BlsClaimedLoaders;
    while (Claim != NULL) {
        if (Claim->Volume == Volume && MrdStrEqualsCI(Claim->LoaderPath, TestPath)) {
            IsClaimed = TRUE;

            break;
        }

        Claim = Claim->NextEntry;
    }

    MRD_FREE_POOL(TestPath);

    return IsClaimed;
}

static BOOLEAN BlsLoaderPathsMatch(IN CHAR16 *PathA, IN CHAR16 *PathB)
{
    CHAR16 *CleanPathA;
    CHAR16 *CleanPathB;
    BOOLEAN Match;

    if (PathA == NULL || PathB == NULL) {
        return FALSE;
    }

    CleanPathA = StrDuplicate(PathA);
    CleanPathB = StrDuplicate(PathB);
    if (CleanPathA == NULL || CleanPathB == NULL) {
        MRD_FREE_POOL(CleanPathA);
        MRD_FREE_POOL(CleanPathB);

        return FALSE;
    }

    CleanUpPathNameSlashes(CleanPathA);
    CleanUpPathNameSlashes(CleanPathB);

    Match = MrdStrEqualsCI(CleanPathA, CleanPathB);

    MRD_FREE_POOL(CleanPathA);
    MRD_FREE_POOL(CleanPathB);

    return Match;
}

static BOOLEAN IsBlindLinuxLoaderForBls(IN LOADER_ENTRY *LoaderEntry, IN MERIDIAN_VOLUME *Volume,
                                        IN CHAR16 *LoaderPath)
{
    if (LoaderEntry == NULL || LoaderEntry->Volume != Volume || LoaderEntry->OSType != 'L' ||
        LoaderEntry->DiscoveryType != DISCOVERY_TYPE_AUTO) {
        return FALSE;
    }

    return BlsLoaderPathsMatch(LoaderEntry->LoaderPath, LoaderPath);
}

static VOID RemoveBlindLinuxLoaderForBls(IN MERIDIAN_VOLUME *Volume, IN CHAR16 *LoaderPath)
{
    UINTN i;
    UINTN j;
    LOADER_ENTRY *KeepEntry;
    LOADER_ENTRY *LoaderEntry;
    MERIDIAN_MENU_ENTRY *MenuEntry;
    MERIDIAN_MENU_ENTRY *RemovedEntry;

    if (MainMenu == NULL || MainMenu->Entries == NULL || Volume == NULL || LoaderPath == NULL) {
        return;
    }

    KeepEntry = NULL;
    for (i = 0; i < MainMenu->EntryCount; i++) {
        MenuEntry = MainMenu->Entries[i];
        if (MenuEntry == NULL || MenuEntry->Tag != TAG_LOADER) {
            continue;
        }

        LoaderEntry = (LOADER_ENTRY *)MenuEntry;
        if (IsBlindLinuxLoaderForBls(LoaderEntry, Volume, LoaderPath)) {

            KeepEntry = LoaderEntry;
        }
    }

    if (KeepEntry == NULL) {
        return;
    }

    for (i = 0; i < MainMenu->EntryCount; i++) {
        MenuEntry = MainMenu->Entries[i];
        if (MenuEntry == NULL || MenuEntry->Tag != TAG_LOADER) {
            continue;
        }

        LoaderEntry = (LOADER_ENTRY *)MenuEntry;
        if (LoaderEntry == KeepEntry ||
            !IsBlindLinuxLoaderForBls(LoaderEntry, Volume, LoaderPath)) {
            continue;
        }

        RemovedEntry = MenuEntry;
        for (j = i + 1; j < MainMenu->EntryCount; j++) {
            MainMenu->Entries[j - 1] = MainMenu->Entries[j];
        }

        MainMenu->EntryCount--;
        MainMenu->Entries[MainMenu->EntryCount] = NULL;

        FreeMenuEntry(&RemovedEntry);

        return;
    }
}

static LOADER_ENTRY *AddBlsLoaderEntry(IN MERIDIAN_VOLUME *Volume, IN CHAR16 *LoaderPath,
                                       IN CHAR16 *EntryTitle, IN CHAR16 *LoadOptions)
{
    CHAR16 *TmpName;
    LOADER_ENTRY *LoaderEntry;

    if (Volume == NULL || LoaderPath == NULL || EntryTitle == NULL) {
        return NULL;
    }

    LoaderEntry = InitializeLoaderEntry(NULL);
    if (LoaderEntry == NULL) {
        return NULL;
    }

    LoaderEntry->Title = StrDuplicate(EntryTitle);
    LoaderEntry->LoaderPath = StrDuplicate(LoaderPath);
    LoaderEntry->Volume = Volume;
    LoaderEntry->OSType = 'L';
    LoaderEntry->DiscoveryType = DISCOVERY_TYPE_AUTO;
    LoaderEntry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_LINUX;
    LoaderEntry->me.Row = 0;

    SetLoaderDefaults(LoaderEntry, LoaderPath, Volume);

    LoaderEntry->OSType = 'L';
    LoaderEntry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_LINUX;

    MRD_FREE_POOL(LoaderEntry->LoadOptions);
    LoaderEntry->LoadOptions = (LoadOptions != NULL) ? StrDuplicate(LoadOptions) : NULL;

    TmpName = Volume->VolName;
    LoaderEntry->me.Title = (TmpName != NULL)
                                ? BuildLoaderTitle(LoaderEntry->Title, TmpName, Volume->FSType)
                                : PoolPrint(L"Load %s", LoaderEntry->Title);

    GenerateSubScreen(LoaderEntry, Volume, TRUE);

    AddMenuEntry(MainMenu, (MERIDIAN_MENU_ENTRY *)LoaderEntry);

#if MERIDIAN_DEBUG > 0
    INFO_LOG("%s  - Found BLS Entry: %s", OffsetNext, LoaderEntry->Title);
#endif

    return LoaderEntry;
}

static LOADER_ENTRY *ParseBlsEntryFile(IN MERIDIAN_VOLUME *Volume, IN CHAR16 *EntryPath,
                                       IN CHAR16 *EntryFileName)
{
    EFI_STATUS Status;
    UINTN FileSize;
    CHAR16 *Line;
    CHAR16 *Key;
    CHAR16 *Value;
    CHAR16 *Title;
    CHAR16 *Version;
    CHAR16 *MachineId;
    CHAR16 *LinuxPath;
    CHAR16 *EfiPath;
    CHAR16 *Options;
    CHAR16 *InitrdPath;
    CHAR16 *EntryTitle;
    CHAR16 *LoadOptions;
    STRING_LIST *Initrds;
    STRING_LIST *LastInitrd;
    MERIDIAN_FILE *File;
    LOADER_ENTRY *LoaderEntry;
    BOOLEAN WrongArch;

    File = AllocateZeroPool(sizeof(MERIDIAN_FILE));
    if (File == NULL) {
        return NULL;
    }

    FileSize = 0;
    Status = MeridianReadFile(Volume->RootDir, EntryPath, File, &FileSize);
    if (EFI_ERROR(Status)) {
        MRD_FREE_FILE(File);

        return NULL;
    }

    Title = NULL;
    Version = NULL;
    MachineId = NULL;
    LinuxPath = NULL;
    EfiPath = NULL;
    Options = NULL;
    Initrds = NULL;
    LastInitrd = NULL;
    WrongArch = FALSE;

    while (1) {
        Line = ReadLine(File);
        if (Line == NULL) {
            break;
        }

        Value = BlsTrimLeft(Line);
        if (Value == NULL || Value[0] == L'\0' || Value[0] == L'#') {
            MRD_FREE_POOL(Line);

            continue;
        }

        Key = Value;
        while (*Value != L'\0' && !BlsIsWhitespace(*Value)) {
            Value++;
        }

        if (*Value != L'\0') {
            *Value = L'\0';
            Value++;
        }

        Value = BlsTrimLeft(Value);
        BlsTrimRight(Value);

        if (MrdStrEqualsCI(Key, L"title")) {
            MRD_FREE_POOL(Title);
            Title = BlsDuplicateValue(Value);
        }
        else if (MrdStrEqualsCI(Key, L"version")) {
            MRD_FREE_POOL(Version);
            Version = BlsDuplicateValue(Value);
        }
        else if (MrdStrEqualsCI(Key, L"machine-id")) {
            MRD_FREE_POOL(MachineId);
            MachineId = BlsDuplicateValue(Value);
        }
        else if (MrdStrEqualsCI(Key, L"architecture")) {
            InitrdPath = BlsDuplicateValue(Value);
            if (!BlsIsSupportedArchitecture(InitrdPath)) {
                WrongArch = TRUE;
            }
            MRD_FREE_POOL(InitrdPath);
        }
        else if (MrdStrEqualsCI(Key, L"linux")) {
            MRD_FREE_POOL(LinuxPath);
            LinuxPath = BlsNormalizePath(Value, TRUE);
        }
        else if (MrdStrEqualsCI(Key, L"efi")) {
            MRD_FREE_POOL(EfiPath);
            EfiPath = BlsNormalizePath(Value, TRUE);
        }
        else if (MrdStrEqualsCI(Key, L"initrd")) {
            InitrdPath = BlsNormalizePath(Value, TRUE);
            BlsAppendString(&Initrds, &LastInitrd, InitrdPath);
            MRD_FREE_POOL(InitrdPath);
        }
        else if (MrdStrEqualsCI(Key, L"options")) {
            InitrdPath = BlsDuplicateValue(Value);
            if (InitrdPath != NULL) {
                MergeStrings(&Options, InitrdPath, L' ');
                MRD_FREE_POOL(InitrdPath);
            }
        }

        MRD_FREE_POOL(Line);
    }

    MRD_FREE_FILE(File);

    LoaderEntry = NULL;
    if (!WrongArch && LinuxPath != NULL && FileExists(Volume->RootDir, LinuxPath) &&
        IsValidLoader(Volume->RootDir, LinuxPath)) {
        EntryTitle = BlsTitleFromFields(Title, Version, EntryFileName);
        LoadOptions = BlsBuildLoadOptions(Options, Initrds);

        LoaderEntry = AddBlsLoaderEntry(Volume, LinuxPath, EntryTitle, LoadOptions);

        if (LoaderEntry != NULL) {
            AddBlsClaimedLinuxPath(Volume, LinuxPath);
            RemoveBlindLinuxLoaderForBls(Volume, LinuxPath);
        }

        MRD_FREE_POOL(EntryTitle);
        MRD_FREE_POOL(LoadOptions);
    }
    else if (!WrongArch && LinuxPath == NULL && EfiPath != NULL) {
#if MERIDIAN_DEBUG > 0
        INFO_LOG("%s  - Skip BLS Entry '%s' ... Generic 'efi' Entries Not Implemented", OffsetNext,
                 EntryFileName);
#endif
    }

    MRD_FREE_POOL(Title);
    MRD_FREE_POOL(Version);
    MRD_FREE_POOL(MachineId);
    MRD_FREE_POOL(LinuxPath);
    MRD_FREE_POOL(EfiPath);
    MRD_FREE_POOL(Options);
    DeleteStringList(Initrds);

    return LoaderEntry;
}

VOID ScanBLSEntries(IN MERIDIAN_VOLUME *Volume)
{
    EFI_STATUS Status;
    MERIDIAN_DIR_ITER DirIter;
    EFI_FILE_INFO *DirEntry;
    EFI_FILE_HANDLE BlsRootDir;
    CHAR16 *EntryPath;
    CHAR16 *EntryFileName;
    STRING_LIST *EntryPaths;
    STRING_LIST *LastEntryPath;
    STRING_LIST *CurrentEntryPath;
#if MERIDIAN_DEBUG > 0
    BOOLEAN FoundEntry;
#endif

    if (Volume == NULL || Volume->DeviceHandle == NULL || Volume->RootDir == NULL ||
        !ShouldScan(Volume, BLS_ENTRIES_DIR)) {
        return;
    }

#if MERIDIAN_DEBUG > 0
    FoundEntry = FALSE;

    DEBUG_LOG(1, LOG_LINE_NORMAL, L"Scan Boot Loader Specification Entries on Volume:- '%s'",
              Volume->VolName);
#endif

    EntryPaths = NULL;
    LastEntryPath = NULL;
    CurrentEntryPath = NULL;
    BlsRootDir = EfiLibOpenRoot(Volume->DeviceHandle);
    if (BlsRootDir == NULL) {
        return;
    }

    DirIterOpen(BlsRootDir, BLS_ENTRIES_DIR, &DirIter);

    while (1) {
        if (!DirIterNext(&DirIter, 2, BLS_MATCH_PATTERNS, &DirEntry)) {
            break;
        }

        EntryPath = PoolPrint(L"%s\\%s", BLS_ENTRIES_DIR, DirEntry->FileName);
        if (EntryPath != NULL) {
            BlsAppendString(&EntryPaths, &LastEntryPath, EntryPath);

            MRD_FREE_POOL(EntryPath);
        }

        MRD_FREE_POOL(DirEntry);
    }

    Status = DirIterClose(&DirIter);

    BlsRootDir->Close(BlsRootDir);

    if (EFI_ERROR(Status) && Status != EFI_NOT_FOUND && Status != EFI_INVALID_PARAMETER &&
        Status != EFI_UNSUPPORTED) {
        CheckError(Status, L"While Scanning Boot Loader Specification Entries");
    }

    CurrentEntryPath = EntryPaths;
    while (CurrentEntryPath != NULL) {
        EntryFileName = Basename(CurrentEntryPath->Value);

        if (ParseBlsEntryFile(Volume, CurrentEntryPath->Value, EntryFileName) != NULL) {
            DisplayLoader = TRUE;
#if MERIDIAN_DEBUG > 0
            FoundEntry = TRUE;
#endif
        }

        MRD_FREE_POOL(EntryFileName);
        CurrentEntryPath = CurrentEntryPath->Next;
    }

    DeleteStringList(EntryPaths);

#if MERIDIAN_DEBUG > 0
    if (FoundEntry) {
        DEBUG_LOG(1, LOG_BLANK_LINE_SEP, L"X");
    }
#endif

    return;
}
