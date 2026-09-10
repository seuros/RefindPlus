// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#include "cydia.h"
#include <Library/IoLib.h>
#include <IndustryStandard/SmBios.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/EdidActive.h>
#include <Protocol/BlockIo.h>
#include <Protocol/SimpleNetwork.h>
#include <Protocol/UsbIo.h>
#include <Protocol/DiskInfo.h>
#include <Protocol/AtaPassThru.h>
#include <Protocol/Tcg2Protocol.h>
#include <Guid/SmBios.h>

CY_COMPONENT *CyAdd(IN OUT CY_REPORT *R, IN CONST CHAR8 *Name, IN CY_STATE State)
{
    CY_COMPONENT *C;
    if (R->Count >= CY_MAX_COMPONENTS) {
        return &R->Comp[CY_MAX_COMPONENTS - 1];
    }
    C = &R->Comp[R->Count++];
    ZeroMem(C, sizeof(*C));
    AsciiStrnCpyS(C->Name, sizeof(C->Name), Name, sizeof(C->Name) - 1);
    C->State = State;
    C->Detail[0] = '\0';
    return C;
}

static UINTN CountHandles(IN EFI_GUID *Guid)
{
    EFI_HANDLE *H = NULL;
    UINTN N = 0;
    if (EFI_ERROR(gBS->LocateHandleBuffer(ByProtocol, Guid, NULL, &N, &H)) || H == NULL) {
        return 0;
    }
    FreePool(H);
    return N;
}

static UINT8 *SmbiosTableAddr(OUT UINTN *Len)
{
    UINTN i;
    *Len = 0;

    for (i = 0; i < gST->NumberOfTableEntries; i++) {
        if (CompareGuid(&gST->ConfigurationTable[i].VendorGuid, &gEfiSmbios3TableGuid)) {
            SMBIOS_TABLE_3_0_ENTRY_POINT *E = gST->ConfigurationTable[i].VendorTable;
            *Len = (UINTN)E->TableMaximumSize;
            return (UINT8 *)(UINTN)E->TableAddress; // NOLINT(performance-no-int-to-ptr)
        }
    }
    for (i = 0; i < gST->NumberOfTableEntries; i++) {
        if (CompareGuid(&gST->ConfigurationTable[i].VendorGuid, &gEfiSmbiosTableGuid)) {
            SMBIOS_TABLE_ENTRY_POINT *E = gST->ConfigurationTable[i].VendorTable;
            *Len = (UINTN)E->TableLength;
            return (UINT8 *)(UINTN)E->TableAddress; // NOLINT(performance-no-int-to-ptr)
        }
    }
    return NULL;
}

static CONST CHAR8 *SmbiosStr(IN SMBIOS_STRUCTURE *Hdr, IN UINT8 Index)
{
    CONST CHAR8 *P = (CONST CHAR8 *)Hdr + Hdr->Length;
    UINT8 n = 1;
    if (Index == 0) {
        return "";
    }
    while (*P != '\0') {
        if (n == Index) {
            return P;
        }
        while (*P != '\0') {
            P++;
        }
        P++;
        n++;
    }
    return "";
}

static VOID SmbiosWalk(BOOLEAN (*Cb)(SMBIOS_STRUCTURE *, VOID *), VOID *Ctx)
{
    UINTN Len;
    UINT8 *Tab = SmbiosTableAddr(&Len);
    UINT8 *P, *End;
    if (Tab == NULL || Len == 0) {
        return;
    }
    P = Tab;
    End = Tab + Len;
    while (P + sizeof(SMBIOS_STRUCTURE) <= End) {
        SMBIOS_STRUCTURE *Hdr = (SMBIOS_STRUCTURE *)P;
        UINT8 *S;
        if (Hdr->Length < sizeof(SMBIOS_STRUCTURE)) {
            break;
        }
        if (Hdr->Type == 127) {
            break;
        }
        if (Cb(Hdr, Ctx)) {
            return;
        }

        S = P + Hdr->Length;
        while (S + 1 < End && !(S[0] == 0 && S[1] == 0)) {
            S++;
        }
        P = S + 2;
    }
}

static BOOLEAN Cb_Sys(SMBIOS_STRUCTURE *Hdr, VOID *Ctx)
{
    CY_REPORT *R = Ctx;
    if (Hdr->Type == 0) {
        SMBIOS_TABLE_TYPE0 *T0 = (SMBIOS_TABLE_TYPE0 *)Hdr;
        AsciiStrnCpyS(R->FwVendor, sizeof(R->FwVendor), SmbiosStr(Hdr, T0->Vendor),
                      sizeof(R->FwVendor) - 1);
        AsciiStrnCpyS(R->FwVersion, sizeof(R->FwVersion), SmbiosStr(Hdr, T0->BiosVersion),
                      sizeof(R->FwVersion) - 1);
    }
    else if (Hdr->Type == 1) {
        SMBIOS_TABLE_TYPE1 *T1 = (SMBIOS_TABLE_TYPE1 *)Hdr;
        CONST CHAR8 *Mfr = SmbiosStr(Hdr, T1->Manufacturer);
        AsciiStrnCpyS(R->Model, sizeof(R->Model), SmbiosStr(Hdr, T1->ProductName),
                      sizeof(R->Model) - 1);
        AsciiStrnCpyS(R->Serial, sizeof(R->Serial), SmbiosStr(Hdr, T1->SerialNumber),
                      sizeof(R->Serial) - 1);
        if (AsciiStrStr(Mfr, "Apple") != NULL) {
            R->IsApple = TRUE;
        }
    }
    else if (Hdr->Type == 4) {
        SMBIOS_TABLE_TYPE4 *T4 = (SMBIOS_TABLE_TYPE4 *)Hdr;
        AsciiStrnCpyS(R->Cpu, sizeof(R->Cpu), SmbiosStr(Hdr, T4->ProcessorVersion),
                      sizeof(R->Cpu) - 1);
    }
    return FALSE;
}

