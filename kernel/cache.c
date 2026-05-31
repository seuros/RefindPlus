// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#include "global.h"
#include "lib.h"
#include "gpt.h"
#include "scan.h"
#include "menu.h"
#include "mystrings.h"
#include "version.h"
#include "cache.h"

#define CACHE_FILE_NAME L"menu.cache"
#define CACHE_MAGIC 0x4E454D4E4452454DULL
#define CACHE_VERSION 5

#pragma pack(push, 1)
typedef struct
{
    UINT64 Magic;
    UINT32 FormatVersion;
    UINT32 Fingerprint;
    UINT32 BuildId;
    UINT32 EntryCount;
    UINT32 Flags;
    UINT32 HeaderCrc32;
    UINT32 BodyCrc32;
} CACHE_FILE_HEADER;
#pragma pack(pop)

#define CACHE_HEADER_CRC_LEN (OFFSET_OF(CACHE_FILE_HEADER, HeaderCrc32))

typedef struct
{
    UINT8 OSType;
    UINT32 Tag;
    UINT32 Row;
    EFI_GUID PartGuid;
    EFI_GUID VolUuid;
    CHAR16 *Title;
    CHAR16 *LoaderPath;
    CHAR16 *LoadOptions;
    CHAR16 *InitrdPath;

    UINT64 LoaderSize;
    UINT64 LoaderMtime;
    UINT64 InitrdSize;
    UINT64 InitrdMtime;
} CACHE_RECORD;

UINT32 ComputeBuildId(VOID)
{
    CONST CHAR8 *BuildId = VERSION_STRING_ASCII " " __DATE__ " " __TIME__;
    return CalculateCrc32((VOID *)BuildId, AsciiStrLen(BuildId));
}

UINT32 ComputeTopologyFingerprint(VOID)
{
    GPT_DATA *Walker;
    UINT32 DiskCount;
    UINT32 Fingerprint;
    UINT8 *Buffer;
    UINTN BufLen;
    UINTN Pos;

    UINT8 OcResident = MeridianLaunchedByOpenCore() ? 1 : 0;

    DiskCount = 0;
    for (Walker = gPartitions; Walker != NULL; Walker = Walker->NextEntry) {
        if (Walker->Header != NULL) {
            DiskCount++;
        }
    }

    if (DiskCount == 0) {

        DEBUG_LOG(1, LOG_LINE_NORMAL, L"Menu Cache: Topology fp=0x00000000, disks=0");
        return 0;
    }

    BufLen = sizeof(UINT32) + (UINTN)DiskCount * (16 + sizeof(UINT32)) + 1;
    Buffer = AllocateZeroPool(BufLen);
    if (Buffer == NULL) {
        return 0;
    }

    Pos = 0;
    CopyMem(Buffer + Pos, &DiskCount, sizeof(DiskCount));
    Pos += sizeof(DiskCount);

    for (Walker = gPartitions; Walker != NULL; Walker = Walker->NextEntry) {
        if (Walker->Header == NULL) {
            continue;
        }
        CopyMem(Buffer + Pos, Walker->Header->disk_guid, 16);
        Pos += 16;
        CopyMem(Buffer + Pos, &(Walker->Header->entry_crc32), sizeof(UINT32));
        Pos += sizeof(UINT32);
    }

    Buffer[Pos] = OcResident;
    Pos += 1;

    Fingerprint = CalculateCrc32(Buffer, BufLen);
    MRD_FREE_POOL(Buffer);

    if (Fingerprint == 0) {
        Fingerprint = 0xFFFFFFFF;
    }

    DEBUG_LOG(1, LOG_LINE_NORMAL, L"Menu Cache: Topology fp=0x%08x, disks=%d, oc=%d", Fingerprint,
              DiskCount, OcResident);

    return Fingerprint;
}

