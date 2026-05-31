// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2020-2026 Dayo Akanji

#include "fsw_efi.h"
#define gMyEfiBlockIoProtocolGuid gEfiBlockIoProtocolGuid
#define gMyEfiDiskIoProtocolGuid gEfiDiskIoProtocolGuid

extern struct fsw_host_table   fsw_efi_host_table;
static void dummy_volume_free (struct fsw_volume *vol) { }
static struct fsw_fstype_table   dummy_fstype = {
    { FSW_STRING_TYPE_UTF08, 4, 4, "dummy" },
    sizeof (struct fsw_volume),
    sizeof (struct fsw_dnode),

    NULL,
    dummy_volume_free,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

static
struct fsw_volume * dsk_btrfs_create_dummy_volume (
    EFI_DISK_IO_PROTOCOL *diskio,
    UINT32 mediaid
) {
    fsw_status_t err;
    struct fsw_volume *vol;
    FSW_VOLUME_DATA *Volume;

    err = fsw_alloc_zero (sizeof (struct fsw_volume), (void **) &vol);
    if (err) return NULL;

    err = fsw_alloc_zero (sizeof (FSW_VOLUME_DATA), (void **) &Volume);
    if (err) {
        FSW_DO_FREE(vol);
        return NULL;
    }

    vol->fstype_table = &dummy_fstype;

    Volume->DiskIo = diskio;
    Volume->MediaId = mediaid;

    vol->host_data = Volume;
    vol->host_table = &fsw_efi_host_table;
    return vol;
}

static
struct fsw_volume * dsk_btrfs_clone_dummy_volume (
    struct fsw_volume *vol
) {
    FSW_VOLUME_DATA *Volume = (FSW_VOLUME_DATA *)vol->host_data;
    return dsk_btrfs_create_dummy_volume(Volume->DiskIo, Volume->MediaId);
}

static
void dsk_btrfs_free_dummy_volume (
    struct fsw_volume *vol
) {
    FSW_DO_FREE(vol->host_data);
    fsw_unmount (vol);
}

static
int dsk_btrfs_scan_disks (
    int (*hook)(
        struct fsw_volume *,
        struct fsw_volume *
    ), struct fsw_volume *master
) {
    EFI_STATUS  Status;
    EFI_HANDLE *Handles;
    UINTN       i;
    UINTN       HandleCount = 0;
    UINTN       scanned = 0;

    FSW_MSG_L03((
        FSW_MSG_STR(
            "SCANDISK: dsk_btrfs_scan_disks ... Scanning Disks\n"
        )
    ));

    Status = gBS->LocateHandleBuffer(ByProtocol, &gMyEfiDiskIoProtocolGuid, NULL, &HandleCount, &Handles);
    if (Status == EFI_NOT_FOUND) {
        return -1;
    }

    for (i = 0; i < HandleCount; i++) {
        EFI_DISK_IO_PROTOCOL *diskio;
        EFI_BLOCK_IO_PROTOCOL *blockio;

        Status = gBS->HandleProtocol(Handles[i], &gMyEfiDiskIoProtocolGuid, (VOID **) &diskio);
        if (Status != 0) {
            continue;
        }

        Status = gBS->HandleProtocol(Handles[i], &gMyEfiBlockIoProtocolGuid, (VOID **) &blockio);
        if (Status != 0) {
            continue;
        }

        struct fsw_volume *vol = dsk_btrfs_create_dummy_volume (
            diskio, blockio->Media->MediaId
        );

        if (vol) {
            FSW_MSG_L03((
                FSW_MSG_STR(
                    "SCANDISK: dsk_btrfs_scan_disks ... Checking Disk %d\n"
                ), i
            ));

            if (hook(master, vol) == FSW_SUCCESS) {
                scanned++;
            }
            dsk_btrfs_free_dummy_volume (vol);
        }
    }

    return scanned;
}
