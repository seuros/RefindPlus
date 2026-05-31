// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2020-2026 Dayo Akanji
// SPDX-FileCopyrightText: 2012-2020 Roderick W. Smith
// SPDX-FileCopyrightText: 2006-2009 Christoph Pfisterer

#ifndef __CONFIG_H_
#define __CONFIG_H_

#include "tiano_includes.h"
#include "global.h"

typedef struct
{
    UINT8 *BufferData;
    UINTN BufferSize;
    UINTN Encoding;
    CHAR8 *End08Ptr;
    CHAR8 *Current08Ptr;
    CHAR16 *Current16Ptr;
    CHAR16 *End16Ptr;
} MERIDIAN_FILE;

#define DONT_SCAN_VOLUMES L"LRS_ESP"
#define ALSO_SCAN_DIRS L"boot,@/boot"

#if defined(EFIX64)
#define MRD_ARCH_STR L"x86_64"
#elif defined(EFIAARCH64)
#define MRD_ARCH_STR L"aarch64"
#else
#define MRD_ARCH_STR L"unknown"
#endif
#define MRD_FWTYPE_STR L"uefi"

#if defined(EFIX64)
#define DONT_SCAN_FILES                                                                            \
    L"shim.efi,shimx64.efi,shim-fedora.efi,shim-centos.efi,PreLoader.efi,fb.efi,fbx64.efi"
#elif defined(EFIAARCH64)
#define DONT_SCAN_FILES                                                                            \
    L"shim.efi,shimaa64.efi,shim-fedora.efi,shim-centos.efi,PreLoader.efi,fb.efi,fbaa64.efi"
#else
#define DONT_SCAN_FILES L"shim.efi,shim-fedora.efi,shim-centos.efi,PreLoader.efi,fb.efi"
#endif

VOID ReadConfig(CHAR16 *FileName);
VOID ScanUserConfigured(CHAR16 *FileName);

VOID BootstrapMissingConfig(VOID);

VOID ApplySmbiosConfig(VOID);

MERIDIAN_VOLUME *GetStanzaVolume(MERIDIAN_FILE *File, MERIDIAN_VOLUME *Volume);

VOID SyncLinuxPrefixes(VOID);
VOID SyncToolPaths(VOID);
VOID SyncAlsoScan(VOID);
VOID SyncDontScanDirs(VOID);
VOID SyncDontScanFiles(VOID);
VOID SyncShowTools(VOID);

BOOLEAN KeepReading(IN OUT CHAR16 *InString, IN OUT BOOLEAN *IsQuoted);
VOID HandleSignedInt(IN CHAR16 **TokenList, IN UINTN TokenCount, OUT INTN *Value);
VOID HandleUnsignedInt(IN CHAR16 **TokenList, IN UINTN TokenCount, OUT UINTN *Value);
VOID HandleString(IN CHAR16 **TokenList, IN UINTN TokenCount, OUT CHAR16 **Target);
VOID HandleStrings(IN CHAR16 **TokenList, IN UINTN TokenCount, OUT CHAR16 **Target);
VOID HandleHexes(IN CHAR16 **TokenList, IN UINTN TokenCount, IN UINTN MaxValue,
                 OUT UINT32_LIST **Target);
UINTN HandleTime(IN CHAR16 *TimeString);
BOOLEAN HandleBoolean(IN CHAR16 **TokenList, IN UINTN TokenCount);

EFI_STATUS MeridianReadFile(IN EFI_FILE_HANDLE BaseDir, IN CHAR16 *FileName,
                            IN OUT MERIDIAN_FILE *File, OUT UINTN *size);

UINTN ReadTokenLine(IN MERIDIAN_FILE *File, OUT CHAR16 ***TokenList);

VOID FreeTokenLine(IN OUT CHAR16 ***TokenList, IN OUT UINTN *TokenCount);

MERIDIAN_FILE *ReadLinuxOptionsFile(IN CHAR16 *LoaderPath, IN MERIDIAN_VOLUME *Volume);

CHAR16 *GetFirstOptionsFromFile(IN CHAR16 *LoaderPath, IN MERIDIAN_VOLUME *Volume);
CHAR16 *ReadLine(IN MERIDIAN_FILE *File);

BOOLEAN AddSubmenu(LOADER_ENTRY *Entry, MERIDIAN_FILE *File, MERIDIAN_VOLUME *Volume,
                   CHAR16 *Title);

#endif
