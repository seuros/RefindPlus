// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#include "conn_core.h"

#define TRANSPARENT 0u

#define VISIBLE_ROWS CONN_MENU_PAGE_ROWS

const ConnTextTheme CONN_TXT_EFI = {
    .bg = CONN_RGB(0, 0, 0),
    .frame = CONN_AMBER,
    .text = CONN_RGB(0xB9, 0xC0, 0xCC),
    .dim = CONN_RGB(0x6B, 0x72, 0x80),
    .hi = CONN_AMBER,
    .name = "EFI",
    .accents = TRUE,
    .crt = FALSE,
};

const ConnTextTheme CONN_TXT_ATARI = {
    .bg = CONN_RGB(0x02, 0x12, 0x0A),
    .frame = CONN_RGB(0x33, 0xE0, 0x6B),
    .text = CONN_RGB(0x46, 0xFF, 0x80),
    .dim = CONN_RGB(0x1E, 0x9A, 0x50),
    .hi = CONN_RGB(0xA8, 0xFF, 0xC6),
    .name = "ATARI",
    .accents = FALSE,
    .crt = TRUE,
};

const ConnTextTheme CONN_TXT_PDP = {
    .bg = CONN_RGB(0x07, 0x04, 0x00),
    .frame = CONN_RGB(0xCC, 0x77, 0x00),
    .text = CONN_RGB(0xFF, 0xA5, 0x00),
    .dim = CONN_RGB(0x88, 0x50, 0x00),
    .hi = CONN_RGB(0xFF, 0xDD, 0x88),
    .name = "PDP",
    .accents = FALSE,
    .crt = TRUE,
};

const ConnTextTheme CONN_TXT_C64 = {
    .bg = CONN_RGB(0x00, 0x00, 0xAA),
    .frame = CONN_RGB(0x55, 0x55, 0xFF),
    .text = CONN_RGB(0x77, 0x77, 0xFF),
    .dim = CONN_RGB(0x33, 0x33, 0xCC),
    .hi = CONN_RGB(0xAA, 0xAA, 0xFF),
    .name = "C64",
    .accents = FALSE,
    .crt = FALSE,
};

const ConnTextTheme *const conn_themes[CONN_THEME_COUNT] = {
    &CONN_TXT_ATARI,
    &CONN_TXT_PDP,
    &CONN_TXT_C64,
};

#define BX_H 0x2500
#define BX_V 0x2502
#define BX_TL 0x250C
#define BX_TR 0x2510
#define BX_BL 0x2514
#define BX_BR 0x2518
#define BX_VR 0x251C
#define BX_VL 0x2524
#define BX_TD 0x252C
#define BX_TU 0x2534

#define RAIL_COL 59
#define RAIL_HDR_COL (RAIL_COL + 2)

static INT32 utf8_next(const char *p, UINT32 *cp) {
    unsigned char c = (unsigned char)p[0];
    if (c < 0x80) { *cp = c; return 1; }
    if ((c & 0xE0) == 0xC0) { *cp = ((c & 0x1F) << 6) | (p[1] & 0x3F); return 2; }
    if ((c & 0xF0) == 0xE0) {
        *cp = ((c & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F); return 3;
    }
    *cp = c; return 1;
}

static INTN ucs_len(const char *s) {
    INTN n = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++)
        if ((*p & 0xC0) != 0x80) n++;
    return n;
}

static VOID title_case(const char *src, char *dst, UINTN cap) {
    INTN prev_alpha = 0; UINTN j = 0;
    for (UINTN i = 0; src[i] && j + 1 < cap; i++) {
        unsigned char c = (unsigned char)src[i];
        INTN alpha = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
        if (alpha) dst[j++] = !prev_alpha ? (char)(c >= 'a' ? c - 32 : c)
                                          : (char)(c <= 'Z' ? c + 32 : c);
        else       dst[j++] = (char)c;
        prev_alpha = alpha;
    }
    dst[j] = '\0';
}

static VOID set_cell(ConnGrid *g, INTN col, INTN row, UINT32 ch,
                     ConnColor fg, ConnColor bg, BOOLEAN bold) {
    if (col < 0 || col >= CONN_COLS || row < 0 || row >= CONN_ROWS) return;
    ConnCell *c = &g->cell[row][col];
    c->ch = ch; c->fg = fg; c->bg = bg; c->bold = bold;

    c->efi_bg = (CONN_A(bg) == 0) ? 0 : conn_efi_bg(bg);
    c->efi_fg = (c->efi_bg == 0) ? conn_efi_fg(fg) : conn_efi_inverse_fg(c->efi_bg);
}