// CPUID is x86-only. On AArch64 the SMBIOS Type 4 string Cb_Sys already
// collected is the answer, so leave it alone rather than clobbering it.
#if defined(MDE_CPU_IA32) || defined(MDE_CPU_X64)
static VOID CpuBrand(OUT CHAR8 *Out, UINTN Cap)
{
    UINT32 Regs[13];
    UINT32 Max = 0;
    AsmCpuid(0x80000000, &Max, NULL, NULL, NULL);
    if (Max < 0x80000004) {
        return;
    }
    AsmCpuid(0x80000002, &Regs[0], &Regs[1], &Regs[2], &Regs[3]);
    AsmCpuid(0x80000003, &Regs[4], &Regs[5], &Regs[6], &Regs[7]);
    AsmCpuid(0x80000004, &Regs[8], &Regs[9], &Regs[10], &Regs[11]);
    Regs[12] = 0;
    AsciiStrnCpyS(Out, Cap, (CHAR8 *)Regs, Cap - 1);
}
#endif

VOID CyScanSystem(IN OUT CY_REPORT *R)
{
    CY_COMPONENT *C;
    SmbiosWalk(Cb_Sys, R);
#if defined(MDE_CPU_IA32) || defined(MDE_CPU_X64)
    CpuBrand(R->Cpu, sizeof(R->Cpu));
#endif
    if (R->Cpu[0] == '\0') {
        AsciiStrCpyS(R->Cpu, sizeof(R->Cpu), "unknown");
    }

    C = CyAdd(R, "System", R->Model[0] ? CY_PRESENT : CY_UNKNOWN);
    AsciiSPrint(C->Detail, sizeof(C->Detail), "%a  SN=%a", R->Model[0] ? R->Model : "unknown",
                R->Serial[0] ? R->Serial : "n/a");

    C = CyAdd(R, "CPU",
              (R->Cpu[0] && AsciiStrStr(R->Cpu, "unknown") == NULL) ? CY_PRESENT : CY_UNKNOWN);

    {
        CHAR8 *B = R->Cpu;
        while (*B == ' ') {
            B++;
        }
        AsciiStrnCpyS(C->Detail, sizeof(C->Detail), B, sizeof(C->Detail) - 1);
    }
}

typedef struct
{
    UINTN Slots, Populated;
    UINT64 TotalMB;
} MEM_CTX;

static BOOLEAN Cb_Mem(SMBIOS_STRUCTURE *Hdr, VOID *Ctx)
{
    MEM_CTX *M = Ctx;
    if (Hdr->Type == 17) {
        SMBIOS_TABLE_TYPE17 *T = (SMBIOS_TABLE_TYPE17 *)Hdr;
        M->Slots++;
        if (T->Size != 0 && T->Size != 0xFFFF) {
            M->Populated++;
            if (T->Size == 0x7FFF && Hdr->Length > OFFSET_OF(SMBIOS_TABLE_TYPE17, ExtendedSize)) {
                M->TotalMB += (T->ExtendedSize & 0x7FFFFFFF);
            }
            else if (T->Size & 0x8000) {
                M->TotalMB += (T->Size & 0x7FFF) / 1024;
            }
            else {
                M->TotalMB += T->Size;
            }
        }
    }
    return FALSE;
}

VOID CyScanMemory(IN OUT CY_REPORT *R)
{
    MEM_CTX M = {0, 0, 0};
    CY_COMPONENT *C;
    CY_STATE St;
    SmbiosWalk(Cb_Mem, &M);

    if (M.Slots == 0) {
        C = CyAdd(R, "Memory", CY_UNKNOWN);
        AsciiStrCpyS(C->Detail, sizeof(C->Detail), "no SMBIOS memory records");
        return;
    }

    St = (M.Populated == 0) ? CY_ABSENT : CY_PRESENT;
    C = CyAdd(R, "Memory", St);
    AsciiSPrint(C->Detail, sizeof(C->Detail), "%u of %u slot(s) populated, %lu MB total",
                (UINT32)M.Populated, (UINT32)M.Slots, (UINT64)M.TotalMB);
}

#define CY_MAXDISK 8

static VOID CySanitizeModel(IN CONST UINT8 *Raw, IN UINTN RawLen, OUT CHAR8 *Out, IN UINTN Cap)
{
    UINTN i, o = 0;
    if (Cap == 0) {
        return;
    }
    for (i = 0; i < RawLen && o < Cap - 1; i++) {
        CHAR8 c = (CHAR8)Raw[i];
        if (c < 0x20 || c >= 0x7F) {
            continue;
        }
        if (c == ' ' && (o == 0 || Out[o - 1] == ' ')) {
            continue;
        }
        Out[o++] = c;
    }
    while (o > 0 && Out[o - 1] == ' ') {
        o--;
    }
    Out[o] = '\0';
}

static VOID CyDiskModel(IN EFI_HANDLE Handle, OUT CHAR8 *Model, IN UINTN Cap)
{
    EFI_GUID DiGuid = EFI_DISK_INFO_PROTOCOL_GUID;
    EFI_GUID Nvme = EFI_DISK_INFO_NVME_INTERFACE_GUID;
    EFI_GUID Ahci = EFI_DISK_INFO_AHCI_INTERFACE_GUID;
    EFI_GUID Ide = EFI_DISK_INFO_IDE_INTERFACE_GUID;
    EFI_DISK_INFO_PROTOCOL *Di;
    UINT8 *Buf;
    UINT8 Raw[64];
    UINTN RawLen = 0, i;
    UINT32 Size = 4096;

    Model[0] = '\0';
    if (Cap < 2 || EFI_ERROR(gBS->HandleProtocol(Handle, &DiGuid, (VOID **)&Di))) {
        return;
    }
    Buf = AllocateZeroPool(Size);
    if (Buf == NULL) {
        return;
    }
    ZeroMem(Raw, sizeof(Raw));

    if (CompareGuid(&Di->Interface, &Nvme) && !EFI_ERROR(Di->Identify(Di, Buf, &Size))) {
        CopyMem(Raw, Buf + 24, 40);
        RawLen = 40;
    }
    else if ((CompareGuid(&Di->Interface, &Ahci) || CompareGuid(&Di->Interface, &Ide)) &&
             !EFI_ERROR(Di->Identify(Di, Buf, &Size))) {
        for (i = 0; i < 40; i += 2) {
            Raw[i] = Buf[54 + i + 1];
            Raw[i + 1] = Buf[54 + i];
        }
        RawLen = 40;
    }
    else {
        Size = 4096;
        if (!EFI_ERROR(Di->Inquiry(Di, Buf, &Size)) && Size >= 32) {
            CopyMem(Raw, Buf + 8, 24);
            RawLen = 24;
        }
    }
    FreePool(Buf);
    CySanitizeModel(Raw, RawLen, Model, Cap);
}

