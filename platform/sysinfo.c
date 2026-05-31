// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#include "global.h"
#include "lib.h"
#include "sysinfo.h"
#include "mystrings.h"
#include "meridian_funcs.h"
#include "apple/smc.h"
#include <Protocol/Tcg2Protocol.h>
#include <Protocol/NvmExpressPassthru.h>
#include <IndustryStandard/Pci.h>
#include <IndustryStandard/Nvme.h>
#include <Protocol/UsbIo.h>

#define MRD_SMBIOS_TABLE_GUID                                                                      \
    {0xEB9D2D31, 0x2D88, 0x11D3, {0x9A, 0x16, 0x00, 0x90, 0x27, 0x3F, 0xC1, 0x4D}}
#define MRD_SMBIOS3_TABLE_GUID                                                                     \
    {0xF2FD1544, 0x9794, 0x4A2C, {0x99, 0x2E, 0xE5, 0xBB, 0xCF, 0x20, 0xE3, 0x94}}

#define PCI_VENDOR_AMD 0x1002
#define PCI_VENDOR_NVIDIA 0x10DE
#define PCI_VENDOR_INTEL 0x8086
#define PCI_VENDOR_MATROX 0x102B
#define PCI_VENDOR_ASPEED 0x1A03
#define PCI_VENDOR_QEMU 0x1234

#define SMBIOS_DIMM_SIZE_UNKNOWN 0xFFFF
#define SMBIOS_DIMM_SIZE_EXTENDED 0x7FFF
#define SMBIOS_DIMM_SIZE_KB_FLAG 0x8000
#define SMBIOS_DIMM_SIZE_MASK 0x7FFF
#define SMBIOS_DIMM_EXTSIZE_MASK 0x7FFFFFFFUL
#define SMBIOS_T17_EXTSIZE_MIN_LEN 0x28
#define SMBIOS_WIDTH_UNKNOWN 0xFFFF

#define BATTERY_MINUTES_UNKNOWN 0xFFFF

typedef VOID (*MRD_SMBIOS_RECORD_VISITOR)(IN SMBIOS_STRUCTURE *Hdr, IN VOID *Context);

FIRMWARE_VENDOR_TYPE MeridianFirmwareVendor = FW_VENDOR_UNKNOWN;
UINTN MeridianDiskCount = 0;
MERIDIAN_DISK_INFO *MeridianDisks = NULL;
UINTN MeridianDimmCount = 0;
MERIDIAN_DIMM_INFO *MeridianDimms = NULL;
UINT64 MeridianTotalRamBytes = 0;
BOOLEAN MeridianMemorySoldered = FALSE;
UINT64 MeridianSolderedTotalMB = 0;
UINT8 MeridianSolderedMemType = 0;
MERIDIAN_BATTERY_INFO MeridianBattery = {FALSE, FALSE, FALSE, FALSE, 0, BATTERY_MINUTES_UNKNOWN};
MERIDIAN_CPU_INFO MeridianCpu = {FALSE, "", 0, 0, 0};
UINTN MeridianGpuCount = 0;
MERIDIAN_GPU_INFO *MeridianGpus = NULL;
MERIDIAN_DISPLAY_INFO MeridianDisplay = {FALSE, FALSE, "", 0, 0};
MERIDIAN_TPM_INFO MeridianTpm = {FALSE, FALSE, FALSE};
MERIDIAN_SYSTEM_INFO MeridianSystem = {FALSE, "", "", "", ""};
CHAR16 *MeridianSystemLabel = NULL;

static DISK_BUS_TYPE GetDiskBusFromPath(IN EFI_DEVICE_PATH_PROTOCOL *DevPath)
{
    EFI_DEVICE_PATH_PROTOCOL *Node;

    Node = DevPath;
    while (Node != NULL && !IsDevicePathEndType(Node)) {
        if (DevicePathType(Node) == MESSAGING_DEVICE_PATH) {
            switch (DevicePathSubType(Node)) {
            case MSG_NVME_NAMESPACE_DP:
                return DISK_BUS_NVME;
            case MSG_SATA_DP:
                return DISK_BUS_SATA;
            case MSG_USB_DP:
            case MSG_USB_CLASS_DP:
                return DISK_BUS_USB;
            case MSG_SCSI_DP:
                return DISK_BUS_SCSI;
            case MSG_1394_DP:
                return DISK_BUS_FIREWIRE;
            default:
                break;
            }
        }
        Node = NextDevicePathNode(Node);
    }

    return DISK_BUS_UNKNOWN;
}

FIRMWARE_VENDOR_TYPE
DetectFirmwareVendor(VOID)
{
    CHAR16 *Vendor;

    if (gST == NULL || gST->FirmwareVendor == NULL) {
        MeridianFirmwareVendor = FW_VENDOR_UNKNOWN;
        return FW_VENDOR_UNKNOWN;
    }

    Vendor = gST->FirmwareVendor;

    if (MrdStrStartsWithCI(L"Apple", Vendor)) {
        MeridianFirmwareVendor = FW_VENDOR_APPLE;
    }
    else if (MrdStrIncludesCI(Vendor, L"coreboot")) {
        MeridianFirmwareVendor = FW_VENDOR_COREBOOT;
    }
    else if (MrdStrIncludesCI(Vendor, L"American Megatrends") || MrdStrIncludesCI(Vendor, L"AMI")) {
        MeridianFirmwareVendor = FW_VENDOR_AMI;
    }
    else if (MrdStrIncludesCI(Vendor, L"Insyde")) {
        MeridianFirmwareVendor = FW_VENDOR_INSYDE;
    }
    else if (MrdStrIncludesCI(Vendor, L"Phoenix")) {
        MeridianFirmwareVendor = FW_VENDOR_PHOENIX;
    }
    else if (MrdStrIncludesCI(Vendor, L"EDK")) {
        MeridianFirmwareVendor = FW_VENDOR_EDK2;
    }
    else {
        MeridianFirmwareVendor = FW_VENDOR_UNKNOWN;
    }

    return MeridianFirmwareVendor;
}

