// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#include "global.h"
#include "config.h"
#include "lib.h"
#include "menu.h"
#include "scan.h"
#include "mystrings.h"
#include "btrfs_snapshots.h"

#define SNAPSHOT_DIR L"@.snapshots"

#define MAX_SNAPSHOT_ENTRIES 10

#define MAX_INFO_XML_SIZE 65536

static BOOLEAN IsSnapshotId(IN CHAR16 *Name)
{
    UINTN i;

    if (Name == NULL || Name[0] == L'\0') {
        return FALSE;
    }

    for (i = 0; Name[i] != L'\0'; i++) {
        if (Name[i] < L'0' || Name[i] > L'9') {
            return FALSE;
        }
    }

    return TRUE;
}

static UINT64 SnapshotIdValue(IN CHAR16 *Name)
{
    UINTN i;
    UINT64 Value;

    Value = 0;
    for (i = 0; Name[i] != L'\0'; i++) {
        if (Value > (MAX_UINT64 / 10)) {
            return MAX_UINT64;
        }
        Value *= 10;

        if (Value > (MAX_UINT64 - (UINT64)(Name[i] - L'0'))) {
            return MAX_UINT64;
        }
        Value += (UINT64)(Name[i] - L'0');
    }

    return Value;
}

static BOOLEAN HasOptionToken(IN CHAR16 *Options, IN CHAR16 *Token)
{
    UINTN i, j;

    if (Options == NULL || Token == NULL) {
        return FALSE;
    }

    i = 0;
    while (Options[i] != L'\0') {
        while (Options[i] == L' ') {
            i++;
        }
        if (Options[i] == L'\0') {
            break;
        }

        for (j = 0; Token[j] != L'\0' && Options[i + j] == Token[j]; j++) {

        }

        if (Token[j] == L'\0' && (Options[i + j] == L' ' || Options[i + j] == L'\0')) {
            return TRUE;
        }

        while (Options[i] != L' ' && Options[i] != L'\0') {
            i++;
        }
    }

    return FALSE;
}

static CHAR16 *SnapshotLoadOptions(IN CHAR16 *BaseOptions, IN CHAR16 *SubvolPath)
{
    CHAR16 *Match;
    CHAR16 *Tail;
    CHAR16 *Prefix;
    CHAR16 *Result;
    CHAR16 *WithRo;
    UINTN PrefixLen;

    if (BaseOptions == NULL) {

        return PoolPrint(L"ro rootflags=subvol=%s", SubvolPath);
    }

    Match = MrdStrFind(BaseOptions, L"rootflags=subvol=");
    if (Match == NULL) {
        Result = PoolPrint(L"%s rootflags=subvol=%s", BaseOptions, SubvolPath);
    }
    else {

        PrefixLen = (UINTN)(Match - BaseOptions);

        Prefix = AllocateZeroPool((PrefixLen + 1) * sizeof(CHAR16));
        if (Prefix == NULL) {
            return NULL;
        }
        CopyMem(Prefix, BaseOptions, PrefixLen * sizeof(CHAR16));

        Tail = Match;
        while (*Tail != L' ' && *Tail != L'\0') {
            Tail++;
        }

        Result = PoolPrint(L"%srootflags=subvol=%s%s", Prefix, SubvolPath, Tail);

        MRD_FREE_POOL(Prefix);
    }

    if (Result == NULL) {
        return NULL;
    }

    if (!HasOptionToken(Result, L"ro") && !HasOptionToken(Result, L"rw")) {
        WithRo = PoolPrint(L"ro %s", Result);
        MRD_FREE_POOL(Result);
        Result = WithRo;
    }

    return Result;
}

static CHAR16 *InfoXmlField(IN CHAR8 *Text, IN UINTN Size, IN CHAR8 *Field)
{
    UINTN i, j;
    UINTN Start, End;
    UINTN FieldLen;

    if (Text == NULL || Field == NULL || Size == 0) {
        return NULL;
    }

    for (FieldLen = 0; Field[FieldLen] != '\0'; FieldLen++) {

    }

    if (Size < ((2 * FieldLen) + 5)) {
        return NULL;
    }

    for (i = 0; i < (Size - FieldLen - 1); i++) {
        if (Text[i] != '<') {
            continue;
        }

        for (j = 0; j < FieldLen && Text[i + 1 + j] == Field[j]; j++) {

        }

        if (j < FieldLen || Text[i + 1 + FieldLen] != '>') {
            continue;
        }

        Start = i + FieldLen + 2;
        for (End = Start; End < Size && Text[End] != '<'; End++) {

        }

        if (End <= Start || End >= Size) {
            return NULL;
        }

        return MrdAsciiToUnicode(&Text[Start], End - Start);
    }

    return NULL;
}

