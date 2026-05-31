// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#include "reset_reason.h"
#include "sysinfo.h"
#include "apple/smc.h"

#include <Library/IoLib.h>
#include <Library/PrintLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/PciIo.h>
#include <IndustryStandard/Acpi.h>
#include <Guid/Acpi.h>

#define ACPI_SIG_FACP SIGNATURE_32('F', 'A', 'C', 'P')
#define FADT_PM1A_EVT_BLK 56
#define FADT_X_PM1A_EVT 148
#define PM1_WAK_STS 0x8000

#define PMC_BUS 0
#define PMC_DEV 0x1F
#define PMC_FN 2
#define PCI_VID_INTEL 0x8086
#define PMC_PWRMBASE 0x48
#define GEN_PMCON_A 0x1020
#define GEN_PMCON_B 0x1024

#define PMC_PWRMBASE_FIXED 0xFE000000u
#define PMCON_A_GBL_RST_STS (1u << 16)
#define PMCON_B_SUS_PWR_FLR (1u << 14)
#define PMCON_B_HOST_RST_STS (1u << 9)
#define PMCON_B_RTC_BATTERY_DEAD (1u << 2)
#define PMCON_B_PWR_FLR (1u << 1)

#define PCI_VID_AMD 0x1022
#define AMD_FCH_RST_STATUS 0xFED803C0u
#define AMD_RST_THERMALTRIP (1u << 0)
#define AMD_RST_FOURSEC_PWRBTN (1u << 1)
#define AMD_RST_SHUTDOWN (1u << 2)
#define AMD_RST_THERMTRIP_TEMP (1u << 3)
#define AMD_RST_SHUTDOWN_FAN0 (1u << 5)
#define AMD_RST_INT_THERMALTRIP (1u << 9)
#define AMD_RST_USERRST (1u << 16)
#define AMD_RST_SOFTPCIRST (1u << 17)
#define AMD_RST_DOINIT (1u << 18)
#define AMD_RST_DORESET (1u << 19)
#define AMD_RST_DOFULLRESET (1u << 20)
#define AMD_RST_KBRESET (1u << 22)
#define AMD_RST_FAILBOOTRST (1u << 24)
#define AMD_RST_WATCHDOG (1u << 25)
#define AMD_RST_SYNCFLOOD (1u << 27)
#define AMD_RST_HANGRESET (1u << 28)
#define AMD_RST_ECWATCHDOG (1u << 29)
#define AMD_RST_CRASH                                                                              \
    (AMD_RST_THERMALTRIP | AMD_RST_THERMTRIP_TEMP | AMD_RST_SHUTDOWN_FAN0 |                        \
     AMD_RST_INT_THERMALTRIP | AMD_RST_FAILBOOTRST | AMD_RST_WATCHDOG | AMD_RST_SYNCFLOOD |        \
     AMD_RST_HANGRESET | AMD_RST_ECWATCHDOG)
#define AMD_RST_WARM                                                                               \
    (AMD_RST_USERRST | AMD_RST_SOFTPCIRST | AMD_RST_DOINIT | AMD_RST_DORESET |                     \
     AMD_RST_DOFULLRESET | AMD_RST_KBRESET)
#define AMD_RST_COLD (AMD_RST_FOURSEC_PWRBTN | AMD_RST_SHUTDOWN)

#define CB_HDR_SIG SIGNATURE_32('L', 'B', 'I', 'O')
#define CB_HDR_BYTES 24
#define LB_TAG_FORWARD 0x0011
#define LB_TAG_CBMEM_ENTRY 0x0031
#define CBMEM_ID_POWER_STATE 0x50535454
#define CPS_SIZE_SPT 64
#define CPS_GEN_PMCON_A 44
#define CPS_GEN_PMCON_B 48
#define CPS_PREV_SLEEP 60

MERIDIAN_BOOT_CAUSE MeridianBootCause = BOOT_CAUSE_UNKNOWN;

static BOOLEAN gDone = FALSE;
static CHAR8 gDetail[64] = "not detected";

