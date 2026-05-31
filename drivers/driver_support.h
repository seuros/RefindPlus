// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2025 Dayo Akanji
// SPDX-FileCopyrightText: Intel Corporation

#ifndef _DRIVER_SUPPORT
#define _DRIVER_SUPPORT

#include "tiano_includes.h"
#include "global.h"

VOID ConnectAllDriversToAllControllers(VOID);

BOOLEAN LoadDrivers(VOID);
#endif