static BOOLEAN CyAtaIdentify(IN EFI_ATA_PASS_THRU_PROTOCOL *Ata, IN UINT16 Port, IN UINT16 Pmp,
                             OUT UINT8 *Id512)
{
    EFI_ATA_PASS_THRU_COMMAND_PACKET Pkt;
    EFI_ATA_COMMAND_BLOCK Acb;
    UINTN Align = (Ata->Mode != NULL && Ata->Mode->IoAlign > 1) ? Ata->Mode->IoAlign : 1;
    UINT8 *RawBuf, *DataBuf;
    EFI_ATA_STATUS_BLOCK *Asb;
    BOOLEAN Ok = FALSE;

    RawBuf = AllocateZeroPool((UINTN)512 + sizeof(EFI_ATA_STATUS_BLOCK) + 2 * Align);
    if (RawBuf == NULL) {
        return FALSE;
    }
    // NOLINTNEXTLINE(performance-no-int-to-ptr)  align pointer to IoAlign
    DataBuf = (UINT8 *)(((UINTN)RawBuf + Align - 1) & ~(Align - 1));
    // NOLINTNEXTLINE(performance-no-int-to-ptr)  align pointer to IoAlign
    Asb = (EFI_ATA_STATUS_BLOCK *)(((UINTN)(DataBuf + 512) + Align - 1) & ~(Align - 1));

    ZeroMem(&Acb, sizeof(Acb));
    ZeroMem(&Pkt, sizeof(Pkt));
    Acb.AtaCommand = 0xEC;
    Pkt.Acb = &Acb;
    Pkt.Asb = Asb;
    Pkt.InDataBuffer = DataBuf;
    Pkt.InTransferLength = 512;
    Pkt.Protocol = EFI_ATA_PASS_THRU_PROTOCOL_PIO_DATA_IN;
    Pkt.Length = EFI_ATA_PASS_THRU_LENGTH_BYTES;
    Pkt.Timeout = 30000000ULL;

    if (!EFI_ERROR(Ata->PassThru(Ata, Port, Pmp, &Pkt, NULL))) {
        CopyMem(Id512, DataBuf, 512);
        Ok = TRUE;
    }
    FreePool(RawBuf);
    return Ok;
}

static UINTN CyAtaModels(OUT CHAR8 Models[][48], IN UINTN MaxN)
{
    EFI_GUID Guid = EFI_ATA_PASS_THRU_PROTOCOL_GUID;
    EFI_HANDLE *H = NULL;
    UINTN N = 0, i, Count = 0;

    if (EFI_ERROR(gBS->LocateHandleBuffer(ByProtocol, &Guid, NULL, &N, &H)) || H == NULL) {
        return 0;
    }
    for (i = 0; i < N && Count < MaxN; i++) {
        EFI_ATA_PASS_THRU_PROTOCOL *Ata;
        UINT16 Port = 0xFFFF;
        if (EFI_ERROR(gBS->HandleProtocol(H[i], &Guid, (VOID **)&Ata))) {
            continue;
        }
        while (Count < MaxN && Ata->GetNextPort(Ata, &Port) == EFI_SUCCESS) {
            UINT16 Pmp = 0xFFFF;
            while (Count < MaxN && Ata->GetNextDevice(Ata, Port, &Pmp) == EFI_SUCCESS) {
                UINT8 Id[512];
                if (CyAtaIdentify(Ata, Port, Pmp, Id)) {
                    UINT8 Raw[40];
                    UINTN k;
                    for (k = 0; k < 40; k += 2) {
                        Raw[k] = Id[54 + k + 1];
                        Raw[k + 1] = Id[54 + k];
                    }
                    CySanitizeModel(Raw, 40, Models[Count], 48);
                    if (AsciiStrLen(Models[Count]) >= 3) {
                        Count++;
                    }
                }
            }
        }
    }
    FreePool(H);
    return Count;
}

VOID CyScanStorage(IN OUT CY_REPORT *R)
{
    EFI_HANDLE *H = NULL;
    UINTN N = 0, i, Disks = 0, AtaN, AtaUsed = 0;
    CY_COMPONENT *C;
    CHAR8 List[160];
    CHAR8 AtaPool[CY_MAXDISK][48];

    List[0] = '\0';

    AtaN = CyAtaModels(AtaPool, CY_MAXDISK);

    if (!EFI_ERROR(gBS->LocateHandleBuffer(ByProtocol, &gEfiBlockIoProtocolGuid, NULL, &N, &H)) &&
        H != NULL) {
        for (i = 0; i < N; i++) {
            EFI_BLOCK_IO_PROTOCOL *Bio;
            CHAR8 Model[64];
            UINT64 Gb;
            if (EFI_ERROR(gBS->HandleProtocol(H[i], &gEfiBlockIoProtocolGuid, (VOID **)&Bio))) {
                continue;
            }

            if (Bio->Media == NULL || !Bio->Media->MediaPresent || Bio->Media->LogicalPartition) {
                continue;
            }
            Disks++;
            Gb = ((Bio->Media->LastBlock + 1) * Bio->Media->BlockSize) / 1000000000ULL;

            CyDiskModel(H[i], Model, sizeof(Model));
            if (AsciiStrLen(Model) < 3 && AtaUsed < AtaN) {
                AsciiStrCpyS(Model, sizeof(Model), AtaPool[AtaUsed++]);
            }

            if (AsciiStrLen(Model) >= 3) {

                if (AsciiStrStr(Model, "APPLE") == NULL && AsciiStrStr(Model, "Apple") == NULL) {
                    R->NonAppleDisk = TRUE;
                }
            }
            else {
                AsciiStrCpyS(Model, sizeof(Model), "unknown");
            }
            if (AsciiStrLen(List) + AsciiStrLen(Model) + 12 < sizeof(List)) {
                AsciiSPrint(List + AsciiStrLen(List), sizeof(List) - AsciiStrLen(List),
                            "%a%a %luGB", (Disks > 1) ? ", " : "", Model, (UINT64)Gb);
            }
        }
        FreePool(H);
    }

    C = CyAdd(R, "Storage", Disks ? CY_PRESENT : CY_ABSENT);
    if (Disks) {
        AsciiSPrint(C->Detail, sizeof(C->Detail), "%u disk(s): %a", (UINT32)Disks, List);
    }
    else {
        AsciiStrCpyS(C->Detail, sizeof(C->Detail), "no physical disk detected");
    }
}