static const CHAR8 *MssdName(INT8 Code)
{
    switch (Code) {
    case -128:
        return "power-loss";
    case -127:
        return "software-powerdown";
    case -126:
        return "vsb-low";
    case -125:
        return "thermtrip";
    case -72:
        return "nmi";
    case -71:
        return "rtc-reset";
    case -68:
        return "mbss-shutdown";
    case -60:
        return "shutdown-timeout";
    case -50:
        return "overtemp-shutdown";
    case -20:
        return "watchdog";
    case -19:
        return "battery-thermal";
    case -15:
        return "battery-low";
    case -14:
        return "other";
    case 0:
        return "unknown";
    case 1:
        return "overtemp-sleep";
    case 3:
        return "power-button";
    case 5:
        return "good-shutdown";
    default:
        return "code?";
    }
}

static MERIDIAN_BOOT_CAUSE MssdCause(INT8 Code)
{
    switch (Code) {
    case 3:
        return BOOT_CAUSE_COLD;
    case 5:
    case -127:
    case -71:
    case -68:
        return BOOT_CAUSE_WARM;
    case -20:
    case -72:
    case -125:
    case -50:
    case -60:
    case -14:
        return BOOT_CAUSE_CRASH;
    case -128:
    case -126:
    case -19:
    case -15:
        return BOOT_CAUSE_POWERFAIL;
    case 1:
        return BOOT_CAUSE_RESUME;
    default:
        return BOOT_CAUSE_UNKNOWN;
    }
}

static UINT16 ReadPm1Sts(VOID)
{
    UINTN i;
    UINT8 *Rsdp = NULL;
    UINT32 XsdtAddr32;
    UINT64 XsdtAddr = 0;
    EFI_ACPI_DESCRIPTION_HEADER *Xsdt;
    UINTN Count, e;
    UINT8 *Base;
    UINT32 Pm1a = 0;

    for (i = 0; i < gST->NumberOfTableEntries; i++) {
        if (CompareGuid(&gST->ConfigurationTable[i].VendorGuid, &gEfiAcpiTableGuid)) {
            Rsdp = (UINT8 *)gST->ConfigurationTable[i].VendorTable;
            break;
        }
    }
    if (Rsdp == NULL) {
        return 0;
    }
    // NOLINTBEGIN(performance-no-int-to-ptr)  ACPI physical addresses
    CopyMem(&XsdtAddr, Rsdp + 24, sizeof(UINT64));
    if (XsdtAddr == 0) {
        CopyMem(&XsdtAddr32, Rsdp + 16, sizeof(UINT32));
        XsdtAddr = XsdtAddr32;
    }
    if (XsdtAddr == 0) {
        return 0;
    }
    Xsdt = (EFI_ACPI_DESCRIPTION_HEADER *)(UINTN)XsdtAddr;
    Base = (UINT8 *)Xsdt + sizeof(EFI_ACPI_DESCRIPTION_HEADER);
    Count = (Xsdt->Length - sizeof(EFI_ACPI_DESCRIPTION_HEADER)) / sizeof(UINT64);
    for (e = 0; e < Count; e++) {
        UINT64 Ptr = 0;
        EFI_ACPI_DESCRIPTION_HEADER *Tbl;
        CopyMem(&Ptr, Base + e * sizeof(UINT64), sizeof(UINT64));
        if (Ptr == 0) {
            continue;
        }
        Tbl = (EFI_ACPI_DESCRIPTION_HEADER *)(UINTN)Ptr;
        if (Tbl->Signature == ACPI_SIG_FACP) {
            if (Tbl->Length >= FADT_PM1A_EVT_BLK + 4) {
                CopyMem(&Pm1a, (UINT8 *)Tbl + FADT_PM1A_EVT_BLK, sizeof(UINT32));
            }
            if (Pm1a == 0 && Tbl->Length >= FADT_X_PM1A_EVT + 12) {
                UINT64 X = 0;
                CopyMem(&X, (UINT8 *)Tbl + FADT_X_PM1A_EVT + 4, sizeof(UINT64));
                Pm1a = (UINT32)X;
            }
            break;
        }
    }
    // NOLINTEND(performance-no-int-to-ptr)
    if (Pm1a == 0) {
        return 0;
    }
    return IoRead16(Pm1a);
}

