// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#include "dp.h"

#include <Library/UefiLib.h>
#include <Library/PrintLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/IoLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/PciRootBridgeIo.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/AcpiTable.h>
#include <IndustryStandard/Pci.h>
#include <IndustryStandard/Acpi10.h>
#include <IndustryStandard/Acpi.h>
#include <Guid/Acpi.h>

#define IOBPIRI 0x2330
#define IOBPD 0x2334
#define IOBPS 0x2338
#define IOBPS_READY 0x0001
#define IOBPS_TX_MASK 0x0006
#define IOBPS_MASK 0xff00
#define IOBPS_READ 0x0600
#define IOBPS_WRITE 0x0700
#define IOBPU 0x233a
#define IOBPU_MAGIC 0xf000

#define DW_CONTROL 0x00
#define DW_TARGET 0x04
#define DW_CMD_DATA 0x10
#define DW_SS_HCNT 0x14
#define DW_SS_LCNT 0x18
#define DW_INTR_MASK 0x30
#define DW_RAW_INTR 0x34
#define DW_CLR_TX_ABRT 0x54
#define DW_ENABLE 0x6c
#define DW_TX_ABRT_SRC 0x80
#define DW_ENABLE_STAT 0x9c
#define DW_CTRL_MASTER (1u << 0)
#define DW_CTRL_SS (1u << 1)
#define DW_CTRL_RESTART (1u << 5)
#define DW_CTRL_SLVDIS (1u << 6)
#define DW_CMD_READ (1u << 8)
#define DW_INTR_RX_FULL (1u << 2)
#define DW_INTR_TX_ABRT (1u << 6)
#define DW_ABRT_ADDR_NOACK (1u << 0)
#define DW_SS_HCNT_SLOW 0x0258
#define DW_SS_LCNT_SLOW 0x02ee
#define DW_IC_COMP_PARAM_1 0xf4
#define DW_IC_COMP_VERSION 0xf8
#define DW_IC_COMP_TYPE 0xfc
#define DW_COMP_TYPE_VALUE 0x44570140
#define DW_PPR_RESET 0x804
#define DW_PPR_RESET_BITS 0x3

#define PCI_REG_CMD 0x04
#define PCI_REG_BAR0 0x10
#define PCI_REG_CAPPTR 0x34
#define PCI_CMD_MEM_BM 0x0006
#define PCI_CAP_ID_PM 0x01
#define PCI_PMCSR_OFFSET 4
#define PCI_PMCSR_PS_MASK 0x3

#define ACPI_QWORD_DESC 0x8A
#define FOUR_GIB 0x100000000ULL

VOID EFIAPI DpEmit(DP_CTX *Ctx, const CHAR8 *Fmt, ...)
{
    CHAR8 Line[256];
    VA_LIST Marker;
    UINTN Len;

    VA_START(Marker, Fmt);
    AsciiVSPrint(Line, sizeof(Line), Fmt, Marker);
    VA_END(Marker);

    Print(L"%a", Line);

    if (Ctx != NULL && Ctx->Buf != NULL) {
        Len = AsciiStrLen(Line);
        if (Ctx->Off + Len + 1 < Ctx->Cap) {
            AsciiStrCpyS(Ctx->Buf + Ctx->Off, Ctx->Cap - Ctx->Off, Line);
            Ctx->Off += Len;
        }
    }
}

