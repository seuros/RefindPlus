// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#ifndef MERIDIAN_APPLE_SMC_H
#define MERIDIAN_APPLE_SMC_H

#include <Uefi.h>

#define APPLE_SMC_MAXVAL 32

#define APPLE_SMC_KEY(a, b, c, d) ((UINT32)((a) << 24 | (b) << 16 | (c) << 8 | (d)))

typedef enum
{
    APPLE_SMC_NONE = 0,
    APPLE_SMC_PROTOCOL,
    APPLE_SMC_MMIO,
    APPLE_SMC_PORT
} APPLE_SMC_BACKEND;

APPLE_SMC_BACKEND MrdAppleSmcBackend(VOID);

BOOLEAN MrdAppleSmcReadKey(CONST CHAR8 Key[4], UINT8 *Buf, UINT8 Len);
BOOLEAN MrdAppleSmcWriteKey(CONST CHAR8 Key[4], CONST UINT8 *Buf, UINT8 Len);

VOID MrdAppleSmcNotifyReset(BOOLEAN Restart);

#endif