VOID ScanDisks(VOID)
{
    EFI_STATUS Status;
    EFI_HANDLE *Handles;
    UINTN HandleCount;
    UINTN i;
    EFI_BLOCK_IO_PROTOCOL *BlockIo;
    EFI_DEVICE_PATH_PROTOCOL *DevPath;
    EFI_BLOCK_IO_MEDIA *Media;
    UINTN DiskIdx;

    if (MeridianDisks != NULL) {
        MRD_FREE_POOL(MeridianDisks);
        MeridianDiskCount = 0;
    }

    Status =
        gBS->LocateHandleBuffer(ByProtocol, &gEfiBlockIoProtocolGuid, NULL, &HandleCount, &Handles);

    if (EFI_ERROR(Status) || HandleCount == 0) {
        return;
    }

    for (i = 0; i < HandleCount; i++) {
        Status = gBS->HandleProtocol(Handles[i], &gEfiBlockIoProtocolGuid, (VOID **)&BlockIo);

        if (EFI_ERROR(Status) || BlockIo == NULL) {
            continue;
        }

        Media = BlockIo->Media;
        if (!Media->MediaPresent || Media->LogicalPartition) {
            continue;
        }

        MeridianDiskCount++;
    }

    if (MeridianDiskCount == 0) {
        gBS->FreePool(Handles);
        return;
    }

    MeridianDisks = AllocateZeroPool(MeridianDiskCount * sizeof(MERIDIAN_DISK_INFO));
    if (MeridianDisks == NULL) {
        MeridianDiskCount = 0;
        gBS->FreePool(Handles);
        return;
    }

    DiskIdx = 0;
    for (i = 0; i < HandleCount && DiskIdx < MeridianDiskCount; i++) {
        Status = gBS->HandleProtocol(Handles[i], &gEfiBlockIoProtocolGuid, (VOID **)&BlockIo);

        if (EFI_ERROR(Status) || BlockIo == NULL) {
            continue;
        }

        Media = BlockIo->Media;
        if (!Media->MediaPresent || Media->LogicalPartition) {
            continue;
        }

        MeridianDisks[DiskIdx].Present = TRUE;
        MeridianDisks[DiskIdx].Removable = Media->RemovableMedia;
        MeridianDisks[DiskIdx].SizeBytes = MultU64x32(Media->LastBlock + 1, Media->BlockSize);

        Status = gBS->HandleProtocol(Handles[i], &gEfiDevicePathProtocolGuid, (VOID **)&DevPath);

        MeridianDisks[DiskIdx].BusType =
            EFI_ERROR(Status) ? DISK_BUS_UNKNOWN : GetDiskBusFromPath(DevPath);

        if (MeridianDisks[DiskIdx].BusType == DISK_BUS_UNKNOWN && !Media->RemovableMedia &&
            Media->BlockSize == 512) {
            MeridianDisks[DiskIdx].BusType = DISK_BUS_VIRTUAL;
        }

        if (MeridianDisks[DiskIdx].BusType == DISK_BUS_NVME) {
            EFI_NVM_EXPRESS_PASS_THRU_PROTOCOL *NvmePassthru;
            Status = gBS->HandleProtocol(Handles[i], &gEfiNvmExpressPassThruProtocolGuid,
                                         (VOID **)&NvmePassthru);
            if (!EFI_ERROR(Status)) {
                NVME_ADMIN_CONTROLLER_DATA *IdData;
                EFI_NVM_EXPRESS_COMMAND Cmd;
                EFI_NVM_EXPRESS_COMPLETION Completion;
                EFI_NVM_EXPRESS_PASS_THRU_COMMAND_PACKET Packet;

                IdData = AllocateZeroPool(sizeof(NVME_ADMIN_CONTROLLER_DATA));
                if (IdData != NULL) {
                    ZeroMem(&Cmd, sizeof(Cmd));
                    ZeroMem(&Completion, sizeof(Completion));
                    ZeroMem(&Packet, sizeof(Packet));

                    Cmd.Cdw0.Opcode = 0x06;
                    Cmd.Cdw0.FusedOperation = 0;
                    Cmd.Flags = CDW10_VALID;
                    Cmd.Nsid = 0;
                    Cmd.Cdw10 = 1;

                    Packet.NvmeCmd = &Cmd;
                    Packet.NvmeCompletion = &Completion;
                    Packet.TransferBuffer = IdData;
                    Packet.TransferLength = sizeof(NVME_ADMIN_CONTROLLER_DATA);
                    Packet.CommandTimeout = EFI_TIMER_PERIOD_SECONDS(5);
                    Packet.QueueType = NVME_ADMIN_QUEUE;

                    Status = NvmePassthru->PassThru(NvmePassthru, 0, &Packet, NULL);
                    if (!EFI_ERROR(Status)) {

                        UINT8 j;
                        CopyMem(MeridianDisks[DiskIdx].Model, IdData->Mn, 40);
                        for (j = 39; j > 0 && MeridianDisks[DiskIdx].Model[j] == ' '; j--) {
                            MeridianDisks[DiskIdx].Model[j] = '\0';
                        }
                        MeridianDisks[DiskIdx].Model[40] = '\0';

                        CopyMem(MeridianDisks[DiskIdx].Serial, IdData->Sn, 20);
                        for (j = 19; j > 0 && MeridianDisks[DiskIdx].Serial[j] == ' '; j--) {
                            MeridianDisks[DiskIdx].Serial[j] = '\0';
                        }
                        MeridianDisks[DiskIdx].Serial[20] = '\0';
                    }
                    FreePool(IdData);
                }
            }
        }

        DiskIdx++;
    }

    MeridianDiskCount = DiskIdx;

    gBS->FreePool(Handles);
}

CHAR16 *FirmwareVendorName(VOID)
{
    switch (MeridianFirmwareVendor) {
    case FW_VENDOR_APPLE:
        return L"Apple";
    case FW_VENDOR_AMI:
        return L"AMI (American Megatrends)";
    case FW_VENDOR_COREBOOT:
        return L"coreboot";
    case FW_VENDOR_INSYDE:
        return L"Insyde";
    case FW_VENDOR_PHOENIX:
        return L"Phoenix";
    case FW_VENDOR_EDK2:
        return L"EDK II";
    default:
        return L"Unknown";
    }
}

CHAR16 *DiskBusName(IN DISK_BUS_TYPE BusType)
{
    switch (BusType) {
    case DISK_BUS_NVME:
        return L"NVMe";
    case DISK_BUS_SATA:
        return L"SATA";
    case DISK_BUS_USB:
        return L"USB";
    case DISK_BUS_SCSI:
        return L"SCSI";
    case DISK_BUS_FIREWIRE:
        return L"FireWire";
    case DISK_BUS_VIRTUAL:
        return L"Virtual";
    default:
        return L"Unknown";
    }
}

CHAR16 *MemoryTypeName(IN UINT8 MemType)
{
    switch (MemType) {
    case 0x12:
        return L"DDR";
    case 0x13:
        return L"DDR2";
    case 0x18:
        return L"DDR3";
    case 0x1A:
        return L"DDR4";
    case 0x1B:
        return L"LPDDR";
    case 0x1C:
        return L"LPDDR2";
    case 0x1D:
        return L"LPDDR3";
    case 0x1E:
        return L"LPDDR4";
    case 0x22:
        return L"DDR5";
    case 0x23:
        return L"LPDDR5";
    default:
        return L"RAM";
    }
}