static VOID AppendBytes(IN OUT UINT8 **Buf, IN OUT UINTN *Len, IN OUT UINTN *Cap,
                        IN CONST VOID *Data, IN UINTN Count)
{
    UINTN NewCap;

    if (*Len + Count > *Cap) {
        NewCap = (*Cap == 0) ? 1024 : *Cap;
        while (NewCap < *Len + Count) {
            NewCap *= 2;
        }
        *Buf = ReallocatePool(*Cap, NewCap, *Buf);
        *Cap = NewCap;
    }
    CopyMem(*Buf + *Len, Data, Count);
    *Len += Count;
}

static VOID AppendU32(IN OUT UINT8 **Buf, IN OUT UINTN *Len, IN OUT UINTN *Cap, IN UINT32 Value)
{
    AppendBytes(Buf, Len, Cap, &Value, sizeof(Value));
}

static VOID AppendU64(IN OUT UINT8 **Buf, IN OUT UINTN *Len, IN OUT UINTN *Cap, IN UINT64 Value)
{
    AppendBytes(Buf, Len, Cap, &Value, sizeof(Value));
}

static VOID AppendStr(IN OUT UINT8 **Buf, IN OUT UINTN *Len, IN OUT UINTN *Cap, IN CHAR16 *Str)
{
    UINT32 ByteLen;

    ByteLen = (Str != NULL) ? (UINT32)(StrLen(Str) * sizeof(CHAR16)) : 0;
    AppendU32(Buf, Len, Cap, ByteLen);
    if (ByteLen > 0) {
        AppendBytes(Buf, Len, Cap, Str, ByteLen);
    }
}

typedef struct
{
    UINT8 *Cur;
    UINTN Remaining;
} CACHE_CURSOR;

static BOOLEAN ReadRaw(IN OUT CACHE_CURSOR *C, OUT VOID *Out, IN UINTN Count)
{
    if (C->Remaining < Count) {
        return FALSE;
    }
    CopyMem(Out, C->Cur, Count);
    C->Cur += Count;
    C->Remaining -= Count;
    return TRUE;
}

static BOOLEAN ReadStr(IN OUT CACHE_CURSOR *C, OUT CHAR16 **Out)
{
    UINT32 ByteLen;
    CHAR16 *Str;

    *Out = NULL;
    if (!ReadRaw(C, &ByteLen, sizeof(ByteLen))) {
        return FALSE;
    }
    if (ByteLen == 0) {

        return TRUE;
    }
    if ((ByteLen % sizeof(CHAR16)) != 0 || ByteLen > C->Remaining) {
        return FALSE;
    }

    Str = AllocateZeroPool(ByteLen + sizeof(CHAR16));
    if (Str == NULL) {
        return FALSE;
    }
    CopyMem(Str, C->Cur, ByteLen);
    C->Cur += ByteLen;
    C->Remaining -= ByteLen;
    *Out = Str;
    return TRUE;
}

static VOID FreeRecords(IN CACHE_RECORD *Records, IN UINTN Count)
{
    UINTN i;

    if (Records == NULL) {
        return;
    }
    for (i = 0; i < Count; i++) {
        MRD_FREE_POOL(Records[i].Title);
        MRD_FREE_POOL(Records[i].LoaderPath);
        MRD_FREE_POOL(Records[i].LoadOptions);
        MRD_FREE_POOL(Records[i].InitrdPath);
    }
    MRD_FREE_POOL(Records);
}

