// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#ifndef CONN_EFI_POINTER_H
#define CONN_EFI_POINTER_H

#include <Uefi.h>
#include <Protocol/AbsolutePointer.h>
#include <Protocol/SimplePointer.h>

typedef struct
{
    INT32 x;
    INT32 y;
    BOOLEAN left_down;
    BOOLEAN left_pressed;
    INT32 wheel_dz;
    BOOLEAN moved;
    BOOLEAN present;
} ConnPointerSample;

typedef struct
{
    EFI_ABSOLUTE_POINTER_PROTOCOL *Abs;
    EFI_SIMPLE_POINTER_PROTOCOL *Simp;
    INTN x, y;
    UINT64 absZ;
    BOOLEAN absZSeeded;
    INT32 wheelAcc;
    BOOLEAN left;
    BOOLEAN present;
} ConnPointer;

VOID ConnPointerInit(ConnPointer *p, UINTN sw, UINTN sh);

VOID ConnPointerPollAbsolute(ConnPointer *p, UINTN sw, UINTN sh, ConnPointerSample *out);

VOID ConnPointerPollSimple(ConnPointer *p, UINTN sw, UINTN sh, ConnPointerSample *out);

#endif