static VOID puts_at(ConnGrid *g, INTN col, INTN row, const char *s,
                    ConnColor fg, ConnColor bg, BOOLEAN bold) {
    INTN c = col;
    for (const char *p = s; *p; ) {
        UINT32 cp; p += utf8_next(p, &cp);
        set_cell(g, c++, row, cp, fg, bg, bold);
    }
}

static VOID puts_center(ConnGrid *g, INTN row, const char *s, ConnColor fg, ConnColor bg,
                        BOOLEAN bold)
{
    INTN col = (CONN_COLS - ucs_len(s)) / 2;
    if (col < 1) {
        col = 1;
    }
    puts_at(g, col, row, s, fg, bg, bold);
}

static VOID puts_clipped(ConnGrid *g, INTN col, INTN row, const char *s, INTN max_cols,
                         ConnColor fg, ConnColor bg, BOOLEAN bold)
{
    INTN c = col;
    INTN used = 0;
    for (const char *p = s; *p && used < max_cols;) {
        UINT32 cp;
        p += utf8_next(p, &cp);
        set_cell(g, c++, row, cp, fg, bg, bold);
        used++;
    }
}

VOID conn_grid_puts(ConnGrid *g, INTN col, INTN row, const char *s,
                    ConnColor fg, ConnColor bg, BOOLEAN bold) {
    puts_at(g, col, row, s, fg, bg, bold);
}

VOID conn_grid_set(ConnGrid *g, INTN col, INTN row, UINT32 cp,
                   ConnColor fg, ConnColor bg, BOOLEAN bold) {
    set_cell(g, col, row, cp, fg, bg, bold);
}

static VOID run_at(ConnGrid *g, INTN col, INTN row, INTN count, UINT32 cp,
                   ConnColor fg, BOOLEAN bold) {
    for (INTN i = 0; i < count; i++) set_cell(g, col + i, row, cp, fg, TRANSPARENT, bold);
}

static VOID put_index(ConnGrid *g, INTN row, INTN num, char mark,
                      ConnColor fg, ConnColor bg, BOOLEAN bold) {
    if (num >= 10) set_cell(g, 1, row, (UINT32)('0' + (num / 10) % 10), fg, bg, bold);
    set_cell(g, 2, row, (UINT32)('0' + num % 10), fg, bg, bold);
    if (mark != ' ') set_cell(g, 3, row, (UINT32)mark, fg, bg, bold);
}

static VOID put_entry(ConnGrid *g, INTN row, INTN num, const ConnEntry *e,
                      const ConnTextTheme *th, BOOLEAN selected) {
    char mark = e->locked ? '*' : ' ';
    char name[64], sub[64];
    title_case(e->name, name, sizeof name);

    title_case(e->sub, sub, RAIL_COL - 23 + 1);

    INTN part_col = (RAIL_COL - 1) - ucs_len(e->part);

    if (selected) {
        ConnColor selbg = th->accents ? e->accent : th->frame;
        ConnColor ink = conn_contrast(selbg);
        for (INTN c = 1; c <= RAIL_COL - 1; c++)
            set_cell(g, c, row, 0x20, ink, selbg, TRUE);
        put_index(g, row, num, mark, ink, selbg, TRUE);
        puts_at(g, 5, row, name, ink, selbg, TRUE);
        puts_at(g, 23, row, sub, ink, selbg, TRUE);
        puts_at(g, part_col, row, e->part, ink, selbg, TRUE);
    } else {
        ConnColor tc = th->text;
        puts_at(g, 5, row, name, tc, TRANSPARENT, FALSE);
        puts_at(g, 23, row, sub, tc, TRANSPARENT, FALSE);
        puts_at(g, part_col, row, e->part, tc, TRANSPARENT, FALSE);
        put_index(g, row, num, mark, th->accents ? e->accent : th->frame, TRANSPARENT, TRUE);
    }
}

VOID conn_grid_clear(ConnGrid *g, const ConnTextTheme *th) {
    for (INTN r = 0; r < CONN_ROWS; r++)
        for (INTN c = 0; c < CONN_COLS; c++)
            set_cell(g, c, r, 0x20, th->text, TRANSPARENT, FALSE);
}

