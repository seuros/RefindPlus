// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#include "conn_core.h"

VOID conn_widget_clock(ConnGrid *g, ConnRect area, const char *clock,
                       const ConnTextTheme *theme) {
    const char *text = (clock != NULL && clock[0] != '\0')
                     ? clock
                     : "09:47   TUE 02 JUN 2026";

    (void) area.w;
    (void) area.h;

    conn_grid_puts(g, area.col, area.row, text, theme->text, CONN_TRANSPARENT, FALSE);
}
