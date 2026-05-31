// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#include "netinfo.h"
#include "kernel/log.h"
#include "kernel/console_capture.h"

#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DevicePathLib.h>
#include <Protocol/DevicePath.h>
#include <Protocol/Ip4Config2.h>
#include <Protocol/SimpleNetwork.h>
#include <Protocol/PxeBaseCode.h>
#include <Protocol/ManagedNetwork.h>
#include <Protocol/Ip4.h>
#include <Protocol/Dhcp4.h>
#include <Protocol/ServiceBinding.h>

UINTN MeridianNicCount = 0;
MERIDIAN_NIC_INFO *MeridianNics = NULL;
BOOLEAN MeridianNetProbed = FALSE;
BOOLEAN MeridianNetDhcpTried = FALSE;

#define NET_MAX_NICS 8

#define DiagEmit(...) INFO_LOG("[NET] " __VA_ARGS__)
#define DiagFlush()

static UINTN CountHandles(EFI_GUID *Guid)
{
    EFI_HANDLE *H = NULL;
    UINTN N = 0;
    if (EFI_ERROR(gBS->LocateHandleBuffer(ByProtocol, Guid, NULL, &N, &H)) || H == NULL) {
        return 0;
    }
    FreePool(H);
    return N;
}

STATIC BOOLEAN Ipv4IsZero(CONST UINT8 *Ip) { return (Ip[0] | Ip[1] | Ip[2] | Ip[3]) == 0; }

STATIC BOOLEAN MacIsValid(CONST UINT8 *Mac, UINTN MacLen)
{
    return Mac != NULL && MacLen >= 6 && (Mac[0] | Mac[1] | Mac[2] | Mac[3] | Mac[4] | Mac[5]) != 0;
}

STATIC VOID CopyMacIfValid(MERIDIAN_NIC_INFO *Nic, CONST UINT8 *Mac, UINTN MacLen)
{
    if (Nic == NULL || !MacIsValid(Mac, MacLen)) {
        return;
    }
    Nic->MacLen = 6;
    CopyMem(Nic->Mac, Mac, 6);
    Nic->HasMac = TRUE;
}

STATIC MERIDIAN_NIC_INFO *FindNicByMac(CONST UINT8 *Mac, UINTN MacLen)
{
    UINTN i;

    if (MeridianNics == NULL || !MacIsValid(Mac, MacLen)) {
        return NULL;
    }
    for (i = 0; i < MeridianNicCount; i++) {
        if (MeridianNics[i].HasMac && CompareMem(MeridianNics[i].Mac, Mac, 6) == 0) {
            return &MeridianNics[i];
        }
    }
    return NULL;
}

STATIC MERIDIAN_NIC_INFO *AppendNic(VOID)
{
    MERIDIAN_NIC_INFO *Nic;

    if (MeridianNics == NULL || MeridianNicCount >= NET_MAX_NICS) {
        return NULL;
    }
    Nic = &MeridianNics[MeridianNicCount++];
    ZeroMem(Nic, sizeof(*Nic));
    Nic->Present = TRUE;
    Nic->Source = NET_IP_NONE;
    return Nic;
}

STATIC MERIDIAN_NIC_INFO *FindOrAppendNicByMac(CONST UINT8 *Mac, UINTN MacLen, UINTN IndexHint)
{
    MERIDIAN_NIC_INFO *Nic;

    Nic = FindNicByMac(Mac, MacLen);
    if (Nic != NULL) {
        return Nic;
    }
    if (MeridianNics == NULL) {
        return NULL;
    }
    if (IndexHint < MeridianNicCount && !MeridianNics[IndexHint].HasMac) {
        Nic = &MeridianNics[IndexHint];
        Nic->Present = TRUE;
        CopyMacIfValid(Nic, Mac, MacLen);
        return Nic;
    }
    Nic = AppendNic();
    CopyMacIfValid(Nic, Mac, MacLen);
    return Nic;
}

