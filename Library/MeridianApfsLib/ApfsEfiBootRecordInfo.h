// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2020-2026 Dayo Akanji
// SPDX-FileCopyrightText: 2019-2021 Acidanthera (OpenCore OcApfsLib, BSD-3-Clause)

#ifndef APFS_EFIBOOTRECORD_INFO_PROTOCOL_H
#define APFS_EFIBOOTRECORD_INFO_PROTOCOL_H

#define APFS_EFIBOOTRECORD_INFO_PROTOCOL_GUID \
  { 0x03B8D751, 0xA02F, 0x4FF8,               \
    { 0x9B, 0x1A, 0x55, 0x24, 0xAF, 0xA3, 0x94, 0x5F } }

typedef struct  _APFS_EFIBOOTRECORD_LOCATION_INFO {

  EFI_HANDLE  ControllerHandle;

  EFI_GUID    ContainerUuid;
} APFS_EFIBOOTRECORD_LOCATION_INFO;

extern EFI_GUID gApfsEfiBootRecordInfoProtocolGuid;

#endif
