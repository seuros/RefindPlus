// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#include "limine_boot.h"
#include "lib.h"
#include "display.h"
#include "version.h"

#define LIMINE_PAGE_SIZE 0x1000ULL

#define TO_VIRT(Ctx, Ptr) ((UINT64)(UINTN)(Ptr) + (Ctx)->HhdmOffset)

#define LIMINE_MEMMAP_SLACK 64

static EFI_GUID LimineAcpi20Guid = {
    0x8868e871, 0xe4f1, 0x11d3, {0xbc, 0x22, 0x00, 0x80, 0xc7, 0x3c, 0x88, 0x81}};
static EFI_GUID LimineAcpi10Guid = {
    0xeb9d2d30, 0x2d88, 0x11d3, {0x9a, 0x16, 0x00, 0x90, 0x27, 0x3f, 0xc1, 0x4d}};
static EFI_GUID LimineSmbiosGuid = {
    0xeb9d2d31, 0x2d88, 0x11d3, {0x9a, 0x16, 0x00, 0x90, 0x27, 0x3f, 0xc1, 0x4d}};
static EFI_GUID LimineSmbios3Guid = {
    0xf2fd1544, 0x9794, 0x4a2c, {0x99, 0x2e, 0xe5, 0xbb, 0xcf, 0x20, 0xe3, 0x94}};

VOID *LimineArenaAlloc(IN OUT LIMINE_CTX *Ctx, IN UINTN Size, IN UINTN Align)
{
    UINTN Offset;
    VOID *Result;

    if (Ctx == NULL || Ctx->Arena == NULL || Align == 0) {
        return NULL;
    }

    Offset = (Ctx->ArenaUsed + Align - 1) & ~(Align - 1);

    if (Offset + Size > Ctx->ArenaSize) {

        return NULL;
    }

    Result = Ctx->Arena + Offset;
    Ctx->ArenaUsed = Offset + Size;

    ZeroMem(Result, Size);

    return Result;
}

static UINT64 ArenaAsciiFromUcs2(IN OUT LIMINE_CTX *Ctx, IN CHAR16 *Src)
{
    UINTN Len;
    UINTN i;
    CHAR8 *Dst;

    if (Src == NULL) {
        return 0;
    }

    Len = StrLen(Src);

    Dst = (CHAR8 *)LimineArenaAlloc(Ctx, Len + 1, 1);
    if (Dst == NULL) {
        return 0;
    }

    for (i = 0; i < Len; i++) {
        Dst[i] = (Src[i] < 0x80) ? (CHAR8)Src[i] : '?';
    }
    Dst[Len] = '\0';

    return TO_VIRT(Ctx, Dst);
}

static UINT64 ArenaAscii(IN OUT LIMINE_CTX *Ctx, IN CHAR8 *Src)
{
    UINTN Len;
    CHAR8 *Dst;

    if (Src == NULL) {
        return 0;
    }

    Len = AsciiStrLen(Src);

    Dst = (CHAR8 *)LimineArenaAlloc(Ctx, Len + 1, 1);
    if (Dst == NULL) {
        return 0;
    }

    CopyMem(Dst, Src, Len + 1);

    return TO_VIRT(Ctx, Dst);
}

static VOID AnswerBootloaderInfo(IN OUT LIMINE_CTX *Ctx, IN OUT LIMINE_REQUEST *Req)
{
    LIMINE_BOOTLOADER_INFO_RESPONSE *Rsp;

    Rsp = LimineArenaAlloc(Ctx, sizeof(*Rsp), 8);
    if (Rsp == NULL) {
        return;
    }

    Rsp->Revision = 0;
    Rsp->Name = ArenaAscii(Ctx, (CHAR8 *)"Meridian");
    Rsp->Version = ArenaAscii(Ctx, (CHAR8 *)VERSION_STRING_ASCII);

    Req->Response = TO_VIRT(Ctx, Rsp);
}

