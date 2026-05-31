// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Protocol/AbsolutePointer.h>
#include <Protocol/SimplePointer.h>
#include "pointer.h"

#define CONN_WHEEL_COUNTS_PER_DETENT 8

#define CONN_MOUSE_SPEED 4

STATIC INTN clamp_i(INTN v, INTN lo, INTN hi)
{
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}

STATIC VOID fill_sample(ConnPointer *p, BOOLEAN moved, BOOLEAN pressed, ConnPointerSample *out)
{
    out->wheel_dz = 0;
    if (p->wheelAcc >= CONN_WHEEL_COUNTS_PER_DETENT) {
        out->wheel_dz = 1;
        p->wheelAcc -= CONN_WHEEL_COUNTS_PER_DETENT;
    }
    else if (p->wheelAcc <= -CONN_WHEEL_COUNTS_PER_DETENT) {
        out->wheel_dz = -1;
        p->wheelAcc += CONN_WHEEL_COUNTS_PER_DETENT;
    }
    out->x = (INT32)p->x;
    out->y = (INT32)p->y;
    out->left_down = p->left;
    out->left_pressed = pressed;
    out->moved = moved;
    out->present = p->present;
}

VOID ConnPointerInit(ConnPointer *p, UINTN sw, UINTN sh)
{
    EFI_STATUS Status;

    if (p == NULL) {
        return;
    }
    p->Abs = NULL;
    p->Simp = NULL;
    p->x = (INTN)(sw / 2);
    p->y = (INTN)(sh / 2);
    p->absZ = 0;
    p->absZSeeded = FALSE;
    p->wheelAcc = 0;
    p->left = FALSE;
    p->present = FALSE;

    if (gST->ConsoleInHandle == NULL ||
        EFI_ERROR(gBS->HandleProtocol(gST->ConsoleInHandle, &gEfiAbsolutePointerProtocolGuid,
                                      (VOID **)&p->Abs))) {
        p->Abs = NULL;
        gBS->LocateProtocol(&gEfiAbsolutePointerProtocolGuid, NULL, (VOID **)&p->Abs);
    }

    if (gST->ConsoleInHandle == NULL ||
        EFI_ERROR(gBS->HandleProtocol(gST->ConsoleInHandle, &gEfiSimplePointerProtocolGuid,
                                      (VOID **)&p->Simp))) {
        p->Simp = NULL;
        gBS->LocateProtocol(&gEfiSimplePointerProtocolGuid, NULL, (VOID **)&p->Simp);
    }
    if (p->Simp != NULL) {
        Status = p->Simp->Reset(p->Simp, FALSE);
        if (EFI_ERROR(Status)) {

            p->Simp = NULL;
        }
    }
}

VOID ConnPointerPollAbsolute(ConnPointer *p, UINTN sw, UINTN sh, ConnPointerSample *out)
{
    EFI_ABSOLUTE_POINTER_STATE State;
    UINT64 RangeX, RangeY;
    INTN PrevX, PrevY;
    BOOLEAN PrevLeft, moved = FALSE, pressed = FALSE;

    if (p == NULL || out == NULL) {
        return;
    }

    if (p->Abs == NULL || p->Abs->Mode == NULL || sw == 0 || sh == 0 ||
        p->Abs->Mode->AbsoluteMaxX <= p->Abs->Mode->AbsoluteMinX ||
        p->Abs->Mode->AbsoluteMaxY <= p->Abs->Mode->AbsoluteMinY) {
        fill_sample(p, FALSE, FALSE, out);
        return;
    }

    RangeX = (UINT64)(p->Abs->Mode->AbsoluteMaxX - p->Abs->Mode->AbsoluteMinX);
    RangeY = (UINT64)(p->Abs->Mode->AbsoluteMaxY - p->Abs->Mode->AbsoluteMinY);
    PrevX = p->x;
    PrevY = p->y;
    PrevLeft = p->left;

    if (!EFI_ERROR(p->Abs->GetState(p->Abs, &State))) {
        UINT64 rx = (State.CurrentX >= p->Abs->Mode->AbsoluteMinX)
                        ? (State.CurrentX - p->Abs->Mode->AbsoluteMinX)
                        : 0;
        UINT64 ry = (State.CurrentY >= p->Abs->Mode->AbsoluteMinY)
                        ? (State.CurrentY - p->Abs->Mode->AbsoluteMinY)
                        : 0;

        p->x = clamp_i((INTN)((rx * (UINT64)sw) / RangeX), 0, (INTN)sw - 1);
        p->y = clamp_i((INTN)((ry * (UINT64)sh) / RangeY), 0, (INTN)sh - 1);
        p->left = (State.ActiveButtons & EFI_ABSP_TouchActive) != 0;

        INT64 rawDz = p->absZSeeded ? ((INT64)State.CurrentZ - (INT64)p->absZ) : 0;
        p->wheelAcc += (INT32)rawDz;
        p->absZ = State.CurrentZ;
        p->absZSeeded = TRUE;

        moved = (p->x != PrevX) || (p->y != PrevY) || (p->left != PrevLeft);
        pressed = p->left && !PrevLeft;
        if (moved || rawDz != 0) {
            p->present = TRUE;
        }
    }
    fill_sample(p, moved, pressed, out);
}

VOID ConnPointerPollSimple(ConnPointer *p, UINTN sw, UINTN sh, ConnPointerSample *out)
{
    EFI_SIMPLE_POINTER_STATE State;
    INTN PrevX, PrevY;
    BOOLEAN PrevLeft, got = FALSE, moved = FALSE, pressed = FALSE;
    INT64 rawZ = 0;

    if (p == NULL || out == NULL) {
        return;
    }
    if (p->Simp == NULL || p->Simp->Mode == NULL || sw == 0 || sh == 0) {
        fill_sample(p, FALSE, FALSE, out);
        return;
    }

    PrevX = p->x;
    PrevY = p->y;
    PrevLeft = p->left;

    while (!EFI_ERROR(p->Simp->GetState(p->Simp, &State))) {
        INT64 resX = (INT64)p->Simp->Mode->ResolutionX;
        INT64 resY = (INT64)p->Simp->Mode->ResolutionY;
        if (resX <= 0)
            resX = 1;
        if (resY <= 0)
            resY = 1;

        got = TRUE;
        p->x += (INTN)(((INT64)State.RelativeMovementX * CONN_MOUSE_SPEED) / resX);
        p->y += (INTN)(((INT64)State.RelativeMovementY * CONN_MOUSE_SPEED) / resY);
        rawZ += (INT64)State.RelativeMovementZ;
        p->left = State.LeftButton;
    }
    p->wheelAcc += (INT32)rawZ;

    if (got) {
        p->x = clamp_i(p->x, 0, (INTN)sw - 1);
        p->y = clamp_i(p->y, 0, (INTN)sh - 1);
        moved = (p->x != PrevX) || (p->y != PrevY) || (p->left != PrevLeft);
        pressed = p->left && !PrevLeft;

        if (moved || rawZ != 0) {
            p->present = TRUE;
        }
    }
    fill_sample(p, moved, pressed, out);
}
