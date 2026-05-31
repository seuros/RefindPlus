// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2026 Dayo Akanji
// SPDX-FileCopyrightText: 2006 Christoph Pfisterer

#ifndef _FSW_BASE_H_
#define _FSW_BASE_H_

#include "fsw_efi_base.h"

#ifndef FSW_DEBUG_LEVEL

#define FSW_DEBUG_LEVEL 0
#endif

#if FSW_DEBUG_LEVEL >= 1
#define FSW_MSG_L01(params) FSW_MSG_OUT params
#else
#define FSW_MSG_L01(params)
#endif

#if FSW_DEBUG_LEVEL >= 2
#define FSW_MSG_L02(params) FSW_MSG_OUT params
#else
#define FSW_MSG_L02(params)
#endif

#if FSW_DEBUG_LEVEL >= 3
#define FSW_MSG_L03(params) FSW_MSG_OUT params
#else
#define FSW_MSG_L03(params)
#endif

#endif