EFI_STATUS DpSaveFile(CHAR16 *FileName, UINT8 *Data, UINTN Size)
{
    EFI_STATUS Status;
    EFI_HANDLE *Handles;
    UINTN Count, i;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *Sfs;
    EFI_FILE_PROTOCOL *Root;
    EFI_FILE_PROTOCOL *File;

    Status = gBS->LocateHandleBuffer(ByProtocol, &gEfiSimpleFileSystemProtocolGuid, NULL, &Count,
                                     &Handles);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Status = EFI_NOT_FOUND;
    for (i = 0; i < Count; i++) {
        if (EFI_ERROR(gBS->HandleProtocol(Handles[i], &gEfiSimpleFileSystemProtocolGuid,
                                          (VOID **)&Sfs))) {
            continue;
        }
        if (EFI_ERROR(Sfs->OpenVolume(Sfs, &Root))) {
            continue;
        }
        Status = Root->Open(Root, &File, FileName,
                            EFI_FILE_MODE_CREATE | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_READ, 0);
        if (!EFI_ERROR(Status)) {
            UINTN Written = Size;
            Status = File->Write(File, &Written, Data);
            File->Close(File);
            Root->Close(Root);
            break;
        }
        Root->Close(Root);
    }

    FreePool(Handles);
    return Status;
}

EFI_PCI_IO_PROTOCOL *DpFindFn(UINTN Bus, UINTN Dev, UINTN Fn)
{
    EFI_STATUS Status;
    EFI_HANDLE *Handles;
    UINTN Count, i, s, b, d, f;
    EFI_PCI_IO_PROTOCOL *Pci;
    EFI_PCI_IO_PROTOCOL *Found;

    Found = NULL;
    Status = gBS->LocateHandleBuffer(ByProtocol, &gEfiPciIoProtocolGuid, NULL, &Count, &Handles);
    if (EFI_ERROR(Status)) {
        return NULL;
    }
    for (i = 0; i < Count; i++) {
        if (EFI_ERROR(gBS->HandleProtocol(Handles[i], &gEfiPciIoProtocolGuid, (VOID **)&Pci))) {
            continue;
        }
        s = b = d = f = 0;
        Pci->GetLocation(Pci, &s, &b, &d, &f);
        if (b == Bus && d == Dev && f == Fn) {
            Found = Pci;
            break;
        }
    }
    FreePool(Handles);
    return Found;
}

BOOLEAN DpIobpPoll(UINTN Base)
{
    UINTN Try;
    for (Try = 1000; Try > 0; Try--) {
        if ((MmioRead16(Base + IOBPS) & IOBPS_READY) == 0) {
            return TRUE;
        }
        gBS->Stall(10);
    }
    return FALSE;
}

UINT32 DpIobpRead(UINTN Base, UINT32 Address, BOOLEAN *Ok)
{
    UINT16 Status;

    *Ok = FALSE;
    if (!DpIobpPoll(Base)) {
        return 0;
    }
    MmioWrite32(Base + IOBPIRI, Address);

    Status = MmioRead16(Base + IOBPS);
    Status &= ~IOBPS_MASK;
    Status |= IOBPS_READ;
    MmioWrite16(Base + IOBPS, Status);

    MmioWrite16(Base + IOBPU, IOBPU_MAGIC);

    Status = MmioRead16(Base + IOBPS);
    Status |= IOBPS_READY;
    MmioWrite16(Base + IOBPS, Status);

    if (!DpIobpPoll(Base)) {
        return 0;
    }
    if (MmioRead16(Base + IOBPS) & IOBPS_TX_MASK) {
        return 0;
    }
    *Ok = TRUE;
    return MmioRead32(Base + IOBPD);
}

VOID DpIobpWrite(UINTN Base, UINT32 Address, UINT32 Data)
{
    UINT16 Status;

    if (!DpIobpPoll(Base)) {
        return;
    }
    MmioWrite32(Base + IOBPIRI, Address);

    Status = MmioRead16(Base + IOBPS);
    Status &= ~IOBPS_MASK;
    Status |= IOBPS_WRITE;
    MmioWrite16(Base + IOBPS, Status);

    MmioWrite32(Base + IOBPD, Data);
    MmioWrite16(Base + IOBPU, IOBPU_MAGIC);

    Status = MmioRead16(Base + IOBPS);
    Status |= IOBPS_READY;
    MmioWrite16(Base + IOBPS, Status);

    DpIobpPoll(Base);
}

VOID DpIobpClearBit(UINTN Base, UINT32 Address, UINT32 Bit)
{
    BOOLEAN Ok;
    UINT32 Val = DpIobpRead(Base, Address, &Ok);
    if (Ok && (Val & Bit)) {
        DpIobpWrite(Base, Address, Val & ~Bit);
    }
}

