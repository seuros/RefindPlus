// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2020-2026 Dayo Akanji
// SPDX-FileCopyrightText: 2012-2023 Roderick W. Smith

#include "global.h"
#include "config.h"
#include "lib.h"
#include "scan.h"
#include "mystrings.h"
#include "launch_efi.h"

#define IPXE_NAME MERIDIAN_ESP_REMOTE_ABS L"\\ipxe.efi"
#define IPXE_DISCOVER_NAME MERIDIAN_ESP_REMOTE_ABS L"\\ipxe_discover.efi"

static CHAR16 *RuniPXEDiscover(EFI_HANDLE Volume)
{
    EFI_STATUS Status;
    EFI_DEVICE_PATH_PROTOCOL *FilePath;
    EFI_HANDLE iPXEHandle;
    CHAR16 *boot_info;
    UINTN boot_info_size;

    FilePath = FileDevicePath(Volume, IPXE_DISCOVER_NAME);
    Status = gBS->LoadImage(FALSE, SelfImageHandle, FilePath, NULL, 0, &iPXEHandle);
    if (EFI_ERROR(Status)) {
        return NULL;
    }

    boot_info = NULL;
    boot_info_size = 0;
    gBS->StartImage(iPXEHandle, &boot_info_size, &boot_info);

    return boot_info;
}

VOID ScanNetboot(VOID)
{
    CHAR16 *Temp;
    CHAR16 *Location;
    MERIDIAN_VOLUME *NetVolume;

#if MERIDIAN_DEBUG > 0
    DEBUG_LOG(1, LOG_THREE_STAR_SEP, L"Netboot (IPXE) Options");
#endif

    if (FileExists(SelfVolume->RootDir, IPXE_NAME) &&
        FileExists(SelfVolume->RootDir, IPXE_DISCOVER_NAME) &&
        IsValidLoader(SelfVolume->RootDir, IPXE_DISCOVER_NAME) &&
        IsValidLoader(SelfVolume->RootDir, IPXE_NAME)) {
        Location = RuniPXEDiscover(SelfVolume->DeviceHandle);
        if (Location != NULL && FileExists(SelfVolume->RootDir, IPXE_NAME)) {
            NetVolume = CopyVolume(SelfVolume);
            if (NetVolume != NULL) {
                NetVolume->DiskKind = DISK_KIND_NET;

                MRD_FREE_POOL(NetVolume->PartName);
                MRD_FREE_POOL(NetVolume->VolName);
                MRD_FREE_POOL(NetVolume->FsName);

                DisplayLoader = TRUE;
                Temp = StrDuplicate(IPXE_NAME);
                AddLoaderEntry(Temp, Location, NetVolume, TRUE, FALSE, NULL);
                MRD_FREE_POOL(Temp);

                FreeVolume(&NetVolume);
            }
        }

        MRD_FREE_POOL(Location);
    }
}