static EFI_STATUS SerializeRecords(IN CACHE_RECORD *Records, IN UINTN Count, IN UINT32 Fingerprint,
                                   IN UINT32 BuildId, OUT UINT8 **OutBuf, OUT UINTN *OutLen)
{
    UINT8 *Body;
    UINTN BodyLen;
    UINTN BodyCap;
    UINTN i;
    UINT8 *FileBuf;
    UINTN FileLen;
    CACHE_FILE_HEADER Header;

    *OutBuf = NULL;
    *OutLen = 0;

    Body = NULL;
    BodyLen = 0;
    BodyCap = 0;

    for (i = 0; i < Count; i++) {
        AppendBytes(&Body, &BodyLen, &BodyCap, &(Records[i].OSType), sizeof(UINT8));
        AppendU32(&Body, &BodyLen, &BodyCap, Records[i].Tag);
        AppendU32(&Body, &BodyLen, &BodyCap, Records[i].Row);
        AppendBytes(&Body, &BodyLen, &BodyCap, &(Records[i].PartGuid), sizeof(EFI_GUID));
        AppendBytes(&Body, &BodyLen, &BodyCap, &(Records[i].VolUuid), sizeof(EFI_GUID));
        AppendStr(&Body, &BodyLen, &BodyCap, Records[i].Title);
        AppendStr(&Body, &BodyLen, &BodyCap, Records[i].LoaderPath);
        AppendStr(&Body, &BodyLen, &BodyCap, Records[i].LoadOptions);
        AppendStr(&Body, &BodyLen, &BodyCap, Records[i].InitrdPath);
        AppendU64(&Body, &BodyLen, &BodyCap, Records[i].LoaderSize);
        AppendU64(&Body, &BodyLen, &BodyCap, Records[i].LoaderMtime);
        AppendU64(&Body, &BodyLen, &BodyCap, Records[i].InitrdSize);
        AppendU64(&Body, &BodyLen, &BodyCap, Records[i].InitrdMtime);
    }

    ZeroMem(&Header, sizeof(Header));
    Header.Magic = CACHE_MAGIC;
    Header.FormatVersion = CACHE_VERSION;
    Header.Fingerprint = Fingerprint;
    Header.BuildId = BuildId;
    Header.EntryCount = (UINT32)Count;
    Header.Flags = 0;
    Header.HeaderCrc32 = CalculateCrc32(&Header, CACHE_HEADER_CRC_LEN);
    Header.BodyCrc32 = (BodyLen > 0) ? CalculateCrc32(Body, BodyLen) : 0;

    FileLen = sizeof(Header) + BodyLen;
    FileBuf = AllocatePool(FileLen);
    if (FileBuf == NULL) {
        MRD_FREE_POOL(Body);
        return EFI_OUT_OF_RESOURCES;
    }

    CopyMem(FileBuf, &Header, sizeof(Header));
    if (BodyLen > 0) {
        CopyMem(FileBuf + sizeof(Header), Body, BodyLen);
    }
    MRD_FREE_POOL(Body);

    *OutBuf = FileBuf;
    *OutLen = FileLen;
    return EFI_SUCCESS;
}