static BOOLEAN CyApplePanelDead(VOID)
{
    static CONST CHAR8 Keys[4][4] = {
        {'T', 'L', '0', 'P'}, {'T', 'L', '0', 'p'}, {'T', 'L', '1', 'P'}, {'T', 'L', '1', 'p'}};
    UINT8 v[2];
    UINTN i;
    for (i = 0; i < 4; i++) {
        if (CySmcRead(Keys[i], v, 2) && (v[0] == 0x82 || v[0] == 0x80) && v[1] == 0x00) {
            return TRUE;
        }
    }
    return FALSE;
}

VOID CyScanDisplay(IN OUT CY_REPORT *R)
{
    EFI_HANDLE *H = NULL;
    UINTN N = 0, i;
    UINT32 W = 0, Hgt = 0;
    BOOLEAN Gop = FALSE, Edid = FALSE;
    CY_COMPONENT *C;

    if (!EFI_ERROR(
            gBS->LocateHandleBuffer(ByProtocol, &gEfiGraphicsOutputProtocolGuid, NULL, &N, &H)) &&
        H != NULL) {
        for (i = 0; i < N; i++) {
            EFI_GRAPHICS_OUTPUT_PROTOCOL *G;
            EFI_EDID_ACTIVE_PROTOCOL *Ed;
            if (!EFI_ERROR(
                    gBS->HandleProtocol(H[i], &gEfiGraphicsOutputProtocolGuid, (VOID **)&G)) &&
                G->Mode != NULL && G->Mode->Info != NULL) {
                Gop = TRUE;
                W = G->Mode->Info->HorizontalResolution;
                Hgt = G->Mode->Info->VerticalResolution;
            }

            if (!EFI_ERROR(gBS->HandleProtocol(H[i], &gEfiEdidActiveProtocolGuid, (VOID **)&Ed)) &&
                Ed->SizeOfEdid >= 128 && Ed->Edid != NULL) {
                Edid = TRUE;
                if (W == 0) {
                    W = Ed->Edid[56] | ((Ed->Edid[58] >> 4) << 8);
                    Hgt = Ed->Edid[59] | ((Ed->Edid[61] >> 4) << 8);
                }
            }
        }
        FreePool(H);
    }

    EFI_GUID UgaGuid = {
        0x982C298B, 0xF4FA, 0x41CB, {0xB8, 0x38, 0x77, 0xAA, 0x68, 0x8F, 0xB8, 0x39}};
    BOOLEAN Uga = CountHandles(&UgaGuid) > 0;

    BOOLEAN InternalPanel = (CyExpectFor(R->Model, R->IsApple, NULL) & CY_EXP_DISPLAY) != 0;

    BOOLEAN AppleNotDriven = R->IsApple && CyApplePanelDead();

    if (!InternalPanel) {

        C = CyAdd(R, "Display", CY_PRESENT);
        if (Gop && Edid) {
            AsciiSPrint(C->Detail, sizeof(C->Detail), "external display %ux%u connected", W, Hgt);
        }
        else if (Gop) {
            AsciiStrCpyS(C->Detail, sizeof(C->Detail),
                         "video output present; no monitor connected (no EDID)");
        }
        else if (Uga) {
            AsciiStrCpyS(C->Detail, sizeof(C->Detail), "video output present (UGA, pre-GOP Mac)");
        }
        else {
            AsciiStrCpyS(C->Detail, sizeof(C->Detail), "no graphics output");
        }
        return;
    }

    if (!Gop && !Uga) {
        C = CyAdd(R, "Display", CY_ABSENT);
        AsciiStrCpyS(C->Detail, sizeof(C->Detail), "no graphics output (panel/GPU/cable?)");
    }
    else if (!Gop && Uga) {

        C = CyAdd(R, "Display", CY_PRESENT);
        AsciiStrCpyS(C->Detail, sizeof(C->Detail), "panel driven via UGA (pre-GOP Mac)");
    }
    else if (!Edid) {
        C = CyAdd(R, "Display", CY_DEGRADED);
        AsciiSPrint(C->Detail, sizeof(C->Detail), "framebuffer %ux%u but no panel EDID", W, Hgt);
    }
    else if (AppleNotDriven) {

        C = CyAdd(R, "Display", CY_DEGRADED);
        AsciiSPrint(C->Detail, sizeof(C->Detail),
                    "%ux%u EDID present but SMC LCD sensor disconnected -- panel dead/unplugged?",
                    W, Hgt);
    }
    else {
        C = CyAdd(R, "Display", CY_PRESENT);
        AsciiSPrint(C->Detail, sizeof(C->Detail), "%ux%u, panel EDID present, framebuffer driven",
                    W, Hgt);
    }
}

static BOOLEAN CySmcU16(IN CONST CHAR8 Key[4], OUT UINT16 *Out)
{
    UINT8 v[2] = {0, 0};
    if (!CySmcRead(Key, v, 2)) {
        return FALSE;
    }
    *Out = (UINT16)((v[0] << 8) | v[1]);
    return TRUE;
}

