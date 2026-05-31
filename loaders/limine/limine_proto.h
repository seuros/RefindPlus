// SPDX-License-Identifier: 0BSD
// SPDX-FileCopyrightText: 2022-2026 Mintsuki and contributors
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#ifndef __MERIDIAN_LIMINE_PROTO_H_
#define __MERIDIAN_LIMINE_PROTO_H_

#include "tiano_includes.h"

#define LIMINE_COMMON_MAGIC_0 0xc7b1dd30df4c8b88ULL
#define LIMINE_COMMON_MAGIC_1 0x0a82e883a194f07bULL

#define LIMINE_REQUESTS_START_0 0xf6b8f4b39de7d1aeULL
#define LIMINE_REQUESTS_START_1 0xfab91a6940fcb9cfULL
#define LIMINE_REQUESTS_START_2 0x785c6ed015d3e316ULL
#define LIMINE_REQUESTS_START_3 0x181e920a7852b9d9ULL

#define LIMINE_REQUESTS_END_0 0xadc0e0531bb10d03ULL
#define LIMINE_REQUESTS_END_1 0x9572709f31764c62ULL

#define LIMINE_BASE_REVISION_0 0xf9562b2d5c95a6c8ULL
#define LIMINE_BASE_REVISION_1 0x6a7b384944536bdcULL

#define LIMINE_MAX_BASE_REVISION 4

typedef struct
{
    UINT32 a;
    UINT16 b;
    UINT16 c;
    UINT8 d[8];
} LIMINE_UUID;

#define LIMINE_MEDIA_TYPE_GENERIC 0
#define LIMINE_MEDIA_TYPE_OPTICAL 1
#define LIMINE_MEDIA_TYPE_TFTP 2

typedef struct
{
    UINT64 Revision;
    UINT64 Address;
    UINT64 Size;
    UINT64 Path;
    UINT64 String;
    UINT32 MediaType;
    UINT32 Unused;
    UINT8 TftpIpv4[4];
    UINT32 TftpPort;
    UINT32 PartitionIndex;
    UINT32 MbrDiskId;
    LIMINE_UUID GptDiskUuid;
    LIMINE_UUID GptPartUuid;
    LIMINE_UUID PartUuid;
} LIMINE_FILE;

typedef struct
{
    UINT64 Id[4];
    UINT64 Revision;
    UINT64 Response;
} LIMINE_REQUEST;

#define LIMINE_ID_BOOTLOADER_INFO_2 0xf55038d8e2a1202fULL
#define LIMINE_ID_BOOTLOADER_INFO_3 0x279426fcf5f59740ULL

#define LIMINE_ID_EXECUTABLE_CMDLINE_2 0x4b161536e598651eULL
#define LIMINE_ID_EXECUTABLE_CMDLINE_3 0xb390ad4a2f1f303aULL

#define LIMINE_ID_FIRMWARE_TYPE_2 0x8c2f75d90bef28a8ULL
#define LIMINE_ID_FIRMWARE_TYPE_3 0x7045a4688eac00c3ULL

#define LIMINE_ID_STACK_SIZE_2 0x224ef0460a8e8926ULL
#define LIMINE_ID_STACK_SIZE_3 0xe1cb0fc25f46ea3dULL

#define LIMINE_ID_HHDM_2 0x48dcf1cb8ad2b852ULL
#define LIMINE_ID_HHDM_3 0x63984e959a98244bULL

#define LIMINE_ID_FRAMEBUFFER_2 0x9d5827dcd881dd75ULL
#define LIMINE_ID_FRAMEBUFFER_3 0xa3148604f6fab11bULL

#define LIMINE_ID_PAGING_MODE_2 0x95c1a0edab0944cbULL
#define LIMINE_ID_PAGING_MODE_3 0xa4e5cb3842f7488aULL

#define LIMINE_ID_MP_2 0x95a67b819a1b857eULL
#define LIMINE_ID_MP_3 0xa0b61b723b6a73e0ULL