static EFI_STATUS ParseBuffer(IN UINT8 *Buf, IN UINTN Len, IN UINT32 ExpectedFingerprint,
                              IN UINT32 ExpectedBuildId, OUT CACHE_RECORD **OutRecords,
                              OUT UINTN *OutCount, OUT CONST CHAR16 **FailReason OPTIONAL)
{
    CACHE_FILE_HEADER Header;
    UINT8 *Body;
    UINTN BodyLen;
    UINT32 BodyCrc;
    CACHE_RECORD *Records;
    CACHE_CURSOR Cursor;
    UINTN i;

    *OutRecords = NULL;
    *OutCount = 0;

#define FAIL(reason, status)                                                                       \
    do {                                                                                           \
        if (FailReason != NULL)                                                                    \
            *FailReason = (reason);                                                                \
        return (status);                                                                           \
    } while (0)

    if (Buf == NULL || Len < sizeof(CACHE_FILE_HEADER)) {
        FAIL(L"truncated/empty", EFI_NOT_FOUND);
    }

    CopyMem(&Header, Buf, sizeof(Header));

    if (Header.Magic != CACHE_MAGIC) {
        FAIL(L"bad magic", EFI_NOT_FOUND);
    }
    if (Header.FormatVersion != CACHE_VERSION) {
        FAIL(L"version mismatch", EFI_INCOMPATIBLE_VERSION);
    }
    if (Header.HeaderCrc32 != CalculateCrc32(&Header, CACHE_HEADER_CRC_LEN)) {
        FAIL(L"header crc", EFI_CRC_ERROR);
    }

    if (Header.BuildId != ExpectedBuildId) {
        FAIL(L"build changed", EFI_INCOMPATIBLE_VERSION);
    }

    Body = Buf + sizeof(Header);
    BodyLen = Len - sizeof(Header);
    BodyCrc = (BodyLen > 0) ? CalculateCrc32(Body, BodyLen) : 0;
    if (Header.BodyCrc32 != BodyCrc) {
        FAIL(L"body crc", EFI_CRC_ERROR);
    }
    if (Header.Fingerprint != ExpectedFingerprint) {
        FAIL(L"fp mismatch", EFI_NOT_FOUND);
    }

    if (Header.EntryCount == 0) {

        FAIL(L"empty", EFI_NOT_FOUND);
    }

    Records = AllocateZeroPool((UINTN)Header.EntryCount * sizeof(CACHE_RECORD));
    if (Records == NULL) {
        FAIL(L"no memory", EFI_OUT_OF_RESOURCES);
    }

    Cursor.Cur = Body;
    Cursor.Remaining = BodyLen;

    for (i = 0; i < Header.EntryCount; i++) {
        if (!ReadRaw(&Cursor, &(Records[i].OSType), sizeof(UINT8)) ||
            !ReadRaw(&Cursor, &(Records[i].Tag), sizeof(UINT32)) ||
            !ReadRaw(&Cursor, &(Records[i].Row), sizeof(UINT32)) ||
            !ReadRaw(&Cursor, &(Records[i].PartGuid), sizeof(EFI_GUID)) ||
            !ReadRaw(&Cursor, &(Records[i].VolUuid), sizeof(EFI_GUID)) ||
            !ReadStr(&Cursor, &(Records[i].Title)) || !ReadStr(&Cursor, &(Records[i].LoaderPath)) ||
            !ReadStr(&Cursor, &(Records[i].LoadOptions)) ||
            !ReadStr(&Cursor, &(Records[i].InitrdPath)) ||
            !ReadRaw(&Cursor, &(Records[i].LoaderSize), sizeof(UINT64)) ||
            !ReadRaw(&Cursor, &(Records[i].LoaderMtime), sizeof(UINT64)) ||
            !ReadRaw(&Cursor, &(Records[i].InitrdSize), sizeof(UINT64)) ||
            !ReadRaw(&Cursor, &(Records[i].InitrdMtime), sizeof(UINT64))) {
            FreeRecords(Records, Header.EntryCount);
            FAIL(L"corrupt record", EFI_CRC_ERROR);
        }
    }

#undef FAIL

    *OutRecords = Records;
    *OutCount = Header.EntryCount;
    return EFI_SUCCESS;
}

static UINT64 PackEfiTime(IN EFI_TIME *T)
{
    return ((UINT64)T->Year << 48) | ((UINT64)T->Month << 40) | ((UINT64)T->Day << 32) |
           ((UINT64)T->Hour << 24) | ((UINT64)T->Minute << 16) | ((UINT64)T->Second << 8);
}

static BOOLEAN StatPath(IN EFI_FILE_PROTOCOL *BaseDir, IN CHAR16 *Path, OUT UINT64 *Size,
                        OUT UINT64 *Mtime)
{
    EFI_STATUS Status;
    EFI_FILE_HANDLE Handle;
    EFI_FILE_INFO *Info;

    *Size = 0;
    *Mtime = 0;
    if (BaseDir == NULL || Path == NULL) {
        return FALSE;
    }
    Status = BaseDir->Open(BaseDir, &Handle, Path, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status)) {
        return FALSE;
    }
    Info = LibFileInfo(Handle);
    Handle->Close(Handle);
    if (Info == NULL) {
        return FALSE;
    }
    *Size = Info->FileSize;
    *Mtime = PackEfiTime(&(Info->ModificationTime));
    MRD_FREE_POOL(Info);
    return TRUE;
}

