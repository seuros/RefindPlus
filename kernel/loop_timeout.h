// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#ifndef __LOOP_TIMEOUT_H_
#define __LOOP_TIMEOUT_H_

#include "tiano_includes.h"

typedef struct
{
    EFI_EVENT Timer;
    BOOLEAN HaveTimer;
    UINTN Iterations;
    UINTN MaxIterations;
} LOOP_TIMER;

VOID LoopTimerStart(OUT LOOP_TIMER *Lt, IN UINTN Seconds);

BOOLEAN LoopTimerLive(IN OUT LOOP_TIMER *Lt);

VOID LoopTimerStop(IN OUT LOOP_TIMER *Lt);

#define WHILE_TIMEOUT(_lt, _secs) for (LoopTimerStart(&(_lt), (_secs)); LoopTimerLive(&(_lt));)

#endif