UINT32 DpPciMem32Top(VOID)
{
    EFI_STATUS Status;
    EFI_HANDLE *Handles;
    UINTN Count, i;
    EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL *Rb;
    EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *Desc;
    UINT32 Best = 0;

    Status = gBS->LocateHandleBuffer(ByProtocol, &gEfiPciRootBridgeIoProtocolGuid, NULL, &Count,
                                     &Handles);
    if (EFI_ERROR(Status)) {
        return 0;
    }
    for (i = 0; i < Count; i++) {
        if (EFI_ERROR(
                gBS->HandleProtocol(Handles[i], &gEfiPciRootBridgeIoProtocolGuid, (VOID **)&Rb))) {
            continue;
        }
        if (EFI_ERROR(Rb->Configuration(Rb, (VOID **)&Desc))) {
            continue;
        }
        while (Desc->Desc == ACPI_QWORD_DESC) {
            if (Desc->ResType == ACPI_ADDRESS_SPACE_TYPE_MEM && Desc->AddrSpaceGranularity == 32 &&
                Desc->AddrLen > 0) {
                UINT64 Top = Desc->AddrRangeMin + Desc->AddrLen;
                if (Top <= FOUR_GIB && (UINT32)Top > Best) {
                    Best = (UINT32)Top;
                }
            }
            Desc = (EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *)((UINT8 *)Desc + 3 + Desc->Len);
        }
    }
    FreePool(Handles);
    return Best;
}

BOOLEAN DpDwI2cPing(UINT32 Bar, UINT8 Addr)
{
    UINTN i;
    UINT32 Stat, Abrt;

    MmioWrite32(Bar + DW_ENABLE, 0);
    for (i = 0; i < 100; i++) {
        if ((MmioRead32(Bar + DW_ENABLE_STAT) & 1) == 0)
            break;
        gBS->Stall(5);
    }
    MmioWrite32(Bar + DW_CONTROL, DW_CTRL_MASTER | DW_CTRL_SS | DW_CTRL_RESTART | DW_CTRL_SLVDIS);
    MmioWrite32(Bar + DW_SS_HCNT, DW_SS_HCNT_SLOW);
    MmioWrite32(Bar + DW_SS_LCNT, DW_SS_LCNT_SLOW);
    MmioWrite32(Bar + DW_INTR_MASK, 0);
    MmioWrite32(Bar + DW_TARGET, Addr);
    MmioWrite32(Bar + DW_ENABLE, 1);
    (VOID) MmioRead32(Bar + DW_CLR_TX_ABRT);
    MmioWrite32(Bar + DW_CMD_DATA, DW_CMD_READ);

    for (i = 0; i < 2000; i++) {
        Stat = MmioRead32(Bar + DW_RAW_INTR);
        if (Stat & (DW_INTR_RX_FULL | DW_INTR_TX_ABRT))
            break;
        gBS->Stall(5);
    }
    Stat = MmioRead32(Bar + DW_RAW_INTR);
    Abrt = MmioRead32(Bar + DW_TX_ABRT_SRC);
    (VOID) MmioRead32(Bar + DW_CLR_TX_ABRT);
    MmioWrite32(Bar + DW_ENABLE, 0);

    return (Stat & DW_INTR_RX_FULL) && !(Abrt & DW_ABRT_ADDR_NOACK);
}