STATIC VOID CopyIfaceName(CHAR8 *Dst, UINTN Cap, CONST CHAR16 *Src)
{
    UINTN i = 0;

    if (Dst == NULL || Cap == 0) {
        return;
    }
    if (Src != NULL) {
        for (; Src[i] != L'\0' && i + 1 < Cap; i++) {
            CHAR16 c = Src[i];
            Dst[i] = (c >= 0x20 && c < 0x7F) ? (CHAR8)c : '?';
        }
    }
    Dst[i] = '\0';
}

#if MERIDIAN_DEBUG > 0

STATIC CONST CHAR8 *LinkStateText(CONST MERIDIAN_NIC_INFO *Nic)
{
    if (Nic == NULL || !Nic->LinkKnown) {
        return "unknown";
    }
    return Nic->LinkUp ? "up" : "down";
}
#endif

STATIC BOOLEAN AnyLinkUsable(VOID)
{
    UINTN i;
    if (MeridianNics == NULL) {
        return FALSE;
    }
    for (i = 0; i < MeridianNicCount; i++) {
        if (MeridianNics[i].LinkUp || !MeridianNics[i].LinkKnown) {
            return TRUE;
        }
    }
    return FALSE;
}

STATIC BOOLEAN AnyHasIp(VOID)
{
    UINTN i;
    if (MeridianNics == NULL) {
        return FALSE;
    }
    for (i = 0; i < MeridianNicCount; i++) {
        if (MeridianNics[i].HasIp) {
            return TRUE;
        }
    }
    return FALSE;
}

static UINTN ConnectAllControllers(VOID)
{
    EFI_HANDLE *Handles = NULL;
    UINTN Count = 0, i, Ok = 0;

    if (EFI_ERROR(gBS->LocateHandleBuffer(AllHandles, NULL, NULL, &Count, &Handles)) ||
        Handles == NULL) {
        return 0;
    }
    MrdConsoleCaptureBegin();
    for (i = 0; i < Count; i++) {
        if (!EFI_ERROR(gBS->ConnectController(Handles[i], NULL, NULL, TRUE))) {
            Ok++;
        }
    }
    MrdConsoleCaptureEnd();
    FreePool(Handles);
    return Ok;
}

static EFI_HANDLE *NetLocateHandles(EFI_GUID *Protocol, CONST CHAR8 *Label, UINTN *Count)
{
    EFI_HANDLE *Handles = NULL;

    *Count = 0;
    if (EFI_ERROR(gBS->LocateHandleBuffer(ByProtocol, Protocol, NULL, Count, &Handles)) ||
        Handles == NULL) {
        DiagEmit("%a handles: 0\n", Label);
        return NULL;
    }
    DiagEmit("%a handles: %d\n", Label, (UINT32)*Count);
    return Handles;
}

