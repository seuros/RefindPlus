// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#include "limine_boot.h"
#include "elf64.h"
#include "lib.h"

#define LIMINE_PAGE_SIZE 0x1000ULL

static UINT64 RoundUpPage(IN UINT64 Value)
{
    return (Value + LIMINE_PAGE_SIZE - 1) & ~(LIMINE_PAGE_SIZE - 1);
}

static UINT64 RoundDownPage(IN UINT64 Value) { return Value & ~(LIMINE_PAGE_SIZE - 1); }

static EFI_STATUS ApplyRelocations(IN LIMINE_CTX *Ctx, IN ELF64_EHDR *Ehdr, IN ELF64_PHDR *Phdrs)
{
    UINT16 i;
    UINTN j;
    UINT64 RelaVirt;
    UINT64 RelaSize;
    UINT64 RelaEnt;
    UINT64 DynCount;
    ELF64_DYN *Dyn;
    ELF64_RELA *Rela;
    UINT64 *Target;

    RelaVirt = 0;
    RelaSize = 0;
    RelaEnt = sizeof(ELF64_RELA);

    for (i = 0; i < Ehdr->e_phnum; i++) {
        if (Phdrs[i].p_type != PT_DYNAMIC) {
            continue;
        }

        Dyn = (ELF64_DYN *)(UINTN)(Ctx->ImagePhys +
                                   (Phdrs[i].p_vaddr - (Ctx->ImageVirt - Ctx->Slide)));
        DynCount = Phdrs[i].p_memsz / sizeof(ELF64_DYN);

        for (j = 0; j < DynCount && Dyn[j].d_tag != DT_NULL; j++) {
            switch (Dyn[j].d_tag) {
            case DT_RELA:
                RelaVirt = Dyn[j].d_val;
                break;
            case DT_RELASZ:
                RelaSize = Dyn[j].d_val;
                break;
            case DT_RELAENT:
                RelaEnt = Dyn[j].d_val;
                break;
            default:
                break;
            }
        }
    }

    if (RelaVirt == 0 || RelaSize == 0 || RelaEnt == 0) {

        return EFI_SUCCESS;
    }

    for (j = 0; j + RelaEnt <= RelaSize; j += RelaEnt) {
        Rela =
            (ELF64_RELA *)(UINTN)(Ctx->ImagePhys + (RelaVirt - (Ctx->ImageVirt - Ctx->Slide)) + j);

        if (ELF64_R_TYPE(Rela->r_info) != R_X86_64_RELATIVE) {
            return EFI_UNSUPPORTED;
        }

        Target =
            (UINT64 *)(UINTN)(Ctx->ImagePhys + (Rela->r_offset - (Ctx->ImageVirt - Ctx->Slide)));
        *Target = (UINT64)Rela->r_addend + Ctx->Slide;
    }

    return EFI_SUCCESS;
}

EFI_STATUS LimineLoadElf(IN OUT LIMINE_CTX *Ctx)
{
    EFI_STATUS Status;
    UINT16 i;
    UINT64 MinVaddr;
    UINT64 MaxVaddr;
    UINT64 Pages;
    EFI_PHYSICAL_ADDRESS Image;
    ELF64_EHDR *Ehdr;
    ELF64_PHDR *Phdrs;
    BOOLEAN SawLoad;

    if (Ctx == NULL || Ctx->FileData == NULL || Ctx->FileSize < sizeof(ELF64_EHDR)) {
        return EFI_INVALID_PARAMETER;
    }

    Ehdr = (ELF64_EHDR *)Ctx->FileData;

    if (Ehdr->e_phoff == 0 || Ehdr->e_phnum == 0 || Ehdr->e_phentsize < sizeof(ELF64_PHDR) ||
        Ehdr->e_phoff + ((UINT64)Ehdr->e_phnum * Ehdr->e_phentsize) > Ctx->FileSize) {
        return EFI_LOAD_ERROR;
    }

    Phdrs = (ELF64_PHDR *)(Ctx->FileData + Ehdr->e_phoff);

    MinVaddr = MAX_UINT64;
    MaxVaddr = 0;
    SawLoad = FALSE;

    for (i = 0; i < Ehdr->e_phnum; i++) {
        if (Phdrs[i].p_type != PT_LOAD || Phdrs[i].p_memsz == 0) {
            continue;
        }

        if (Phdrs[i].p_filesz > Phdrs[i].p_memsz ||
            Phdrs[i].p_offset + Phdrs[i].p_filesz > Ctx->FileSize) {
            return EFI_LOAD_ERROR;
        }

        SawLoad = TRUE;

        if (Phdrs[i].p_vaddr < MinVaddr) {
            MinVaddr = Phdrs[i].p_vaddr;
        }
        if (Phdrs[i].p_vaddr + Phdrs[i].p_memsz > MaxVaddr) {
            MaxVaddr = Phdrs[i].p_vaddr + Phdrs[i].p_memsz;
        }
    }

    if (!SawLoad) {
        return EFI_LOAD_ERROR;
    }

    MinVaddr = RoundDownPage(MinVaddr);
    MaxVaddr = RoundUpPage(MaxVaddr);

    if (MinVaddr < LIMINE_HIGHER_HALF_BASE) {
        if (Ehdr->e_type != ET_DYN) {

            return EFI_UNSUPPORTED;
        }
        Ctx->Slide = LIMINE_HIGHER_HALF_BASE - MinVaddr;
    }
    else {
        Ctx->Slide = 0;
    }

    Ctx->ImageVirt = MinVaddr + Ctx->Slide;
    Ctx->ImageSize = MaxVaddr - MinVaddr;

    Pages = Ctx->ImageSize / LIMINE_PAGE_SIZE;

    Image = 0;
    Status = gBS->AllocatePages(AllocateAnyPages, EfiLoaderData, (UINTN)Pages, &Image);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Ctx->ImagePhys = (UINT64)Image;

    ZeroMem((VOID *)(UINTN)Ctx->ImagePhys, (UINTN)Ctx->ImageSize);

    for (i = 0; i < Ehdr->e_phnum; i++) {
        if (Phdrs[i].p_type != PT_LOAD || Phdrs[i].p_memsz == 0) {
            continue;
        }

        CopyMem((VOID *)(UINTN)(Ctx->ImagePhys + (Phdrs[i].p_vaddr - MinVaddr)),
                Ctx->FileData + Phdrs[i].p_offset, (UINTN)Phdrs[i].p_filesz);
    }

    if (Ctx->Slide != 0) {
        Status = ApplyRelocations(Ctx, Ehdr, Phdrs);
        if (EFI_ERROR(Status)) {
            gBS->FreePages(Image, (UINTN)Pages);
            Ctx->ImagePhys = 0;

            return Status;
        }
    }

    Ctx->EntryPoint = Ehdr->e_entry + Ctx->Slide;

    Ctx->ReqSearchStart = Ctx->ImagePhys;
    Ctx->ReqSearchEnd = Ctx->ImagePhys + Ctx->ImageSize;

    return EFI_SUCCESS;
}
