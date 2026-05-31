// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#ifndef __MERIDIAN_CACHE_H_
#define __MERIDIAN_CACHE_H_

#include "global.h"

UINT32 ComputeTopologyFingerprint(VOID);

UINT32 ComputeBuildId(VOID);

EFI_STATUS CacheStoreMenu(IN MERIDIAN_MENU_SCREEN *Menu, IN UINT32 Fingerprint);

EFI_STATUS CacheLoadMenu(IN UINT32 ExpectedFingerprint);

VOID CacheInvalidate(VOID);

#if MERIDIAN_DEBUG > 0

VOID CacheSelfTest(VOID);
#endif

#endif
