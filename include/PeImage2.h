// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: Intel Corporation

#ifndef _MERIDIAN_PEIMAGE2_H_
#define _MERIDIAN_PEIMAGE2_H_

typedef struct _MERIDIAN_PE_COFF_LOADER_IMAGE_CONTEXT {
   UINT64                             ImageAddress;
   UINT64                             ImageSize;
   UINT64                             EntryPoint;
   UINTN                              SizeOfHeaders;
   UINT16                             ImageType;
   UINT16                             NumberOfSections;
   EFI_IMAGE_SECTION_HEADER          *FirstSection;
   EFI_IMAGE_DATA_DIRECTORY          *RelocDir;
   EFI_IMAGE_DATA_DIRECTORY          *SecDir;
   UINT64                             NumberOfRvaAndSizes;
   EFI_IMAGE_OPTIONAL_HEADER_UNION   *PEHdr;
} MERIDIAN_PE_COFF_LOADER_IMAGE_CONTEXT;

#endif
