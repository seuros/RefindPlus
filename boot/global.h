// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2020-2026 Dayo Akanji
// SPDX-FileCopyrightText: 2012-2023 Roderick W. Smith
// SPDX-FileCopyrightText: 2006-2009 Christoph Pfisterer

#ifndef __GLOBAL_H_
#define __GLOBAL_H_

#include "tiano_includes.h"

#include "esp_layout.h"
#include "GenericBdsLib.h"
#include "guid_values.h"
#include "menu_types.h"
#include "volume_types.h"
#include "loader_types.h"
#include "config_types.h"
#include "config_symbols.h"
#include "loader_defaults.h"
#include "tool_defaults.h"
#include "policy.h"

#include "runtime.h"
#include "log.h"

VOID StoreLoaderName (IN CHAR16 *Name);
VOID StoreLoaderIdentity(IN LOADER_ENTRY *Entry);
CHAR16 *BuildEntryIdentity(IN LOADER_ENTRY *Entry);
CHAR16 *GetPreviousBootIdentity(VOID);
BOOLEAN EntryMatchesIdentity(IN MERIDIAN_MENU_ENTRY *Entry, IN CHAR16 *Identity);
VOID RescanAll (BOOLEAN Reconnect);

extern BOOLEAN DefaultIsPreviousBoot;

LOADER_ENTRY * MakeGenericLoaderEntry (VOID);

#endif
