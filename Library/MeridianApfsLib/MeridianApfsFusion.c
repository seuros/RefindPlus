// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2020-2026 Dayo Akanji
// SPDX-FileCopyrightText: 2019-2021 Acidanthera (OpenCore OcApfsLib, BSD-3-Clause)

#include "MeridianApfsInternal.h"
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include "MeridianApfsLib.h"
#include <Protocol/BlockIo.h>

VOID InternalApfsInitFusionData (
    IN  APFS_NX_SUPERBLOCK   *SuperBlock,
    OUT APFS_PRIVATE_DATA    *PrivateData
) {
    LIST_ENTRY         *Entry;
    APFS_PRIVATE_DATA  *Sibling;
    UINT32              BlockSize;

    if (IsZeroGuid (&SuperBlock->FusionUuid)) {
        PrivateData->CanLoadDriver = TRUE;
        return;
    }

    CopyGuid (&PrivateData->FusionUuid, &SuperBlock->FusionUuid);
    PrivateData->IsFusion = TRUE;

    PrivateData->IsFusionMaster = (SuperBlock->FusionUuid.Data4[7] & BIT0) == 0;

    PrivateData->FusionUuid.Data4[7] &= ~BIT0;

    for (
        Entry = GetFirstNode (&mApfsPrivateDataList);
        !IsNull (&mApfsPrivateDataList, Entry);
        Entry = GetNextNode (&mApfsPrivateDataList, Entry)
    ) {
        Sibling = CR(Entry, APFS_PRIVATE_DATA, Link, APFS_PRIVATE_DATA_SIGNATURE);

        if (!Sibling->IsFusion                                     ||
            Sibling->CanLoadDriver                                 ||
            Sibling->IsFusionMaster == PrivateData->IsFusionMaster ||
            !CompareGuid (&Sibling->FusionUuid, &PrivateData->FusionUuid)
        ) {
            continue;
        }

        PrivateData->FusionSibling = Sibling;
        PrivateData->CanLoadDriver = TRUE;

        PrivateData->FusionMask    = APFS_FUSION_TIER2_DEVICE_BYTE_ADDR;
        BlockSize                  = PrivateData->ApfsBlockSize;
        while ((BlockSize & BIT0) == 0) {
            PrivateData->FusionMask >>= 1U;
            BlockSize               >>= 1U;
        }

        PrivateData->FusionSibling->FusionSibling = PrivateData;
        PrivateData->FusionSibling->CanLoadDriver = TRUE;
        PrivateData->FusionSibling->FusionMask    = PrivateData->FusionMask;
        break;
    }
}

EFI_BLOCK_IO_PROTOCOL * InternalApfsTranslateBlock (
    IN  APFS_PRIVATE_DATA    *PrivateData,
    IN  UINT64                Block,
    OUT EFI_LBA              *Lba
) {
    BOOLEAN  IsFusionMaster;

    if (!PrivateData->CanLoadDriver) {
        return NULL;
    }

    if (!PrivateData->IsFusion) {
        *Lba = Block * PrivateData->LbaMultiplier;
        return PrivateData->BlockIo;
    }

    if (PrivateData->FusionSibling == NULL) {
        return NULL;
    }

    if ((Block & PrivateData->FusionMask) == 0) {
        IsFusionMaster = TRUE;
    }
    else {
        Block         &= ~PrivateData->FusionMask;
        IsFusionMaster = FALSE;
    }

    *Lba = Block * PrivateData->LbaMultiplier;

    if (IsFusionMaster == PrivateData->IsFusionMaster) {
        return PrivateData->BlockIo;
    }

    return PrivateData->FusionSibling->BlockIo;
}