VOID DpDwI2cProbe(DP_CTX *Ctx, EFI_PCI_IO_PROTOCOL *P, UINT32 Bar, const CHAR8 *Name)
{
    UINT16 Cmd;
    UINT32 BarRead, Type;

    if (P == NULL) {
        DpEmit(Ctx, "[DP] %a: no handle\n", Name);
        return;
    }

    BarRead = 0;
    P->Pci.Write(P, EfiPciIoWidthUint32, PCI_REG_BAR0, 1, &Bar);
    P->Pci.Read(P, EfiPciIoWidthUint32, PCI_REG_BAR0, 1, &BarRead);
    P->Pci.Read(P, EfiPciIoWidthUint16, PCI_REG_CMD, 1, &Cmd);
    Cmd |= PCI_CMD_MEM_BM;
    P->Pci.Write(P, EfiPciIoWidthUint16, PCI_REG_CMD, 1, &Cmd);
    P->Pci.Read(P, EfiPciIoWidthUint16, PCI_REG_CMD, 1, &Cmd);

    {
        UINT8 CapPtr = 0, CapId;
        UINT16 Pmcsr;
        UINTN Guard = 0;
        P->Pci.Read(P, EfiPciIoWidthUint8, PCI_REG_CAPPTR, 1, &CapPtr);
        while (CapPtr >= 0x40 && Guard++ < 48) {
            P->Pci.Read(P, EfiPciIoWidthUint8, CapPtr, 1, &CapId);
            if (CapId == PCI_CAP_ID_PM) {
                P->Pci.Read(P, EfiPciIoWidthUint16, CapPtr + PCI_PMCSR_OFFSET, 1, &Pmcsr);
                Pmcsr &= ~(UINT16)PCI_PMCSR_PS_MASK;
                P->Pci.Write(P, EfiPciIoWidthUint16, CapPtr + PCI_PMCSR_OFFSET, 1, &Pmcsr);
                break;
            }
            P->Pci.Read(P, EfiPciIoWidthUint8, CapPtr + 1, 1, &CapPtr);
        }
    }
    gBS->Stall(2000);

    {
        UINT32 Rst = MmioRead32(Bar + DW_PPR_RESET);
        MmioWrite32(Bar + DW_PPR_RESET, Rst & ~(UINT32)DW_PPR_RESET_BITS);
        gBS->Stall(1000);
    }

    Type = MmioRead32(Bar + DW_IC_COMP_TYPE);
    DpEmit(Ctx, "[DP] %a @%08x: BARrd=%08x CMD=%04x COMP_TYPE=%08x %a\n", Name, Bar, BarRead, Cmd,
           Type, (Type == DW_COMP_TYPE_VALUE) ? "DW_apb_i2c LIVE" : "(no decode)");

    if (Type == DW_COMP_TYPE_VALUE) {
        UINT8 Addr;
        for (Addr = 0x08; Addr <= 0x77; Addr++) {
            if (DpDwI2cPing(Bar, Addr)) {
                DpEmit(Ctx, "[DP]   I2C device ACK @ 0x%02x\n", Addr);
            }
        }
        DpEmit(Ctx, "[DP] %a scan complete\n", Name);
    }
}

#pragma pack(1)
typedef struct
{
    UINT8 Signature[8];
    UINT8 Checksum;
    UINT8 OemId[6];
    UINT8 Revision;
    UINT32 RsdtAddress;
    UINT32 Length;
    UINT64 XsdtAddress;
    UINT8 ExtendedChecksum;
    UINT8 Reserved[3];
} DP_RSDP;
#pragma pack()

#define DP_MAX_TABLE 0x100000
#define DP_SIG_FACP SIGNATURE_32('F', 'A', 'C', 'P')

static DP_RSDP *DpFindRsdp(VOID)
{
    UINTN i;
    DP_RSDP *Rsdp = NULL;

    for (i = 0; i < gST->NumberOfTableEntries; i++) {
        EFI_GUID *G = &(gST->ConfigurationTable[i].VendorGuid);
        if (CompareGuid(G, &gEfiAcpiTableGuid)) {
            return (DP_RSDP *)gST->ConfigurationTable[i].VendorTable;
        }
        if (CompareGuid(G, &gEfiAcpi10TableGuid)) {
            Rsdp = (DP_RSDP *)gST->ConfigurationTable[i].VendorTable;
        }
    }
    return Rsdp;
}