static CHAR16 *SnapshotTitle(IN MERIDIAN_VOLUME *Volume, IN UINT64 Id)
{
    EFI_STATUS Status;
    UINTN Size;
    CHAR16 *Path;
    CHAR16 *Date;
    CHAR16 *Desc;
    CHAR16 *Title;
    MERIDIAN_FILE Info;

    Date = NULL;
    Desc = NULL;

    Path = PoolPrint(L"%s\\%ld\\info.xml", SNAPSHOT_DIR, Id);
    if (Path != NULL) {
        ZeroMem(&Info, sizeof(Info));

        Status = MeridianReadFile(Volume->RootDir, Path, &Info, &Size);
        if (!EFI_ERROR(Status) && Info.BufferData != NULL && Info.BufferSize > 0 &&
            Info.BufferSize <= MAX_INFO_XML_SIZE) {
            Date = InfoXmlField((CHAR8 *)Info.BufferData, Info.BufferSize, "date");
            Desc = InfoXmlField((CHAR8 *)Info.BufferData, Info.BufferSize, "description");
        }

        MRD_FREE_POOL(Info.BufferData);
        MRD_FREE_POOL(Path);
    }

    if (Date != NULL && Desc != NULL) {
        Title = PoolPrint(L"Boot Snapshot %ld - %s (%s)", Id, Date, Desc);
    }
    else if (Date != NULL) {
        Title = PoolPrint(L"Boot Snapshot %ld - %s", Id, Date);
    }
    else if (Desc != NULL) {
        Title = PoolPrint(L"Boot Snapshot %ld - %s", Id, Desc);
    }
    else {
        Title = PoolPrint(L"Boot Snapshot %ld", Id);
    }

    MRD_FREE_POOL(Date);
    MRD_FREE_POOL(Desc);

    return Title;
}

static MERIDIAN_VOLUME *FindSnapshotVolume(VOID)
{
    UINTN i;
    MERIDIAN_VOLUME *Volume;

    for (i = 0; i < VolumesCount; i++) {
        Volume = Volumes[i];

        if (Volume == NULL || Volume->RootDir == NULL) {
            continue;
        }
        if (Volume->FSType != FS_TYPE_BTRFS) {
            continue;
        }
        if (!FileExists(Volume->RootDir, SNAPSHOT_DIR)) {
            continue;
        }

        return Volume;
    }

    return NULL;
}

static UINTN CollectSnapshotIds(IN MERIDIAN_VOLUME *Volume, OUT UINT64 *Ids)
{
    UINTN i, j;
    UINTN Count;
    UINT64 Id;
    CHAR16 *Probe;
    EFI_FILE_INFO *DirEntry;
    MERIDIAN_DIR_ITER DirIter;

    Count = 0;

    DirIterOpen(Volume->RootDir, SNAPSHOT_DIR, &DirIter);

    while (1) {

        if (!DirIterNext(&DirIter, 1, NULL, &DirEntry)) {
            break;
        }

        if (!IsSnapshotId(DirEntry->FileName)) {
            MRD_FREE_POOL(DirEntry);
            continue;
        }

        Probe = PoolPrint(L"%s\\%s\\snapshot", SNAPSHOT_DIR, DirEntry->FileName);
        if (Probe == NULL) {
            MRD_FREE_POOL(DirEntry);
            continue;
        }

        if (!FileExists(Volume->RootDir, Probe)) {
            MRD_FREE_POOL(Probe);
            MRD_FREE_POOL(DirEntry);
            continue;
        }

        Id = SnapshotIdValue(DirEntry->FileName);

        for (i = 0; i < Count; i++) {
            if (Id > Ids[i]) {
                break;
            }
        }

        if (i < MAX_SNAPSHOT_ENTRIES) {
            for (j = (Count < MAX_SNAPSHOT_ENTRIES) ? Count : (MAX_SNAPSHOT_ENTRIES - 1); j > i;
                 j--) {
                Ids[j] = Ids[j - 1];
            }

            Ids[i] = Id;

            if (Count < MAX_SNAPSHOT_ENTRIES) {
                Count++;
            }
        }

        MRD_FREE_POOL(Probe);
        MRD_FREE_POOL(DirEntry);
    }

    DirIterClose(&DirIter);

    return Count;
}

VOID AddBtrfsSnapshotSubEntries(IN LOADER_ENTRY *Entry, IN MERIDIAN_MENU_SCREEN *SubScreen,
                                IN CHAR16 *BaseOptions)
{
    UINTN i;
    UINTN Count;
    CHAR16 *Subvol;
    CHAR16 *Options;
    UINT64 Ids[MAX_SNAPSHOT_ENTRIES];
    LOADER_ENTRY *SubEntry;
    MERIDIAN_VOLUME *Volume;

    if (Entry == NULL || SubScreen == NULL) {
        return;
    }

    Volume = FindSnapshotVolume();
    if (Volume == NULL) {
        return;
    }

    Count = CollectSnapshotIds(Volume, Ids);

#if MERIDIAN_DEBUG > 0
    DEBUG_LOG(1, LOG_THREE_STAR_MID, L"Found %d btrfs Snapshot%s on '%s'", Count,
              (Count == 1) ? L"" : L"s", Volume->VolName);
#endif

    for (i = 0; i < Count; i++) {

        Subvol = PoolPrint(L"@.snapshots/%ld/snapshot", Ids[i]);
        if (Subvol == NULL) {
            continue;
        }

        Options = SnapshotLoadOptions(BaseOptions, Subvol);
        MRD_FREE_POOL(Subvol);

        if (Options == NULL) {
            continue;
        }

        SubEntry = CopyLoaderEntry(Entry);
        if (SubEntry == NULL) {
            MRD_FREE_POOL(Options);
            continue;
        }

        SubEntry->me.Title = SnapshotTitle(Volume, Ids[i]);

        MRD_FREE_POOL(SubEntry->LoadOptions);
        SubEntry->LoadOptions = Options;

        SubEntry->UseGraphicsMode = (GlobalConfig.GraphicsFor & GRAPHICS_FOR_LINUX);

        AddMenuEntry(SubScreen, (MERIDIAN_MENU_ENTRY *)SubEntry);

#if MERIDIAN_DEBUG > 0
        DEBUG_LOG(1, LOG_THREE_STAR_MID, L"Added Snapshot Entry:- '%s' ... Options:- '%s'",
                  SubEntry->me.Title, SubEntry->LoadOptions);
#endif
    }
}
