// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2026 Dayo Akanji
// SPDX-FileCopyrightText: 2006 Christoph Pfisterer

#ifndef _FSW_EFI_H_
#define _FSW_EFI_H_

#include "fsw_core.h"

#define MERIDIAN_EFI_DISK_IO_PROTOCOL_GUID \
  { \
    0xce345171, 0xba0b, 0x11d2, {0x8e, 0x4f, 0x0, 0xa0, 0xc9, 0x69, 0x72, 0x3b } \
  }

#define MERIDIAN_EFI_BLOCK_IO_PROTOCOL_GUID \
  { \
    0x964e5b21, 0x6459, 0x11d2, {0x8e, 0x39, 0x0, 0xa0, 0xc9, 0x69, 0x72, 0x3b } \
  }

typedef struct {
    UINT64                                Signature;

    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL       FileSystem;

    EFI_HANDLE                            Handle;
    EFI_DISK_IO_PROTOCOL                 *DiskIo;
    UINT32                                MediaId;
    EFI_STATUS                            LastIOStatus;

    struct fsw_volume                    *vol;

} FSW_VOLUME_DATA;

#define FSW_VOLUME_DATA_SIGNATURE  EFI_SIGNATURE_32('f', 's', 'w', 'V')

#define FSW_VOLUME_FROM_FILE_SYSTEM(a)  CR(a, FSW_VOLUME_DATA, FileSystem, FSW_VOLUME_DATA_SIGNATURE)

typedef struct {
    UINT64                      Signature;

    EFI_FILE_PROTOCOL           FileHandle;

    UINT64                      Type;
    struct fsw_shandle          shand;

} FSW_FILE_DATA;

#define FSW_EFI_FILE_TYPE_FILE  (0)

#define FSW_EFI_FILE_TYPE_DIR   (1)

#define FSW_FILE_DATA_SIGNATURE    EFI_SIGNATURE_32('f', 's', 'w', 'F')

#define FSW_FILE_FROM_FILE_HANDLE(a)  CR(a, FSW_FILE_DATA, FileHandle, FSW_FILE_DATA_SIGNATURE)

extern struct fsw_fstype_table  *fsw_active_fstype_table;
extern CONST CHAR16             *fsw_active_fstype_name;

VOID fsw_efi_decode_time (OUT EFI_TIME *EfiTime, IN UINT32 UnixTime);

UINTN fsw_efi_strsize (struct fsw_string *s);
VOID fsw_efi_strcpy (CHAR16 *Dest, struct fsw_string *src);
VOID EFIAPI fsw_efi_clear_cache (VOID);

#endif