static VOID AnswerCmdline(IN OUT LIMINE_CTX *Ctx, IN OUT LIMINE_REQUEST *Req)
{
    LIMINE_EXECUTABLE_CMDLINE_RESPONSE *Rsp;

    Rsp = LimineArenaAlloc(Ctx, sizeof(*Rsp), 8);
    if (Rsp == NULL) {
        return;
    }

    Rsp->Revision = 0;
    Rsp->Cmdline = (Ctx->Cmdline != NULL) ? ArenaAsciiFromUcs2(Ctx, Ctx->Cmdline)
                                          : ArenaAscii(Ctx, (CHAR8 *)"");

    Req->Response = TO_VIRT(Ctx, Rsp);
}

static VOID AnswerFirmwareType(IN OUT LIMINE_CTX *Ctx, IN OUT LIMINE_REQUEST *Req)
{
    LIMINE_FIRMWARE_TYPE_RESPONSE *Rsp;

    Rsp = LimineArenaAlloc(Ctx, sizeof(*Rsp), 8);
    if (Rsp == NULL) {
        return;
    }

    Rsp->Revision = 0;

    Rsp->FirmwareType = LIMINE_FIRMWARE_TYPE_EFI64;

    Req->Response = TO_VIRT(Ctx, Rsp);
}

static VOID AnswerHhdm(IN OUT LIMINE_CTX *Ctx, IN OUT LIMINE_REQUEST *Req)
{
    LIMINE_HHDM_RESPONSE *Rsp;

    Rsp = LimineArenaAlloc(Ctx, sizeof(*Rsp), 8);
    if (Rsp == NULL) {
        return;
    }

    Rsp->Revision = 0;
    Rsp->Offset = Ctx->HhdmOffset;

    Req->Response = TO_VIRT(Ctx, Rsp);
}

static VOID AnswerExecutableAddress(IN OUT LIMINE_CTX *Ctx, IN OUT LIMINE_REQUEST *Req)
{
    LIMINE_EXECUTABLE_ADDRESS_RESPONSE *Rsp;

    Rsp = LimineArenaAlloc(Ctx, sizeof(*Rsp), 8);
    if (Rsp == NULL) {
        return;
    }

    Rsp->Revision = 0;
    Rsp->PhysicalBase = Ctx->ImagePhys;
    Rsp->VirtualBase = Ctx->ImageVirt;

    Req->Response = TO_VIRT(Ctx, Rsp);
}

static VOID AnswerStackSize(IN OUT LIMINE_CTX *Ctx, IN OUT LIMINE_REQUEST *Req)
{
    LIMINE_STACK_SIZE_REQUEST *Ssr;
    LIMINE_STACK_SIZE_RESPONSE *Rsp;

    Ssr = (LIMINE_STACK_SIZE_REQUEST *)Req;

    if (Ssr->StackSize > Ctx->StackSize) {
        Ctx->StackSize = Ssr->StackSize;
    }

    Rsp = LimineArenaAlloc(Ctx, sizeof(*Rsp), 8);
    if (Rsp == NULL) {
        return;
    }

    Rsp->Revision = 0;

    Req->Response = TO_VIRT(Ctx, Rsp);
}

static VOID AnswerEntryPoint(IN OUT LIMINE_CTX *Ctx, IN OUT LIMINE_REQUEST *Req)
{
    LIMINE_ENTRY_POINT_REQUEST *Epr;
    LIMINE_ENTRY_POINT_RESPONSE *Rsp;

    Epr = (LIMINE_ENTRY_POINT_REQUEST *)Req;

    if (Epr->Entry != 0) {
        Ctx->EntryPoint = Epr->Entry;
    }

    Rsp = LimineArenaAlloc(Ctx, sizeof(*Rsp), 8);
    if (Rsp == NULL) {
        return;
    }

    Rsp->Revision = 0;

    Req->Response = TO_VIRT(Ctx, Rsp);
}