#define LIMINE_ID_MEMMAP_2 0x67cf3d9d378a806fULL
#define LIMINE_ID_MEMMAP_3 0xe304acdfc50c3c62ULL

#define LIMINE_ID_ENTRY_POINT_2 0x13d86c035a1cd3e1ULL
#define LIMINE_ID_ENTRY_POINT_3 0x2b0caa89d8f3026aULL

#define LIMINE_ID_EXECUTABLE_FILE_2 0xad97e90e83f1ed67ULL
#define LIMINE_ID_EXECUTABLE_FILE_3 0x31eb5d1c5ff23b69ULL

#define LIMINE_ID_MODULE_2 0x3e7e279702be32afULL
#define LIMINE_ID_MODULE_3 0xca1c4f3bd1280ceeULL

#define LIMINE_ID_RSDP_2 0xc5e77b6b397e7b43ULL
#define LIMINE_ID_RSDP_3 0x27637845accdcf3cULL

#define LIMINE_ID_SMBIOS_2 0x9e9046f11e095391ULL
#define LIMINE_ID_SMBIOS_3 0xaa4a520fefbde5eeULL

#define LIMINE_ID_EFI_SYSTEM_TABLE_2 0x5ceba5163eaaf6d6ULL
#define LIMINE_ID_EFI_SYSTEM_TABLE_3 0x0a6981610cf65fccULL

#define LIMINE_ID_EFI_MEMMAP_2 0x7df62a431d6872d5ULL
#define LIMINE_ID_EFI_MEMMAP_3 0xa4fcdfb3e57306c8ULL

#define LIMINE_ID_DATE_AT_BOOT_2 0x502746e184c088aaULL
#define LIMINE_ID_DATE_AT_BOOT_3 0xfbc5ec83e6327893ULL

#define LIMINE_ID_EXECUTABLE_ADDRESS_2 0x71ba76863cc55f63ULL
#define LIMINE_ID_EXECUTABLE_ADDRESS_3 0xb2644a48c516a487ULL

typedef struct
{
    UINT64 Revision;
    UINT64 Name;
    UINT64 Version;
} LIMINE_BOOTLOADER_INFO_RESPONSE;

typedef struct
{
    UINT64 Revision;
    UINT64 Cmdline;
} LIMINE_EXECUTABLE_CMDLINE_RESPONSE;

#define LIMINE_FIRMWARE_TYPE_X86BIOS 0
#define LIMINE_FIRMWARE_TYPE_EFI32 1
#define LIMINE_FIRMWARE_TYPE_EFI64 2
#define LIMINE_FIRMWARE_TYPE_SBI 3

typedef struct
{
    UINT64 Revision;
    UINT64 FirmwareType;
} LIMINE_FIRMWARE_TYPE_RESPONSE;

typedef struct
{
    UINT64 Revision;
} LIMINE_STACK_SIZE_RESPONSE;

typedef struct
{
    UINT64 Id[4];
    UINT64 Revision;
    UINT64 Response;
    UINT64 StackSize;
} LIMINE_STACK_SIZE_REQUEST;

typedef struct
{
    UINT64 Revision;
    UINT64 Offset;
} LIMINE_HHDM_RESPONSE;

#define LIMINE_FRAMEBUFFER_RGB 1

typedef struct
{
    UINT64 Address;
    UINT64 Width;
    UINT64 Height;
    UINT64 Pitch;
    UINT16 Bpp;
    UINT8 MemoryModel;
    UINT8 RedMaskSize;
    UINT8 RedMaskShift;
    UINT8 GreenMaskSize;
    UINT8 GreenMaskShift;
    UINT8 BlueMaskSize;
    UINT8 BlueMaskShift;
    UINT8 Unused[7];
    UINT64 EdidSize;
    UINT64 Edid;

    UINT64 ModeCount;
    UINT64 Modes;
} LIMINE_FRAMEBUFFER;