VOID CyScanBattery(IN OUT CY_REPORT *R)
{
    UINT8 Num = 0, Info = 0;
    UINT16 Pct = 0, Cycles = 0, FullCap = 0, DesignCap = 0, Volt = 0, Health = 0;
    BOOLEAN HaveSoc, HaveCyc, HaveFull, HaveDesign, HaveVolt, Worn;
    CY_COMPONENT *C;
    CHAR8 *D;
    UINTN Cap;

    if (!R->IsApple) {

        C = CyAdd(R, "Battery", CY_UNKNOWN);
        AsciiStrCpyS(C->Detail, sizeof(C->Detail), "no SMC (non-Apple); check ACPI battery in OS");
        return;
    }

    if (!CySmcRead("BNum", &Num, 1)) {
        C = CyAdd(R, "Battery", CY_UNKNOWN);
        AsciiStrCpyS(C->Detail, sizeof(C->Detail), "SMC did not answer BNum (SMC cable/board?)");
        return;
    }
    if (Num == 0) {
        C = CyAdd(R, "Battery", CY_ABSENT);
        AsciiStrCpyS(C->Detail, sizeof(C->Detail), "SMC reports 0 batteries installed");
        return;
    }

    CySmcRead("BSIn", &Info, 1);
    HaveSoc = CySmcU16("BRSC", &Pct);
    if (HaveSoc && Pct > 100) {
        Pct = (UINT16)(Pct >> 8);
    }
    HaveCyc = CySmcU16("B0CT", &Cycles);
    HaveFull = CySmcU16("B0FC", &FullCap);
    HaveDesign = CySmcU16("B0DC", &DesignCap);
    HaveVolt = CySmcU16("B0AV", &Volt);

    BOOLEAN CapSane = HaveFull && HaveDesign && DesignCap >= 2000 && DesignCap <= 20000 &&
                      FullCap >= 500 && FullCap <= DesignCap + DesignCap / 10;
    if (CapSane) {
        Health = (UINT16)(((UINT32)FullCap * 100) / DesignCap);
    }

    Worn = (Health != 0 && Health < 80) || (HaveCyc && Cycles > 1000);
    C = CyAdd(R, "Battery", Worn ? CY_DEGRADED : CY_PRESENT);

    D = C->Detail;
    Cap = sizeof(C->Detail);
    AsciiSPrint(D, Cap, "%u battery(ies)", Num);
    if (HaveSoc) {
        AsciiSPrint(D + AsciiStrLen(D), Cap - AsciiStrLen(D), ", charge %u%%", Pct);
    }
    if (Health != 0) {
        AsciiSPrint(D + AsciiStrLen(D), Cap - AsciiStrLen(D), ", health %u%% (%u/%u mAh)", Health,
                    FullCap, DesignCap);
    }
    else if (CapSane && FullCap > 0) {

        AsciiSPrint(D + AsciiStrLen(D), Cap - AsciiStrLen(D), ", full-charge %u mAh", FullCap);
    }
    if (HaveCyc) {
        AsciiSPrint(D + AsciiStrLen(D), Cap - AsciiStrLen(D), ", %u cycles", Cycles);
    }
    if (HaveVolt && Volt > 0) {
        AsciiSPrint(D + AsciiStrLen(D), Cap - AsciiStrLen(D), ", %u.%03uV", Volt / 1000,
                    Volt % 1000);
    }
    if (Worn) {
        AsciiSPrint(D + AsciiStrLen(D), Cap - AsciiStrLen(D), " -- SERVICE: replace battery");
    }
    else if (!(Info & BIT0)) {
        AsciiSPrint(D + AsciiStrLen(D), Cap - AsciiStrLen(D), " (info flag clear)");
    }
}

VOID CyScanNetwork(IN OUT CY_REPORT *R)
{
    EFI_HANDLE *H = NULL;
    UINTN N = 0, i, Nics = 0;
    UINT8 Mac[6] = {0};
    BOOLEAN GotMac = FALSE;
    CY_COMPONENT *C;

    if (!EFI_ERROR(
            gBS->LocateHandleBuffer(ByProtocol, &gEfiSimpleNetworkProtocolGuid, NULL, &N, &H)) &&
        H != NULL) {
        for (i = 0; i < N; i++) {
            EFI_SIMPLE_NETWORK_PROTOCOL *Snp;
            if (EFI_ERROR(
                    gBS->HandleProtocol(H[i], &gEfiSimpleNetworkProtocolGuid, (VOID **)&Snp))) {
                continue;
            }
            Nics++;
            if (!GotMac && Snp->Mode != NULL) {
                CopyMem(Mac, &Snp->Mode->CurrentAddress, 6);
                GotMac = TRUE;
            }
        }
        FreePool(H);
    }

    C = CyAdd(R, "Network", Nics ? CY_PRESENT : CY_ABSENT);
    if (Nics && GotMac) {
        AsciiSPrint(C->Detail, sizeof(C->Detail), "%u NIC(s), MAC %02x:%02x:%02x:%02x:%02x:%02x",
                    (UINT32)Nics, Mac[0], Mac[1], Mac[2], Mac[3], Mac[4], Mac[5]);
    }
    else if (Nics) {
        AsciiSPrint(C->Detail, sizeof(C->Detail), "%u NIC(s)", (UINT32)Nics);
    }
    else {
        AsciiStrCpyS(C->Detail, sizeof(C->Detail), "no wired NIC visible pre-OS (Wi-Fi is normal)");
    }
}

static BOOLEAN CyAppleHddSensorBad(OUT CHAR8 KeyOut[5], OUT INT8 *CelsiusOut)
{
    static CONST CHAR8 Keys[6][4] = {{'T', 'H', '0', 'P'}, {'T', 'H', '0', 'O'},
                                     {'T', 'H', '0', 'F'}, {'T', 'H', '0', 'R'},
                                     {'T', 'H', '1', 'P'}, {'T', 'H', '1', 'O'}};
    UINT8 v[2];
    UINTN i;
    for (i = 0; i < 6; i++) {
        if (CySmcRead(Keys[i], v, 2)) {
            INT8 c = (INT8)v[0];

            if (c <= 10) {
                CopyMem(KeyOut, Keys[i], 4);
                KeyOut[4] = '\0';
                *CelsiusOut = c;
                return TRUE;
            }
        }
    }
    return FALSE;
}

