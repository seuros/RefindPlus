// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2020-2023 Dayo Akanji

#ifndef __MEMLOG_LIB_H__
#define __MEMLOG_LIB_H__

#define MEM_LOG_INITIAL_SIZE    (128 * 1024)
#define MEM_LOG_MAX_SIZE        (10 * 1024 * 1024)
#define MEM_LOG_MAX_LINE_SIZE   1024

typedef VOID (EFIAPI *MEM_LOG_CALLBACK) (IN INTN DebugMode, IN CHAR8 *LastMessage);

VOID EFIAPI MemLogVA (
  IN  const BOOLEAN  Timing,
  IN  const INTN     DebugMode,
  IN  const CHAR8   *Format,
  IN  VA_LIST        Marker
);

VOID EFIAPI MemLog (
  IN  const BOOLEAN  Timing,
  IN  const INTN     DebugMode,
  IN  const CHAR8   *Format,
  ...
);

CHAR8 * EFIAPI GetMemLogBuffer (VOID);

UINTN EFIAPI GetMemLogLen (VOID);

VOID EFIAPI SetMemLogCallback (
  MEM_LOG_CALLBACK  Callback
);

UINT64 EFIAPI GetMemLogTscTicksPerSecond (VOID);

UINT64 GetCurrentMS (VOID);

#if MERIDIAN_DEBUG > 0
VOID EFIAPI DebugLog (
    IN const CHAR8 *FormatString,
    ...
);
#endif

#endif
