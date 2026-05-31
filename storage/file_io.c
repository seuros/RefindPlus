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
#define DevicePathProtocol gEfiDevicePathProtocolGuid
#define BlockIoProtocol gEfiBlockIoProtocolGuid
#define LibFileSystemInfo EfiLibFileSystemInfo
#define LibOpenRoot EfiLibOpenRoot

#define MAX_MERIDIAN_FILE_SIZE (1024 * 1024 * 1024)

BOOLEAN FileExists(IN EFI_FILE_PROTOCOL *BaseDir, IN CHAR16 *RelativePath)
{
    EFI_STATUS Status;
    EFI_FILE_HANDLE TestFile;

    if (BaseDir != NULL) {
        Status = BaseDir->Open(BaseDir, &TestFile, RelativePath, MeridianReadOnly, 0);
        if (!EFI_ERROR(Status)) {
            TestFile->Close(TestFile);

            return TRUE;
        }
    }

    return FALSE;
}

EFI_STATUS MrdLoadFile(IN EFI_FILE_PROTOCOL *BaseDir, IN CHAR16 *FileName, OUT UINT8 **FileData,
                       OUT UINTN *FileDataLength)
{
    EFI_STATUS Status;
    UINT64 ReadSize;
    UINTN BufferSize;
    UINT8 *Buffer;
    EFI_FILE_INFO *FileInfo;
    EFI_FILE_HANDLE FileHandle;

    if (BaseDir == NULL || FileName == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    Status = BaseDir->Open(BaseDir, &FileHandle, FileName, MeridianReadOnly, 0);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    FileInfo = LibFileInfo(FileHandle);
    if (FileInfo == NULL) {
        FileHandle->Close(FileHandle);

        return EFI_NOT_FOUND;
    }

    ReadSize = FileInfo->FileSize;
    if (ReadSize > MAX_MERIDIAN_FILE_SIZE) {
        ReadSize = MAX_MERIDIAN_FILE_SIZE;
    }

    MRD_FREE_POOL(FileInfo);

    BufferSize = (UINTN)ReadSize;
    Buffer = (UINT8 *)AllocatePool(BufferSize);
    if (Buffer == NULL) {
        FileHandle->Close(FileHandle);

        return EFI_OUT_OF_RESOURCES;
    }

    Status = FileHandle->Read(FileHandle, &BufferSize, Buffer);
    FileHandle->Close(FileHandle);
    if (EFI_ERROR(Status)) {
        MRD_FREE_POOL(Buffer);

        return Status;
    }

    if (FileData != NULL) {
        *FileData = Buffer;
    }
    else {
        MRD_FREE_POOL(Buffer);
    }

    if (FileDataLength != NULL) {
        *FileDataLength = BufferSize;
    }

#if MERIDIAN_DEBUG > 0
    DEBUG_LOG(1, LOG_THREE_STAR_MID, L"In MrdLoadFile ... Loaded File:- '%s'", FileName);
#endif

    return EFI_SUCCESS;
}

EFI_STATUS MrdFindESP(OUT EFI_FILE_HANDLE *RootDir)
{
    EFI_STATUS Status;
    EFI_HANDLE *Handles;
    UINTN i, HandleCount;

    HandleCount = 0;
    *RootDir = NULL;
    Status = LibLocateHandle(ByProtocol, &GuidESP, NULL, &HandleCount, &Handles);
    if (!EFI_ERROR(Status)) {
        Status = EFI_NOT_FOUND;

        for (i = 0; i < HandleCount; i++) {
            *RootDir = LibOpenRoot(Handles[i]);
            if (*RootDir != NULL) {
                Status = EFI_SUCCESS;

                break;
            }
        }
    }

    MRD_FREE_POOL(Handles);

    return Status;
}

EFI_STATUS MrdSaveFile(IN EFI_FILE_PROTOCOL *BaseDir OPTIONAL, IN CHAR16 *FileName,
                       IN UINT8 *FileData, IN UINTN FileDataLength)
{
    EFI_STATUS Status;
    UINTN BufferSize;
    EFI_FILE_HANDLE FileHandle;

    if (BaseDir == NULL) {
        Status = MrdFindESP(&BaseDir);
        if (EFI_ERROR(Status)) {
            return Status;
        }
    }

    Status = BaseDir->Open(BaseDir, &FileHandle, FileName, MeridianReadWriteCreate, 0);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    if (FileDataLength == 0) {
        Status = FileHandle->Delete(FileHandle);
    }
    else {
        BufferSize = FileDataLength;
        Status = FileHandle->Write(FileHandle, &BufferSize, FileData);
        FileHandle->Close(FileHandle);
    }

    return Status;
}

static EFI_STATUS DirNextEntry(IN EFI_FILE_PROTOCOL *Directory, OUT EFI_FILE_INFO **DirEntry,
                               IN UINTN FilterMode)
{
#if MERIDIAN_DEBUG > 0
    BOOLEAN FirstRun;
    CHAR16 *MsgStr;
    CHAR16 *TmpMsg;
#endif

    EFI_STATUS Status;
    UINTN LastBufferSize;
    UINTN BufferSize;
    VOID *Buffer;
    INTN IterCount;

#if MERIDIAN_DEBUG > 0
    FirstRun = TRUE;
#endif
    while (1) {
        *DirEntry = NULL;

        LastBufferSize = BufferSize = 256;
        Buffer = AllocatePool(BufferSize);
        if (Buffer == NULL) {

            return EFI_BAD_BUFFER_SIZE;
        }

        for (IterCount = 0;; IterCount++) {
            Status = Directory->Read(Directory, &BufferSize, Buffer);

            if (Status != EFI_BUFFER_TOO_SMALL || IterCount > 3) {
#if MERIDIAN_DEBUG > 0
                if (!FirstRun) {
                    if (IterCount > 3) {
                        TmpMsg = L"IterCount > 3";
                    }
                    else {
                        TmpMsg = L"OK";
                    }
                    DEBUG_LOG(1, LOG_LINE_NORMAL, L"%s", TmpMsg);
                    INFO_LOG(":- '%s ... Break'", TmpMsg);
                }
#endif

                break;
            }

#if MERIDIAN_DEBUG > 0
            if (!FirstRun) {
                TmpMsg = L"NOT OK!!";
                DEBUG_LOG(1, LOG_LINE_NORMAL, L"%s", TmpMsg);
                INFO_LOG(":- '%s'", TmpMsg);
            }

            INFO_LOG("\n");
#endif
            if (BufferSize <= LastBufferSize) {
#if MERIDIAN_DEBUG > 0
                MsgStr = PoolPrint(
                    L"Bad Filesystem Driver Buffer Size Request %d (was %d) ... Using %d Instead",
                    BufferSize, LastBufferSize, LastBufferSize * 2);
                DEBUG_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
                INFO_LOG("%s", MsgStr);
                MRD_FREE_POOL(MsgStr);
#endif

                BufferSize = LastBufferSize * 2;
            }
            else {
#if MERIDIAN_DEBUG > 0
                MsgStr = PoolPrint(L"Resizing DirEntry Buffer from %d to %d Bytes", LastBufferSize,
                                   BufferSize);
                DEBUG_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
                INFO_LOG("%s", MsgStr);
                MRD_FREE_POOL(MsgStr);
#endif
            }
#if MERIDIAN_DEBUG > 0
            FirstRun = FALSE;
#endif

            Buffer = EfiReallocatePool(Buffer, LastBufferSize, BufferSize);
            LastBufferSize = BufferSize;
        }

#if MERIDIAN_DEBUG > 0
        FirstRun = TRUE;
#endif

        if (EFI_ERROR(Status)) {

            MRD_FREE_POOL(Buffer);
            break;
        }

        if (BufferSize == 0) {

            MRD_FREE_POOL(Buffer);
            break;
        }

        *DirEntry = (EFI_FILE_INFO *)Buffer;

        if (FilterMode == 1) {

            if (((*DirEntry)->Attribute & EFI_FILE_DIRECTORY)) {

                break;
            }
        }
        else if (FilterMode == 2) {

            if (((*DirEntry)->Attribute & EFI_FILE_DIRECTORY) == 0) {

                break;
            }
        }
        else {

            break;
        }

        MRD_FREE_POOL(*DirEntry);

    }

    return Status;
}

typedef struct _DIR_CACHE_NODE
{
    EFI_FILE_PROTOCOL *BaseDir;
    CHAR16 *Path;
    EFI_FILE_INFO **Entries;
    UINTN Count;
    struct _DIR_CACHE_NODE *Next;
} DIR_CACHE_NODE;

static DIR_CACHE_NODE *gDirCache = NULL;

static DIR_CACHE_NODE *DirCacheFind(IN EFI_FILE_PROTOCOL *BaseDir, IN CHAR16 *KeyPath)
{
    DIR_CACHE_NODE *Node;

    Node = gDirCache;
    while (Node != NULL) {
        if (Node->BaseDir == BaseDir && MrdStrEqualsCI(Node->Path, KeyPath)) {
            return Node;
        }
        Node = Node->Next;
    }

    return NULL;
}

VOID ClearDirCache(VOID)
{
    DIR_CACHE_NODE *Node;
    DIR_CACHE_NODE *NextNode;
    UINTN i;

    Node = gDirCache;
    while (Node != NULL) {
        NextNode = Node->Next;
        if (Node->Entries != NULL) {
            for (i = 0; i < Node->Count; i++) {
                MRD_FREE_POOL(Node->Entries[i]);
            }
            MRD_FREE_POOL(Node->Entries);
        }
        MRD_FREE_POOL(Node->Path);
        MRD_FREE_POOL(Node);
        Node = NextNode;
    }
    gDirCache = NULL;
}

VOID DirIterOpen(IN EFI_FILE_PROTOCOL *BaseDir, IN CHAR16 *RelativePath OPTIONAL,
                 OUT MERIDIAN_DIR_ITER *DirIter)
{
    DIR_CACHE_NODE *Node;
    CHAR16 *KeyPath;
    EFI_FILE_HANDLE Handle;
    BOOLEAN OwnHandle;
    EFI_FILE_INFO *Entry;
    EFI_FILE_INFO **Entries;
    UINTN Count;
    UINTN Capacity;
    EFI_STATUS Status;

    DirIter->LastStatus = EFI_SUCCESS;
    DirIter->DirHandle = NULL;
    DirIter->CloseDirHandle = FALSE;
    DirIter->Cache = NULL;
    DirIter->CacheCount = 0;
    DirIter->CacheIndex = 0;

    KeyPath = (RelativePath != NULL) ? StrDuplicate(RelativePath) : StrDuplicate(L"");
    if (KeyPath == NULL) {
        DirIter->LastStatus = EFI_OUT_OF_RESOURCES;
        return;
    }
    CleanUpPathNameSlashes(KeyPath);

    Node = DirCacheFind(BaseDir, KeyPath);
    if (Node != NULL) {
        DirIter->Cache = Node->Entries;
        DirIter->CacheCount = Node->Count;
        MRD_FREE_POOL(KeyPath);
        return;
    }

    OwnHandle = FALSE;
    Handle = NULL;
    if (RelativePath == NULL) {
        Handle = BaseDir;
    }
    else {
        Status = BaseDir->Open(BaseDir, &Handle, RelativePath, MeridianReadOnly, 0);
        if (EFI_ERROR(Status) || Handle == NULL) {
            DirIter->LastStatus = Status;
            MRD_FREE_POOL(KeyPath);
            return;
        }
        OwnHandle = TRUE;
    }

    Entries = NULL;
    Count = 0;
    Capacity = 0;
    while (TRUE) {
        Entry = NULL;
        Status = DirNextEntry(Handle, &Entry, 0);
        if (EFI_ERROR(Status) || Entry == NULL) {
            break;
        }
        if (Count >= Capacity) {
            UINTN OldBytes = Capacity * sizeof(EFI_FILE_INFO *);
            Capacity = (Capacity == 0) ? 32 : (Capacity * 2);
            Entries = EfiReallocatePool(Entries, OldBytes, Capacity * sizeof(EFI_FILE_INFO *));
            if (Entries == NULL) {
                MRD_FREE_POOL(Entry);
                Count = 0;
                break;
            }
        }
        Entries[Count++] = Entry;
    }

    if (OwnHandle && Handle != NULL && Handle->Close != NULL) {
        Handle->Close(Handle);
    }

    Node = AllocateZeroPool(sizeof(DIR_CACHE_NODE));
    if (Node != NULL) {
        Node->BaseDir = BaseDir;
        Node->Path = KeyPath;
        Node->Entries = Entries;
        Node->Count = Count;
        Node->Next = gDirCache;
        gDirCache = Node;

        DirIter->Cache = Entries;
        DirIter->CacheCount = Count;
    }
    else {

        DirIter->Cache = Entries;
        DirIter->CacheCount = Count;
        MRD_FREE_POOL(KeyPath);

    }

    DirIter->LastStatus = EFI_SUCCESS;
}

static BOOLEAN MeridianWildcardMatch(IN CHAR16 *String, IN CHAR16 *Pattern)
{
    CHAR16 s, p;

    while (*Pattern != L'\0') {
        if (*Pattern == L'*') {
            Pattern++;
            if (*Pattern == L'\0') {

                return TRUE;
            }
            while (*String != L'\0') {
                if (MeridianWildcardMatch(String, Pattern)) {
                    return TRUE;
                }
                String++;
            }

            return MeridianWildcardMatch(String, Pattern);
        }

        if (*Pattern == L'?') {
            if (*String == L'\0') {
                return FALSE;
            }
            String++;
            Pattern++;
            continue;
        }

        s = *String;
        p = *Pattern;
        if (s >= L'a' && s <= L'z')
            s = (CHAR16)(s - (L'a' - L'A'));
        if (p >= L'a' && p <= L'z')
            p = (CHAR16)(p - (L'a' - L'A'));
        if (s != p) {
            return FALSE;
        }
        String++;
        Pattern++;
    }

    return (*String == L'\0');
}

BOOLEAN MeridianMetaiMatch(IN CHAR16 *String, IN CHAR16 *Pattern)
{
    if (String == NULL || Pattern == NULL) {
        return FALSE;
    }

    return MeridianWildcardMatch(String, Pattern);
}

BOOLEAN DirIterNext(IN OUT MERIDIAN_DIR_ITER *DirIter, IN UINTN FilterMode,
                    IN CHAR16 *FilePattern OPTIONAL, OUT EFI_FILE_INFO **DirEntry)
{
    UINTN i;
    BOOLEAN Found;
    BOOLEAN IsDir;
    CHAR16 *OnePattern;
    EFI_FILE_INFO *Info;

    *DirEntry = NULL;

    while (DirIter->CacheIndex < DirIter->CacheCount) {
        Info = DirIter->Cache[DirIter->CacheIndex++];
        if (Info == NULL) {
            continue;
        }

        IsDir = (Info->Attribute & EFI_FILE_DIRECTORY) != 0;

        if (FilterMode == 1 && !IsDir) {
            continue;
        }
        if (FilterMode == 2 && IsDir) {
            continue;
        }

        if (FilePattern == NULL || IsDir) {
            *DirEntry = AllocateCopyPool((UINTN)Info->Size, Info);
            return (*DirEntry != NULL);
        }

        i = 0;
        Found = FALSE;
        while (!Found) {
            OnePattern = FindCommaDelimited(FilePattern, i++);
            if (OnePattern == NULL) {
                break;
            }
            if (MeridianMetaiMatch(Info->FileName, OnePattern)) {
                Found = TRUE;
            }
            MRD_FREE_POOL(OnePattern);
        }

        if (Found) {
            *DirEntry = AllocateCopyPool((UINTN)Info->Size, Info);
            return (*DirEntry != NULL);
        }
    }

    return FALSE;
}

EFI_STATUS DirIterClose(IN OUT MERIDIAN_DIR_ITER *DirIter)
{

    DirIter->DirHandle = NULL;
    DirIter->CloseDirHandle = FALSE;
    DirIter->Cache = NULL;
    DirIter->CacheCount = 0;
    DirIter->CacheIndex = 0;

    return DirIter->LastStatus;
}

CHAR16 *Basename(IN CHAR16 *Path)
{
    CHAR16 *FileName;
    UINTN i;

    FileName = Path;

    if (Path != NULL) {
        for (i = StrLen(Path); i > 0; i--) {
            if (Path[i - 1] == '\\' || Path[i - 1] == '/') {
                FileName = Path + i;
                break;
            }
        }
    }

    return StrDuplicate(FileName);
}

CHAR16 *StripSetExtension(IN CHAR16 *Extension, IN CHAR16 *FileName)
{
    UINTN LengthExt;
    UINTN LengthFile;
    CHAR16 *Copy;

    if (FileName == NULL) {
        return NULL;
    }

    if (!MrdStrStartsWithCI(L".", Extension)) {
        return NULL;
    }

    Copy = StrDuplicate(FileName);
    LengthFile = StrLen(Copy);
    LengthExt = StrLen(Extension);

    if (LengthFile >= LengthExt) {
        if (MrdStrEqualsCI(&Copy[LengthFile - LengthExt], Extension)) {
            Copy[LengthFile - LengthExt] = 0;
        }
    }

    return Copy;
}

CHAR16 *StripEfiExtension(IN CHAR16 *FileName) { return StripSetExtension(L".efi", FileName); }

INTN FindMem(IN VOID *Buffer, IN UINTN BufferLength, IN VOID *SearchString,
             IN UINTN SearchStringLength)
{
    UINT8 *BufferPtr;
    UINTN Offset;

    BufferPtr = Buffer;
    BufferLength -= SearchStringLength;
    for (Offset = 0; Offset < BufferLength; Offset++, BufferPtr++) {
        if (CompareMem(BufferPtr, SearchString, SearchStringLength) == 0) {
            return (INTN)Offset;
        }
    }

    return -1;
}

CHAR16 *FindExtension(IN CHAR16 *Path)
{
    INTN i;
    CHAR16 *Extension;
    BOOLEAN FoundSlash;
    BOOLEAN Found;

    if (Path == NULL) {
        return NULL;
    }

    Extension = AllocateZeroPool(sizeof(CHAR16));
    if (Extension == NULL) {
        return NULL;
    }

    i = StrLen(Path);
    FoundSlash = Found = FALSE;
    while (!Found && !FoundSlash && i >= 0) {
        if (Path[i] == L'.') {
            Found = TRUE;
        }
        else if ((Path[i] == L'/') || (Path[i] == L'\\')) {
            FoundSlash = TRUE;
        }

        if (!Found) {
            i--;
        }
    }

    if (!Found) {
        MRD_FREE_POOL(Extension);
    }
    else {
        MergeStrings(&Extension, &Path[i], 0);
        ToLower(Extension);
    }

    return Extension;
}

CHAR16 *FindLastDirName(IN CHAR16 *Path)
{
    UINTN i;
    UINTN PathLength;
    UINTN CopyLength;
    UINTN EndOfElement;
    UINTN StartOfElement;
    CHAR16 *Found;

    if (Path == NULL) {
        return NULL;
    }

    PathLength = StrLen(Path);

    EndOfElement = StartOfElement = 0;
    for (i = 0; i < PathLength; i++) {
        if (Path[i] == '\\') {
            StartOfElement = EndOfElement;
            EndOfElement = i;
        }
    }

    Found = NULL;
    if (EndOfElement > 0) {
        while ((StartOfElement < PathLength) && (Path[StartOfElement] == '\\')) {
            StartOfElement++;
        }

        EndOfElement--;
        if (EndOfElement >= StartOfElement) {
            CopyLength = EndOfElement - StartOfElement + 1;
            Found = StrDuplicate(&Path[StartOfElement]);
            if (Found != NULL) {
                Found[CopyLength] = 0;
            }
        }
    }

    return Found;
}

CHAR16 *FindPath(IN CHAR16 *FullPath)
{
    UINTN i;
    UINTN LastBackslash;
    CHAR16 *PathOnly;

    if (FullPath == NULL) {
        return NULL;
    }

    LastBackslash = 0;
    for (i = 0; i < StrLen(FullPath); i++) {
        if (FullPath[i] == '\\') {
            LastBackslash = i;
        }
    }

    PathOnly = StrDuplicate(FullPath);
    if (PathOnly != NULL) {
        PathOnly[LastBackslash] = 0;
    }

    return PathOnly;
}

VOID FindVolumeAndFilename(IN EFI_DEVICE_PATH_PROTOCOL *loadpath,
                           OUT MERIDIAN_VOLUME **DeviceVolume, OUT CHAR16 **loader)
{
    CHAR16 *DeviceString, *VolumeDeviceString, *Temp;
    UINTN i;
    BOOLEAN Found;

    if (loader == NULL || loadpath == NULL || DeviceVolume == NULL) {
        return;
    }

    MRD_FREE_POOL(*loader);
    MRD_FREE_POOL(*DeviceVolume);

    DeviceString = DevicePathToStr(loadpath);
    *loader = SplitDeviceString(DeviceString);

    i = 0;
    Found = FALSE;
    while (!Found && i < VolumesCount) {
        if (Volumes[i]->DevicePath == NULL) {
            i++;
            continue;
        }

        VolumeDeviceString = DevicePathToStr(Volumes[i]->DevicePath);
        Temp = SplitDeviceString(VolumeDeviceString);

        if (MrdStrFind(VolumeDeviceString, DeviceString)) {
            Found = TRUE;
            *DeviceVolume = Volumes[i];
        }
        i++;

        MRD_FREE_POOL(Temp);
        MRD_FREE_POOL(VolumeDeviceString);
    }

    MRD_FREE_POOL(DeviceString);
}

BOOLEAN SplitVolumeAndFilename(IN OUT CHAR16 **Path, OUT CHAR16 **VolName)
{
    UINTN Length, i;
    CHAR16 *Filename;

    if (*Path == NULL) {
        return FALSE;
    }

    Length = StrLen(*Path);
    i = 0;
    while ((i < Length) && ((*Path)[i] != L':')) {
        i++;
    }

    if (i >= Length) {
        return FALSE;
    }

    MRD_FREE_POOL(*VolName);

    Filename = StrDuplicate((*Path) + i + 1);
    (*Path)[i] = 0;
    *VolName = *Path;
    *Path = Filename;

    return TRUE;
}

VOID SplitPathName(IN CHAR16 *InPath, IN OUT CHAR16 **VolName, IN OUT CHAR16 **Path,
                   IN OUT CHAR16 **Filename)
{
    CHAR16 *Temp;

    MRD_FREE_POOL(*Path);
    MRD_FREE_POOL(*VolName);
    MRD_FREE_POOL(*Filename);

    Temp = StrDuplicate(InPath);
    SplitVolumeAndFilename(&Temp, VolName);
    CleanUpPathNameSlashes(Temp);

    *Path = FindPath(Temp);
    *Filename = StrDuplicate(Temp + StrLen(*Path));

    CleanUpPathNameSlashes(*Filename);

    if (StrLen(*Path) == 0) {
        MRD_FREE_POOL(*Path);
    }

    if (StrLen(*Filename) == 0) {
        MRD_FREE_POOL(*Filename);
    }

    MRD_FREE_POOL(Temp);
}