static UINTN CyModelGen(IN CONST CHAR8 *Model, IN CONST CHAR8 *Prefix)
{
    UINTN L = AsciiStrLen(Prefix), n = 0;
    CONST CHAR8 *p;
    if (AsciiStrnCmp(Model, Prefix, L) != 0) {
        return 0;
    }
    for (p = Model + L; *p >= '0' && *p <= '9'; p++) {
        n = n * 10 + (UINTN)(*p - '0');
    }
    return n;
}

static BOOLEAN CyModelHasDriveSensor(IN CONST CHAR8 *Model)
{
    UINTN Pro, Mac;
    if (Model == NULL || Model[0] == '\0') {
        return FALSE;
    }
    Pro = CyModelGen(Model, "MacBookPro");
    if (Pro != 0) {
        return Pro <= 9;
    }
    if (AsciiStrnCmp(Model, "MacBookAir", 10) == 0) {
        return FALSE;
    }
    Mac = CyModelGen(Model, "MacBook");
    if (Mac != 0) {
        return Mac <= 7;
    }
    if (AsciiStrnCmp(Model, "MacPro", 6) == 0) {
        return FALSE;
    }
    if (AsciiStrnCmp(Model, "iMac", 4) == 0 || AsciiStrnCmp(Model, "Macmini", 7) == 0) {
        return TRUE;
    }
    return FALSE;
}

VOID CyScanThermal(IN OUT CY_REPORT *R)
{
    UINT8 Fans = 0, Tc0p[2] = {0, 0};
    CHAR8 Fan[48], HddKey[5];
    INT8 HddC = 0;
    CY_COMPONENT *C;

    if (!R->IsApple) {
        C = CyAdd(R, "Thermal", CY_UNKNOWN);
        AsciiStrCpyS(C->Detail, sizeof(C->Detail), "no SMC (non-Apple)");
        return;
    }
    if (!CySmcRead("FNum", &Fans, 1)) {
        C = CyAdd(R, "Thermal", CY_UNKNOWN);
        AsciiStrCpyS(C->Detail, sizeof(C->Detail), "SMC did not answer FNum");
        return;
    }

    if (CySmcRead("TC0P", Tc0p, 2)) {

        AsciiSPrint(Fan, sizeof(Fan), "%u fan(s), CPU proximity ~%uC", Fans, Tc0p[0]);
    }
    else {
        AsciiSPrint(Fan, sizeof(Fan), "%u fan(s)", Fans);
    }

    if (CyModelHasDriveSensor(R->Model) && CyAppleHddSensorBad(HddKey, &HddC)) {
        C = CyAdd(R, "Thermal", CY_DEGRADED);

        AsciiSPrint(C->Detail, sizeof(C->Detail),
                    "%a; HDD sensor %a reads %dC -- %a, SMC ramps fans", Fan, HddKey, (INT32)HddC,
                    R->NonAppleDisk ? "aftermarket drive has no Apple thermal sensor"
                                    : "dead sensor (swapped/sensorless drive?)");
    }
    else {
        C = CyAdd(R, "Thermal", CY_PRESENT);
        AsciiStrCpyS(C->Detail, sizeof(C->Detail), Fan);
    }
}

typedef struct
{
    INT8 Code;
    CONST CHAR8 *Desc;
} CY_CAUSE;

static CONST CY_CAUSE gCyCause[] = {
    {5, "good-shutdown"},
    {3, "power-button"},
    {2, "low-battery-sleep"},
    {1, "overtemp-sleep"},
    {0, "initial"},
    {-1, "health-check"},
    {-2, "power-supply"},
    {-3, "temperature-multisleep"},
    {-4, "sensor-fan"},
    {-30, "temperature-overlimit-timeout"},
    {-40, "pswr-smrst"},
    {-50, "unmapped"},
    {-60, "low-battery"},
    {-61, "ninja-shutdown"},
    {-62, "ninja-restart"},
    {-70, "palm-rest-temperature"},
    {-71, "sodimm-temperature"},
    {-72, "heatpipe-temperature"},
    {-74, "battery-temperature"},
    {-75, "adapter-timeout"},
    {-77, "manual-temperature"},
    {-78, "adapter-current"},
    {-79, "battery-current"},
    {-82, "skin-temperature"},
    {-83, "skin-temperature-sensors"},
    {-84, "backup-temperature"},
    {-86, "cpu-proximity-temperature"},
    {-95, "cpu-temperature"},
    {-100, "power-supply-temperature"},
    {-101, "lcd-temperature"},
    {-102, "rsm-power-fail"},
    {-103, "battery-cuv"},
    {-127, "pmu-forced-shutdown"},
    {-128, "unknown"},
};

static CONST CHAR8 *CyCauseStr(IN INT8 Code)
{
    UINTN i;
    for (i = 0; i < sizeof(gCyCause) / sizeof(gCyCause[0]); i++) {
        if (gCyCause[i].Code == Code) {
            return gCyCause[i].Desc;
        }
    }
    return "unrecognised";
}

static BOOLEAN CyCauseIsFault(IN INT8 Code) { return Code <= -2 && Code != -60 && Code != -128; }

VOID CyScanShutdown(IN OUT CY_REPORT *R)
{
    UINT8 Ss = 0, Sp = 0;
    BOOLEAN HaveSs, HaveSp;
    INT8 SsC, SpC;
    CY_COMPONENT *C;

    if (!R->IsApple) {
        return;
    }
    HaveSs = CySmcRead("MSSD", &Ss, 1);
    HaveSp = CySmcRead("MSSP", &Sp, 1);
    if (!HaveSs && !HaveSp) {
        return;
    }
    SsC = (INT8)Ss;
    SpC = (INT8)Sp;

    C = CyAdd(R, "Last Shutdown", (HaveSs && CyCauseIsFault(SsC)) ? CY_DEGRADED : CY_PRESENT);
    if (HaveSs && HaveSp) {
        AsciiSPrint(C->Detail, sizeof(C->Detail), "shutdown %d (%a); sleep %d (%a)", (INT32)SsC,
                    CyCauseStr(SsC), (INT32)SpC, CyCauseStr(SpC));
    }
    else if (HaveSs) {
        AsciiSPrint(C->Detail, sizeof(C->Detail), "shutdown %d (%a)", (INT32)SsC, CyCauseStr(SsC));
    }
    else {
        AsciiSPrint(C->Detail, sizeof(C->Detail), "sleep %d (%a)", (INT32)SpC, CyCauseStr(SpC));
    }
}

