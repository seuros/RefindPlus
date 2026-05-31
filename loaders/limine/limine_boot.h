// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#ifndef __MERIDIAN_LIMINE_BOOT_H_
#define __MERIDIAN_LIMINE_BOOT_H_

#include "global.h"
#include "limine_proto.h"

#define LIMINE_HHDM_BASE 0xffff800000000000ULL

#define LIMINE_HIGHER_HALF_BASE 0xffffffff80000000ULL

#define LIMINE_DEFAULT_STACK_SIZE (64ULL * 1024)

typedef struct
{
    UINT8 *FileData;
    UINTN FileSize;

    UINT64 ImagePhys;
    UINT64 ImageVirt;
    UINT64 ImageSize;
    UINT64 Slide;
    UINT64 EntryPoint;

    UINT64 BaseRevision;
    UINT64 *BaseRevisionTag;

    UINT64 HhdmOffset;
    UINT64 Pml4Phys;

    UINT64 TablePoolPhys;
    UINTN TablePoolPages;
    UINTN TablePoolUsed;

    UINT64 StackPhys;
    UINT64 StackSize;

    UINT64 GdtPhys;

    UINT8 *Arena;
    UINTN ArenaSize;
    UINTN ArenaUsed;

    UINT64 ReqSearchStart;
    UINT64 ReqSearchEnd;

    VOID *MemmapResponse;
    VOID *MemmapEntries;
    VOID *MemmapPointers;

    UINTN MemmapMaxEntries;
    VOID *EfiMemmapResponse;
    VOID *EfiMemmapBuffer;
    UINTN EfiMemmapBufferSize;

    UINT64 FbPhys;
    UINT64 FbSize;

    UINT64 ExecFilePhys;
    UINT64 ExecFilePages;

    MERIDIAN_VOLUME *Volume;
    CHAR16 *LoaderPath;
    CHAR16 *Cmdline;
} LIMINE_CTX;

BOOLEAN IsLimineExecutable(IN MERIDIAN_VOLUME *Volume, IN CHAR16 *LoaderPath);

EFI_STATUS LimineLoadElf(IN OUT LIMINE_CTX *Ctx);

EFI_STATUS LimineAnswerRequests(IN OUT LIMINE_CTX *Ctx);
VOID *LimineArenaAlloc(IN OUT LIMINE_CTX *Ctx, IN UINTN Size, IN UINTN Align);

VOID LimineFillMemmap(IN OUT LIMINE_CTX *Ctx, IN EFI_MEMORY_DESCRIPTOR *Map, IN UINTN MapSize,
                      IN UINTN DescSize);

EFI_STATUS LimineBuildPageTables(IN OUT LIMINE_CTX *Ctx);
EFI_STATUS LimineMapPage(IN OUT LIMINE_CTX *Ctx, IN UINT64 Virt, IN UINT64 Phys, IN UINT64 Flags,
                         IN BOOLEAN Large);

EFI_STATUS LimineHandoff(IN OUT LIMINE_CTX *Ctx);

EFI_STATUS LaunchLimineKernel(IN LOADER_ENTRY *Entry);

#endif
