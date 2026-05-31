// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#ifndef __MERIDIAN_ELF64_H_
#define __MERIDIAN_ELF64_H_

#include "tiano_includes.h"

#define ELF_MAG0 0x7f
#define ELF_MAG1 'E'
#define ELF_MAG2 'L'
#define ELF_MAG3 'F'

#define ELFCLASS64 2
#define ELFDATA2LSB 1

#define ET_EXEC 2
#define ET_DYN 3

#define EM_X86_64 62

#define PT_LOAD 1
#define PT_DYNAMIC 2

#define PF_X 0x1
#define PF_W 0x2
#define PF_R 0x4

#define DT_NULL 0
#define DT_RELA 7
#define DT_RELASZ 8
#define DT_RELAENT 9

#define R_X86_64_RELATIVE 8

#define ELF64_R_TYPE(Info) ((UINT32)((Info) & 0xffffffffULL))

#pragma pack(1)

typedef struct
{
    UINT8 e_ident[16];
    UINT16 e_type;
    UINT16 e_machine;
    UINT32 e_version;
    UINT64 e_entry;
    UINT64 e_phoff;
    UINT64 e_shoff;
    UINT32 e_flags;
    UINT16 e_ehsize;
    UINT16 e_phentsize;
    UINT16 e_phnum;
    UINT16 e_shentsize;
    UINT16 e_shnum;
    UINT16 e_shstrndx;
} ELF64_EHDR;

typedef struct
{
    UINT32 p_type;
    UINT32 p_flags;
    UINT64 p_offset;
    UINT64 p_vaddr;
    UINT64 p_paddr;
    UINT64 p_filesz;
    UINT64 p_memsz;
    UINT64 p_align;
} ELF64_PHDR;

typedef struct
{
    INT64 d_tag;
    UINT64 d_val;
} ELF64_DYN;

typedef struct
{
    UINT64 r_offset;
    UINT64 r_info;
    INT64 r_addend;
} ELF64_RELA;

#pragma pack()

#endif
