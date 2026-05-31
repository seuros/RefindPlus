// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#ifndef __BTRFS_SNAPSHOTS_H_
#define __BTRFS_SNAPSHOTS_H_

#include "tiano_includes.h"
#include "loader_types.h"
#include "menu_types.h"

VOID AddBtrfsSnapshotSubEntries(IN LOADER_ENTRY *Entry, IN MERIDIAN_MENU_SCREEN *SubScreen,
                                IN CHAR16 *BaseOptions);

#endif
