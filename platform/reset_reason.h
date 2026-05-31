// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#ifndef MERIDIAN_RESET_REASON_H
#define MERIDIAN_RESET_REASON_H

#include <Uefi.h>

typedef enum
{
    BOOT_CAUSE_UNKNOWN = 0,
    BOOT_CAUSE_COLD,
    BOOT_CAUSE_WARM,
    BOOT_CAUSE_CRASH,
    BOOT_CAUSE_RESUME,
    BOOT_CAUSE_POWERFAIL,
} MERIDIAN_BOOT_CAUSE;

extern MERIDIAN_BOOT_CAUSE MeridianBootCause;

MERIDIAN_BOOT_CAUSE MrdDetectBootCause(VOID);

const CHAR8 *MrdBootCauseStr(MERIDIAN_BOOT_CAUSE Cause);

const CHAR8 *MrdBootCauseDetail(VOID);

#endif