CHAR8 *MemoryTypeNameA(IN UINT8 MemType)
{
    switch (MemType) {
    case 0x12:
        return "DDR";
    case 0x13:
        return "DDR2";
    case 0x18:
        return "DDR3";
    case 0x1A:
        return "DDR4";
    case 0x1B:
        return "LPDDR";
    case 0x1C:
        return "LPDDR2";
    case 0x1D:
        return "LPDDR3";
    case 0x1E:
        return "LPDDR4";
    case 0x22:
        return "DDR5";
    case 0x23:
        return "LPDDR5";
    default:
        return "RAM";
    }
}

static CHAR8 *GetSmbiosString(IN SMBIOS_STRUCTURE *Hdr, IN SMBIOS_TABLE_STRING Index)
{
    CHAR8 *Ptr;
    UINT8 i;

    if (Index == 0) {
        return "";
    }

    Ptr = (CHAR8 *)Hdr + Hdr->Length;
    for (i = 1; i < Index; i++) {
        while (*Ptr != '\0') {
            Ptr++;
        }
        Ptr++;
        if (*Ptr == '\0') {
            return "";
        }
    }

    return Ptr;
}

static BOOLEAN WalkSmbiosViaProtocol(IN MRD_SMBIOS_RECORD_VISITOR Visitor, IN VOID *Context)
{
    EFI_STATUS Status;
    EFI_SMBIOS_PROTOCOL *Smbios;
    EFI_SMBIOS_HANDLE Handle;
    EFI_SMBIOS_TABLE_HEADER *Record;
    BOOLEAN Seen;

    Status = gBS->LocateProtocol(&gEfiSmbiosProtocolGuid, NULL, (VOID **)&Smbios);
    if (EFI_ERROR(Status)) {
        return FALSE;
    }

    Seen = FALSE;
    Handle = SMBIOS_HANDLE_PI_RESERVED;
    while (!EFI_ERROR(Smbios->GetNext(Smbios, &Handle, NULL, &Record, NULL))) {
        Seen = TRUE;
        Visitor((SMBIOS_STRUCTURE *)Record, Context);
    }

    return Seen;
}

static BOOLEAN WalkSmbiosViaConfigTable(IN MRD_SMBIOS_RECORD_VISITOR Visitor, IN VOID *Context)
{
    EFI_GUID Smbios3Guid = MRD_SMBIOS3_TABLE_GUID;
    EFI_GUID SmbiosGuid = MRD_SMBIOS_TABLE_GUID;
    UINT8 *Tbl = NULL;
    UINT8 *End;
    UINT8 *p;
    UINTN Size = 0;
    UINTN i;
    BOOLEAN Seen = FALSE;

    if (gST == NULL || gST->ConfigurationTable == NULL) {
        return FALSE;
    }

    for (i = 0; i < gST->NumberOfTableEntries; i++) {
        EFI_GUID *Guid = &gST->ConfigurationTable[i].VendorGuid;
        VOID *Vendor = gST->ConfigurationTable[i].VendorTable;

        if (CompareGuid(Guid, &Smbios3Guid)) {
            SMBIOS_TABLE_3_0_ENTRY_POINT *Ep = (SMBIOS_TABLE_3_0_ENTRY_POINT *)Vendor;
            Tbl = (UINT8 *)(UINTN)Ep->TableAddress; // NOLINT(performance-no-int-to-ptr)
            Size = Ep->TableMaximumSize;
            break;
        }
        if (CompareGuid(Guid, &SmbiosGuid)) {
            SMBIOS_TABLE_ENTRY_POINT *Ep = (SMBIOS_TABLE_ENTRY_POINT *)Vendor;
            Tbl = (UINT8 *)(UINTN)Ep->TableAddress; // NOLINT(performance-no-int-to-ptr)
            Size = Ep->TableLength;

        }
    }

    if (Tbl == NULL || Size == 0) {
        return FALSE;
    }

    p = Tbl;
    End = Tbl + Size;
    while (p + sizeof(SMBIOS_STRUCTURE) <= End) {
        SMBIOS_STRUCTURE *Hdr = (SMBIOS_STRUCTURE *)p;
        UINT8 *Next;

        if (Hdr->Type == SMBIOS_TYPE_END_OF_TABLE || Hdr->Length < sizeof(SMBIOS_STRUCTURE)) {
            break;
        }
        if ((UINTN)(End - p) < Hdr->Length) {
            break;
        }

        Seen = TRUE;
        Visitor(Hdr, Context);

        Next = p + Hdr->Length;
        while (Next + 1 < End && (Next[0] != 0 || Next[1] != 0)) {
            Next++;
        }
        p = Next + 2;
    }

    return Seen;
}

static VOID CountDimmRecord(IN SMBIOS_STRUCTURE *Hdr, IN VOID *Context)
{
    UINTN *Count;

    if (Hdr->Type != SMBIOS_TYPE_MEMORY_DEVICE) {
        return;
    }

    Count = (UINTN *)Context;
    *Count += 1;
}

typedef struct
{
    UINTN Index;
    UINTN Count;
} MRD_DIMM_SCAN_CONTEXT;

static BOOLEAN AsciiStrIsBlankOrZero(IN CONST CHAR8 *Str)
{
    UINTN i;

    if (Str == NULL || Str[0] == '\0') {
        return TRUE;
    }
    for (i = 0; Str[i] != '\0'; i++) {
        if (Str[i] != '0') {
            return FALSE;
        }
    }
    return TRUE;
}

static BOOLEAN AsciiStrStartsWithCI(IN CONST CHAR8 *Str, IN CONST CHAR8 *Prefix)
{
    UINTN i;

    for (i = 0; Prefix[i] != '\0'; i++) {
        CHAR8 A = Str[i];
        CHAR8 B = Prefix[i];
        if (A >= 'a' && A <= 'z') {
            A = (CHAR8)(A - 0x20);
        }
        if (B >= 'a' && B <= 'z') {
            B = (CHAR8)(B - 0x20);
        }
        if (A != B) {
            return FALSE;
        }
    }
    return TRUE;
}

static BOOLEAN AsciiStrLooksLikePlaceholder(IN CONST CHAR8 *Str)
{
    STATIC CONST CHAR8 *CONST Placeholders[] = {
        "Unknown", "Not Specified", "To Be Filled", "NO DIMM", "None", "N/A", "Empty",
    };
    UINTN i;

    if (Str == NULL || Str[0] == '\0') {
        return TRUE;
    }
    for (i = 0; i < ARRAY_SIZE(Placeholders); i++) {
        if (AsciiStrStartsWithCI(Str, Placeholders[i])) {
            return TRUE;
        }
    }
    return FALSE;
}

