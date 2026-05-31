// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2020-2026 Dayo Akanji
// SPDX-FileCopyrightText: 2012-2023 Roderick W. Smith

#ifndef __MERIDIAN_LOG_H_
#define __MERIDIAN_LOG_H_

#include "tiano_includes.h"
#include "runtime.h"

#define MRD_OFFSET_OF(st, m) ((UINTN)((char *)&((st *)0x100000)->m - (char *)0x100000))

#define LOG_BLOCK_SEP (0)
#define LOG_BLANK_LINE_SEP (1)
#define LOG_BLANK_LINE_TWO (2)
#define LOG_LINE_SPECIAL (3)
#define LOG_LINE_SAME (4)
#define LOG_LINE_NORMAL (5)
#define LOG_LINE_SEPARATOR (6)
#define LOG_LINE_THIN_SEP (7)
#define LOG_STAR_SEPARATOR (8)
#define LOG_THREE_STAR_SEP (9)
#define LOG_THREE_STAR_MID (10)
#define LOG_THREE_STAR_END (11)
#define LOG_STAR_HEAD_SEP (12)
#define LOG_STAR_HEAD_SEPX (13)
#define LOG_LINE_FORENSIC (14)
#define LOG_LINE_EXIT (15)
#define LOG_LINE_BASE (16)

#define OUR_MSG_STR(s) L##s
#define OUR_MSG_OUT Print

#define OUR_MSG_L00(params) OUR_MSG_OUT params

#if MERIDIAN_DEBUG >= 1
#define OUR_MSG_L01(params) OUR_MSG_OUT params
#else
#define OUR_MSG_L01(params)
#endif

#if MERIDIAN_DEBUG >= 2
#define OUR_MSG_L02(params) OUR_MSG_OUT params
#else
#define OUR_MSG_L02(params)
#endif

VOID DeepLoggger(IN INTN level, IN INTN type, IN CHAR16 **Msg);
VOID WayPointer(IN CHAR16 *Msg);

extern CHAR16 *gLogTemp;

#if MERIDIAN_DEBUG > 0
extern VOID LogPadding(BOOLEAN Increment);
extern VOID EFIAPI DebugLog(IN const CHAR8 *FormatString, ...);

#define DEBUG_LOG(level, type, ...)                                                                \
    do {                                                                                           \
        if (!gKernelStarted) {                                                                     \
            gLogTemp = PoolPrint(__VA_ARGS__);                                                     \
            DeepLoggger(level, type, &gLogTemp);                                                   \
        }                                                                                          \
    } while (0)
#define INFO_LOG(...) DebugLog(__VA_ARGS__);
#define OUT_TAG() WayPointer(L"<<------ * ------>>");
#define RET_TAG() WayPointer(L"------>> * <<------");
#define END_TAG() WayPointer(L"< << <<< * >>> >> >");
#else
#define END_TAG()
#define RET_TAG()
#define OUT_TAG()
#define INFO_LOG(...)
#define DEBUG_LOG(...)
#endif

#if MERIDIAN_DEBUG < 1
#define LOG_INCREMENT(...)
#define LOG_DECREMENT(...)
#define LOG_SEP(...)
#define BRK_MAX(...)
#define BRK_MOD(...)
#define BRK_MIN(...)
#elif MERIDIAN_DEBUG < 2
#define BRK_MIN(...)                                                                               \
    do {                                                                                           \
        if (!gKernelStarted) {                                                                     \
            DebugLog(__VA_ARGS__);                                                                 \
        }                                                                                          \
    } while (0)
#define BRK_MOD(...)                                                                               \
    do {                                                                                           \
        if (!gKernelStarted) {                                                                     \
            DebugLog(__VA_ARGS__);                                                                 \
        }                                                                                          \
    } while (0)
#define BRK_MAX(...)
#define LOG_SEP(...)
#define LOG_DECREMENT(...)
#define LOG_INCREMENT(...)
#else
#define LOG_INCREMENT(...)                                                                         \
    do {                                                                                           \
        if (!gKernelStarted) {                                                                     \
            LogPadding(TRUE);                                                                      \
        }                                                                                          \
    } while (0)
#define LOG_DECREMENT(...)                                                                         \
    do {                                                                                           \
        if (!gKernelStarted) {                                                                     \
            LogPadding(FALSE);                                                                     \
        }                                                                                          \
    } while (0)
#define LOG_SEP(...)                                                                               \
    do {                                                                                           \
        if (!gKernelStarted) {                                                                     \
            gLogTemp = PoolPrint(__VA_ARGS__);                                                     \
            DeepLoggger(2, LOG_BLOCK_SEP, &gLogTemp);                                              \
        }                                                                                          \
    } while (0)
#define BRK_MAX(...)                                                                               \
    do {                                                                                           \
        if (!gKernelStarted) {                                                                     \
            DebugLog(__VA_ARGS__);                                                                 \
        }                                                                                          \
    } while (0)
#define BRK_MOD(...)                                                                               \
    do {                                                                                           \
        if (!gKernelStarted) {                                                                     \
            DebugLog(__VA_ARGS__);                                                                 \
        }                                                                                          \
    } while (0)
#define BRK_MIN(...)                                                                               \
    do {                                                                                           \
        if (!gKernelStarted) {                                                                     \
            DebugLog(__VA_ARGS__);                                                                 \
        }                                                                                          \
    } while (0)
#endif

#endif
