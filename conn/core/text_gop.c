// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#include "conn_core.h"

#define CH 27

VOID conn_text_to_gop(ConnSurface *s, const ConnGrid *g, const ConnTextTheme *th) {
    conn_clear(s, th->bg);

    for (INTN row = 0; row < CONN_ROWS; row++) {
        INTN y = conn_row_y(row);
        for (INTN col = 0; col < CONN_COLS; col++) {
            const ConnCell *c = &g->cell[row][col];
            INTN x  = conn_col_x(col);
            INTN xw = conn_col_x(col + 1) - x;
            if (CONN_A(c->bg) != 0)
                conn_fill_rect(s, x, y, xw, CH, c->bg);
            if (c->ch != 0x20)
                conn_draw_glyph(s, x, y, c->ch, c->bold, c->fg);
        }
    }

    if (th->crt) {
        conn_scanlines(s, 76);
        conn_vignette(s, 153);
    }
}

VOID conn_render_to_surface(ConnSurface *s, const ConnEntryList *list,
                            const ConnModel *model, const ConnTextTheme *th) {
    ConnGrid grid;
    conn_build_text_grid(&grid, list, model, th);
    conn_text_to_gop(s, &grid, th);
}