static BOOLEAN DimmLooksSoldered(IN SMBIOS_STRUCTURE *Hdr, IN SMBIOS_TABLE_TYPE17 *T17)
{
    BOOLEAN IsLpddr;
    CHAR8 *Mfr;
    CHAR8 *Serial;
    CHAR8 *Part;

    if (T17->FormFactor == MemoryFormFactorCamm || T17->FormFactor == MemoryFormFactorCuDimm ||
        T17->FormFactor == MemoryFormFactorCsoDimm) {
        return FALSE;
    }

    IsLpddr = (T17->MemoryType >= 0x1B && T17->MemoryType <= 0x1E) || T17->MemoryType == 0x23;
    if (IsLpddr || T17->FormFactor == MemoryFormFactorDie) {
        return TRUE;
    }

    Mfr = GetSmbiosString(Hdr, T17->Manufacturer);
    Serial = GetSmbiosString(Hdr, T17->SerialNumber);
    Part = GetSmbiosString(Hdr, T17->PartNumber);
    if (!AsciiStrLooksLikePlaceholder(Mfr) || !AsciiStrIsBlankOrZero(Serial) ||
        !AsciiStrLooksLikePlaceholder(Part)) {
        return FALSE;
    }
    return TRUE;
}

static VOID PopulateDimmRecord(IN SMBIOS_STRUCTURE *Hdr, IN VOID *Context)
{
    SMBIOS_TABLE_TYPE17 *T17;
    MRD_DIMM_SCAN_CONTEXT *Scan;
    CHAR8 *Str;
    UINTN DimmIdx;

    if (Hdr->Type != SMBIOS_TYPE_MEMORY_DEVICE) {
        return;
    }

    Scan = (MRD_DIMM_SCAN_CONTEXT *)Context;
    if (Scan->Index >= Scan->Count) {
        return;
    }

    DimmIdx = Scan->Index;
    T17 = (SMBIOS_TABLE_TYPE17 *)Hdr;

    if (T17->Size == 0 || T17->Size == SMBIOS_DIMM_SIZE_UNKNOWN) {
        MeridianDimms[DimmIdx].SizeMB = 0;
        MeridianDimms[DimmIdx].Populated = FALSE;
    }
    else {
        MeridianDimms[DimmIdx].Populated = TRUE;
        if (T17->Size == SMBIOS_DIMM_SIZE_EXTENDED &&
            T17->Hdr.Length >= SMBIOS_T17_EXTSIZE_MIN_LEN) {
            MeridianDimms[DimmIdx].SizeMB = T17->ExtendedSize & SMBIOS_DIMM_EXTSIZE_MASK;
        }
        else if (T17->Size & SMBIOS_DIMM_SIZE_KB_FLAG) {

            MeridianDimms[DimmIdx].SizeMB = (T17->Size & SMBIOS_DIMM_SIZE_MASK) / 1024;
        }
        else {
            MeridianDimms[DimmIdx].SizeMB = T17->Size;
        }
    }

    MeridianDimms[DimmIdx].SpeedMTs = T17->Speed;
    MeridianDimms[DimmIdx].MemoryType = T17->MemoryType;
    MeridianDimms[DimmIdx].FormFactor = T17->FormFactor;
    MeridianDimms[DimmIdx].IsEcc = (T17->TotalWidth != SMBIOS_WIDTH_UNKNOWN) &&
                                   (T17->DataWidth != SMBIOS_WIDTH_UNKNOWN) &&
                                   (T17->TotalWidth > T17->DataWidth);
    MeridianDimms[DimmIdx].Soldered =
        MeridianDimms[DimmIdx].Populated && DimmLooksSoldered(Hdr, T17);

    Str = GetSmbiosString(Hdr, T17->DeviceLocator);
    AsciiStrnCpyS(MeridianDimms[DimmIdx].Slot, sizeof(MeridianDimms[DimmIdx].Slot), Str,
                  sizeof(MeridianDimms[DimmIdx].Slot) - 1);

    Str = GetSmbiosString(Hdr, T17->Manufacturer);
    AsciiStrnCpyS(MeridianDimms[DimmIdx].Maker, sizeof(MeridianDimms[DimmIdx].Maker), Str,
                  sizeof(MeridianDimms[DimmIdx].Maker) - 1);

    Scan->Index++;
}

VOID ScanMemory(VOID)
{
    EFI_STATUS Status;
    UINTN i;
    BOOLEAN UseConfigTable;
    MRD_DIMM_SCAN_CONTEXT Scan;

    EFI_MEMORY_DESCRIPTOR *MemMap = NULL;
    UINTN MapSize = 0;
    UINTN MapKey = 0;
    UINTN DescSize = 0;
    UINT32 DescVersion = 0;
    EFI_MEMORY_DESCRIPTOR *Desc;

    MeridianMemorySoldered = FALSE;
    MeridianSolderedTotalMB = 0;
    MeridianSolderedMemType = 0;

    MeridianTotalRamBytes = 0;
    Status = gBS->GetMemoryMap(&MapSize, MemMap, &MapKey, &DescSize, &DescVersion);
    if (Status == EFI_BUFFER_TOO_SMALL) {
        MapSize += 2 * DescSize;
        MemMap = AllocatePool(MapSize);
        if (MemMap != NULL) {
            Status = gBS->GetMemoryMap(&MapSize, MemMap, &MapKey, &DescSize, &DescVersion);
            if (!EFI_ERROR(Status)) {
                Desc = MemMap;
                for (i = 0; i < MapSize / DescSize; i++) {
                    if (Desc->Type == EfiConventionalMemory) {
                        MeridianTotalRamBytes += MultU64x32(Desc->NumberOfPages, EFI_PAGE_SIZE);
                    }
                    Desc = NEXT_MEMORY_DESCRIPTOR(Desc, DescSize);
                }
            }
            FreePool(MemMap);
        }
    }

    if (MeridianDimms != NULL) {
        MRD_FREE_POOL(MeridianDimms);
        MeridianDimmCount = 0;
    }

    UseConfigTable = FALSE;
    WalkSmbiosViaProtocol(CountDimmRecord, &MeridianDimmCount);
    if (MeridianDimmCount == 0) {
        UseConfigTable = TRUE;
        WalkSmbiosViaConfigTable(CountDimmRecord, &MeridianDimmCount);
    }

    if (MeridianDimmCount == 0) {
        return;
    }

    MeridianDimms = AllocateZeroPool(MeridianDimmCount * sizeof(MERIDIAN_DIMM_INFO));
    if (MeridianDimms == NULL) {
        MeridianDimmCount = 0;
        return;
    }

    Scan.Index = 0;
    Scan.Count = MeridianDimmCount;
    if (UseConfigTable) {
        WalkSmbiosViaConfigTable(PopulateDimmRecord, &Scan);
    }
    else {
        WalkSmbiosViaProtocol(PopulateDimmRecord, &Scan);
    }

    MeridianDimmCount = Scan.Index;

    for (i = 0; i < MeridianDimmCount; i++) {
        if (!MeridianDimms[i].Soldered) {
            continue;
        }
        MeridianMemorySoldered = TRUE;
        MeridianSolderedTotalMB += MeridianDimms[i].SizeMB;
        if (MeridianSolderedMemType == 0) {
            MeridianSolderedMemType = MeridianDimms[i].MemoryType;
        }
    }
}