static VOID DpDumpTable(DP_CTX *Ctx, EFI_ACPI_DESCRIPTION_HEADER *Hdr, UINT32 *SeenSig,
                        UINTN *SeenCnt, UINTN SeenMax, CHAR8 *Index, UINTN IndexSize)
{
    CHAR16 FileName[24];
    CHAR8 Line[160];
    UINT8 *S;
    UINTN Dup, s, j;

    if (Hdr == NULL || Hdr->Length < sizeof(EFI_ACPI_DESCRIPTION_HEADER) ||
        Hdr->Length > DP_MAX_TABLE) {
        return;
    }

    Dup = 0;
    for (s = 0; s < *SeenCnt; s++) {
        if (SeenSig[s] == Hdr->Signature) {
            Dup++;
        }
    }
    if (*SeenCnt < SeenMax) {
        SeenSig[(*SeenCnt)++] = Hdr->Signature;
    }

    {
        CHAR16 Sig[5];
        S = (UINT8 *)&Hdr->Signature;
        for (j = 0; j < 4; j++) {
            Sig[j] = (S[j] >= 0x20 && S[j] < 0x7F) ? (CHAR16)S[j] : L'_';
        }
        Sig[4] = L'\0';
        if (Dup == 0) {
            UnicodeSPrint(FileName, sizeof(FileName), L"%s.aml", Sig);
        }
        else {
            UnicodeSPrint(FileName, sizeof(FileName), L"%s%d.aml", Sig, Dup);
        }
    }

    DpSaveFile(FileName, (UINT8 *)Hdr, Hdr->Length);
    AsciiSPrint(Line, sizeof(Line), "[DP]   %c%c%c%c  len=%u  rev=%u  -> %s\n", S[0], S[1], S[2],
                S[3], Hdr->Length, Hdr->Revision, FileName);
    AsciiStrCatS(Index, IndexSize, Line);
    DpEmit(Ctx, "%a", Line + 5);
}