static VOID AnswerPagingMode(IN OUT LIMINE_CTX *Ctx, IN OUT LIMINE_REQUEST *Req)
{
    LIMINE_PAGING_MODE_RESPONSE *Rsp;

    Rsp = LimineArenaAlloc(Ctx, sizeof(*Rsp), 8);
    if (Rsp == NULL) {
        return;
    }

    Rsp->Revision = 0;
    Rsp->Mode = LIMINE_PAGING_MODE_X86_64_4LVL;

    Req->Response = TO_VIRT(Ctx, Rsp);
}

static VOID AnswerFramebuffer(IN OUT LIMINE_CTX *Ctx, IN OUT LIMINE_REQUEST *Req)
{
    EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info;
    LIMINE_FRAMEBUFFER_RESPONSE *Rsp;
    LIMINE_FRAMEBUFFER *Fb;
    UINT64 *Array;
    UINT32 Bpp;

    Gop = GOPDraw;
    if (Gop == NULL || Gop->Mode == NULL || Gop->Mode->Info == NULL) {

        return;
    }

    Info = Gop->Mode->Info;

    if (Info->PixelFormat != PixelRedGreenBlueReserved8BitPerColor &&
        Info->PixelFormat != PixelBlueGreenRedReserved8BitPerColor) {

        return;
    }

    Bpp = 32;

    Rsp = LimineArenaAlloc(Ctx, sizeof(*Rsp), 8);
    Fb = LimineArenaAlloc(Ctx, sizeof(*Fb), 8);
    Array = LimineArenaAlloc(Ctx, sizeof(UINT64), 8);

    if (Rsp == NULL || Fb == NULL || Array == NULL) {
        return;
    }

    Fb->Address = Gop->Mode->FrameBufferBase + Ctx->HhdmOffset;
    Fb->Width = Info->HorizontalResolution;
    Fb->Height = Info->VerticalResolution;
    Fb->Pitch = (UINT64)Info->PixelsPerScanLine * (Bpp / 8);
    Fb->Bpp = (UINT16)Bpp;
    Fb->MemoryModel = LIMINE_FRAMEBUFFER_RGB;

    Fb->RedMaskSize = 8;
    Fb->GreenMaskSize = 8;
    Fb->BlueMaskSize = 8;

    if (Info->PixelFormat == PixelRedGreenBlueReserved8BitPerColor) {
        Fb->RedMaskShift = 0;
        Fb->GreenMaskShift = 8;
        Fb->BlueMaskShift = 16;
    }
    else {
        Fb->BlueMaskShift = 0;
        Fb->GreenMaskShift = 8;
        Fb->RedMaskShift = 16;
    }

    Fb->ModeCount = 0;
    Fb->Modes = 0;

    Array[0] = TO_VIRT(Ctx, Fb);

    Rsp->Revision = 0;
    Rsp->FramebufferCount = 1;
    Rsp->Framebuffers = TO_VIRT(Ctx, Array);

    Req->Response = TO_VIRT(Ctx, Rsp);

    Ctx->FbPhys = Gop->Mode->FrameBufferBase;
    Ctx->FbSize = Gop->Mode->FrameBufferSize;
}

static VOID *FindConfigTable(IN EFI_GUID *Guid)
{
    UINTN i;

    for (i = 0; i < gST->NumberOfTableEntries; i++) {
        if (GuidsAreEqual(&gST->ConfigurationTable[i].VendorGuid, Guid)) {
            return gST->ConfigurationTable[i].VendorTable;
        }
    }

    return NULL;
}

static VOID AnswerRsdp(IN OUT LIMINE_CTX *Ctx, IN OUT LIMINE_REQUEST *Req)
{
    LIMINE_RSDP_RESPONSE *Rsp;
    VOID *Rsdp;

    Rsdp = FindConfigTable(&LimineAcpi20Guid);
    if (Rsdp == NULL) {
        Rsdp = FindConfigTable(&LimineAcpi10Guid);
    }

    if (Rsdp == NULL) {
        return;
    }

    Rsp = LimineArenaAlloc(Ctx, sizeof(*Rsp), 8);
    if (Rsp == NULL) {
        return;
    }

    Rsp->Revision = 0;

    Rsp->Address = (UINT64)(UINTN)Rsdp;

    Req->Response = TO_VIRT(Ctx, Rsp);
}

