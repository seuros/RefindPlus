// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#include "conn_core.h"
#include "efi_env.h"

#define ATTR(c) EFI_TEXT_ATTR((c)->efi_fg, (c)->efi_bg)

VOID conn_text_to_conout(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut, const ConnGrid *g) {
    if (ConOut == NULL) return;

    CHAR16 probe[2] = { 0x00B7, 0 };
    BOOLEAN middot_ok = !EFI_ERROR(ConOut->TestString(ConOut, probe));

    for (INTN row = 0; row < CONN_ROWS; row++) {

        INTN row_cols = (row == CONN_ROWS - 1) ? CONN_COLS - 1 : CONN_COLS;
        INTN col = 0;
        while (col < row_cols) {
            UINT8 attr = ATTR(&g->cell[row][col]);
            CHAR16 run[CONN_COLS + 1];
            INTN n = 0, c = col;
            for (; c < row_cols && ATTR(&g->cell[row][c]) == attr; c++) {
                UINT32 cp = g->cell[row][c].ch;
                run[n++] = (cp == 0x00B7 && !middot_ok) ? L'.' : (CHAR16)cp;
            }
            run[n] = 0;
            ConOut->SetAttribute(ConOut, attr);
            ConOut->SetCursorPosition(ConOut, (UINTN)col, (UINTN)row);
            ConOut->OutputString(ConOut, run);
            col = c;
        }
    }
}