VOID conn_grid_frame(ConnGrid *g, const ConnTextTheme *th) {
    const ConnColor fr = th->frame;
    run_at(g, 1, 0, CONN_COLS - 2, BX_H, fr, FALSE);
    run_at(g, 1, CONN_ROWS - 1, CONN_COLS - 2, BX_H, fr, FALSE);
    run_at(g, 1, 2, CONN_COLS - 2, BX_H, fr, FALSE);
    run_at(g, 1, CONN_ROWS - 3, CONN_COLS - 2, BX_H, fr, FALSE);
    for (INTN r = 1; r <= CONN_ROWS - 2; r++) {
        set_cell(g, 0, r, BX_V, fr, TRANSPARENT, FALSE);
        set_cell(g, CONN_COLS - 1, r, BX_V, fr, TRANSPARENT, FALSE);
    }
    set_cell(g, 0, 0, BX_TL, fr, TRANSPARENT, FALSE);
    set_cell(g, CONN_COLS - 1, 0, BX_TR, fr, TRANSPARENT, FALSE);
    set_cell(g, 0, CONN_ROWS - 1, BX_BL, fr, TRANSPARENT, FALSE);
    set_cell(g, CONN_COLS - 1, CONN_ROWS - 1, BX_BR, fr, TRANSPARENT, FALSE);
    set_cell(g, 0, 2, BX_VR, fr, TRANSPARENT, FALSE);
    set_cell(g, CONN_COLS - 1, 2, BX_VL, fr, TRANSPARENT, FALSE);
    set_cell(g, 0, CONN_ROWS - 3, BX_VR, fr, TRANSPARENT, FALSE);
    set_cell(g, CONN_COLS - 1, CONN_ROWS - 3, BX_VL, fr, TRANSPARENT, FALSE);
}

static UINT32 saver_mix(UINT32 x)
{
    x ^= x >> 16;
    x *= 0x7FEB352Du;
    x ^= x >> 15;
    x *= 0x846CA68Bu;
    x ^= x >> 16;
    return x;
}

static UINT32 saver_seed(const ConnEntryList *list, UINT32 frame)
{
    UINT32 s = 0x4D52444Eu ^ (frame * 0x9E3779B9u);
    if (list != NULL) {
        for (UINTN i = 0; i < 8 && list->rng_nonce[i] != '\0'; i++) {
            s = (s * 31u) + (UINT32)(unsigned char)list->rng_nonce[i];
        }
    }
    return s;
}

static INTN conn_menu_window_start(INTN count, UINTN selected)
{
    INTN start = 0;
    if (count > VISIBLE_ROWS) {
        start = (INTN)selected - VISIBLE_ROWS / 2;
        if (start < 0)
            start = 0;
        if (start > count - VISIBLE_ROWS)
            start = count - VISIBLE_ROWS;
    }
    return start;
}

INTN conn_menu_hit_index(const ConnEntryList *list, UINTN selected, INTN gcol, INTN grow)
{
    if (list == NULL || list->count == 0)
        return -1;

    if (gcol < 1 || gcol >= RAIL_COL)
        return -1;

    INTN count = (INTN)list->count;
    INTN start = conn_menu_window_start(count, selected);
    INTN shown = count - start;
    if (shown > VISIBLE_ROWS)
        shown = VISIBLE_ROWS;

    INTN i = grow - 4;
    if (i < 0 || i >= shown)
        return -1;
    return start + i;
}