static VOID AnswerSmbios(IN OUT LIMINE_CTX *Ctx, IN OUT LIMINE_REQUEST *Req)
{
    LIMINE_SMBIOS_RESPONSE *Rsp;
    VOID *Entry32;
    VOID *Entry64;

    Entry32 = FindConfigTable(&LimineSmbiosGuid);
    Entry64 = FindConfigTable(&LimineSmbios3Guid);

    if (Entry32 == NULL && Entry64 == NULL) {
        return;
    }

    Rsp = LimineArenaAlloc(Ctx, sizeof(*Rsp), 8);
    if (Rsp == NULL) {
        return;
    }

    Rsp->Revision = 0;
    Rsp->Entry32 = (UINT64)(UINTN)Entry32;
    Rsp->Entry64 = (UINT64)(UINTN)Entry64;

    Req->Response = TO_VIRT(Ctx, Rsp);
}

static VOID AnswerEfiSystemTable(IN OUT LIMINE_CTX *Ctx, IN OUT LIMINE_REQUEST *Req)
{
    LIMINE_EFI_SYSTEM_TABLE_RESPONSE *Rsp;

    Rsp = LimineArenaAlloc(Ctx, sizeof(*Rsp), 8);
    if (Rsp == NULL) {
        return;
    }

    Rsp->Revision = 0;
    Rsp->Address = (UINT64)(UINTN)gST;

    Req->Response = TO_VIRT(Ctx, Rsp);
}

static VOID AnswerEfiMemmap(IN OUT LIMINE_CTX *Ctx, IN OUT LIMINE_REQUEST *Req)
{
    LIMINE_EFI_MEMMAP_RESPONSE *Rsp;

    Rsp = LimineArenaAlloc(Ctx, sizeof(*Rsp), 8);
    if (Rsp == NULL) {
        return;
    }

    Rsp->Revision = 0;

    Ctx->EfiMemmapResponse = Rsp;

    Req->Response = TO_VIRT(Ctx, Rsp);
}

static INT64 UnixTimeFromEfi(IN EFI_TIME *Time)
{
    INT64 Year;
    INT64 Month;
    INT64 Day;
    INT64 Era;
    INT64 Yoe;
    INT64 Doy;
    INT64 Doe;
    INT64 Days;

    Year = Time->Year;
    Month = Time->Month;
    Day = Time->Day;

    Year -= (Month <= 2) ? 1 : 0;

    Era = ((Year >= 0) ? Year : Year - 399) / 400;
    Yoe = Year - Era * 400;
    Doy = (153 * (Month + ((Month > 2) ? -3 : 9)) + 2) / 5 + Day - 1;
    Doe = Yoe * 365 + Yoe / 4 - Yoe / 100 + Doy;

    Days = Era * 146097 + Doe - 719468;

    return Days * 86400 + (INT64)Time->Hour * 3600 + (INT64)Time->Minute * 60 + (INT64)Time->Second;
}

static VOID AnswerDateAtBoot(IN OUT LIMINE_CTX *Ctx, IN OUT LIMINE_REQUEST *Req)
{
    LIMINE_DATE_AT_BOOT_RESPONSE *Rsp;
    EFI_STATUS Status;
    EFI_TIME Time;

    ZeroMem(&Time, sizeof(Time));

    Status = gRT->GetTime(&Time, NULL);
    if (EFI_ERROR(Status)) {

        return;
    }

    Rsp = LimineArenaAlloc(Ctx, sizeof(*Rsp), 8);
    if (Rsp == NULL) {
        return;
    }

    Rsp->Revision = 0;
    Rsp->Timestamp = UnixTimeFromEfi(&Time);

    Req->Response = TO_VIRT(Ctx, Rsp);
}

