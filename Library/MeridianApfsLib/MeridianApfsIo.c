// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2020-2026 Dayo Akanji
// SPDX-FileCopyrightText: 2019-2021 Acidanthera (OpenCore OcApfsLib, BSD-3-Clause)

#include "MeridianApfsInternal.h"
#include <IndustryStandard/PeImage.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include "MeridianApfsLib.h"
#include <Library/SafeIntLib.h>

static
UINT64 ApfsFletcher64 (
    VOID    *Data,
    UINTN    DataSize
) {
    UINT32        *Walker;
    UINT32        *WalkerEnd;
    UINT64         Sum1;
    UINT64         Sum2;
    UINT32         Rem;

    ASSERT (DataSize >= APFS_NX_MINIMUM_BLOCK_SIZE - sizeof (UINT64));
    ASSERT (DataSize <= APFS_NX_MAXIMUM_BLOCK_SIZE - sizeof (UINT64));
    ASSERT (DataSize % sizeof (UINT32) == 0);

    Sum1 = 0;
    Sum2 = 0;

    Walker     = Data;
    WalkerEnd  = Walker + DataSize / sizeof (UINT32);

    while (Walker < WalkerEnd) {

        Sum1 += *Walker;

        Sum2 += Sum1;
        ++Walker;
    }

    Sum2 += Sum1;
    APFS_MOD_MAX_UINT32 (Sum2, &Rem);
    Sum2  = ~Rem;

    Sum1 += Sum2;
    APFS_MOD_MAX_UINT32 (Sum1, &Rem);
    Sum1  = ~Rem;

    return (Sum1 << 32U) | Sum2;
}

static
BOOLEAN ApfsBlockChecksumVerify (
    APFS_OBJ_PHYS   *Block,
    UINTN            DataSize
) {
    UINT64  NewChecksum;

    ASSERT (DataSize > sizeof (*Block));

    NewChecksum = ApfsFletcher64 (
        &Block->ObjectOid,
        DataSize - sizeof (Block->Checksum)
    );

    if (NewChecksum == Block->Checksum) {
        return TRUE;
    }

    return FALSE;
}

static
EFI_STATUS ApfsReadJumpStart (
    IN  APFS_PRIVATE_DATA      *PrivateData,
    OUT APFS_NX_EFI_JUMPSTART  **JumpStartPtr
) {
    EFI_STATUS              Status;
    APFS_NX_EFI_JUMPSTART  *JumpStart;
    EFI_BLOCK_IO_PROTOCOL  *BlockIo;
    EFI_LBA                 Lba;
    UINT32                  MaxExtents;
    BOOLEAN                 Verified;

    if (PrivateData->EfiJumpStart == 0) {
        return EFI_UNSUPPORTED;
    }

    JumpStart = AllocateZeroPool (PrivateData->ApfsBlockSize);
    if (JumpStart == NULL) {
        return EFI_OUT_OF_RESOURCES;
    }

    BlockIo = InternalApfsTranslateBlock (
        PrivateData,
        PrivateData->EfiJumpStart, &Lba
    );
    if (BlockIo == NULL) {
        FreePool (JumpStart);
        return EFI_UNSUPPORTED;
    }

    Status = BlockIo->ReadBlocks(BlockIo, BlockIo->Media->MediaId, Lba, PrivateData->ApfsBlockSize, JumpStart);
    if (EFI_ERROR(Status)) {
        FreePool (JumpStart);
        return Status;
    }

    if (JumpStart->Magic != APFS_NX_EFI_JUMPSTART_MAGIC) {
        FreePool (JumpStart);
        return EFI_UNSUPPORTED;
    }

    Verified = ApfsBlockChecksumVerify (
        &JumpStart->BlockHeader,
        PrivateData->ApfsBlockSize
    );
    if (!Verified) {
        FreePool (JumpStart);
        return EFI_UNSUPPORTED;
    }

    MaxExtents = (
        PrivateData->ApfsBlockSize - sizeof (*JumpStart)
    ) / sizeof (JumpStart->RecordExtents[0]);
    if (MaxExtents < JumpStart->NumExtents) {
        FreePool (JumpStart);
        return EFI_UNSUPPORTED;
    }

    *JumpStartPtr = JumpStart;
    return EFI_SUCCESS;
}