static VOID ScanSnp(VOID)
{
    EFI_HANDLE *Handles;
    UINTN Count, i;

    Handles = NetLocateHandles(&gEfiSimpleNetworkProtocolGuid, "SNP", &Count);
    if (Handles == NULL) {
        return;
    }

    DiagEmit("SNP scan: %d handle(s)\n", (UINT32)Count);

    for (i = 0; i < Count && MeridianNicCount < NET_MAX_NICS; i++) {
        EFI_SIMPLE_NETWORK_PROTOCOL *Snp = NULL;
        EFI_DEVICE_PATH_PROTOCOL *Dp = NULL;
        CHAR16 *DpStr = NULL;
        MERIDIAN_NIC_INFO *Nic;
        CONST UINT8 *Mac;
        UINTN MacLen;
        BOOLEAN LinkKnown, LinkUp;

        if (EFI_ERROR(
                gBS->HandleProtocol(Handles[i], &gEfiSimpleNetworkProtocolGuid, (VOID **)&Snp)) ||
            Snp == NULL || Snp->Mode == NULL) {
            DiagEmit("  SNP[%d] no usable protocol/mode - skipped\n", (UINT32)i);
            continue;
        }

        Mac = Snp->Mode->CurrentAddress.Addr;
        MacLen = Snp->Mode->HwAddressSize;
        LinkKnown = Snp->Mode->MediaPresentSupported;
        LinkUp = !LinkKnown || Snp->Mode->MediaPresent;

        if (!EFI_ERROR(
                gBS->HandleProtocol(Handles[i], &gEfiDevicePathProtocolGuid, (VOID **)&Dp)) &&
            Dp != NULL) {
            DpStr = ConvertDevicePathToText(Dp, FALSE, FALSE);
        }

        DiagEmit("  SNP[%d] state=%d mps=%d mp=%d iftype=%d maclen=%d "
                 "mac=%02X:%02X:%02X:%02X:%02X:%02X dp=%s\n",
                 (UINT32)i, Snp->Mode->State, Snp->Mode->MediaPresentSupported,
                 Snp->Mode->MediaPresent, Snp->Mode->IfType, (UINT32)MacLen, Mac[0], Mac[1], Mac[2],
                 Mac[3], Mac[4], Mac[5], DpStr != NULL ? DpStr : L"(none)");

        Nic = MacIsValid(Mac, MacLen) ? FindNicByMac(Mac, MacLen) : NULL;
        if (Nic != NULL) {

            if (LinkKnown) {
                if (!Nic->LinkKnown) {
                    Nic->LinkKnown = TRUE;
                    Nic->LinkUp = LinkUp;
                }
                else {
                    Nic->LinkUp = Nic->LinkUp || LinkUp;
                }
            }
            DiagEmit("    -> duplicate MAC, collapsed into NIC%d\n", (UINT32)(Nic - MeridianNics));
            if (DpStr != NULL) {
                FreePool(DpStr);
            }
            continue;
        }

        if (!MacIsValid(Mac, MacLen)) {
            DiagEmit("    -> MAC is zero/invalid; appending undeduped\n");
        }

        Nic = AppendNic();
        if (Nic == NULL) {
            DiagEmit("    -> NIC table full (max %d) - dropped\n", (UINT32)NET_MAX_NICS);
            if (DpStr != NULL) {
                FreePool(DpStr);
            }
            continue;
        }
        Nic->Present = TRUE;
        Nic->Source = NET_IP_NONE;
        Nic->LinkKnown = LinkKnown;
        Nic->LinkUp = LinkUp;
        CopyMacIfValid(Nic, Mac, MacLen);
        DiagEmit("    -> NEW NIC%d link=%a\n", (UINT32)(MeridianNicCount - 1), LinkStateText(Nic));
        if (DpStr != NULL) {
            FreePool(DpStr);
        }
    }
    DiagEmit("SNP scan done: %d distinct NIC(s)\n", (UINT32)MeridianNicCount);
    FreePool(Handles);
}

static VOID ScanIp4Config2(VOID)
{
    EFI_HANDLE *Handles;
    UINTN Count, i;

    Handles = NetLocateHandles(&gEfiIp4Config2ProtocolGuid, "Ip4Config2", &Count);
    if (Handles == NULL) {
        return;
    }

    for (i = 0; i < Count; i++) {
        EFI_IP4_CONFIG2_PROTOCOL *Cfg = NULL;
        EFI_IP4_CONFIG2_INTERFACE_INFO *Info;
        UINTN Size = 0;
        EFI_IP4_CONFIG2_POLICY Policy;
        UINTN PolicySize;
        BOOLEAN PolicyDhcp = FALSE;
        MERIDIAN_NIC_INFO *Nic;

        if (EFI_ERROR(
                gBS->HandleProtocol(Handles[i], &gEfiIp4Config2ProtocolGuid, (VOID **)&Cfg)) ||
            Cfg == NULL) {
            continue;
        }
        if (Cfg->GetData(Cfg, Ip4Config2DataTypeInterfaceInfo, &Size, NULL) !=
                EFI_BUFFER_TOO_SMALL ||
            Size == 0) {
            continue;
        }
        Info = AllocateZeroPool(Size);
        if (Info == NULL) {
            continue;
        }
        if (!EFI_ERROR(Cfg->GetData(Cfg, Ip4Config2DataTypeInterfaceInfo, &Size, Info)) &&
            !Ipv4IsZero(Info->StationAddress.Addr)) {
            PolicySize = sizeof(Policy);
            if (!EFI_ERROR(Cfg->GetData(Cfg, Ip4Config2DataTypePolicy, &PolicySize, &Policy))) {
                PolicyDhcp = (Policy == Ip4Config2PolicyDhcp);
            }

            Nic = FindOrAppendNicByMac(Info->HwAddress.Addr, Info->HwAddressSize, NET_MAX_NICS);
            if (Nic != NULL) {
                CopyMem(Nic->Ip, Info->StationAddress.Addr, 4);
                CopyMem(Nic->Mask, Info->SubnetMask.Addr, 4);
                Nic->HasIp = TRUE;
                Nic->Source = PolicyDhcp ? NET_IP_DHCP : NET_IP_STATIC;
                Nic->LinkUp = TRUE;
                CopyIfaceName(Nic->Name, sizeof(Nic->Name), Info->Name);
                DiagEmit("  Ip4Config2 IP=%d.%d.%d.%d\n", Nic->Ip[0], Nic->Ip[1], Nic->Ip[2],
                         Nic->Ip[3]);
            }
        }

        FreePool(Info);
    }
    FreePool(Handles);
}