static VOID FillLimineFile(IN OUT LIMINE_CTX *Ctx, OUT LIMINE_FILE *File, IN UINT64 Address,
                           IN UINT64 Size, IN CHAR16 *Path, IN CHAR16 *Cmdline)
{
    File->Revision = 0;
    File->Address = Address;
    File->Size = Size;
    File->Path = ArenaAsciiFromUcs2(Ctx, Path);
    File->String =
        (Cmdline != NULL) ? ArenaAsciiFromUcs2(Ctx, Cmdline) : ArenaAscii(Ctx, (CHAR8 *)"");
    File->MediaType = LIMINE_MEDIA_TYPE_GENERIC;

    if (Ctx->Volume != NULL) {

        CopyMem(&File->GptDiskUuid, &Ctx->Volume->VolUuid, sizeof(LIMINE_UUID));
        CopyMem(&File->GptPartUuid, &Ctx->Volume->PartGuid, sizeof(LIMINE_UUID));
        CopyMem(&File->PartUuid, &Ctx->Volume->PartGuid, sizeof(LIMINE_UUID));
    }
}

static VOID AnswerExecutableFile(IN OUT LIMINE_CTX *Ctx, IN OUT LIMINE_REQUEST *Req)
{
    EFI_STATUS Status;
    EFI_PHYSICAL_ADDRESS Copy;
    LIMINE_EXECUTABLE_FILE_RESPONSE *Rsp;
    LIMINE_FILE *File;
    UINTN Pages;

    if (Ctx->FileData == NULL || Ctx->FileSize == 0) {
        return;
    }

    Pages = (Ctx->FileSize + LIMINE_PAGE_SIZE - 1) / LIMINE_PAGE_SIZE;

    Copy = 0;
    Status = gBS->AllocatePages(AllocateAnyPages, EfiLoaderData, Pages, &Copy);
    if (EFI_ERROR(Status)) {
        return;
    }

    CopyMem((VOID *)(UINTN)Copy, Ctx->FileData, Ctx->FileSize);

    Ctx->ExecFilePhys = (UINT64)Copy;
    Ctx->ExecFilePages = Pages;

    Rsp = LimineArenaAlloc(Ctx, sizeof(*Rsp), 8);
    File = LimineArenaAlloc(Ctx, sizeof(*File), 8);

    if (Rsp == NULL || File == NULL) {
        return;
    }

    FillLimineFile(Ctx, File, (UINT64)Copy + Ctx->HhdmOffset, Ctx->FileSize, Ctx->LoaderPath,
                   Ctx->Cmdline);

    Rsp->Revision = 0;
    Rsp->ExecutableFile = TO_VIRT(Ctx, File);

    Req->Response = TO_VIRT(Ctx, Rsp);
}

static VOID AnswerModules(IN OUT LIMINE_CTX *Ctx, IN OUT LIMINE_REQUEST *Req)
{
    LIMINE_MODULE_RESPONSE *Rsp;

    Rsp = LimineArenaAlloc(Ctx, sizeof(*Rsp), 8);
    if (Rsp == NULL) {
        return;
    }

    Rsp->Revision = 0;
    Rsp->ModuleCount = 0;
    Rsp->Modules = 0;

    Req->Response = TO_VIRT(Ctx, Rsp);
}

