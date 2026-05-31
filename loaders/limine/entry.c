// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#include "limine_boot.h"
#include "lib.h"
#include "screenmgt.h"

#define LIMINE_PAGE_SIZE 0x1000ULL

#define LIMINE_ARENA_PAGES 32

static VOID LimineFreeCtx(IN OUT LIMINE_CTX *Ctx)
{
    if (Ctx->FileData != NULL) {
        MRD_FREE_POOL(Ctx->FileData);
    }
}

EFI_STATUS LaunchLimineKernel(IN LOADER_ENTRY *Entry)
{
    EFI_STATUS Status;
    EFI_PHYSICAL_ADDRESS Arena;
    LIMINE_CTX Ctx;

    if (Entry == NULL || Entry->Volume == NULL || Entry->Volume->RootDir == NULL ||
        Entry->LoaderPath == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    ZeroMem(&Ctx, sizeof(Ctx));

    Ctx.Volume = Entry->Volume;
    Ctx.LoaderPath = Entry->LoaderPath;
    Ctx.Cmdline = Entry->LoadOptions;

    Ctx.HhdmOffset = LIMINE_HHDM_BASE;

    INFO_LOG("Loading Limine kernel:- '%s'", Entry->LoaderPath);

    Status = MrdLoadFile(Entry->Volume->RootDir, Entry->LoaderPath, &Ctx.FileData, &Ctx.FileSize);
    if (EFI_ERROR(Status)) {
        INFO_LOG("Limine: could not read the executable");

        return Status;
    }

    Arena = 0;
    Status = gBS->AllocatePages(AllocateAnyPages, EfiLoaderData, LIMINE_ARENA_PAGES, &Arena);
    if (EFI_ERROR(Status)) {
        LimineFreeCtx(&Ctx);

        return Status;
    }

    Ctx.Arena = (UINT8 *)(UINTN)Arena;
    Ctx.ArenaSize = (UINTN)(LIMINE_ARENA_PAGES * LIMINE_PAGE_SIZE);
    Ctx.ArenaUsed = 0;

    ZeroMem(Ctx.Arena, Ctx.ArenaSize);

    Status = LimineLoadElf(&Ctx);
    if (EFI_ERROR(Status)) {
        INFO_LOG("Limine: executable rejected (%r)", Status);
        LimineFreeCtx(&Ctx);

        return Status;
    }

    Status = LimineAnswerRequests(&Ctx);
    if (EFI_ERROR(Status)) {
        INFO_LOG("Limine: could not answer requests (%r)", Status);
        LimineFreeCtx(&Ctx);

        return Status;
    }

    INFO_LOG("Limine: base revision %d, entry %lx, image %lx -> %lx (%lx bytes)", Ctx.BaseRevision,
             Ctx.EntryPoint, Ctx.ImagePhys, Ctx.ImageVirt, Ctx.ImageSize);

    Status = LimineBuildPageTables(&Ctx);
    if (EFI_ERROR(Status)) {
        INFO_LOG("Limine: could not build page tables (%r)", Status);
        LimineFreeCtx(&Ctx);

        return Status;
    }

    BltClearScreen(FALSE);

    Status = LimineHandoff(&Ctx);

    INFO_LOG("Limine: handoff failed (%r)", Status);
    LimineFreeCtx(&Ctx);

    return Status;
}