static BOOLEAN ReadGenPmcon(UINT32 *A, UINT32 *B)
{
    EFI_STATUS Status;
    EFI_HANDLE *Handles;
    UINTN Count, i, Seg, Bus, Dev, Fn;
    EFI_PCI_IO_PROTOCOL *Pci;
    EFI_PCI_IO_PROTOCOL *Pmc = NULL;
    UINT32 Id = 0, Bar0 = 0;
    UINT16 Did;
    UINTN Base;
    BOOLEAN HostIntel = FALSE;

    Status = gBS->LocateHandleBuffer(ByProtocol, &gEfiPciIoProtocolGuid, NULL, &Count, &Handles);
    if (EFI_ERROR(Status)) {
        return FALSE;
    }
    for (i = 0; i < Count; i++) {
        if (EFI_ERROR(gBS->HandleProtocol(Handles[i], &gEfiPciIoProtocolGuid, (VOID **)&Pci))) {
            continue;
        }
        Seg = Bus = Dev = Fn = 0;
        Pci->GetLocation(Pci, &Seg, &Bus, &Dev, &Fn);
        if (Bus == 0 && Dev == 0 && Fn == 0) {
            UINT32 HostId = 0;
            Pci->Pci.Read(Pci, EfiPciIoWidthUint32, 0, 1, &HostId);
            HostIntel = (HostId & 0xFFFF) == PCI_VID_INTEL;
        }
        if (Bus == PMC_BUS && Dev == PMC_DEV && Fn == PMC_FN) {
            Pmc = Pci;
        }
    }
    FreePool(Handles);

    // generations relocate/hide it -> fall through to the fixed path.
    if (Pmc != NULL) {
        Pmc->Pci.Read(Pmc, EfiPciIoWidthUint32, 0, 1, &Id);
        Did = (UINT16)(Id >> 16);
        if ((Id & 0xFFFF) == PCI_VID_INTEL &&
            ((Did >= 0x9D00 && Did <= 0x9DFF) || (Did >= 0xA100 && Did <= 0xA2FF))) {
            Pmc->Pci.Read(Pmc, EfiPciIoWidthUint32, PMC_PWRMBASE, 1, &Bar0);
            Base = (UINTN)(Bar0 & ~0xFFFu);
            if (Base != 0) {
                *A = MmioRead32(Base + GEN_PMCON_A);
                *B = MmioRead32(Base + GEN_PMCON_B);
                if (!(*A == 0xFFFFFFFF && *B == 0xFFFFFFFF)) {
                    return TRUE;
                }
            }
        }
    }

    if (HostIntel) {
        *A = MmioRead32(PMC_PWRMBASE_FIXED + GEN_PMCON_A);
        *B = MmioRead32(PMC_PWRMBASE_FIXED + GEN_PMCON_B);
        if (!(*A == 0xFFFFFFFF && *B == 0xFFFFFFFF)) {
            return TRUE;
        }
    }
    return FALSE;
}

static UINT16 HostBridgeVid(VOID)
{
    EFI_STATUS Status;
    EFI_HANDLE *Handles;
    UINTN Count, i, Seg, Bus, Dev, Fn;
    EFI_PCI_IO_PROTOCOL *Pci;
    UINT16 Vid = 0;

    Status = gBS->LocateHandleBuffer(ByProtocol, &gEfiPciIoProtocolGuid, NULL, &Count, &Handles);
    if (EFI_ERROR(Status)) {
        return 0;
    }
    for (i = 0; i < Count; i++) {
        if (EFI_ERROR(gBS->HandleProtocol(Handles[i], &gEfiPciIoProtocolGuid, (VOID **)&Pci))) {
            continue;
        }
        Seg = Bus = Dev = Fn = 0;
        Pci->GetLocation(Pci, &Seg, &Bus, &Dev, &Fn);
        if (Bus == 0 && Dev == 0 && Fn == 0) {
            UINT32 Id = 0;
            Pci->Pci.Read(Pci, EfiPciIoWidthUint32, 0, 1, &Id);
            Vid = (UINT16)(Id & 0xFFFF);
            break;
        }
    }
    FreePool(Handles);
    return Vid;
}

static BOOLEAN ReadAmdResetStatus(UINT32 *Status)
{
    UINT32 Val;
    if (HostBridgeVid() != PCI_VID_AMD) {
        return FALSE;
    }
    Val = MmioRead32(AMD_FCH_RST_STATUS);
    if (Val == 0xFFFFFFFF) {
        return FALSE;
    }
    *Status = Val;
    return TRUE;
}

static MERIDIAN_BOOT_CAUSE AmdRstCause(UINT32 S)
{
    if (S & AMD_RST_CRASH) {
        return BOOT_CAUSE_CRASH;
    }
    if (S & AMD_RST_WARM) {
        return BOOT_CAUSE_WARM;
    }
    if (S & AMD_RST_COLD) {
        return BOOT_CAUSE_COLD;
    }
    return BOOT_CAUSE_UNKNOWN;
}