static VOID CopySmbiosField(IN SMBIOS_STRUCTURE *Hdr, IN SMBIOS_TABLE_STRING Index, OUT CHAR8 *Dst,
                            IN UINTN DstSize)
{
    CHAR8 *Str = GetSmbiosString(Hdr, Index);
    AsciiStrnCpyS(Dst, DstSize, Str, DstSize - 1);
}

static VOID ApplySysRecord(IN SMBIOS_STRUCTURE *Hdr, IN VOID *Context)
{
    (VOID) Context;

    if (Hdr->Type == SMBIOS_TYPE_BIOS_INFORMATION) {
        SMBIOS_TABLE_TYPE0 *T0 = (SMBIOS_TABLE_TYPE0 *)Hdr;
        CopySmbiosField(Hdr, T0->Vendor, MeridianSystem.BiosVendor,
                        sizeof(MeridianSystem.BiosVendor));
        CopySmbiosField(Hdr, T0->BiosVersion, MeridianSystem.BiosVersion,
                        sizeof(MeridianSystem.BiosVersion));
        MeridianSystem.Present = TRUE;
    }
    else if (Hdr->Type == SMBIOS_TYPE_SYSTEM_INFORMATION) {
        SMBIOS_TABLE_TYPE1 *T1 = (SMBIOS_TABLE_TYPE1 *)Hdr;
        CopySmbiosField(Hdr, T1->Manufacturer, MeridianSystem.Manufacturer,
                        sizeof(MeridianSystem.Manufacturer));
        CopySmbiosField(Hdr, T1->ProductName, MeridianSystem.ProductName,
                        sizeof(MeridianSystem.ProductName));
        MeridianSystem.Present = TRUE;
    }
}

VOID ScanSystemInfo(VOID)
{
    MeridianSystem.Present = FALSE;
    MeridianSystem.Manufacturer[0] = '\0';
    MeridianSystem.ProductName[0] = '\0';
    MeridianSystem.BiosVendor[0] = '\0';
    MeridianSystem.BiosVersion[0] = '\0';
    MRD_FREE_POOL(MeridianSystemLabel);

    WalkSmbiosViaProtocol(ApplySysRecord, NULL);
    if (MeridianSystem.Manufacturer[0] == '\0' && MeridianSystem.ProductName[0] == '\0') {
        WalkSmbiosViaConfigTable(ApplySysRecord, NULL);
    }

    MeridianSystemLabel = SystemInfoString();
}

CHAR16 *SystemInfoString(VOID)
{
    BOOLEAN HaveMfr = (MeridianSystem.Manufacturer[0] != '\0');
    BOOLEAN HaveModel = (MeridianSystem.ProductName[0] != '\0');

    if (HaveMfr && HaveModel) {
        return PoolPrint(L"%a %a", MeridianSystem.Manufacturer, MeridianSystem.ProductName);
    }
    if (HaveMfr) {
        return PoolPrint(L"%a", MeridianSystem.Manufacturer);
    }
    if (HaveModel) {
        return PoolPrint(L"%a", MeridianSystem.ProductName);
    }
    return NULL;
}

#define SBS_ADDR 0x0B
#define SBS_CMD_RELATIVE_SOC 0x0D
#define SBS_CMD_AVG_TIME_TO_EMPTY 0x12
#define SBS_CMD_AVG_TIME_TO_FULL 0x13
#define SBS_CMD_BATTERY_STATUS 0x16

#define SBS_STATUS_DISCHARGING (1 << 6)
#define SBS_STATUS_FULLY_CHARGED (1 << 5)
#define SBS_STATUS_FULLY_DISCHARGED (1 << 4)

VOID ScanBattery(VOID)
{
    EFI_STATUS Status;
    EFI_SMBUS_HC_PROTOCOL *Smbus;
    EFI_SMBUS_DEVICE_ADDRESS Dev;
    UINTN Len;
    UINT16 Soc, BatStatus, TimeToEmpty, TimeToFull;

    MeridianBattery.Present = FALSE;

    if (AppleFirmware) {
        if (MrdAppleSmcBackend() != APPLE_SMC_NONE) {
            UINT8 BatPresent = 0;
            UINT8 BatPercent = 0;
            UINT16 SmcStatus = 0;
            UINT16 TimeLeft = 0;

            MrdAppleSmcReadKey("BATP", &BatPresent, 1);
            if (BatPresent) {
                MeridianBattery.Present = TRUE;

                MrdAppleSmcReadKey("BCLM", &BatPercent, 1);
                MeridianBattery.Percent = BatPercent;

                MrdAppleSmcReadKey("B0St", (UINT8 *)&SmcStatus, 2);
                MeridianBattery.Charging = (SmcStatus & BIT2) != 0;
                MeridianBattery.FullyCharged = (SmcStatus & BIT3) != 0;
                MeridianBattery.FullyDischarged = BatPercent == 0;

                if (MeridianBattery.Charging) {
                    MrdAppleSmcReadKey("B0TF", (UINT8 *)&TimeLeft, 2);
                }
                else {
                    MrdAppleSmcReadKey("B0TE", (UINT8 *)&TimeLeft, 2);
                }
                MeridianBattery.MinutesLeft =
                    (TimeLeft == BATTERY_MINUTES_UNKNOWN) ? BATTERY_MINUTES_UNKNOWN : TimeLeft;
            }
            return;
        }
    }

    Status = gBS->LocateProtocol(&gEfiSmbusHcProtocolGuid, NULL, (VOID **)&Smbus);
    if (EFI_ERROR(Status)) {
        return;
    }

    Dev.SmbusDeviceAddress = SBS_ADDR;
    Len = sizeof(UINT16);

    Status = Smbus->Execute(Smbus, Dev, SBS_CMD_RELATIVE_SOC, EfiSmbusReadWord, FALSE, &Len, &Soc);
    if (EFI_ERROR(Status) || Soc > 100) {
        return;
    }

    MeridianBattery.Present = TRUE;
    MeridianBattery.Percent = (UINT8)Soc;

    Len = sizeof(UINT16);
    Status = Smbus->Execute(Smbus, Dev, SBS_CMD_BATTERY_STATUS, EfiSmbusReadWord, FALSE, &Len,
                            &BatStatus);
    if (!EFI_ERROR(Status)) {
        MeridianBattery.Charging = !(BatStatus & SBS_STATUS_DISCHARGING);
        MeridianBattery.FullyCharged = (BatStatus & SBS_STATUS_FULLY_CHARGED) != 0;
        MeridianBattery.FullyDischarged = (BatStatus & SBS_STATUS_FULLY_DISCHARGED) != 0;
    }

    Len = sizeof(UINT16);
    if (MeridianBattery.Charging) {
        Status = Smbus->Execute(Smbus, Dev, SBS_CMD_AVG_TIME_TO_FULL, EfiSmbusReadWord, FALSE, &Len,
                                &TimeToFull);
        MeridianBattery.MinutesLeft = EFI_ERROR(Status) ? BATTERY_MINUTES_UNKNOWN : TimeToFull;
    }
    else {
        Status = Smbus->Execute(Smbus, Dev, SBS_CMD_AVG_TIME_TO_EMPTY, EfiSmbusReadWord, FALSE,
                                &Len, &TimeToEmpty);
        MeridianBattery.MinutesLeft = EFI_ERROR(Status) ? BATTERY_MINUTES_UNKNOWN : TimeToEmpty;
    }
}