static VOID ReserveMemmap(IN OUT LIMINE_CTX *Ctx, IN OUT LIMINE_REQUEST *Req)
{
    EFI_STATUS Status;
    LIMINE_MEMMAP_RESPONSE *Rsp;
    LIMINE_MEMMAP_ENTRY *Entries;
    UINT64 *Pointers;
    UINTN MapSize;
    UINTN MapKey;
    UINTN DescSize;
    UINT32 DescVersion;
    UINTN Count;

    MapSize = 0;
    MapKey = 0;
    DescSize = 0;
    DescVersion = 0;

    Status = gBS->GetMemoryMap(&MapSize, NULL, &MapKey, &DescSize, &DescVersion);
    if (Status != EFI_BUFFER_TOO_SMALL || DescSize == 0) {
        return;
    }

    Count = (MapSize / DescSize) + LIMINE_MEMMAP_SLACK;

    Rsp = LimineArenaAlloc(Ctx, sizeof(*Rsp), 8);
    Entries = LimineArenaAlloc(Ctx, Count * sizeof(LIMINE_MEMMAP_ENTRY), 8);

    Pointers = LimineArenaAlloc(Ctx, Count * sizeof(UINT64), 8);

    if (Rsp == NULL || Entries == NULL || Pointers == NULL) {
        return;
    }

    Rsp->Revision = 0;
    Rsp->EntryCount = 0;
    Rsp->Entries = 0;

    Ctx->MemmapResponse = Rsp;
    Ctx->MemmapEntries = Entries;
    Ctx->MemmapPointers = Pointers;
    Ctx->MemmapMaxEntries = Count;

    Req->Response = TO_VIRT(Ctx, Rsp);
}

static VOID DispatchRequest(IN OUT LIMINE_CTX *Ctx, IN OUT LIMINE_REQUEST *Req)
{
    UINT64 Id2;
    UINT64 Id3;

    Id2 = Req->Id[2];
    Id3 = Req->Id[3];

#define LIMINE_IS(Name) (Id2 == LIMINE_ID_##Name##_2 && Id3 == LIMINE_ID_##Name##_3)

    if (LIMINE_IS(BOOTLOADER_INFO)) {
        AnswerBootloaderInfo(Ctx, Req);
    }
    else if (LIMINE_IS(EXECUTABLE_CMDLINE)) {
        AnswerCmdline(Ctx, Req);
    }
    else if (LIMINE_IS(FIRMWARE_TYPE)) {
        AnswerFirmwareType(Ctx, Req);
    }
    else if (LIMINE_IS(STACK_SIZE)) {
        AnswerStackSize(Ctx, Req);
    }
    else if (LIMINE_IS(HHDM)) {
        AnswerHhdm(Ctx, Req);
    }
    else if (LIMINE_IS(FRAMEBUFFER)) {
        AnswerFramebuffer(Ctx, Req);
    }
    else if (LIMINE_IS(PAGING_MODE)) {
        AnswerPagingMode(Ctx, Req);
    }
    else if (LIMINE_IS(MEMMAP)) {
        ReserveMemmap(Ctx, Req);
    }
    else if (LIMINE_IS(ENTRY_POINT)) {
        AnswerEntryPoint(Ctx, Req);
    }
    else if (LIMINE_IS(EXECUTABLE_FILE)) {
        AnswerExecutableFile(Ctx, Req);
    }
    else if (LIMINE_IS(MODULE)) {
        AnswerModules(Ctx, Req);
    }
    else if (LIMINE_IS(RSDP)) {
        AnswerRsdp(Ctx, Req);
    }
    else if (LIMINE_IS(SMBIOS)) {
        AnswerSmbios(Ctx, Req);
    }
    else if (LIMINE_IS(EFI_SYSTEM_TABLE)) {
        AnswerEfiSystemTable(Ctx, Req);
    }
    else if (LIMINE_IS(EFI_MEMMAP)) {
        AnswerEfiMemmap(Ctx, Req);
    }
    else if (LIMINE_IS(EXECUTABLE_ADDRESS)) {
        AnswerExecutableAddress(Ctx, Req);
    }
    else if (LIMINE_IS(DATE_AT_BOOT)) {
        AnswerDateAtBoot(Ctx, Req);
    }
    else if (LIMINE_IS(MP)) {

        INFO_LOG("Limine: MP request left unanswered (single-processor boot)");
    }

#undef LIMINE_IS
}

