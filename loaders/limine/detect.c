// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#include "limine_boot.h"
#include "elf64.h"
#include "lib.h"

static BOOLEAN HasLimineMarker(IN UINT8 *Data, IN UINTN Size)
{
    UINTN i;
    UINTN Words;
    UINT64 *Word;

    if (Data == NULL || Size < 16) {
        return FALSE;
    }

    Word = (UINT64 *)Data;
    Words = Size / sizeof(UINT64);

    for (i = 0; i + 1 < Words; i++) {
        if (Word[i] == LIMINE_COMMON_MAGIC_0 && Word[i + 1] == LIMINE_COMMON_MAGIC_1) {
            return TRUE;
        }
        if (Word[i] == LIMINE_BASE_REVISION_0 && Word[i + 1] == LIMINE_BASE_REVISION_1) {
            return TRUE;
        }
    }

    return FALSE;
}

static BOOLEAN IsX64Elf(IN UINT8 *Data, IN UINTN Size)
{
    ELF64_EHDR *Ehdr;

    if (Data == NULL || Size < sizeof(ELF64_EHDR)) {
        return FALSE;
    }

    Ehdr = (ELF64_EHDR *)Data;

    if (Ehdr->e_ident[0] != ELF_MAG0 || Ehdr->e_ident[1] != ELF_MAG1 ||
        Ehdr->e_ident[2] != ELF_MAG2 || Ehdr->e_ident[3] != ELF_MAG3) {
        return FALSE;
    }

    if (Ehdr->e_ident[4] != ELFCLASS64 || Ehdr->e_ident[5] != ELFDATA2LSB) {
        return FALSE;
    }

    if (Ehdr->e_machine != EM_X86_64) {
        return FALSE;
    }

    return (Ehdr->e_type == ET_EXEC || Ehdr->e_type == ET_DYN);
}

BOOLEAN IsLimineExecutable(IN MERIDIAN_VOLUME *Volume, IN CHAR16 *LoaderPath)
{
    EFI_STATUS Status;
    BOOLEAN Found;
    UINT8 *Data;
    UINTN Size;

    if (Volume == NULL || Volume->RootDir == NULL || LoaderPath == NULL) {
        return FALSE;
    }

    Data = NULL;
    Size = 0;

    Status = MrdLoadFile(Volume->RootDir, LoaderPath, &Data, &Size);
    if (EFI_ERROR(Status) || Data == NULL) {
        return FALSE;
    }

    Found = IsX64Elf(Data, Size) && HasLimineMarker(Data, Size);

    MRD_FREE_POOL(Data);

    return Found;
}
