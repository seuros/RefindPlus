// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#include "conn_core.h"
#include "efi_env.h"

static VOID zero_bytes(VOID *ptr, UINTN size)
{
    volatile UINT8 *p = (volatile UINT8 *)ptr;
    while (size-- > 0) {
        *p++ = 0;
    }
}

EFI_STATUS EFIAPI ConnUiRun(ConnFw *Fw) {
    return ConnUiRunThemed(Fw, &CONN_TXT_ATARI);
}

EFI_STATUS EFIAPI ConnUiRunThemed(ConnFw *Fw, const ConnTextTheme *theme) {
    UINTN w = conn_text_width();
    UINTN h = conn_text_height();

    EFI_GRAPHICS_OUTPUT_BLT_PIXEL *back = NULL;
    UINTN bytes = w * h * sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL);
    EFI_STATUS st = Fw->BS->AllocatePool(EfiBootServicesData, bytes, (VOID **)&back);
    if (EFI_ERROR(st)) return st;

    ConnEntryList list;
    ConnBootTarget targets[CONN_MAX_ENTRIES];
    zero_bytes(&list, sizeof list);
    zero_bytes(targets, sizeof targets);
    conn_scan(Fw, &list, targets);

    ConnModel model;
    conn_model_init(&model, &list);

    ConnSurface surf = { back, w, h };
    conn_render_to_surface(&surf, &list, &model, theme);

    if (Fw->Gop != NULL) {
        UINTN sw = Fw->Gop->Mode->Info->HorizontalResolution;
        UINTN sh = Fw->Gop->Mode->Info->VerticalResolution;
        UINTN bw = w < sw ? w : sw;
        UINTN bh = h < sh ? h : sh;
        st = Fw->Gop->Blt(Fw->Gop, back, EfiBltBufferToVideo, 0, 0, 0, 0, bw, bh,
                          w * sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL));
    }

    Fw->BS->FreePool(back);
    return st;
}
