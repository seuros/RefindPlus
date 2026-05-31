// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2020-2023 Dayo Akanji
// SPDX-FileCopyrightText: 2012-2023 Roderick W. Smith
// SPDX-FileCopyrightText: 2006-2010 Christoph Pfisterer

#ifndef __MERIDIAN_LAUNCH_EFI_H_
#define __MERIDIAN_LAUNCH_EFI_H_

#include "tiano_includes.h"
#include "global.h"

EFI_STATUS RebootIntoFirmware(VOID);
EFI_STATUS StartEFIImage(IN MERIDIAN_VOLUME *Volume, IN CHAR16 *Filename, IN CHAR16 *LoadOptions,
                         IN CHAR16 *ImageTitle, IN CHAR8 OSType, IN BOOLEAN Verbose,
                         IN BOOLEAN IsDriver, IN CONST CHAR16 *SplashHint OPTIONAL,
                         OUT EFI_HANDLE *NewImageHandle OPTIONAL);
EFI_STATUS ConstructBootEntry(EFI_HANDLE *TargetVolume, CHAR16 *Loader, CHAR16 *Label,
                              CHAR8 **Entry, UINTN *Size);

BOOLEAN IsValidLoader(EFI_FILE_PROTOCOL *RootDir, CHAR16 *FileName);

VOID StartTool(IN LOADER_ENTRY *Entry);
VOID RebootIntoLoader(LOADER_ENTRY *Entry);
VOID StartLoader(IN LOADER_ENTRY *Entry, IN CHAR16 *SelectionName, IN BOOLEAN TrustSynced);

#endif