static const CHAR8 *AmdRstName(UINT32 S)
{
    if (S & (AMD_RST_THERMALTRIP | AMD_RST_THERMTRIP_TEMP | AMD_RST_INT_THERMALTRIP)) {
        return "thermal-trip";
    }
    if (S & (AMD_RST_WATCHDOG | AMD_RST_ECWATCHDOG)) {
        return "watchdog";
    }
    if (S & AMD_RST_SYNCFLOOD) {
        return "sync-flood";
    }
    if (S & AMD_RST_HANGRESET) {
        return "hang-reset";
    }
    if (S & AMD_RST_FAILBOOTRST) {
        return "failboot-reset";
    }
    if (S & AMD_RST_SHUTDOWN_FAN0) {
        return "fan0-shutdown";
    }
    if (S & (AMD_RST_DORESET | AMD_RST_DOFULLRESET | AMD_RST_DOINIT)) {
        return "reset";
    }
    if (S & AMD_RST_KBRESET) {
        return "kb-reset(cf9)";
    }
    if (S & AMD_RST_SOFTPCIRST) {
        return "soft-pci-reset";
    }
    if (S & AMD_RST_USERRST) {
        return "user-reset";
    }
    if (S & AMD_RST_FOURSEC_PWRBTN) {
        return "4s-powerbtn-off";
    }
    if (S & AMD_RST_SHUTDOWN) {
        return "prev-shutdown";
    }
    return "no-cause-bit";
}

static MERIDIAN_BOOT_CAUSE IntelPmconCause(UINT32 A, UINT32 B)
{
    if (B & (PMCON_B_PWR_FLR | PMCON_B_SUS_PWR_FLR | PMCON_B_RTC_BATTERY_DEAD)) {
        return (B & (PMCON_B_SUS_PWR_FLR | PMCON_B_RTC_BATTERY_DEAD)) ? BOOT_CAUSE_POWERFAIL
                                                                      : BOOT_CAUSE_COLD;
    }
    if ((A & PMCON_A_GBL_RST_STS) || (B & PMCON_B_HOST_RST_STS)) {
        return BOOT_CAUSE_WARM;
    }
    return BOOT_CAUSE_UNKNOWN;
}

static UINT32 PhysRead32(UINTN Addr)
{
    UINT32 V = 0;
    // NOLINTNEXTLINE(performance-no-int-to-ptr)  fixed physical address
    CopyMem(&V, (VOID *)Addr, sizeof(UINT32));
    return V;
}

static UINTN CbFindHeader(VOID)
{
    CONST UINTN Ranges[][2] = {{0x00000000, 0x1000}, {0x000F0000, 0x10000}};
    UINTN r, o;
    for (r = 0; r < 2; r++) {
        for (o = 0; o + CB_HDR_BYTES <= Ranges[r][1]; o += 16) {
            UINTN Phys = Ranges[r][0] + o;
            if (PhysRead32(Phys) == CB_HDR_SIG && PhysRead32(Phys + 4) == CB_HDR_BYTES) {
                return Phys;
            }
        }
    }
    return 0;
}

static BOOLEAN ReadCorebootPowerState(UINT32 *A, UINT32 *B, UINT32 *Prev)
{
    UINTN Hdr = CbFindHeader();
    UINTN Hops = 0;

    while (Hdr != 0 && Hops < 4) {
        UINTN Rec = Hdr + PhysRead32(Hdr + 4);
        UINTN End = Rec + PhysRead32(Hdr + 12);
        BOOLEAN Forwarded = FALSE;
        while (Rec + 8 <= End) {
            UINT32 Tag = PhysRead32(Rec);
            UINT32 Size = PhysRead32(Rec + 4);
            if (Size < 8) {
                return FALSE;
            }
            if (Tag == LB_TAG_FORWARD) {
                UINT64 Fwd = 0;
                // NOLINTNEXTLINE(performance-no-int-to-ptr)  forwarded coreboot table phys addr
                CopyMem(&Fwd, (VOID *)(Rec + 8), sizeof(UINT64));
                Hdr = (UINTN)Fwd;
                Forwarded = TRUE;
                Hops++;
                break;
            }

            if (Tag == LB_TAG_CBMEM_ENTRY && PhysRead32(Rec + 20) == CBMEM_ID_POWER_STATE &&
                PhysRead32(Rec + 16) == CPS_SIZE_SPT) {
                UINT64 Addr = 0;
                // NOLINTNEXTLINE(performance-no-int-to-ptr)  CBMEM entry address field
                CopyMem(&Addr, (VOID *)(Rec + 8), sizeof(UINT64));
                *A = PhysRead32((UINTN)Addr + CPS_GEN_PMCON_A);
                *B = PhysRead32((UINTN)Addr + CPS_GEN_PMCON_B);
                *Prev = PhysRead32((UINTN)Addr + CPS_PREV_SLEEP);
                return TRUE;
            }
            Rec += Size;
        }
        if (!Forwarded) {
            return FALSE;
        }
    }
    return FALSE;
}