static VOID ScanPxeDhcp(VOID)
{
    EFI_HANDLE *Handles;
    UINTN Count, i;

    MeridianNetDhcpTried = TRUE;

    Handles = NetLocateHandles(&gEfiPxeBaseCodeProtocolGuid, "PXE-BC", &Count);
    if (Handles == NULL) {
        return;
    }

    for (i = 0; i < Count && i < NET_MAX_NICS; i++) {
        EFI_PXE_BASE_CODE_PROTOCOL *Pxe = NULL;
        EFI_STATUS Status;
        MERIDIAN_NIC_INFO *Nic;
        BOOLEAN WeStarted = FALSE;

        if (EFI_ERROR(
                gBS->HandleProtocol(Handles[i], &gEfiPxeBaseCodeProtocolGuid, (VOID **)&Pxe)) ||
            Pxe == NULL) {
            continue;
        }

        Status = Pxe->Start(Pxe, FALSE);
        if (Status == EFI_SUCCESS) {
            WeStarted = TRUE;
        }
        else if (Status != EFI_ALREADY_STARTED) {
            DiagEmit("  PXE%d Start fail %r\n", (UINT32)i, Status);
            continue;
        }
        Status = Pxe->Dhcp(Pxe, TRUE);
        if (EFI_ERROR(Status) || Pxe->Mode == NULL) {
            DiagEmit("  PXE%d Dhcp fail %r\n", (UINT32)i, Status);
            if (WeStarted) {
                Pxe->Stop(Pxe);
            }
            continue;
        }

        if (!Ipv4IsZero(Pxe->Mode->StationIp.v4.Addr)) {
            Nic = FindOrAppendNicByMac(Pxe->Mode->DhcpAck.Dhcpv4.BootpHwAddr,
                                       Pxe->Mode->DhcpAck.Dhcpv4.BootpHwAddrLen, i);
            if (Nic != NULL) {
                Nic->Present = TRUE;
                CopyMem(Nic->Ip, Pxe->Mode->StationIp.v4.Addr, 4);
                CopyMem(Nic->Mask, Pxe->Mode->SubnetMask.v4.Addr, 4);
                Nic->HasIp = TRUE;
                Nic->Source = NET_IP_DHCP;
                Nic->LinkKnown = TRUE;
                Nic->LinkUp = TRUE;
                DiagEmit("  PXE%d DHCP IP=%d.%d.%d.%d\n", (UINT32)i, Nic->Ip[0], Nic->Ip[1],
                         Nic->Ip[2], Nic->Ip[3]);
            }
        }
        else {
            DiagEmit("  PXE%d DHCP ack but StationIp=0\n", (UINT32)i);
        }
        if (WeStarted) {
            Pxe->Stop(Pxe);
        }
    }
    FreePool(Handles);
}