VOID DpDumpAcpi(DP_CTX *Ctx)
{
    DP_RSDP *Rsdp;
    EFI_ACPI_DESCRIPTION_HEADER *Root;
    UINT8 *EntryBase;
    UINTN EntryCount, i, SeenCnt = 0;
    UINT32 SeenSig[64];
    CHAR8 Index[4096];
    EFI_ACPI_TABLE_PROTOCOL *AcpiProto = NULL;
    EFI_STATUS PStatus;

    PStatus = gBS->LocateProtocol(&gEfiAcpiTableProtocolGuid, NULL, (VOID **)&AcpiProto);
    DpEmit(Ctx, "[DP] ACPI: EFI_ACPI_TABLE_PROTOCOL %a\n",
           (!EFI_ERROR(PStatus) && AcpiProto != NULL) ? "PRESENT (clean InstallAcpiTable)"
                                                      : "ABSENT (will need XSDT surgery)");

    Rsdp = DpFindRsdp();
    if (Rsdp == NULL) {
        DpEmit(Ctx, "[DP] ACPI: no RSDP in EFI config table\n");
        return;
    }
    Index[0] = '\0';

    // NOLINTBEGIN(performance-no-int-to-ptr)
    if (Rsdp->Revision >= 2 && Rsdp->XsdtAddress != 0) {
        Root = (EFI_ACPI_DESCRIPTION_HEADER *)(UINTN)Rsdp->XsdtAddress;
        EntryBase = (UINT8 *)Root + sizeof(EFI_ACPI_DESCRIPTION_HEADER);
        EntryCount = (Root->Length - sizeof(EFI_ACPI_DESCRIPTION_HEADER)) / sizeof(UINT64);
        DpEmit(Ctx, "[DP] ACPI: RSDP rev=%u XSDT@%lx (%u entries)\n", Rsdp->Revision,
               (UINT64)Rsdp->XsdtAddress, (UINT32)EntryCount);
        DpDumpTable(Ctx, Root, SeenSig, &SeenCnt, 64, Index, sizeof(Index));
        for (i = 0; i < EntryCount; i++) {
            UINT64 Ptr = 0;
            EFI_ACPI_DESCRIPTION_HEADER *Tbl;
            CopyMem(&Ptr, EntryBase + i * sizeof(UINT64), sizeof(UINT64));
            if (Ptr == 0) {
                continue;
            }
            Tbl = (EFI_ACPI_DESCRIPTION_HEADER *)(UINTN)Ptr;
            DpDumpTable(Ctx, Tbl, SeenSig, &SeenCnt, 64, Index, sizeof(Index));
            if (Tbl->Signature == DP_SIG_FACP) {
                UINT64 XDsdt = 0;
                UINT32 Dsdt = 0;
                EFI_ACPI_DESCRIPTION_HEADER *D = NULL;
                if (Tbl->Length >= 44) {
                    CopyMem(&Dsdt, (UINT8 *)Tbl + 40, sizeof(UINT32));
                }
                if (Tbl->Length >= 148) {
                    CopyMem(&XDsdt, (UINT8 *)Tbl + 140, sizeof(UINT64));
                }
                D = (XDsdt != 0)  ? (EFI_ACPI_DESCRIPTION_HEADER *)(UINTN)XDsdt
                    : (Dsdt != 0) ? (EFI_ACPI_DESCRIPTION_HEADER *)(UINTN)Dsdt
                                  : NULL;
                if (D != NULL) {
                    DpDumpTable(Ctx, D, SeenSig, &SeenCnt, 64, Index, sizeof(Index));
                }
            }
        }
    }
    else if (Rsdp->RsdtAddress != 0) {
        Root = (EFI_ACPI_DESCRIPTION_HEADER *)(UINTN)Rsdp->RsdtAddress;
        EntryBase = (UINT8 *)Root + sizeof(EFI_ACPI_DESCRIPTION_HEADER);
        EntryCount = (Root->Length - sizeof(EFI_ACPI_DESCRIPTION_HEADER)) / sizeof(UINT32);
        DpEmit(Ctx, "[DP] ACPI: RSDP rev=%u RSDT@%x (%u entries)\n", Rsdp->Revision,
               Rsdp->RsdtAddress, (UINT32)EntryCount);
        DpDumpTable(Ctx, Root, SeenSig, &SeenCnt, 64, Index, sizeof(Index));
        for (i = 0; i < EntryCount; i++) {
            UINT32 Ptr = 0;
            EFI_ACPI_DESCRIPTION_HEADER *Tbl;
            CopyMem(&Ptr, EntryBase + i * sizeof(UINT32), sizeof(UINT32));
            if (Ptr == 0) {
                continue;
            }
            Tbl = (EFI_ACPI_DESCRIPTION_HEADER *)(UINTN)Ptr;
            DpDumpTable(Ctx, Tbl, SeenSig, &SeenCnt, 64, Index, sizeof(Index));
            if (Tbl->Signature == DP_SIG_FACP && Tbl->Length >= 44) {
                UINT32 Dsdt = 0;
                CopyMem(&Dsdt, (UINT8 *)Tbl + 40, sizeof(UINT32));
                if (Dsdt != 0) {
                    DpDumpTable(Ctx, (EFI_ACPI_DESCRIPTION_HEADER *)(UINTN)Dsdt, SeenSig, &SeenCnt,
                                64, Index, sizeof(Index));
                }
            }
        }
    }
    // NOLINTEND(performance-no-int-to-ptr)

    if (AsciiStrLen(Index) > 0) {
        DpSaveFile(L"darkpassenger-acpi-index.txt", (UINT8 *)Index, AsciiStrLen(Index));
    }
    DpEmit(Ctx, "[DP] ACPI: dumped %u tables to ESP\n", (UINT32)SeenCnt);
}

static UINT8 DpAcpiSum(const VOID *Ptr, UINTN Len)
{
    UINT8 Sum = 0;
    const UINT8 *B = Ptr;
    while (Len-- > 0) {
        Sum = (UINT8)(Sum + *B++);
    }
    return Sum;
}