static VOID ApplyCpuRecord(IN SMBIOS_STRUCTURE *Hdr, IN VOID *Context)
{
    SMBIOS_TABLE_TYPE4 *T4;
    MERIDIAN_CPU_INFO *Cpu;
    CHAR8 *Model;

    if (Hdr->Type != SMBIOS_TYPE_PROCESSOR_INFORMATION) {
        return;
    }

    Cpu = (MERIDIAN_CPU_INFO *)Context;
    if (Cpu->Present) {
        return;
    }

    T4 = (SMBIOS_TABLE_TYPE4 *)Hdr;

    if (!(T4->Status & 0x40)) {
        return;
    }

    Model = GetSmbiosString(Hdr, T4->ProcessorVersion);
    AsciiStrnCpyS(Cpu->Model, sizeof(Cpu->Model), (*Model != '\0') ? Model : "Unknown",
                  sizeof(Cpu->Model) - 1);

    Cpu->SpeedMHz = T4->CurrentSpeed;

    if (T4->CoreCount == 0xFF && T4->Hdr.Length >= 0x30) {
        Cpu->Cores = (UINT8)T4->CoreCount2;
        Cpu->Threads = (UINT8)T4->ThreadCount2;
    }
    else {
        Cpu->Cores = T4->CoreCount;
        Cpu->Threads = T4->ThreadCount;
    }

    Cpu->Present = TRUE;
}

VOID ScanCpu(VOID)
{
    MeridianCpu.Present = FALSE;
    MeridianCpu.Model[0] = '\0';
    MeridianCpu.SpeedMHz = 0;
    MeridianCpu.Cores = 0;
    MeridianCpu.Threads = 0;

    WalkSmbiosViaProtocol(ApplyCpuRecord, &MeridianCpu);
    if (!MeridianCpu.Present) {
        WalkSmbiosViaConfigTable(ApplyCpuRecord, &MeridianCpu);
    }
}

CHAR16 *GpuVendorName(IN UINT16 VendorId)
{
    switch (VendorId) {
    case PCI_VENDOR_AMD:
        return L"AMD";
    case PCI_VENDOR_NVIDIA:
        return L"NVIDIA";
    case PCI_VENDOR_INTEL:
        return L"Intel";
    case PCI_VENDOR_MATROX:
        return L"Matrox";
    case PCI_VENDOR_ASPEED:
        return L"ASPEED";
    case PCI_VENDOR_QEMU:
        return L"QEMU";
    default:
        return L"GPU";
    }
}

VOID ScanGpu(VOID)
{
    EFI_STATUS Status;
    EFI_HANDLE *Handles;
    UINTN HandleCount;
    UINTN i;
    EFI_PCI_IO_PROTOCOL *PciIo;
    PCI_TYPE00 Pci;
    UINTN GpuIdx;

    if (MeridianGpus != NULL) {
        MRD_FREE_POOL(MeridianGpus);
        MeridianGpuCount = 0;
    }

    Status =
        gBS->LocateHandleBuffer(ByProtocol, &gEfiPciIoProtocolGuid, NULL, &HandleCount, &Handles);
    if (EFI_ERROR(Status)) {
        return;
    }

    for (i = 0; i < HandleCount; i++) {
        Status = gBS->HandleProtocol(Handles[i], &gEfiPciIoProtocolGuid, (VOID **)&PciIo);
        if (EFI_ERROR(Status)) {
            continue;
        }

        Status = PciIo->Pci.Read(PciIo, EfiPciIoWidthUint8, 0, sizeof(PCI_TYPE00), &Pci);
        if (!EFI_ERROR(Status) && IS_PCI_DISPLAY(&Pci)) {
            MeridianGpuCount++;
        }
    }

    if (MeridianGpuCount == 0) {
        gBS->FreePool(Handles);
        return;
    }

    MeridianGpus = AllocateZeroPool(MeridianGpuCount * sizeof(MERIDIAN_GPU_INFO));
    if (MeridianGpus == NULL) {
        MeridianGpuCount = 0;
        gBS->FreePool(Handles);
        return;
    }

    GpuIdx = 0;
    for (i = 0; i < HandleCount && GpuIdx < MeridianGpuCount; i++) {
        Status = gBS->HandleProtocol(Handles[i], &gEfiPciIoProtocolGuid, (VOID **)&PciIo);
        if (EFI_ERROR(Status)) {
            continue;
        }

        Status = PciIo->Pci.Read(PciIo, EfiPciIoWidthUint8, 0, sizeof(PCI_TYPE00), &Pci);
        if (EFI_ERROR(Status) || !IS_PCI_DISPLAY(&Pci)) {
            continue;
        }

        MeridianGpus[GpuIdx].Present = TRUE;
        MeridianGpus[GpuIdx].VendorId = Pci.Hdr.VendorId;
        MeridianGpus[GpuIdx].DeviceId = Pci.Hdr.DeviceId;

        UINT64 BarAttr;
        VOID *BarRes;
        Status = PciIo->GetBarAttributes(PciIo, 0, &BarAttr, &BarRes);
        if (!EFI_ERROR(Status) && BarRes != NULL) {
            EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *Desc = BarRes;
            MeridianGpus[GpuIdx].VramBytes = Desc->AddrLen;
            gBS->FreePool(BarRes);
        }

        CHAR16 *Vn = GpuVendorName(Pci.Hdr.VendorId);
        UnicodeStrToAsciiStrS(Vn, MeridianGpus[GpuIdx].VendorName,
                              sizeof(MeridianGpus[GpuIdx].VendorName));

        GpuIdx++;
    }

    MeridianGpuCount = GpuIdx;
    gBS->FreePool(Handles);
}