static VOID HandleBaseRevision(IN OUT LIMINE_CTX *Ctx, IN OUT UINT64 *Tag)
{
    UINT64 Wanted;

    Wanted = Tag[2];

    Ctx->BaseRevisionTag = Tag;

    if (Wanted <= LIMINE_MAX_BASE_REVISION) {
        Ctx->BaseRevision = Wanted;
        Tag[2] = 0;
    }
    else {

        Ctx->BaseRevision = LIMINE_MAX_BASE_REVISION;
        Tag[1] = LIMINE_MAX_BASE_REVISION;
    }
}

static VOID NarrowSearchArea(IN OUT LIMINE_CTX *Ctx)
{
    UINT64 *Word;
    UINTN Words;
    UINTN i;
    UINT64 Start;
    UINT64 End;

    Word = (UINT64 *)(UINTN)Ctx->ImagePhys;
    Words = (UINTN)(Ctx->ImageSize / sizeof(UINT64));

    Start = 0;
    End = 0;

    for (i = 0; i + 3 < Words; i++) {
        if (Word[i] == LIMINE_REQUESTS_START_0 && Word[i + 1] == LIMINE_REQUESTS_START_1 &&
            Word[i + 2] == LIMINE_REQUESTS_START_2 && Word[i + 3] == LIMINE_REQUESTS_START_3) {
            Start = (UINT64)(UINTN)&Word[i + 4];
        }
        else if (Word[i] == LIMINE_REQUESTS_END_0 && Word[i + 1] == LIMINE_REQUESTS_END_1) {
            End = (UINT64)(UINTN)&Word[i];
        }
    }

    if (Start != 0 && End != 0 && End > Start) {
        Ctx->ReqSearchStart = Start;
        Ctx->ReqSearchEnd = End;
    }
}