static VOID StampRecord(IN MERIDIAN_VOLUME *Volume, IN OUT CACHE_RECORD *Record)
{
    UINT64 Size;
    UINT64 Mtime;

    Record->LoaderSize = Record->LoaderMtime = 0;
    Record->InitrdSize = Record->InitrdMtime = 0;

    if (Volume == NULL || Volume->RootDir == NULL) {
        return;
    }
    if (Record->LoaderPath != NULL &&
        StatPath(Volume->RootDir, Record->LoaderPath, &Size, &Mtime)) {
        Record->LoaderSize = Size;
        Record->LoaderMtime = Mtime;
    }
    if (Record->InitrdPath != NULL &&
        StatPath(Volume->RootDir, Record->InitrdPath, &Size, &Mtime)) {
        Record->InitrdSize = Size;
        Record->InitrdMtime = Mtime;
    }
}

static BOOLEAN RecordContentUnchanged(IN MERIDIAN_VOLUME *Volume, IN CACHE_RECORD *Record)
{
    UINT64 Size;
    UINT64 Mtime;

    if (Volume == NULL || Volume->RootDir == NULL) {
        return TRUE;
    }

    if (Record->LoaderPath != NULL && (Record->LoaderSize != 0 || Record->LoaderMtime != 0)) {
        if (!StatPath(Volume->RootDir, Record->LoaderPath, &Size, &Mtime)) {
            return FALSE;
        }
        if (Size != Record->LoaderSize || Mtime != Record->LoaderMtime) {
            return FALSE;
        }
    }
    if (Record->InitrdPath != NULL && (Record->InitrdSize != 0 || Record->InitrdMtime != 0)) {
        if (!StatPath(Volume->RootDir, Record->InitrdPath, &Size, &Mtime)) {
            return FALSE;
        }
        if (Size != Record->InitrdSize || Mtime != Record->InitrdMtime) {
            return FALSE;
        }
    }
    return TRUE;
}

static BOOLEAN GuidIsZero(IN EFI_GUID *Guid)
{
    STATIC CONST EFI_GUID ZeroGuid = {0};
    return GuidsAreEqual(Guid, (EFI_GUID *)&ZeroGuid);
}

static MERIDIAN_VOLUME *ResolveVolume(IN CACHE_RECORD *Record)
{
    UINTN i;
    BOOLEAN HavePart;
    BOOLEAN HaveVol;

    HavePart = !GuidIsZero(&(Record->PartGuid));
    HaveVol = !GuidIsZero(&(Record->VolUuid));

    if (HavePart && HaveVol) {
        for (i = 0; i < VolumesCount; i++) {
            if (Volumes[i] != NULL && GuidsAreEqual(&(Volumes[i]->PartGuid), &(Record->PartGuid)) &&
                GuidsAreEqual(&(Volumes[i]->VolUuid), &(Record->VolUuid))) {
                return Volumes[i];
            }
        }
    }
    if (HavePart) {
        for (i = 0; i < VolumesCount; i++) {
            if (Volumes[i] != NULL && GuidsAreEqual(&(Volumes[i]->PartGuid), &(Record->PartGuid))) {
                return Volumes[i];
            }
        }
    }
    if (HaveVol) {
        for (i = 0; i < VolumesCount; i++) {
            if (Volumes[i] != NULL && GuidsAreEqual(&(Volumes[i]->VolUuid), &(Record->VolUuid))) {
                return Volumes[i];
            }
        }
    }
    return NULL;
}

