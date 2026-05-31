// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#include "loop_timeout.h"

#define LOOP_TIMER_UNITS_PER_SECOND (10000000ULL)

#define LOOP_TIMER_HARD_CAP (1000000)

VOID LoopTimerStart(OUT LOOP_TIMER *Lt, IN UINTN Seconds)
{
    EFI_STATUS Status;

    if (Lt == NULL) {
        return;
    }

    Lt->Timer = NULL;
    Lt->HaveTimer = FALSE;
    Lt->Iterations = 0;
    Lt->MaxIterations = LOOP_TIMER_HARD_CAP;

    Status = gBS->CreateEvent(EVT_TIMER, 0, NULL, NULL, &Lt->Timer);
    if (!EFI_ERROR(Status) && Lt->Timer != NULL) {
        Status = gBS->SetTimer(Lt->Timer, TimerRelative,
                               MultU64x32((UINT64)Seconds, (UINT32)LOOP_TIMER_UNITS_PER_SECOND));
        if (!EFI_ERROR(Status)) {
            Lt->HaveTimer = TRUE;
        }
        else {
            gBS->CloseEvent(Lt->Timer);
            Lt->Timer = NULL;
        }
    }
}

BOOLEAN LoopTimerLive(IN OUT LOOP_TIMER *Lt)
{
    if (Lt == NULL) {
        return FALSE;
    }

    if (Lt->Iterations >= Lt->MaxIterations) {
        return FALSE;
    }
    Lt->Iterations++;

    if (Lt->HaveTimer) {

        if (!EFI_ERROR(gBS->CheckEvent(Lt->Timer))) {
            return FALSE;
        }
    }

    return TRUE;
}

VOID LoopTimerStop(IN OUT LOOP_TIMER *Lt)
{
    if (Lt == NULL || Lt->Timer == NULL) {
        return;
    }

    gBS->SetTimer(Lt->Timer, TimerCancel, 0);
    gBS->CloseEvent(Lt->Timer);
    Lt->Timer = NULL;
    Lt->HaveTimer = FALSE;
}
