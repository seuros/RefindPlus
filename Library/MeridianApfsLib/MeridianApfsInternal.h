// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2020-2026 Dayo Akanji
// SPDX-FileCopyrightText: 2019-2021 Acidanthera (OpenCore OcApfsLib, BSD-3-Clause)

#ifndef MERIDIAN_APFS_INTERNAL_H
#define MERIDIAN_APFS_INTERNAL_H

#include "Apfs.h"
#include <Protocol/BlockIo.h>
#include "ApfsEfiBootRecordInfo.h"

#define APFS_PRIVATE_DATA_SIGNATURE  SIGNATURE_32 ('A', 'F', 'J', 'S')

#ifdef MDE_CPU_X64
  #define APFS_MOD_MAX_UINT32(Value, Result) do { *(Result) = ((Value) % MAX_UINT32); } while (0)
#else
  #define APFS_MOD_MAX_UINT32(Value, Result) do { DivU64x32Remainder ((Value), MAX_UINT32, (Result)); } while (0)
#endif

typedef struct APFS_PRIVATE_DATA_ APFS_PRIVATE_DATA;

typedef struct APFS_PRIVATE_DATA_ {

  UINT32                              Signature;

  LIST_ENTRY                          Link;

  APFS_EFIBOOTRECORD_LOCATION_INFO    LocationInfo;

  EFI_BLOCK_IO_PROTOCOL               *BlockIo;

  UINT32                              ApfsBlockSize;

  UINT32                              LbaMultiplier;

  UINT64                              EfiJumpStart;

  GUID                                FusionUuid;

  UINT64                              FusionMask;

  APFS_PRIVATE_DATA                   *FusionSibling;

  BOOLEAN                             CanLoadDriver;

  BOOLEAN                             IsFusion;

  BOOLEAN                             IsFusionMaster;
} APFS_PRIVATE_DATA;

extern LIST_ENTRY  mApfsPrivateDataList;

EFI_STATUS InternalApfsReadSuperBlock (
  IN  EFI_BLOCK_IO_PROTOCOL   *BlockIo,
  OUT APFS_NX_SUPERBLOCK     **SuperBlockPtr
  );

EFI_STATUS InternalApfsReadDriver (
  IN  APFS_PRIVATE_DATA    *PrivateData,
  OUT UINTN                *DriverSize,
  OUT VOID                **DriverBuffer
  );

VOID InternalApfsInitFusionData (
  IN  APFS_NX_SUPERBLOCK   *SuperBlock,
  OUT APFS_PRIVATE_DATA    *PrivateData
  );

EFI_BLOCK_IO_PROTOCOL * InternalApfsTranslateBlock (
  IN  APFS_PRIVATE_DATA    *PrivateData,
  IN  UINT64                Block,
  OUT EFI_LBA              *Lba
  );

#endif