VOID LogDeviceInventory(VOID)
{
#if MERIDIAN_DEBUG > 0
    EFI_STATUS Status;
    EFI_HANDLE *Handles;
    UINTN HandleCount;
    UINTN i;
    UINTN Seg, Bus, Dev, Fn;
    EFI_PCI_IO_PROTOCOL *PciIo;
    EFI_USB_IO_PROTOCOL *UsbIo;
    PCI_TYPE00 Pci;
    EFI_USB_DEVICE_DESCRIPTOR Usb;
    CHAR8 Line[160];
    CHAR8 *Buf;
    UINTN Off, Cap, Len;

    Cap = 16384;
    Off = 0;
    Buf = AllocateZeroPool(Cap);

#define INV_EMIT()                                                                                 \
    do {                                                                                           \
        Print(L"%a", Line);                                                                        \
        Len = AsciiStrLen(Line);                                                                   \
        if (Buf != NULL && Off + Len + 1 < Cap) {                                                  \
            AsciiStrCpyS(Buf + Off, Cap - Off, Line);                                              \
            Off += Len;                                                                            \
        }                                                                                          \
    } while (0)

    Handles = NULL;
    HandleCount = 0;
    Status =
        gBS->LocateHandleBuffer(ByProtocol, &gEfiPciIoProtocolGuid, NULL, &HandleCount, &Handles);
    if (!EFI_ERROR(Status)) {
        AsciiSPrint(Line, sizeof(Line), "[INV] PCI devices: %d\n", HandleCount);
        INV_EMIT();
        for (i = 0; i < HandleCount; i++) {
            Status = gBS->HandleProtocol(Handles[i], &gEfiPciIoProtocolGuid, (VOID **)&PciIo);
            if (EFI_ERROR(Status))
                continue;

            Status = PciIo->Pci.Read(PciIo, EfiPciIoWidthUint8, 0, sizeof(PCI_TYPE00), &Pci);
            if (EFI_ERROR(Status))
                continue;

            Seg = Bus = Dev = Fn = 0;
            PciIo->GetLocation(PciIo, &Seg, &Bus, &Dev, &Fn);

            AsciiSPrint(Line, sizeof(Line),
                        "[INV] PCI %02x:%02x.%x  %04x:%04x  class %02x%02x%02x  rev %02x\n", Bus,
                        Dev, Fn, Pci.Hdr.VendorId, Pci.Hdr.DeviceId, Pci.Hdr.ClassCode[2],
                        Pci.Hdr.ClassCode[1], Pci.Hdr.ClassCode[0], Pci.Hdr.RevisionID);
            INV_EMIT();
        }
        MRD_FREE_POOL(Handles);
    }

    Handles = NULL;
    HandleCount = 0;
    Status =
        gBS->LocateHandleBuffer(ByProtocol, &gEfiUsbIoProtocolGuid, NULL, &HandleCount, &Handles);
    if (!EFI_ERROR(Status)) {
        AsciiSPrint(Line, sizeof(Line), "[INV] USB devices: %d\n", HandleCount);
        INV_EMIT();
        for (i = 0; i < HandleCount; i++) {
            Status = gBS->HandleProtocol(Handles[i], &gEfiUsbIoProtocolGuid, (VOID **)&UsbIo);
            if (EFI_ERROR(Status))
                continue;

            Status = UsbIo->UsbGetDeviceDescriptor(UsbIo, &Usb);
            if (EFI_ERROR(Status))
                continue;

            AsciiSPrint(Line, sizeof(Line),
                        "[INV] USB %04x:%04x  class %02x sub %02x proto %02x  rev %04x\n",
                        Usb.IdVendor, Usb.IdProduct, Usb.DeviceClass, Usb.DeviceSubClass,
                        Usb.DeviceProtocol, Usb.BcdDevice);
            INV_EMIT();
        }
        MRD_FREE_POOL(Handles);
    }

    if (Buf != NULL && Off > 0) {
        MrdSaveFile(NULL, L"meridian-inventory.txt", (UINT8 *)Buf, Off);
    }
    MRD_FREE_POOL(Buf);

    Print(L"[INV] ---- end ---- wrote \\meridian-inventory.txt (%d bytes)\n", Off);
    gBS->Stall(3000000);

#undef INV_EMIT
#endif
}

VOID ScanDisplay(VOID)
{
    EFI_STATUS Status;
    EFI_EDID_DISCOVERED_PROTOCOL *Edid;
    UINT8 *Data;
    UINTN i;

    MeridianDisplay.Present = FALSE;
    MeridianDisplay.FromEdid = FALSE;
    MeridianDisplay.EdidSize = 0;

    Edid = NULL;
    Status =
        gBS->HandleProtocol(gST->ConsoleOutHandle, &gEfiEdidActiveProtocolGuid, (VOID **)&Edid);
    if (EFI_ERROR(Status) || Edid == NULL || Edid->SizeOfEdid < 128 || Edid->Edid == NULL) {
        Edid = NULL;
        Status = gBS->LocateProtocol(&gEfiEdidDiscoveredProtocolGuid, NULL, (VOID **)&Edid);
    }
    if (EFI_ERROR(Status) || Edid == NULL || Edid->SizeOfEdid < 128 || Edid->Edid == NULL) {

        EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop = NULL;

        Status = gBS->HandleProtocol(gST->ConsoleOutHandle, &gEfiGraphicsOutputProtocolGuid,
                                     (VOID **)&Gop);
        if (EFI_ERROR(Status) || Gop == NULL || Gop->Mode == NULL || Gop->Mode->Info == NULL ||
            Gop->Mode->Info->HorizontalResolution == 0 ||
            Gop->Mode->Info->VerticalResolution == 0 ||
            Gop->Mode->Info->HorizontalResolution > MAX_UINT16 ||
            Gop->Mode->Info->VerticalResolution > MAX_UINT16) {
            return;
        }
        MeridianDisplay.HorzRes = (UINT16)Gop->Mode->Info->HorizontalResolution;
        MeridianDisplay.VertRes = (UINT16)Gop->Mode->Info->VerticalResolution;
        MeridianDisplay.Name[0] = '\0';
        MeridianDisplay.FromEdid = FALSE;
        MeridianDisplay.Present = TRUE;
        return;
    }

    Data = Edid->Edid;

    MeridianDisplay.EdidSize = (UINT16)MIN(Edid->SizeOfEdid, sizeof(MeridianDisplay.EdidRaw));
    CopyMem(MeridianDisplay.EdidRaw, Data, MeridianDisplay.EdidSize);

    MeridianDisplay.HorzRes = (UINT16)(Data[56] | ((Data[58] >> 4) << 8));
    MeridianDisplay.VertRes = (UINT16)(Data[59] | ((Data[61] >> 4) << 8));

    for (i = 54; i <= 108; i += 18) {
        if (Data[i] == 0x00 && Data[i + 1] == 0x00 && Data[i + 2] == 0x00 && Data[i + 3] == 0xFC) {
            UINT8 j;
            UINT8 k = 0;
            for (j = 5; j < 18 && k < 13; j++) {
                if (Data[i + j] == 0x0A) {
                    break;
                }
                MeridianDisplay.Name[k++] = (CHAR8)Data[i + j];
            }
            MeridianDisplay.Name[k] = '\0';
            break;
        }
    }

    MeridianDisplay.FromEdid = TRUE;
    MeridianDisplay.Present = TRUE;
}

