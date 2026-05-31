// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#ifndef CONN_EFI_ENV_H
#define CONN_EFI_ENV_H

#include <Uefi.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/SimpleTextOut.h>
#include <Protocol/SimpleTextIn.h>
#include "conn_core.h"

typedef struct {
    EFI_HANDLE                       ImageHandle;
    EFI_SYSTEM_TABLE                *ST;
    EFI_BOOT_SERVICES               *BS;
    EFI_GRAPHICS_OUTPUT_PROTOCOL    *Gop;
    EFI_SIMPLE_TEXT_INPUT_PROTOCOL  *ConIn;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut;
} ConnFw;

EFI_STATUS EFIAPI ConnUiRun(ConnFw *Fw);
// Same, with an explicit theme. ConnUiRun() is this with the default.
EFI_STATUS EFIAPI ConnUiRunThemed(ConnFw *Fw, const ConnTextTheme *theme);

typedef struct {
    EFI_HANDLE volume;
    CHAR16     path[128];
    EFI_GUID   part_guid;
    BOOLEAN    has_guid;
} ConnBootTarget;

EFI_STATUS conn_scan(ConnFw *Fw, ConnEntryList *out, ConnBootTarget *targets);

VOID conn_text_to_conout(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut, const ConnGrid *g);

EFI_STATUS conn_launch(ConnFw *Fw, const ConnBootTarget *target);

STATIC_ASSERT(sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL) == 4, "BLT pixel must be 4 bytes");
STATIC_ASSERT(OFFSET_OF(EFI_GRAPHICS_OUTPUT_BLT_PIXEL, Blue)     == 0, "BLT Blue@0");
STATIC_ASSERT(OFFSET_OF(EFI_GRAPHICS_OUTPUT_BLT_PIXEL, Green)    == 1, "BLT Green@1");
STATIC_ASSERT(OFFSET_OF(EFI_GRAPHICS_OUTPUT_BLT_PIXEL, Red)      == 2, "BLT Red@2");
STATIC_ASSERT(OFFSET_OF(EFI_GRAPHICS_OUTPUT_BLT_PIXEL, Reserved) == 3, "BLT Reserved@3");
STATIC_ASSERT(sizeof(UINTN) == sizeof(VOID *), "UINTN must be pointer-width");

#endif