EFI_STATUS LimineAnswerRequests(IN OUT LIMINE_CTX *Ctx)
{
    EFI_STATUS Status;
    EFI_PHYSICAL_ADDRESS Stack;
    UINT64 *Word;
    UINTN Words;
    UINTN i;

    if (Ctx == NULL || Ctx->ImagePhys == 0 || Ctx->Arena == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    Ctx->BaseRevision = 0;
    Ctx->StackSize = LIMINE_DEFAULT_STACK_SIZE;

    Word = (UINT64 *)(UINTN)Ctx->ImagePhys;
    Words = (UINTN)(Ctx->ImageSize / sizeof(UINT64));

    for (i = 0; i + 2 < Words; i++) {
        if (Word[i] == LIMINE_BASE_REVISION_0 && Word[i + 1] == LIMINE_BASE_REVISION_1) {
            HandleBaseRevision(Ctx, &Word[i]);
            break;
        }
    }

    NarrowSearchArea(Ctx);

    Word = (UINT64 *)(UINTN)Ctx->ReqSearchStart;
    Words = (UINTN)((Ctx->ReqSearchEnd - Ctx->ReqSearchStart) / sizeof(UINT64));

    for (i = 0; i + 5 < Words; i++) {
        if (Word[i] != LIMINE_COMMON_MAGIC_0 || Word[i + 1] != LIMINE_COMMON_MAGIC_1) {
            continue;
        }

        DispatchRequest(Ctx, (LIMINE_REQUEST *)&Word[i]);
    }

    Stack = 0;
    Ctx->StackSize = (Ctx->StackSize + LIMINE_PAGE_SIZE - 1) & ~(LIMINE_PAGE_SIZE - 1);

    Status = gBS->AllocatePages(AllocateAnyPages, EfiLoaderData,
                                (UINTN)(Ctx->StackSize / LIMINE_PAGE_SIZE), &Stack);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    ZeroMem((VOID *)(UINTN)Stack, (UINTN)Ctx->StackSize);
    Ctx->StackPhys = (UINT64)Stack;

    return EFI_SUCCESS;
}

static UINT64 LimineTypeFromEfi(IN UINT32 EfiType)
{
    switch (EfiType) {
    case EfiConventionalMemory:
        return LIMINE_MEMMAP_USABLE;

    case EfiLoaderCode:
    case EfiLoaderData:
    case EfiBootServicesCode:
    case EfiBootServicesData:

        return LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE;

    case EfiACPIReclaimMemory:
        return LIMINE_MEMMAP_ACPI_RECLAIMABLE;

    case EfiACPIMemoryNVS:
        return LIMINE_MEMMAP_ACPI_NVS;

    case EfiUnusableMemory:
        return LIMINE_MEMMAP_BAD_MEMORY;

    default:

        return LIMINE_MEMMAP_RESERVED;
    }
}

static BOOLEAN Overlaps(IN UINT64 Base, IN UINT64 Length, IN UINT64 Start, IN UINT64 Size)
{
    return (BOOLEAN)(Start != 0 && Base < Start + Size && Base + Length > Start);
}

VOID LimineFillMemmap(IN OUT LIMINE_CTX *Ctx, IN EFI_MEMORY_DESCRIPTOR *Map, IN UINTN MapSize,
                      IN UINTN DescSize)
{
    LIMINE_MEMMAP_RESPONSE *Rsp;
    LIMINE_MEMMAP_ENTRY *Entries;
    UINT64 *Pointers;
    EFI_MEMORY_DESCRIPTOR *Desc;
    UINTN Offset;
    UINTN Count;
    UINTN i;
    UINT64 Type;
    UINT64 Base;
    UINT64 Length;

    if (Ctx->MemmapResponse == NULL || Ctx->MemmapEntries == NULL || Ctx->MemmapPointers == NULL) {
        return;
    }

    Rsp = (LIMINE_MEMMAP_RESPONSE *)Ctx->MemmapResponse;
    Entries = (LIMINE_MEMMAP_ENTRY *)Ctx->MemmapEntries;
    Pointers = (UINT64 *)Ctx->MemmapPointers;

    Count = 0;

    for (Offset = 0; Offset + DescSize <= MapSize && Count < Ctx->MemmapMaxEntries;
         Offset += DescSize) {
        Desc = (EFI_MEMORY_DESCRIPTOR *)((UINT8 *)Map + Offset);

        if (Desc->NumberOfPages == 0) {
            continue;
        }

        Base = Desc->PhysicalStart;
        Length = Desc->NumberOfPages * LIMINE_PAGE_SIZE;
        Type = LimineTypeFromEfi(Desc->Type);

        if (Overlaps(Base, Length, Ctx->ImagePhys, Ctx->ImageSize) ||
            Overlaps(Base, Length, Ctx->ExecFilePhys, Ctx->ExecFilePages * LIMINE_PAGE_SIZE)) {
            Type = LIMINE_MEMMAP_EXECUTABLE_AND_MODULES;
        }

        if (Count > 0 && Entries[Count - 1].Type == Type &&
            Entries[Count - 1].Base + Entries[Count - 1].Length == Base) {
            Entries[Count - 1].Length += Length;

            continue;
        }

        Entries[Count].Base = Base;
        Entries[Count].Length = Length;
        Entries[Count].Type = Type;

        Count++;
    }

    for (i = 0; i < Count; i++) {
        Pointers[i] = TO_VIRT(Ctx, &Entries[i]);
    }

    Rsp->EntryCount = Count;
    Rsp->Entries = TO_VIRT(Ctx, Pointers);

    if (Ctx->EfiMemmapResponse != NULL && Ctx->EfiMemmapBuffer != NULL) {
        LIMINE_EFI_MEMMAP_RESPONSE *ERsp;

        ERsp = (LIMINE_EFI_MEMMAP_RESPONSE *)Ctx->EfiMemmapResponse;

        ERsp->Memmap = TO_VIRT(Ctx, Ctx->EfiMemmapBuffer);
        ERsp->MemmapSize = MapSize;
        ERsp->DescSize = DescSize;
        ERsp->DescVersion = EFI_MEMORY_DESCRIPTOR_VERSION;
    }
}