VOID DumpDisplayEdid(VOID)
{
    EFI_STATUS Status;
#if MERIDIAN_DEBUG > 0
    CHAR8 Line[40];
    UINTN Off;
    UINTN i;
    UINTN j;
    UINTN n;
#endif

    if (MeridianDisplay.EdidSize == 0) {
        return;
    }

    Status =
        MrdSaveFile(NULL, L"meridian-edid.bin", MeridianDisplay.EdidRaw, MeridianDisplay.EdidSize);

#if MERIDIAN_DEBUG > 0
    INFO_LOG("MRD-EDID %d bytes -> \\meridian-edid.bin (%r)", (INT32)MeridianDisplay.EdidSize,
             Status);

    for (i = 0; i < MeridianDisplay.EdidSize; i += 16) {
        n = MIN((UINTN)MeridianDisplay.EdidSize - i, 16);
        Off = 0;
        for (j = 0; j < n; j++) {
            Off +=
                AsciiSPrint(Line + Off, sizeof(Line) - Off, "%02x", MeridianDisplay.EdidRaw[i + j]);
        }
        INFO_LOG("MRD-EDID %03x: %a", (INT32)i, Line);
    }
#else
    (VOID) Status;
#endif
}

VOID DumpDisplayModes(VOID)
{
    EFI_STATUS Status;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info;
    UINTN Size;
    UINT32 Mode;
    CONST CHAR8 *Src;
    CHAR8 Buf[4096];
    UINTN Off;

    Gop = NULL;
    Src = "ConOut";
    Status =
        gBS->HandleProtocol(gST->ConsoleOutHandle, &gEfiGraphicsOutputProtocolGuid, (VOID **)&Gop);
    if (EFI_ERROR(Status) || Gop == NULL) {
        Gop = NULL;
        Src = "Locate";
        Status = gBS->LocateProtocol(&gEfiGraphicsOutputProtocolGuid, NULL, (VOID **)&Gop);
    }

    Off = 0;
    if (EFI_ERROR(Status) || Gop == NULL || Gop->Mode == NULL || Gop->Mode->Info == NULL) {

        Off += AsciiSPrint(Buf + Off, sizeof(Buf) - Off, "MRD-GOP none (%r)\n", Status);
        MrdSaveFile(NULL, L"meridian-gop.txt", (UINT8 *)Buf, Off);
#if MERIDIAN_DEBUG > 0
        INFO_LOG("MRD-GOP none (%r)", Status);
#endif
        return;
    }

    Off += AsciiSPrint(
        Buf + Off, sizeof(Buf) - Off,
        "MRD-GOP src=%a cur=%d/%d %dx%d pf=%d ppsl=%d\n"
        "MRD-GOP fb base=0x%016lx size=0x%016lx\n"
        "MRD-GOP pf legend: 0=RGBX8 1=BGRX8 2=BitMask 3=BltOnly\n",
        Src, (INT32)Gop->Mode->Mode, (INT32)Gop->Mode->MaxMode,
        (INT32)Gop->Mode->Info->HorizontalResolution, (INT32)Gop->Mode->Info->VerticalResolution,
        (INT32)Gop->Mode->Info->PixelFormat, (INT32)Gop->Mode->Info->PixelsPerScanLine,
        (UINT64)Gop->Mode->FrameBufferBase, (UINT64)Gop->Mode->FrameBufferSize);

    for (Mode = 0; Mode < Gop->Mode->MaxMode; Mode++) {
        Info = NULL;
        Size = 0;
        Status = Gop->QueryMode(Gop, Mode, &Size, &Info);
        if (EFI_ERROR(Status) || Info == NULL || Size < sizeof(*Info)) {
            Off += AsciiSPrint(Buf + Off, sizeof(Buf) - Off, "  [%02d] query %r\n", (INT32)Mode,
                               Status);
            if (Info != NULL) {
                FreePool(Info);
            }
            continue;
        }
        Off += AsciiSPrint(Buf + Off, sizeof(Buf) - Off, "  [%02d]%a %dx%d pf=%d ppsl=%d\n",
                           (INT32)Mode, (Mode == Gop->Mode->Mode) ? "*" : " ",
                           (INT32)Info->HorizontalResolution, (INT32)Info->VerticalResolution,
                           (INT32)Info->PixelFormat, (INT32)Info->PixelsPerScanLine);
        FreePool(Info);

        if (Off > sizeof(Buf) - 64) {
            break;
        }
    }

    Off += AsciiSPrint(Buf + Off, sizeof(Buf) - Off, "MRD-GOP edid-native=%dx%d present=%d\n",
                       (INT32)MeridianDisplay.HorzRes, (INT32)MeridianDisplay.VertRes,
                       (INT32)MeridianDisplay.Present);

    Status = MrdSaveFile(NULL, L"meridian-gop.txt", (UINT8 *)Buf, Off);
#if MERIDIAN_DEBUG > 0
    INFO_LOG("MRD-GOP src=%a cur=%d/%d %dx%d pf=%d fb=0x%016lx -> \\meridian-gop.txt (%r)", Src,
             (INT32)Gop->Mode->Mode, (INT32)Gop->Mode->MaxMode,
             (INT32)Gop->Mode->Info->HorizontalResolution,
             (INT32)Gop->Mode->Info->VerticalResolution, (INT32)Gop->Mode->Info->PixelFormat,
             (UINT64)Gop->Mode->FrameBufferBase, Status);
#else
    (VOID) Status;
#endif
}

VOID ScanTpm(VOID)
{
    EFI_STATUS Status;
    EFI_TCG2_PROTOCOL *Tcg2;
    EFI_TCG2_BOOT_SERVICE_CAPABILITY Cap;

    MeridianTpm.Present = FALSE;

    Status = gBS->LocateProtocol(&gEfiTcg2ProtocolGuid, NULL, (VOID **)&Tcg2);
    if (EFI_ERROR(Status)) {
        return;
    }

    Cap.Size = sizeof(EFI_TCG2_BOOT_SERVICE_CAPABILITY);
    Status = Tcg2->GetCapability(Tcg2, &Cap);
    if (EFI_ERROR(Status)) {
        return;
    }

    MeridianTpm.Present = Cap.TPMPresentFlag;
    if (!MeridianTpm.Present) {
        return;
    }

    MeridianTpm.IsV2 = (Cap.HashAlgorithmBitmap & EFI_TCG2_BOOT_HASH_ALG_SHA256) != 0;
    MeridianTpm.Active = Cap.TPMPresentFlag && (Cap.HashAlgorithmBitmap != 0);
}