EFI_STATUS CacheStoreMenu(IN MERIDIAN_MENU_SCREEN *Menu, IN UINT32 Fingerprint)
{
    EFI_STATUS Status;
    CACHE_RECORD *Records;
    UINTN Count;
    UINTN i;
    LOADER_ENTRY *Loader;
    UINT8 *FileBuf;
    UINTN FileLen;

    if (Menu == NULL || Menu->EntryCount == 0) {
        return EFI_INVALID_PARAMETER;
    }

    Records = AllocateZeroPool(Menu->EntryCount * sizeof(CACHE_RECORD));
    if (Records == NULL) {
        return EFI_OUT_OF_RESOURCES;
    }

    Count = 0;
    for (i = 0; i < Menu->EntryCount; i++) {
        if (Menu->Entries[i] == NULL || Menu->Entries[i]->Tag != TAG_LOADER) {
            continue;
        }
        Loader = (LOADER_ENTRY *)Menu->Entries[i];

        Records[Count].OSType = (UINT8)Loader->OSType;
        Records[Count].Tag = (UINT32)Loader->me.Tag;
        Records[Count].Row = (UINT32)Loader->me.Row;
        if (Loader->Volume != NULL) {
            CopyMem(&(Records[Count].PartGuid), &(Loader->Volume->PartGuid), sizeof(EFI_GUID));
            CopyMem(&(Records[Count].VolUuid), &(Loader->Volume->VolUuid), sizeof(EFI_GUID));
        }

        Records[Count].Title = (Loader->Title != NULL) ? Loader->Title : Loader->me.Title;
        Records[Count].LoaderPath = Loader->LoaderPath;
        Records[Count].LoadOptions = Loader->LoadOptions;
        Records[Count].InitrdPath = Loader->InitrdPath;

        StampRecord(Loader->Volume, &(Records[Count]));
        Count++;
    }

    if (Count == 0) {
        MRD_FREE_POOL(Records);
        DEBUG_LOG(1, LOG_LINE_NORMAL, L"Menu Cache: store skipped (no loader entries)");
        return EFI_NOT_FOUND;
    }

    Status = SerializeRecords(Records, Count, Fingerprint, ComputeBuildId(), &FileBuf, &FileLen);

    MRD_FREE_POOL(Records);
    if (EFI_ERROR(Status)) {
        DEBUG_LOG(1, LOG_LINE_NORMAL, L"Menu Cache: store FAILED (non-fatal): serialise %r",
                  Status);
        return Status;
    }

    Status = MrdSaveFile(SelfDir, CACHE_FILE_NAME, FileBuf, FileLen);
    MRD_FREE_POOL(FileBuf);

    if (EFI_ERROR(Status)) {
        DEBUG_LOG(1, LOG_LINE_NORMAL, L"Menu Cache: store FAILED (non-fatal): %r", Status);
    }
    else {
        DEBUG_LOG(1, LOG_LINE_NORMAL, L"Menu Cache: stored %d entries (%d bytes), fp=0x%08x", Count,
                  FileLen, Fingerprint);
    }
    return Status;
}