typedef struct
{
    UINT64 Revision;
    UINT64 FramebufferCount;
    UINT64 Framebuffers;
} LIMINE_FRAMEBUFFER_RESPONSE;

#define LIMINE_PAGING_MODE_X86_64_4LVL 0
#define LIMINE_PAGING_MODE_X86_64_5LVL 1

typedef struct
{
    UINT64 Revision;
    UINT64 Mode;
} LIMINE_PAGING_MODE_RESPONSE;

typedef struct
{
    UINT64 Id[4];
    UINT64 Revision;
    UINT64 Response;
    UINT64 Mode;
    UINT64 MaxMode;
    UINT64 MinMode;
} LIMINE_PAGING_MODE_REQUEST;

typedef struct
{
    UINT64 Base;
    UINT64 Length;
    UINT64 Type;
} LIMINE_MEMMAP_ENTRY;

#define LIMINE_MEMMAP_USABLE 0
#define LIMINE_MEMMAP_RESERVED 1
#define LIMINE_MEMMAP_ACPI_RECLAIMABLE 2
#define LIMINE_MEMMAP_ACPI_NVS 3
#define LIMINE_MEMMAP_BAD_MEMORY 4
#define LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE 5
#define LIMINE_MEMMAP_EXECUTABLE_AND_MODULES 6
#define LIMINE_MEMMAP_FRAMEBUFFER 7
#define LIMINE_MEMMAP_RESERVED_MAPPED 8

typedef struct
{
    UINT64 Revision;
    UINT64 EntryCount;
    UINT64 Entries;
} LIMINE_MEMMAP_RESPONSE;

typedef struct
{
    UINT64 Revision;
} LIMINE_ENTRY_POINT_RESPONSE;

typedef struct
{
    UINT64 Id[4];
    UINT64 Revision;
    UINT64 Response;
    UINT64 Entry;
} LIMINE_ENTRY_POINT_REQUEST;

typedef struct
{
    UINT64 Revision;
    UINT64 ExecutableFile;
} LIMINE_EXECUTABLE_FILE_RESPONSE;

#define LIMINE_INTERNAL_MODULE_REQUIRED (1 << 0)
#define LIMINE_INTERNAL_MODULE_COMPRESSED (1 << 1)

typedef struct
{
    UINT64 Path;
    UINT64 String;
    UINT64 Flags;
} LIMINE_INTERNAL_MODULE;

typedef struct
{
    UINT64 Revision;
    UINT64 ModuleCount;
    UINT64 Modules;
} LIMINE_MODULE_RESPONSE;

typedef struct
{
    UINT64 Id[4];
    UINT64 Revision;
    UINT64 Response;
    UINT64 InternalModuleCount;
    UINT64 InternalModules;
} LIMINE_MODULE_REQUEST;

typedef struct
{
    UINT64 Revision;
    UINT64 Address;
} LIMINE_RSDP_RESPONSE;

typedef struct
{
    UINT64 Revision;
    UINT64 Entry32;
    UINT64 Entry64;
} LIMINE_SMBIOS_RESPONSE;

typedef struct
{
    UINT64 Revision;
    UINT64 Address;
} LIMINE_EFI_SYSTEM_TABLE_RESPONSE;

typedef struct
{
    UINT64 Revision;
    UINT64 Memmap;
    UINT64 MemmapSize;
    UINT64 DescSize;
    UINT64 DescVersion;
} LIMINE_EFI_MEMMAP_RESPONSE;

typedef struct
{
    UINT64 Revision;
    INT64 Timestamp;
} LIMINE_DATE_AT_BOOT_RESPONSE;

typedef struct
{
    UINT64 Revision;
    UINT64 PhysicalBase;
    UINT64 VirtualBase;
} LIMINE_EXECUTABLE_ADDRESS_RESPONSE;

#endif