static
EFI_STATUS ApfsReadDriver (
    IN  APFS_PRIVATE_DATA       *PrivateData,
    IN  APFS_NX_EFI_JUMPSTART   *JumpStart,
    OUT UINTN                   *DriverSize,
    OUT VOID                   **DriverBuffer
) {
    EFI_STATUS              Status;
    VOID                   *EfiFile;
    UINTN                   EfiFileSize;
    UINTN                   OrgEfiFileSize;
    UINT8                  *ChunkPtr;
    UINTN                   ChunkSize;
    UINTN                   Index;
    BOOLEAN                 Overflow;
    EFI_BLOCK_IO_PROTOCOL  *BlockIo;
    EFI_LBA                 Lba;

    EfiFileSize = JumpStart->EfiFileLen / PrivateData->ApfsBlockSize + 1;
    if (EFI_ERROR (SafeUintnMult (EfiFileSize, PrivateData->ApfsBlockSize, &EfiFileSize))) {
        return EFI_SECURITY_VIOLATION;
    }

    OrgEfiFileSize = EfiFileSize;

    EfiFile = AllocateZeroPool (EfiFileSize);
    if (EfiFile == NULL) {
        return EFI_OUT_OF_RESOURCES;
    }

    ChunkPtr = EfiFile;

    for (Index = 0; Index < JumpStart->NumExtents || EfiFileSize != 0; ++Index) {
        BlockIo = InternalApfsTranslateBlock (
            PrivateData,
            JumpStart->RecordExtents[Index].StartPhysicalAddr,
            &Lba
        );
        if (BlockIo == NULL) {
            Status = EFI_SECURITY_VIOLATION;
            break;
        }

        Overflow = EFI_ERROR (SafeUintnMult (
            (UINTN) JumpStart->RecordExtents[Index].BlockCount,
            PrivateData->ApfsBlockSize,
            &ChunkSize
        ));
        if (Overflow ||
            ChunkSize > EfiFileSize ||
            JumpStart->RecordExtents[Index].BlockCount > MAX_UINTN
        ) {
            Status = EFI_SECURITY_VIOLATION;
            break;
        }

        Status = BlockIo->ReadBlocks (
            BlockIo,
            BlockIo->Media->MediaId,
            Lba,
            ChunkSize,
            ChunkPtr
        );
        if (EFI_ERROR(Status)) {
            break;
        }

        ChunkPtr    += ChunkSize;
        EfiFileSize -= ChunkSize;
    }

    if (EFI_ERROR(Status)) {
        FreePool (EfiFile);
        return Status;
    }

    if (OrgEfiFileSize != JumpStart->EfiFileLen) {
        ChunkPtr  = EfiFile;
        ChunkPtr += JumpStart->EfiFileLen;
        ZeroMem (ChunkPtr, OrgEfiFileSize - JumpStart->EfiFileLen);
    }

    *DriverSize   = JumpStart->EfiFileLen;
    *DriverBuffer = EfiFile;

    return EFI_SUCCESS;
}

EFI_STATUS InternalApfsReadSuperBlock (
    IN  EFI_BLOCK_IO_PROTOCOL   *BlockIo,
    OUT APFS_NX_SUPERBLOCK     **SuperBlockPtr
) {
    EFI_STATUS            Status;
    APFS_NX_SUPERBLOCK   *SuperBlock;
    UINTN                 ReadSize;
    UINTN                 Retry;
    BOOLEAN               Verified;

    ReadSize = ALIGN_VALUE (APFS_NX_MINIMUM_BLOCK_SIZE, BlockIo->Media->BlockSize);

    SuperBlock = NULL;

    for (Retry = 0; Retry < 2; ++Retry) {

        SuperBlock = AllocateZeroPool (ReadSize);
        if (SuperBlock == NULL) {
            break;
        }

        Status = BlockIo->ReadBlocks(BlockIo, BlockIo->Media->MediaId, 0, ReadSize, SuperBlock);
        if (EFI_ERROR(Status)) {
            break;
        }

        if (SuperBlock->Magic != APFS_NX_SIGNATURE) {
            break;
        }

        if (SuperBlock->BlockSize < BlockIo->Media->BlockSize    ||
            SuperBlock->BlockSize < APFS_NX_MINIMUM_BLOCK_SIZE   ||
            SuperBlock->BlockSize > APFS_NX_MAXIMUM_BLOCK_SIZE   ||
            (SuperBlock->BlockSize & (sizeof (UINT32) - 1)) != 0 ||
            (SuperBlock->BlockSize & (BlockIo->Media->BlockSize - 1)) != 0
        ) {
            break;
        }

        if (SuperBlock->BlockSize > ReadSize) {
            ReadSize = SuperBlock->BlockSize;
            FreePool (SuperBlock);
            SuperBlock = NULL;
            continue;
        }

        Verified = ApfsBlockChecksumVerify (
            &SuperBlock->BlockHeader,
            SuperBlock->BlockSize
        );
        if (!Verified) break;

        if (SuperBlock->BlockHeader.ObjectSubType != 0 ||
            SuperBlock->BlockHeader.ObjectOid     != 1 ||
            SuperBlock->BlockHeader.ObjectType    != (
                APFS_OBJ_EPHEMERAL | APFS_OBJECT_TYPE_NX_SUPERBLOCK
            )
        ) {
            break;
        }

        *SuperBlockPtr = SuperBlock;

        return EFI_SUCCESS;
    }

    if (SuperBlock != NULL) {
        FreePool (SuperBlock);
    }

    return EFI_UNSUPPORTED;
}

EFI_STATUS InternalApfsReadDriver (
    IN  APFS_PRIVATE_DATA    *PrivateData,
    OUT UINTN                *DriverSize,
    OUT VOID                **DriverBuffer
) {
    EFI_STATUS              Status;
    APFS_NX_EFI_JUMPSTART  *JumpStart;

    Status = ApfsReadJumpStart (
        PrivateData, &JumpStart
    );
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Status = ApfsReadDriver (
        PrivateData, JumpStart,
        DriverSize, DriverBuffer
    );

    FreePool (JumpStart);

    return Status;
}