static VOID conn_build_saver_grid(ConnGrid *g, const ConnEntryList *list, const ConnModel *model,
                                  const ConnTextTheme *th)
{
    static const char GLITCH[16] = {'#', '%', '@', '*',  '?', '+', '=', '~',
                                    '<', '>', '/', '\\', '|', '^', '&', '$'};
    const ConnUiState *st = &model->ui;
    const ConnColor fr = th->dim, tc = th->text;
    const UINT32 frame = st->saver_frame;
    char pulse[8] = "[   ]";
    char rng[16] = "LIVE UNAVAIL";
    UINT32 dots = (frame % 4u);

    INTN band = CONN_ROWS - 8;
    INTN lead = 4 + (INTN)(frame % (UINT32)band);

    for (UINT32 i = 0; i < dots; i++) {
        pulse[1 + i] = '.';
    }
    if (list != NULL && list->rng_nonce[0] != '\0') {
        const char *tag = list->rng_source[0] != '\0' ? list->rng_source : "RNG";
        UINTN i = 0, o = 0;

        for (; tag[i] != '\0' && i < 5 && o + 1 < sizeof(rng); i++) {
            rng[o++] = tag[i];
        }
        if (o + 1 < sizeof(rng)) {
            rng[o++] = ' ';
        }
        for (i = 0; i < 8 && list->rng_nonce[i] != '\0' && o + 1 < sizeof(rng); i++) {
            rng[o++] = list->rng_nonce[i];
        }
        rng[o] = '\0';
    }

    conn_grid_clear(g, th);
    conn_grid_frame(g, th);

    if (lead - 2 >= 3) {
        run_at(g, 2, lead - 2, CONN_COLS - 4, 183, fr, FALSE);
    }
    if (lead - 1 >= 3) {
        run_at(g, 2, lead - 1, CONN_COLS - 4, '-', tc, FALSE);
    }
    run_at(g, 2, lead, CONN_COLS - 4, BX_H, th->hi, TRUE);

    puts_at(g, 3, 1, "MERIDIAN", fr, TRANSPARENT, TRUE);
    puts_at(g, 12, 1, "DORMANT CRT", fr, TRANSPARENT, FALSE);
    conn_widget_clock(g, (ConnRect){CONN_COLS - 26, 1, 24, 1}, list != NULL ? list->clock : NULL,
                      th);

    puts_center(g, 5, "MERIDIAN NODE READY", (frame & 1u) ? th->hi : tc, TRANSPARENT, TRUE);
    puts_center(g, 7, "AWAITING SELECTION", th->hi, TRANSPARENT, TRUE);

    {
        UINT32 seed = saver_seed(list, frame);
        INTN gw = 30, gx = (CONN_COLS - gw) / 2;
        for (INTN i = 0; i < gw; i++) {
            UINT32 h = saver_mix(seed ^ ((UINT32)i * 0x85EBCA77u));
            if ((h & 7u) < 3u) {
                char ch = GLITCH[h & 15u];
                ConnColor col = (h & 0x10u) ? th->hi : fr;
                set_cell(g, gx + i, 9, (UINT32)(unsigned char)ch, col, TRANSPARENT,
                         (h & 0x20u) != 0);
            }
        }
    }

    puts_at(g, 6, 12, "NODE IDENTITY", th->hi, TRANSPARENT, TRUE);
    if (list != NULL && list->sys.count > 0) {
        INTN out = 0;
        for (UINTN i = 0; i < list->sys.count && out < 7; i++) {
            const char *ln = list->sys.line[i];
            BOOLEAN head;

            if (ln[0] == '\0') {
                continue;
            }
            head = (ln[0] != ' ');
            puts_clipped(g, 8, 14 + out, ln, 32, head ? th->hi : tc, TRANSPARENT, head);
            out++;
        }
    }
    else {
        puts_at(g, 8, 14, "sysinfo unavailable", fr, TRANSPARENT, FALSE);
    }

    puts_at(g, CONN_COLS - 16, 12, "LIVENESS", th->hi, TRANSPARENT, TRUE);
    puts_at(g, CONN_COLS - 16, 14, rng, tc, TRANSPARENT, TRUE);
    puts_at(g, CONN_COLS - 16, 16, pulse, fr, TRANSPARENT, TRUE);
    puts_center(g, CONN_ROWS - 2, "first local key wakes only", fr, TRANSPARENT, FALSE);
}

