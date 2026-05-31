// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2020-2026 Dayo Akanji
// SPDX-FileCopyrightText: 2012-2023 Roderick W. Smith
// SPDX-FileCopyrightText: 2006-2010 Christoph Pfisterer

#ifndef __MERIDIAN_MAIN_H_
#define __MERIDIAN_MAIN_H_

#include "tiano_includes.h"
#include "global.h"

VOID    PrepToolMenu (UINTN LabelTag);
BOOLEAN ShowInfoCleanNvram (CHAR16 *ToolPurpose);
VOID    HandleToolRun (CHAR16 *TypeStr, BOOLEAN ToolFlag, LOADER_ENTRY *OurLoaderEntry OPTIONAL);

VOID UnexpectedReturn (CHAR16 *ItemType);
VOID InitRotateCSR (VOID);
EFI_STATUS StoreBootArgsNvram(IN VOID *VariableData OPTIONAL);
EFI_STATUS TrimCoerce(VOID);
UINTN RunTrustSync (LOADER_ENTRY *Entry);
VOID RunNVramSync (CHAR16 *SelectionName, BOOLEAN IsMacOS);

VOID AlignCSR (VOID);
VOID RunMacBootSupportFuncs (CHAR16 *SelectionName);

#endif