static VOID ScanDhcp4(MERIDIAN_NET_PUMP Pump, VOID *Ctx)
{
    EFI_HANDLE *Handles;
    UINTN Count, i;

    MeridianNetDhcpTried = TRUE;

    Handles = NetLocateHandles(&gEfiDhcp4ServiceBindingProtocolGuid, "DHCP4-SB", &Count);
    if (Handles == NULL) {
        return;
    }

    for (i = 0; i < Count && i < NET_MAX_NICS; i++) {
        EFI_SERVICE_BINDING_PROTOCOL *Sb = NULL;
        EFI_DHCP4_PROTOCOL *Dhcp4 = NULL;
        EFI_HANDLE Child = NULL;
        EFI_DHCP4_CONFIG_DATA Config;
        EFI_DHCP4_MODE_DATA Mode;
        EFI_STATUS Status;
        BOOLEAN DhcpStarted = FALSE;
        MERIDIAN_NIC_INFO *Nic;

        if (EFI_ERROR(gBS->HandleProtocol(Handles[i], &gEfiDhcp4ServiceBindingProtocolGuid,
                                          (VOID **)&Sb)) ||
            Sb == NULL) {
            continue;
        }
        if (EFI_ERROR(Sb->CreateChild(Sb, &Child)) || Child == NULL) {
            DiagEmit("  DHCP4[%d] CreateChild fail\n", (UINT32)i);
            continue;
        }
        if (EFI_ERROR(gBS->HandleProtocol(Child, &gEfiDhcp4ProtocolGuid, (VOID **)&Dhcp4)) ||
            Dhcp4 == NULL) {
            Sb->DestroyChild(Sb, Child);
            continue;
        }

        static UINT32 Disco[2] = {2, 2};
        static UINT32 Reqto[2] = {2, 2};
        ZeroMem(&Config, sizeof(Config));
        Config.DiscoverTryCount = 2;
        Config.DiscoverTimeout = Disco;
        Config.RequestTryCount = 2;
        Config.RequestTimeout = Reqto;
        Status = Dhcp4->Configure(Dhcp4, &Config);
        if (EFI_ERROR(Status)) {
            DiagEmit("  DHCP4[%d] Configure %r\n", (UINT32)i, Status);
            Sb->DestroyChild(Sb, Child);
            continue;
        }

        if (Pump != NULL) {

            EFI_EVENT Done = NULL;
            EFI_EVENT Tick = NULL;
            UINTN Spins = 0;
            BOOLEAN Aborted = FALSE;
            BOOLEAN Completed = FALSE;
            BOOLEAN Started = FALSE;
            Status = gBS->CreateEvent(0, 0, NULL, NULL, &Done);
            if (EFI_ERROR(Status) || Done == NULL) {
                DiagEmit("  DHCP4[%d] CreateEvent %r\n", (UINT32)i, Status);
                Status = EFI_OUT_OF_RESOURCES;
            }
            else {
                EFI_STATUS TimerStatus;

                TimerStatus = gBS->CreateEvent(EVT_TIMER, 0, NULL, NULL, &Tick);
                if (!EFI_ERROR(TimerStatus) && Tick != NULL) {
                    TimerStatus = gBS->SetTimer(Tick, TimerPeriodic, 800000ULL);
                }
                if (EFI_ERROR(TimerStatus)) {
                    DiagEmit("  DHCP4[%d] timer fallback %r\n", (UINT32)i, TimerStatus);
                    if (Tick != NULL) {
                        gBS->CloseEvent(Tick);
                        Tick = NULL;
                    }
                }

                Status = Dhcp4->Start(Dhcp4, Done);
                if (Status == EFI_SUCCESS || Status == EFI_NOT_READY) {
                    Started = TRUE;
                    DhcpStarted = TRUE;
                    for (Spins = 0; Spins < 250; Spins++) {
                        if (Tick != NULL) {
                            EFI_EVENT Events[2];
                            UINTN EventIndex;
                            EFI_STATUS WaitStatus;

                            Events[0] = Done;
                            Events[1] = Tick;
                            WaitStatus = gBS->WaitForEvent(2, Events, &EventIndex);
                            if (!EFI_ERROR(WaitStatus) && EventIndex == 0) {
                                Completed = TRUE;
                                break;
                            }
                            if (EFI_ERROR(WaitStatus)) {
                                if (!EFI_ERROR(gBS->CheckEvent(Done))) {
                                    Completed = TRUE;
                                    break;
                                }
                                gBS->Stall(80000);
                            }
                        }
                        else {
                            if (!EFI_ERROR(gBS->CheckEvent(Done))) {
                                Completed = TRUE;
                                break;
                            }
                            gBS->Stall(80000);
                        }
                        if (!Pump(Ctx)) {
                            Aborted = TRUE;
                            break;
                        }
                    }
                    if (!Completed && !Aborted && !EFI_ERROR(gBS->CheckEvent(Done))) {
                        Completed = TRUE;
                    }
                    if (!Completed) {

                        if (Started) {
                            Dhcp4->Stop(Dhcp4);
                        }
                        Status = Aborted ? EFI_ABORTED : EFI_TIMEOUT;
                    }
                    else {
                        ZeroMem(&Mode, sizeof(Mode));
                        if (!EFI_ERROR(Dhcp4->GetModeData(Dhcp4, &Mode)) &&
                            Mode.State == Dhcp4Bound && !Ipv4IsZero(Mode.ClientAddress.Addr)) {
                            Status = EFI_SUCCESS;
                        }
                        else {
                            Status = EFI_TIMEOUT;
                        }
                    }
                }
            }
            if (Tick != NULL) {
                gBS->SetTimer(Tick, TimerCancel, 0);
                gBS->CloseEvent(Tick);
            }
            if (Done != NULL) {
                gBS->CloseEvent(Done);
            }
        }
        else {
            Status = Dhcp4->Start(Dhcp4, NULL);
            if (!EFI_ERROR(Status)) {
                DhcpStarted = TRUE;
            }
        }
        DiagEmit("  DHCP4[%d] Start %r\n", (UINT32)i, Status);
        if (EFI_ERROR(Status)) {
            Dhcp4->Configure(Dhcp4, NULL);
            Sb->DestroyChild(Sb, Child);
            if (Status == EFI_ABORTED) {
                break;
            }
            continue;
        }

        ZeroMem(&Mode, sizeof(Mode));
        if (!EFI_ERROR(Dhcp4->GetModeData(Dhcp4, &Mode))) {
            DiagEmit("  DHCP4[%d] state=%d client=%d.%d.%d.%d\n", (UINT32)i, Mode.State,
                     Mode.ClientAddress.Addr[0], Mode.ClientAddress.Addr[1],
                     Mode.ClientAddress.Addr[2], Mode.ClientAddress.Addr[3]);
            if (Mode.State == Dhcp4Bound && !Ipv4IsZero(Mode.ClientAddress.Addr)) {

                Nic = FindOrAppendNicByMac(Mode.ClientMacAddress.Addr, 6, i);
                if (Nic != NULL) {
                    CopyMem(Nic->Ip, Mode.ClientAddress.Addr, 4);
                    CopyMem(Nic->Mask, Mode.SubnetMask.Addr, 4);
                    Nic->HasIp = TRUE;
                    Nic->Source = NET_IP_DHCP;
                    Nic->LinkKnown = TRUE;
                    Nic->LinkUp = TRUE;
                }
            }
        }

        if (DhcpStarted) {
            Dhcp4->Stop(Dhcp4);
        }
        Dhcp4->Configure(Dhcp4, NULL);
        Sb->DestroyChild(Sb, Child);
    }
    FreePool(Handles);
}