MERIDIAN_BOOT_CAUSE MrdDetectBootCause(VOID)
{
    MERIDIAN_BOOT_CAUSE Cause = BOOT_CAUSE_UNKNOWN;
    UINT16 Pm1;

    if (gDone) {
        return MeridianBootCause;
    }
    gDone = TRUE;

    if (MeridianFirmwareVendor == FW_VENDOR_APPLE) {

        APPLE_SMC_BACKEND Be = MrdAppleSmcBackend();
        CONST CHAR8 *BeName = Be == APPLE_SMC_PROTOCOL ? "proto"
                              : Be == APPLE_SMC_MMIO   ? "mmio"
                              : Be == APPLE_SMC_PORT   ? "port"
                                                       : "none";
        UINT8 Raw = 0;
        if (Be != APPLE_SMC_NONE && MrdAppleSmcReadKey("MSSD", &Raw, 1)) {
            INT8 Code = (INT8)Raw;
            Cause = MssdCause(Code);
            AsciiSPrint(gDetail, sizeof(gDetail), "SMC[%a] MSSD=%d %a", BeName, (INT32)Code,
                        MssdName(Code));
        }
        else {

            UINT8 Probe[4] = {0};
            BOOLEAN Alive = Be != APPLE_SMC_NONE && MrdAppleSmcReadKey("#KEY", Probe, 4);
            AsciiSPrint(gDetail, sizeof(gDetail), "SMC[%a] MSSD fail (smc %a)", BeName,
                        Alive ? "alive" : "dead");
        }
    }
    else {

        UINT32 A = 0, B = 0, Prev = 0, Rst = 0;
        if (ReadCorebootPowerState(&A, &B, &Prev)) {
            Cause = IntelPmconCause(A, B);
            if (Prev == 3) {
                Cause = BOOT_CAUSE_RESUME;
            }
            else if (Cause == BOOT_CAUSE_UNKNOWN && Prev == 5) {
                Cause = BOOT_CAUSE_COLD;
            }
            AsciiSPrint(gDetail, sizeof(gDetail), "cb-pstate A=%08x B=%08x s%d", A, B, Prev);
        }
        else if (ReadGenPmcon(&A, &B)) {
            Cause = IntelPmconCause(A, B);
            AsciiSPrint(gDetail, sizeof(gDetail), "GEN_PMCON A=%08x B=%08x", A, B);
        }
        else if (ReadAmdResetStatus(&Rst)) {
            Cause = AmdRstCause(Rst);
            AsciiSPrint(gDetail, sizeof(gDetail), "FCH RST=%08x %a", Rst, AmdRstName(Rst));
        }
        else {
            AsciiStrCpyS(gDetail, sizeof(gDetail), "no reset-status source");
        }
    }

    if (Cause == BOOT_CAUSE_UNKNOWN) {
        Pm1 = ReadPm1Sts();
        if (Pm1 & PM1_WAK_STS) {
            Cause = BOOT_CAUSE_RESUME;
        }
    }

    MeridianBootCause = Cause;
    return Cause;
}

const CHAR8 *MrdBootCauseStr(MERIDIAN_BOOT_CAUSE Cause)
{
    switch (Cause) {
    case BOOT_CAUSE_COLD:
        return "cold power-on";
    case BOOT_CAUSE_WARM:
        return "reset / reboot";
    case BOOT_CAUSE_CRASH:
        return "recovered from crash";
    case BOOT_CAUSE_RESUME:
        return "resume from sleep";
    case BOOT_CAUSE_POWERFAIL:
        return "power restored";
    default:
        return "unknown";
    }
}

const CHAR8 *MrdBootCauseDetail(VOID) { return gDetail; }
