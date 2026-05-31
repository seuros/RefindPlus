// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseLib.h>
#include <Library/PrintLib.h>
#include <Protocol/PxeBaseCode.h>
#include <Protocol/SimpleNetwork.h>

#define MAX_NICS        16
#define IP4_FMT         L"%d.%d.%d.%d"
#define IP4_ARGS(ip)    (ip)[0], (ip)[1], (ip)[2], (ip)[3]

static VOID snp_mac_str (UINT8 *mac, CHAR16 *buf, UINTN bufsz) {
    UnicodeSPrint(buf, bufsz, L"%02X:%02X:%02X:%02X:%02X:%02X",
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static EFI_STATUS try_pxe_dhcp (
    EFI_HANDLE                  Handle,
    EFI_PXE_BASE_CODE_PROTOCOL *Pxe,
    EFI_IPv4_ADDRESS           *ServerIp,
    CHAR8                      *BootFile,
    UINTN                       BootFileSz
) {
    EFI_STATUS Status;

    Status = Pxe->Start(Pxe, FALSE);
    if (EFI_ERROR(Status) && Status != EFI_ALREADY_STARTED) {
        return Status;
    }

    Status = Pxe->Dhcp(Pxe, TRUE);
    if (EFI_ERROR(Status)) {
        Pxe->Stop(Pxe);
        return Status;
    }

    EFI_PXE_BASE_CODE_MODE *Mode = Pxe->Mode;
    if (!Mode->DhcpAckReceived) {
        Pxe->Stop(Pxe);
        return EFI_NOT_FOUND;
    }

    CopyMem(ServerIp,
        &Mode->DhcpAck.Dhcpv4.BootpSiAddr,
        sizeof(EFI_IPv4_ADDRESS));

    AsciiStrnCpyS(BootFile, BootFileSz,
        (CHAR8 *)Mode->DhcpAck.Dhcpv4.BootpBootFile,
        sizeof(Mode->DhcpAck.Dhcpv4.BootpBootFile));

    Pxe->Stop(Pxe);
    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI UefiMain (
    EFI_HANDLE        ImageHandle,
    EFI_SYSTEM_TABLE *SystemTable
) {
    EFI_STATUS  Status;
    UINTN       NumPxe   = 0;
    UINTN       NumSnp   = 0;
    EFI_HANDLE *PxeHandles = NULL;
    EFI_HANDLE *SnpHandles = NULL;

    Status = gBS->LocateHandleBuffer(
        ByProtocol,
        &gEfiPxeBaseCodeProtocolGuid,
        NULL, &NumPxe, &PxeHandles);

    if (!EFI_ERROR(Status) && NumPxe > 0) {
        for (UINTN i = 0; i < NumPxe && i < MAX_NICS; i++) {
            EFI_PXE_BASE_CODE_PROTOCOL *Pxe = NULL;
            Status = gBS->HandleProtocol(
                PxeHandles[i],
                &gEfiPxeBaseCodeProtocolGuid,
                (VOID **)&Pxe);
            if (EFI_ERROR(Status) || Pxe == NULL) continue;

            EFI_IPv4_ADDRESS ServerIp;
            CHAR8 BootFile[128];
            ZeroMem(BootFile, sizeof(BootFile));
            ZeroMem(&ServerIp, sizeof(ServerIp));

            Status = try_pxe_dhcp(PxeHandles[i], Pxe, &ServerIp, BootFile, sizeof(BootFile));
            if (!EFI_ERROR(Status)) {

                CHAR16 *Result = AllocateZeroPool(256 * sizeof(CHAR16));
                if (Result != NULL) {
                    CHAR16 FileW[128];
                    ZeroMem(FileW, sizeof(FileW));
                    AsciiStrToUnicodeStrS(BootFile, FileW, ARRAY_SIZE(FileW));
                    UnicodeSPrint(Result, 256 * sizeof(CHAR16),
                        IP4_FMT L" - %s",
                        IP4_ARGS(ServerIp.Addr),
                        FileW[0] ? FileW : L"/");

                    gBS->Exit(ImageHandle, EFI_SUCCESS,
                        (UINTN)(StrLen(Result) + 1) * sizeof(CHAR16),
                        (CHAR16 *)Result);

                    FreePool(Result);
                }
            }
        }
        FreePool(PxeHandles);
    }

    Status = gBS->LocateHandleBuffer(
        ByProtocol,
        &gEfiSimpleNetworkProtocolGuid,
        NULL, &NumSnp, &SnpHandles);

    if (!EFI_ERROR(Status) && NumSnp > 0) {
        Print(L"snp_discover: %u NIC(s) found, DHCP yielded no offer\n", (UINT32)NumSnp);
        for (UINTN i = 0; i < NumSnp && i < MAX_NICS; i++) {
            EFI_SIMPLE_NETWORK_PROTOCOL *Snp = NULL;
            if (EFI_ERROR(gBS->HandleProtocol(
                    SnpHandles[i],
                    &gEfiSimpleNetworkProtocolGuid,
                    (VOID **)&Snp)) || Snp == NULL) continue;

            CHAR16 MacStr[32];
            ZeroMem(MacStr, sizeof(MacStr));
            snp_mac_str(
                Snp->Mode->CurrentAddress.Addr,
                MacStr, ARRAY_SIZE(MacStr));
            Print(L"  NIC[%u] MAC=%s link=%s\n",
                (UINT32)i, MacStr,
                Snp->Mode->MediaPresent ? L"up" : L"down");
        }
        FreePool(SnpHandles);
    } else {
        Print(L"snp_discover: no SNP handles -- no NIC visible to UEFI\n");
    }

    return EFI_NOT_FOUND;
}