VOID conn_build_text_grid(ConnGrid *g, const ConnEntryList *list,
                          const ConnModel *model, const ConnTextTheme *th) {
    const ConnUiState *st = &model->ui;
    const ConnColor fr = th->frame, tc = th->text;

    if (conn_ui_in_saver(st)) {
        conn_build_saver_grid(g, list, model, th);
        return;
    }

    conn_grid_clear(g, th);
    conn_grid_frame(g, th);

    set_cell(g, RAIL_COL, 2, BX_TD, fr, TRANSPARENT, FALSE);
    for (INTN r = 3; r <= CONN_ROWS - 4; r++)
        set_cell(g, RAIL_COL, r, BX_V, fr, TRANSPARENT, FALSE);
    set_cell(g, RAIL_COL, CONN_ROWS - 3, BX_TU, fr, TRANSPARENT, FALSE);
    puts_at(g, RAIL_HDR_COL, 3, "SYSTEM", fr, TRANSPARENT, TRUE);

    {
        static const UINT32 spin[4] = {'|', '/', '-', '\\'};
        set_cell(g, CONN_COLS - 2, 3, spin[model->globe_frame & 3u], th->hi, TRANSPARENT, TRUE);
    }

    {
        const INTN body_top = 5, body_rows = (CONN_ROWS - 4) - 5 + 1;
        INTN cnt = (INTN)list->sys.count;
        INTN top = (INTN)model->rail_top;
        if (top > cnt - body_rows)
            top = cnt - body_rows;
        if (top < 0)
            top = 0;
        for (INTN i = 0; i < body_rows && top + i < cnt; i++) {
            const char *ln = list->sys.line[top + i];
            if (ln[0] == '\0')
                continue;
            BOOLEAN head = (ln[0] != ' ');
            puts_at(g, RAIL_HDR_COL, body_top + i, ln, head ? fr : tc, TRANSPARENT, head);
        }
        if (top > 0)
            set_cell(g, CONN_COLS - 2, 4, 0x2191, th->dim, TRANSPARENT, FALSE);
        if (top + body_rows < cnt)
            set_cell(g, CONN_COLS - 2, CONN_ROWS - 4, 0x2193, th->dim, TRANSPARENT, FALSE);
    }

    puts_at(g, 3, 1, "MERIDIAN", fr, TRANSPARENT, TRUE);
    puts_at(g, 12, 1, "BOOT MANAGER \xC2\xB7 CONN", tc, TRANSPARENT, FALSE);
    if (th->name != NULL)
        puts_at(g, 33, 1, th->name, th->dim, TRANSPARENT, FALSE);

    if (list->res[0] != '\0') {
        UINTN rl = 0;
        while (rl < sizeof(list->res) && list->res[rl] != '\0')
            rl++;

        if (rl < sizeof(list->res)) {
            puts_at(g, CONN_COLS - 27 - (INTN)rl, 1, list->res, th->dim, TRANSPARENT, FALSE);
        }
    }
    conn_widget_clock(g, (ConnRect){ CONN_COLS - 26, 1, 24, 1 }, list->clock, th);

    INTN count = (INTN)list->count;
    INTN start = conn_menu_window_start(count, st->selected);
    INTN shown = count - start;
    if (shown > VISIBLE_ROWS) shown = VISIBLE_ROWS;
    for (INTN i = 0; i < shown; i++) {
        INTN idx = start + i;
        put_entry(g, 4 + i, idx + 1, &list->entry[idx], th, (UINTN)idx == st->selected);
    }

    if (start > 0)
        set_cell(g, RAIL_COL - 1, 4, 0x2191, th->dim, TRANSPARENT, FALSE);
    if (start + VISIBLE_ROWS < count)
        set_cell(g, RAIL_COL - 1, 4 + VISIBLE_ROWS - 1, 0x2193, th->dim, TRANSPARENT, FALSE);
    if (count == 0)
        puts_at(g, 27, 11, "NO BOOTABLE VOLUMES FOUND", th->dim, TRANSPARENT, FALSE);

    puts_at(g, 3, CONN_ROWS - 2,
            "\xE2\x86\x91\xE2\x86\x93 select   ENTER boot   E options   "
            "\xE2\x86\x90\xE2\x86\x92 sys   T theme",
            th->dim, TRANSPARENT, FALSE);
    // Right-aligned to col 76, mirroring the hint row's left margin of 3. The
    // hint above ends at col 55, so anything starting before 56 clips it.
    if (st->status == CONN_UI_LAUNCH_FAILED) {
        puts_at(g, CONN_COLS - 16, CONN_ROWS - 2, "launch failed", th->hi, TRANSPARENT, TRUE);
    } else if (st->autoboot && !st->cancelled) {
        char rb[16];
        const char *p = "auto-boot 00:"; UINTN j = 0;
        for (; p[j]; j++) rb[j] = p[j];
        UINT32 s = st->countdown_s;
        rb[j++] = (char)('0' + (s / 10) % 10);
        rb[j++] = (char)('0' + s % 10);
        rb[j] = '\0';
        puts_at(g, CONN_COLS - 18, CONN_ROWS - 2, rb, th->hi, TRANSPARENT, TRUE);
    }
}
