// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2020-2026 Dayo Akanji
// SPDX-FileCopyrightText: 2012-2023 Roderick W. Smith
// SPDX-FileCopyrightText: 2006-2009 Christoph Pfisterer

#ifndef __MERIDIAN_VOLUME_TYPES_H_
#define __MERIDIAN_VOLUME_TYPES_H_

#include "tiano_includes.h"
#include "display_types.h"

#define DISCOVERY_TYPE_UNKNOWN (0)
#define DISCOVERY_TYPE_AUTO (1)
#define DISCOVERY_TYPE_MANUAL (2)

#define DEVICE_TYPE_HW (0x01)
#define DEVICE_TYPE_ACPI (0x02)
#define DEVICE_TYPE_MESSAGING (0x03)
#define DEVICE_TYPE_MEDIA (0x04)
#define DEVICE_TYPE_BIOS (0x05)
#define DEVICE_TYPE_END (0x75)

#define FS_TYPE_UNKNOWN (0)
#define FS_TYPE_WHOLEDISK (1)
#define FS_TYPE_FAT12 (2)
#define FS_TYPE_FAT16 (3)
#define FS_TYPE_FAT32 (4)
#define FS_TYPE_EXFAT (5)
#define FS_TYPE_NTFS (6)
#define FS_TYPE_EXT2 (7)
#define FS_TYPE_EXT3 (8)
#define FS_TYPE_EXT4 (9)
#define FS_TYPE_HFSPLUS (10)
#define FS_TYPE_APFS (11)
#define FS_TYPE_BTRFS (12)
#define FS_TYPE_XFS (13)
#define FS_TYPE_JFS (14)
#define FS_TYPE_ISO9660 (15)
#define FS_TYPE_UFS (16)
#define FS_TYPE_BCACHEFS (17)
#define NUM_FS_TYPES (18)

typedef struct
{
    UINT8 Flags;
    UINT8 StartCHS1;
    UINT8 StartCHS2;
    UINT8 StartCHS3;
    UINT8 Type;
    UINT8 EndCHS1;
    UINT8 EndCHS2;
    UINT8 EndCHS3;
    UINT32 StartLBA;
    UINT32 Size;
} MBR_PARTITION_INFO;

typedef struct
{
    EFI_DEVICE_PATH_PROTOCOL *DevicePath;
    EFI_HANDLE DeviceHandle;
    EFI_FILE_PROTOCOL *RootDir;
    CHAR16 *PartName;
    CHAR16 *FsName;
    CHAR16 *VolName;
    UINT32 VolRole;
    EFI_GUID VolUuid;
    EFI_GUID PartGuid;
    EFI_GUID PartTypeGuid;
    BOOLEAN IsMarkedReadOnly;
    UINTN DiskKind;
    BOOLEAN HasBootCode;
    CHAR16 *OSSplashHint;
    CHAR16 *OSName;
    BOOLEAN IsMbrPartition;
    UINTN MbrPartitionIndex;
    EFI_BLOCK_IO_PROTOCOL *BlockIO;
    UINT64 BlockIOOffset;
    EFI_BLOCK_IO_PROTOCOL *WholeDiskBlockIO;
    EFI_DEVICE_PATH_PROTOCOL *WholeDiskDevicePath;
    MBR_PARTITION_INFO *MbrPartitionTable;
    BOOLEAN IsReadable;
    UINT32 FSType;
    BOOLEAN AllowSymlinks;
} MERIDIAN_VOLUME;

#endif