EFI_STATUS CacheLoadMenu(IN UINT32 ExpectedFingerprint)
{
    EFI_STATUS Status;
    UINT8 *FileBuf;
    UINTN FileLen;
    CACHE_RECORD *Records;
    UINTN Count;
    UINTN i;
    CONST CHAR16 *FailReason;
    MERIDIAN_VOLUME *Volume;
    LOADER_ENTRY *Built;
    UINTN Rebuilt;

    if (ExpectedFingerprint == 0) {
        DEBUG_LOG(1, LOG_LINE_NORMAL, L"Menu Cache: MISS - no topology fingerprint");
        return EFI_NOT_FOUND;
    }

    FileBuf = NULL;
    FileLen = 0;
    Status = MrdLoadFile(SelfDir, CACHE_FILE_NAME, &FileBuf, &FileLen);
    if (EFI_ERROR(Status)) {
        DEBUG_LOG(1, LOG_LINE_NORMAL, L"Menu Cache: MISS - no file (%r)", Status);
        return EFI_NOT_FOUND;
    }

    FailReason = L"unknown";
    Status = ParseBuffer(FileBuf, FileLen, ExpectedFingerprint, ComputeBuildId(), &Records, &Count,
                         &FailReason);
    MRD_FREE_POOL(FileBuf);

    if (EFI_ERROR(Status)) {

        if (Status == EFI_INCOMPATIBLE_VERSION) {
            DEBUG_LOG(1, LOG_LINE_NORMAL, L"Menu Cache: %s -> invalidating", FailReason);
            CacheInvalidate();
        }
        else {
            DEBUG_LOG(1, LOG_LINE_NORMAL, L"Menu Cache: MISS - %s (fp=0x%08x)", FailReason,
                      ExpectedFingerprint);
        }
        return EFI_NOT_FOUND;
    }

    for (i = 0; i < Count; i++) {
        Volume = ResolveVolume(&(Records[i]));
        if (Volume == NULL) {
            DEBUG_LOG(1, LOG_LINE_NORMAL, L"Menu Cache: STALE volume gone for '%s' -> full rescan",
                      Records[i].Title ? Records[i].Title : L"?");
            FreeRecords(Records, Count);
            return EFI_NOT_FOUND;
        }
        if (Records[i].LoaderPath != NULL && !FileExists(Volume->RootDir, Records[i].LoaderPath)) {
            DEBUG_LOG(1, LOG_LINE_NORMAL, L"Menu Cache: STALE loader '%s' missing -> full rescan",
                      Records[i].LoaderPath);
            FreeRecords(Records, Count);
            return EFI_NOT_FOUND;
        }
        if (!RecordContentUnchanged(Volume, &(Records[i]))) {
            DEBUG_LOG(1, LOG_LINE_NORMAL, L"Menu Cache: STALE content for '%s' -> full rescan",
                      Records[i].LoaderPath ? Records[i].LoaderPath : L"?");
            FreeRecords(Records, Count);
            return EFI_NOT_FOUND;
        }
    }

    Rebuilt = 0;
    for (i = 0; i < Count; i++) {
        Volume = ResolveVolume(&(Records[i]));
        if (Volume == NULL) {
            continue;
        }

        Built = AddLoaderEntry(Records[i].LoaderPath, Records[i].Title, Volume,
                               TRUE,
                               FALSE,
                               Records[i].LoadOptions);
        if (Built != NULL) {
            Rebuilt++;
        }
    }

    FreeRecords(Records, Count);

    if (Rebuilt == 0) {
        DEBUG_LOG(1, LOG_LINE_NORMAL, L"Menu Cache: MISS - rebuilt 0 entries -> full rescan");
        return EFI_NOT_FOUND;
    }

    DEBUG_LOG(1, LOG_LINE_NORMAL, L"Menu Cache: HIT - rebuilt %d loader entries (scan skipped)",
              Rebuilt);
    return EFI_SUCCESS;
}

VOID CacheInvalidate(VOID)
{

    MrdSaveFile(SelfDir, CACHE_FILE_NAME, NULL, 0);
    DEBUG_LOG(1, LOG_LINE_NORMAL, L"Menu Cache: invalidated");
}

