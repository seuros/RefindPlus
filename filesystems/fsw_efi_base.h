// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2026 Dayo Akanji
// SPDX-FileCopyrightText: 2006 Christoph Pfisterer

#ifndef _FSW_EFI_BASE_H_
#define _FSW_EFI_BASE_H_

#include "fsw_efi_edk2_base.h"

#define FSW_LITTLE_ENDIAN (1)

typedef INT8    fsw_s8;
typedef UINT8   fsw_u8;
typedef INT16   fsw_s16;
typedef UINT16  fsw_u16;
typedef INT32   fsw_s32;
typedef UINT32  fsw_u32;
typedef INT64   fsw_s64;
typedef UINT64  fsw_u64;

#define FSW_DO_ALLOC(size, ptrptr) (((*(ptrptr) = AllocatePool (size)) == NULL) ? FSW_OUT_OF_MEMORY : FSW_SUCCESS)
#define FSW_DO_FREE(ptr) FreePool(ptr)

#define FSW_DO_MEMZERO(dest,size) ZeroMem(dest,size)
#define FSW_DO_MEMCPY(dest,src,size) CopyMem(dest,src,size)
#define FSW_DO_MEMEQ(p1,p2,size) (CompareMem(p1,p2,size) == 0)

#define FSW_MSG_STR(s) L##s
#define FSW_MSG_OUT Print

#define FSW_U64_SHR(val,shiftbits) RShiftU64((val), (shiftbits))
#define FSW_U64_DIV(val,divisor) DivU64x32((val), (divisor), NULL)
#endif