VOID CyScanTpm(IN OUT CY_REPORT *R)
{
    EFI_TCG2_PROTOCOL *Tcg2 = NULL;
    EFI_TCG2_BOOT_SERVICE_CAPABILITY Cap;
    CY_COMPONENT *C;

    if (EFI_ERROR(gBS->LocateProtocol(&gEfiTcg2ProtocolGuid, NULL, (VOID **)&Tcg2)) ||
        Tcg2 == NULL) {
        C = CyAdd(R, "TPM", CY_ABSENT);
        AsciiStrCpyS(C->Detail, sizeof(C->Detail), "no TCG2 (no TPM / disabled in firmware)");
        return;
    }
    ZeroMem(&Cap, sizeof(Cap));
    Cap.Size = sizeof(Cap);
    if (!EFI_ERROR(Tcg2->GetCapability(Tcg2, &Cap)) && Cap.TPMPresentFlag) {
        C = CyAdd(R, "TPM", CY_PRESENT);
        AsciiSPrint(C->Detail, sizeof(C->Detail), "present, active PCR banks 0x%08x",
                    Cap.ActivePcrBanks);
    }
    else {
        C = CyAdd(R, "TPM", CY_ABSENT);
        AsciiStrCpyS(C->Detail, sizeof(C->Detail), "TCG2 present but TPM flag clear");
    }
}

VOID CyScanUsb(IN OUT CY_REPORT *R)
{
    UINTN N = CountHandles(&gEfiUsbIoProtocolGuid);
    CY_COMPONENT *C = CyAdd(R, "USB", N ? CY_PRESENT : CY_UNKNOWN);
    if (N) {
        AsciiSPrint(C->Detail, sizeof(C->Detail), "%u USB device interface(s) enumerated",
                    (UINT32)N);
    }
    else {
        AsciiStrCpyS(C->Detail, sizeof(C->Detail),
                     "no USB devices (controllers may be off pre-OS)");
    }
}

typedef struct
{
    CONST CHAR8 *Prefix;
    UINT16 Mask;
    UINT8 DimmSlots;
} CY_MODEL_ROW;

static CONST CY_MODEL_ROW gModels[] = {

    {"MacBookPro", CY_EXP_BATTERY | CY_EXP_DISPLAY | CY_EXP_STORAGE | CY_EXP_NETWORK, 2},
    {"MacBookAir", CY_EXP_BATTERY | CY_EXP_DISPLAY | CY_EXP_STORAGE | CY_EXP_NETWORK, 0},
    {"MacBook", CY_EXP_BATTERY | CY_EXP_DISPLAY | CY_EXP_STORAGE | CY_EXP_NETWORK, 0},

    {"iMacPro", CY_EXP_DISPLAY | CY_EXP_STORAGE | CY_EXP_NETWORK, 4},
    {"iMac", CY_EXP_DISPLAY | CY_EXP_STORAGE | CY_EXP_NETWORK, 4},

    {"Macmini", CY_EXP_STORAGE | CY_EXP_NETWORK, 2},
    {"MacPro", CY_EXP_STORAGE | CY_EXP_NETWORK, 4},
};

UINT16 CyExpectFor(IN CONST CHAR8 *Model, IN BOOLEAN IsApple, OUT UINT8 *DimmSlots)
{
    UINTN i;
    if (DimmSlots) {
        *DimmSlots = 0;
    }
    if (Model == NULL) {
        return 0;
    }
    for (i = 0; i < sizeof(gModels) / sizeof(gModels[0]); i++) {
        UINTN L = AsciiStrLen(gModels[i].Prefix);
        if (AsciiStrnCmp(Model, gModels[i].Prefix, L) == 0) {
            if (DimmSlots) {
                *DimmSlots = gModels[i].DimmSlots;
            }
            return gModels[i].Mask;
        }
    }

    if (IsApple) {
        return CY_EXP_STORAGE | CY_EXP_NETWORK;
    }
    return 0;
}

#define CY_SMC_PROTO_GUID                                                                          \
    {0xB91867EA, 0x9ED9, 0x4B71, {0xA0, 0xAC, 0x3B, 0x8B, 0x4D, 0x2F, 0x0C, 0xB9}}
typedef struct _CY_SMC_IO_PROTOCOL CY_SMC_IO_PROTOCOL;
typedef EFI_STATUS(EFIAPI *CY_SMC_READ_VALUE)(IN CY_SMC_IO_PROTOCOL *This, IN UINT32 Key,
                                              IN UINT8 Size, OUT UINT8 *Value);
struct _CY_SMC_IO_PROTOCOL
{
    UINT64 Version;
    VOID *U1;
    VOID *U2;
    CY_SMC_READ_VALUE ReadValue;
};
#define CY_SMC_KEY(a, b, c, d) ((UINT32)((a) << 24 | (b) << 16 | (c) << 8 | (d)))

#define CY_MMIO_DATA 0x0000
#define CY_MMIO_KEY_NAME 0x0078
#define CY_MMIO_DATA_LEN 0x007D
#define CY_MMIO_SMC_ID 0x007E
#define CY_MMIO_CMD 0x007F
#define CY_MMIO_STATUS 0x4005
#define CY_MMIO_READY 0x20
#define CY_MMIO_MAX_WAIT 24
#define CY_MMIO_BASE_CANDIDATE 0xFE0B0000ULL