EFI_STATUS DpInstallAcpiTable(DP_CTX *Ctx, const VOID *Table, UINTN Size)
{
    EFI_STATUS Status;
    EFI_ACPI_TABLE_PROTOCOL *Proto = NULL;
    UINTN Key = 0;
    DP_RSDP *Rsdp;
    EFI_ACPI_DESCRIPTION_HEADER *Xsdt;
    EFI_ACPI_DESCRIPTION_HEADER *NewXsdt;
    EFI_PHYSICAL_ADDRESS SsdtMem = 0;
    EFI_PHYSICAL_ADDRESS XsdtMem = 0;
    UINTN NewXsdtLen;
    UINT64 SsdtPtr;

    Status = gBS->LocateProtocol(&gEfiAcpiTableProtocolGuid, NULL, (VOID **)&Proto);
    if (!EFI_ERROR(Status) && Proto != NULL) {
        Status = Proto->InstallAcpiTable(Proto, (VOID *)Table, Size, &Key);
        DpEmit(Ctx, "[DP] ACPI: InstallAcpiTable -> %r\n", Status);
        return Status;
    }

    Rsdp = DpFindRsdp();
    if (Rsdp == NULL || Rsdp->Revision < 2 || Rsdp->XsdtAddress == 0) {
        DpEmit(Ctx, "[DP] ACPI: no XSDT to patch -- inject skipped\n");
        return EFI_UNSUPPORTED;
    }
    // NOLINTBEGIN(performance-no-int-to-ptr)
    Xsdt = (EFI_ACPI_DESCRIPTION_HEADER *)(UINTN)Rsdp->XsdtAddress;

    Status = gBS->AllocatePages(AllocateAnyPages, EfiACPIReclaimMemory, EFI_SIZE_TO_PAGES(Size),
                                &SsdtMem);
    if (EFI_ERROR(Status)) {
        DpEmit(Ctx, "[DP] ACPI: SSDT alloc -> %r\n", Status);
        return Status;
    }
    CopyMem((VOID *)(UINTN)SsdtMem, Table, Size);

    NewXsdtLen = Xsdt->Length + sizeof(UINT64);
    Status = gBS->AllocatePages(AllocateAnyPages, EfiACPIReclaimMemory,
                                EFI_SIZE_TO_PAGES(NewXsdtLen), &XsdtMem);
    if (EFI_ERROR(Status)) {
        gBS->FreePages(SsdtMem, EFI_SIZE_TO_PAGES(Size));
        DpEmit(Ctx, "[DP] ACPI: XSDT alloc -> %r\n", Status);
        return Status;
    }
    NewXsdt = (EFI_ACPI_DESCRIPTION_HEADER *)(UINTN)XsdtMem;
    CopyMem(NewXsdt, Xsdt, Xsdt->Length);
    SsdtPtr = (UINT64)SsdtMem;
    CopyMem((UINT8 *)NewXsdt + Xsdt->Length, &SsdtPtr, sizeof(UINT64));
    NewXsdt->Length = (UINT32)NewXsdtLen;
    NewXsdt->Checksum = 0;
    NewXsdt->Checksum = (UINT8)(0 - DpAcpiSum(NewXsdt, NewXsdtLen));

    Rsdp->XsdtAddress = (UINT64)XsdtMem;
    Rsdp->Checksum = 0;
    Rsdp->Checksum = (UINT8)(0 - DpAcpiSum(Rsdp, 20));
    Rsdp->ExtendedChecksum = 0;
    Rsdp->ExtendedChecksum = (UINT8)(0 - DpAcpiSum(Rsdp, Rsdp->Length));
    // NOLINTEND(performance-no-int-to-ptr)

    DpEmit(Ctx, "[DP] ACPI: XSDT surgery -> SSDT@%lx, new XSDT@%lx (%u entries)\n", (UINT64)SsdtMem,
           (UINT64)XsdtMem, (UINT32)((NewXsdtLen - sizeof(EFI_ACPI_DESCRIPTION_HEADER)) / 8));
    return EFI_SUCCESS;
}