#if MERIDIAN_DEBUG > 0
VOID CacheSelfTest(VOID)
{
    EFI_STATUS Status;
    CACHE_RECORD In[2];
    CACHE_RECORD *Out;
    UINTN OutCount;
    UINT8 *Buf;
    UINTN BufLen;
    CONST CHAR16 *FailReason;
    BOOLEAN Ok;

    ZeroMem(In, sizeof(In));
    In[0].OSType = 'L';
    In[0].Tag = TAG_LOADER;
    In[0].Row = 0;
    In[0].Title = L"Instance: Linux - Arch";
    In[0].LoaderPath = L"\\EFI\\arch\\vmlinuz-linux";
    In[0].LoadOptions = L"root=PARTUUID=abc rw";
    In[0].InitrdPath = L"\\EFI\\arch\\initramfs-linux.img";
    In[0].LoaderSize = 0x00A1B2C3;
    In[0].LoaderMtime = 0x07E9060F0C1E0000ULL;
    In[0].InitrdSize = 0x0044AA55;
    In[0].InitrdMtime = 0x07E9060F0C1F0000ULL;

    In[1].OSType = 'M';
    In[1].Tag = TAG_LOADER;
    In[1].Row = 0;
    In[1].Title = L"Load macOS";
    In[1].LoaderPath = L"\\System\\Library\\CoreServices\\boot.efi";
    In[1].LoadOptions = NULL;
    In[1].InitrdPath = NULL;

    Buf = NULL;
    BufLen = 0;
    Status = SerializeRecords(In, 2, 0xDEADBEEF, 0xB00B1D5, &Buf, &BufLen);
    if (EFI_ERROR(Status)) {
        DEBUG_LOG(1, LOG_LINE_NORMAL, L"Menu Cache: self-test FAIL (serialise %r)", Status);
        return;
    }

    FailReason = L"unknown";
    Status = ParseBuffer(Buf, BufLen, 0xDEADBEEF, 0xB00B1D5, &Out, &OutCount, &FailReason);
    MRD_FREE_POOL(Buf);
    if (EFI_ERROR(Status)) {
        DEBUG_LOG(1, LOG_LINE_NORMAL, L"Menu Cache: self-test FAIL (parse %s)", FailReason);
        return;
    }

    Ok = (OutCount == 2) && (Out[0].OSType == 'L') && (Out[1].OSType == 'M') &&
         MrdStrEqualsCI(Out[0].Title, In[0].Title) &&
         MrdStrEqualsCI(Out[0].LoaderPath, In[0].LoaderPath) &&
         MrdStrEqualsCI(Out[0].LoadOptions, In[0].LoadOptions) &&
         MrdStrEqualsCI(Out[0].InitrdPath, In[0].InitrdPath) &&
         (Out[0].LoaderSize == In[0].LoaderSize) && (Out[0].LoaderMtime == In[0].LoaderMtime) &&
         (Out[0].InitrdSize == In[0].InitrdSize) && (Out[0].InitrdMtime == In[0].InitrdMtime) &&
         MrdStrEqualsCI(Out[1].Title, In[1].Title) && (Out[1].LoadOptions == NULL) &&
         (Out[1].InitrdPath == NULL) && (Out[1].LoaderSize == 0) && (Out[1].InitrdMtime == 0);

    if (Ok) {
        CACHE_RECORD *Neg;
        UINTN NegCount;
        UINT8 *Buf2;
        UINTN Buf2Len;

        Buf2 = NULL;
        if (!EFI_ERROR(SerializeRecords(In, 2, 0xDEADBEEF, 0xB00B1D5, &Buf2, &Buf2Len))) {

            Status =
                ParseBuffer(Buf2, Buf2Len, 0x12345678, 0xB00B1D5, &Neg, &NegCount, &FailReason);
            if (!EFI_ERROR(Status)) {
                FreeRecords(Neg, NegCount);
                Ok = FALSE;
            }

            Status =
                ParseBuffer(Buf2, Buf2Len, 0xDEADBEEF, 0xBADB111D, &Neg, &NegCount, &FailReason);
            if (Status != EFI_INCOMPATIBLE_VERSION) {
                if (!EFI_ERROR(Status)) {
                    FreeRecords(Neg, NegCount);
                }
                Ok = FALSE;
            }
            MRD_FREE_POOL(Buf2);
        }
    }

    FreeRecords(Out, OutCount);

    DEBUG_LOG(1, LOG_LINE_NORMAL, L"Menu Cache: self-test %s", Ok ? L"PASS" : L"FAIL");
}
#endif