#define SMC_PORT_BASE 0x300
#define SMC_PORT_DATA 0x00
#define SMC_PORT_CMD 0x04
#define SMC_PORT_STMASK 0x0f
#define SMC_PORT_ST_ACK 0x0c
#define SMC_PORT_ST_AWAIT 0x04
#define SMC_PORT_ST_READY 0x05
#define SMC_CMD_READ 0x10

static BOOLEAN CyMmioWait(UINTN Base)
{
    UINTN i, Delay = 10;
    for (i = 0; i < CY_MMIO_MAX_WAIT; i++) {
        if (MmioRead8(Base + CY_MMIO_STATUS) & CY_MMIO_READY) {
            return TRUE;
        }
        gBS->Stall(Delay);
        if (Delay < 3200) {
            Delay *= 2;
        }
    }
    return FALSE;
}

static BOOLEAN CyMmioReadKey(UINTN Base, CONST CHAR8 Key[4], UINT8 *Buf, UINT8 Len)
{
    UINT32 KeyInt;
    UINT8 i, RLen;
    if (MmioRead8(Base + CY_MMIO_STATUS)) {
        MmioWrite8(Base + CY_MMIO_STATUS, 0);
    }
    CopyMem(&KeyInt, Key, 4);
    MmioWrite32(Base + CY_MMIO_KEY_NAME, KeyInt);
    MmioWrite8(Base + CY_MMIO_SMC_ID, 0);
    MmioWrite8(Base + CY_MMIO_CMD, SMC_CMD_READ);
    if (!CyMmioWait(Base) || MmioRead8(Base + CY_MMIO_CMD) != 0) {
        return FALSE;
    }
    RLen = MmioRead8(Base + CY_MMIO_DATA_LEN);
    if (RLen > Len) {
        RLen = Len;
    }
    for (i = 0; i < RLen; i++) {
        Buf[i] = MmioRead8(Base + CY_MMIO_DATA + i);
    }
    return TRUE;
}

static BOOLEAN CyMmioIsT2(UINTN Base)
{
    UINT8 Ldkn = 0;
    if (MmioRead8(Base + CY_MMIO_STATUS) == 0xFF) {
        return FALSE;
    }
    return CyMmioReadKey(Base, "LDKN", &Ldkn, 1) && Ldkn >= 2;
}

static BOOLEAN PortWait(UINT8 Want, UINTN Tries)
{
    UINTN i;
    Want &= SMC_PORT_STMASK;
    for (i = 0; i < Tries; i++) {
        if ((IoRead8(SMC_PORT_BASE + SMC_PORT_CMD) & SMC_PORT_STMASK) == Want) {
            return TRUE;
        }
        gBS->Stall(10);
    }
    return FALSE;
}

static BOOLEAN PortCommand(UINT8 Cmd)
{
    UINTN i;
    for (i = 0; i < 10; i++) {
        IoWrite8(SMC_PORT_BASE + SMC_PORT_CMD, Cmd);
        if (PortWait(SMC_PORT_ST_ACK, 100)) {
            return TRUE;
        }
    }
    return FALSE;
}

static BOOLEAN PortSendKeyBytes(CONST CHAR8 Key[4])
{
    UINTN i;
    for (i = 0; i < 4; i++) {
        IoWrite8(SMC_PORT_BASE + SMC_PORT_DATA, (UINT8)Key[i]);
        if (!PortWait(SMC_PORT_ST_AWAIT, 1000)) {
            return FALSE;
        }
    }
    return TRUE;
}

static BOOLEAN CyPortReadKey(CONST CHAR8 Key[4], UINT8 *Buf, UINT8 Len)
{
    UINTN Try, i;
    for (Try = 0; Try < 10; Try++) {
        if (!PortCommand(SMC_CMD_READ)) {
            continue;
        }
        if (!PortSendKeyBytes(Key)) {
            goto retry;
        }
        IoWrite8(SMC_PORT_BASE + SMC_PORT_DATA, Len);
        for (i = 0; i < Len; i++) {
            if (!PortWait(SMC_PORT_ST_READY, 1000)) {
                goto retry;
            }
            Buf[i] = IoRead8(SMC_PORT_BASE + SMC_PORT_DATA);
        }
        return TRUE;
    retry:;
    }
    return FALSE;
}

static struct
{
    BOOLEAN Init;
    CY_SMC_IO_PROTOCOL *Proto;
    UINTN MmioBase;
    BOOLEAN Port;
} gCySmc;

static VOID CySmcInit(VOID)
{
    EFI_GUID Guid = CY_SMC_PROTO_GUID;
    CY_SMC_IO_PROTOCOL *P = NULL;
    UINT8 Probe[4];

    if (gCySmc.Init) {
        return;
    }
    gCySmc.Init = TRUE;

    if (!EFI_ERROR(gBS->LocateProtocol(&Guid, NULL, (VOID **)&P)) && P != NULL &&
        P->ReadValue != NULL &&
        !EFI_ERROR(P->ReadValue(P, CY_SMC_KEY('#', 'K', 'E', 'Y'), 4, Probe))) {
        gCySmc.Proto = P;
    }

    if (CyMmioIsT2((UINTN)CY_MMIO_BASE_CANDIDATE)) {
        gCySmc.MmioBase = (UINTN)CY_MMIO_BASE_CANDIDATE;
    }

    if (CyPortReadKey("#KEY", Probe, 4)) {
        gCySmc.Port = TRUE;
    }
}

BOOLEAN CySmcRead(IN CONST CHAR8 Key[4], OUT UINT8 *Buf, IN UINT8 Len)
{
    CySmcInit();
    ZeroMem(Buf, Len);
    if (gCySmc.Proto != NULL) {
        return !EFI_ERROR(gCySmc.Proto->ReadValue(
            gCySmc.Proto, CY_SMC_KEY(Key[0], Key[1], Key[2], Key[3]), Len, Buf));
    }
    if (gCySmc.MmioBase != 0) {
        return CyMmioReadKey(gCySmc.MmioBase, Key, Buf, Len);
    }
    if (gCySmc.Port) {
        return CyPortReadKey(Key, Buf, Len);
    }
    return FALSE;
}