VOID ScanNetwork(BOOLEAN Connect, BOOLEAN AttemptDhcp, MERIDIAN_NET_PUMP Pump, VOID *Ctx)
{
#if MERIDIAN_DEBUG > 0
    EFI_GUID MnpSb = {0xF36FF770, 0xA7E1, 0x42CF, {0x9E, 0xD2, 0x56, 0xF0, 0xF2, 0x71, 0xF4, 0x4C}};
    EFI_GUID Ip4Sb = {0xC51711E7, 0xB4BF, 0x404A, {0xBF, 0xB8, 0x0A, 0x04, 0x8E, 0xF1, 0xFF, 0xE4}};
    EFI_GUID Dhcp4Sb = {
        0x9D9A39D8, 0xBD42, 0x4A73, {0xA4, 0xD5, 0x8E, 0xE9, 0x4B, 0xE1, 0x13, 0x80}};
#endif

    if (MeridianNics != NULL) {
        FreePool(MeridianNics);
        MeridianNics = NULL;
    }
    MeridianNetProbed = TRUE;
    MeridianNetDhcpTried = FALSE;
    MeridianNicCount = 0;

    DiagEmit("ScanNetwork connect=%a dhcp=%a\n", Connect ? "yes" : "no",
             AttemptDhcp ? "yes" : "no");

    if (Connect) {
#if MERIDIAN_DEBUG > 0
        UINTN PreSnp = CountHandles(&gEfiSimpleNetworkProtocolGuid);
        UINTN Connected;
        DiagEmit("phase: connect-all begin (pre-snp=%d)\n", (UINT32)PreSnp);
        DiagFlush();
        Connected = ConnectAllControllers();
        DiagEmit("phase: connect-all done ok=%d snp->%d\n", (UINT32)Connected,
                 (UINT32)CountHandles(&gEfiSimpleNetworkProtocolGuid));
        DiagFlush();
#else
        (VOID) ConnectAllControllers();
#endif
    }

#if MERIDIAN_DEBUG > 0

    DiagEmit("census: snp=%d ip4cfg2=%d pxebc=%d mnp-sb=%d ip4-sb=%d dhcp4-sb=%d\n",
             (UINT32)CountHandles(&gEfiSimpleNetworkProtocolGuid),
             (UINT32)CountHandles(&gEfiIp4Config2ProtocolGuid),
             (UINT32)CountHandles(&gEfiPxeBaseCodeProtocolGuid), (UINT32)CountHandles(&MnpSb),
             (UINT32)CountHandles(&Ip4Sb), (UINT32)CountHandles(&Dhcp4Sb));
    DiagFlush();
#endif

    MeridianNics = AllocateZeroPool(sizeof(MERIDIAN_NIC_INFO) * NET_MAX_NICS);
    if (MeridianNics == NULL) {
        DiagEmit("alloc fail\n");
        goto Done;
    }

    DiagEmit("phase: snp-read begin\n");
    DiagFlush();
    ScanSnp();
    ScanIp4Config2();
    DiagEmit("phase: snp-read done nics=%d linkup=%a\n", (UINT32)MeridianNicCount,
             AnyLinkUsable() ? "yes" : "no");
    DiagFlush();

    if (AttemptDhcp && (AnyLinkUsable() || CountHandles(&gEfiSimpleNetworkProtocolGuid) == 0)) {
        DiagEmit("phase: dhcp4 begin\n");
        DiagFlush();
        ScanDhcp4(Pump, Ctx);
        DiagEmit("phase: dhcp4 done\n");
        DiagFlush();

        if (!AnyHasIp() && Pump == NULL) {
            DiagEmit("phase: pxe begin\n");
            DiagFlush();
            ScanPxeDhcp();
            DiagEmit("phase: pxe done\n");
            DiagFlush();
        }
        else {
            DiagEmit("phase: pxe skipped (%a)\n", AnyHasIp() ? "already leased" : "interactive");
            DiagFlush();
        }
    }
    else if (AttemptDhcp) {
        DiagEmit("dhcp: skipped (SNP link down)\n");
    }

    DiagEmit("result: nics=%d\n", (UINT32)MeridianNicCount);
    {
        UINTN i;
        for (i = 0; i < MeridianNicCount; i++) {
            MERIDIAN_NIC_INFO *Nic = &MeridianNics[i];
            if (Nic->HasIp) {
                DiagEmit("  NIC%d %d.%d.%d.%d %a\n", (UINT32)i, Nic->Ip[0], Nic->Ip[1], Nic->Ip[2],
                         Nic->Ip[3], Nic->Source == NET_IP_DHCP ? "DHCP" : "STATIC");
            }
            else {
                DiagEmit("  NIC%d link=%a no-ip\n", (UINT32)i, LinkStateText(Nic));
            }
        }
    }

Done:
    DiagEmit("phase: complete\n");
}
