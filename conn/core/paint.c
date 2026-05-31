// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#include "conn_core.h"
#include "font_atlas.h"

#define PAD 24
#define CH  27

#define CW_NUM 66
#define CW_DEN 5

INTN  conn_col_x(INTN col) { return (PAD * CW_DEN + col * CW_NUM + 2) / CW_DEN; }
INTN  conn_row_y(INTN row) { return PAD + row * CH; }
UINTN conn_text_width(VOID)  { return (UINTN)(conn_col_x(CONN_COLS) + PAD); }
UINTN conn_text_height(VOID) { return (UINTN)(PAD + CONN_ROWS * CH + PAD); }

static UINT32 isqrt32(UINT64 v) {
    UINT64 x = v, r = 0, b = (UINT64)1 << 62;
    while (b > x) b >>= 2;
    while (b) {
        if (x >= r + b) { x -= r + b; r = (r >> 1) + b; }
        else r >>= 1;
        b >>= 2;
    }
    return (UINT32)r;
}

static inline VOID put_rgb(EFI_GRAPHICS_OUTPUT_BLT_PIXEL *p,
                           UINT32 r, UINT32 g, UINT32 b) {
    p->Red = (UINT8)r; p->Green = (UINT8)g; p->Blue = (UINT8)b; p->Reserved = 0;
}

static inline VOID blend_px(EFI_GRAPHICS_OUTPUT_BLT_PIXEL *d,
                            ConnColor src, UINT32 cov) {
    UINT32 a = (CONN_A(src) * cov) / 255u;
    if (a == 0) return;
    UINT32 ia = 255u - a;
    put_rgb(d,
            (CONN_R(src) * a + d->Red   * ia) / 255u,
            (CONN_G(src) * a + d->Green * ia) / 255u,
            (CONN_B(src) * a + d->Blue  * ia) / 255u);
}

VOID conn_clear(ConnSurface *s, ConnColor c) {
    UINTN n = s->w * s->h;
    for (UINTN i = 0; i < n; i++) put_rgb(&s->px[i], CONN_R(c), CONN_G(c), CONN_B(c));
}

VOID conn_fill_rect(ConnSurface *s, INTN x, INTN y, INTN w, INTN h, ConnColor c) {
    INTN x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    INTN x1 = x + w, y1 = y + h;
    if (x1 > (INTN)s->w) x1 = (INTN)s->w;
    if (y1 > (INTN)s->h) y1 = (INTN)s->h;
    UINT32 a = CONN_A(c);
    for (INTN yy = y0; yy < y1; yy++) {
        EFI_GRAPHICS_OUTPUT_BLT_PIXEL *row = s->px + (UINTN)yy * s->w;
        for (INTN xx = x0; xx < x1; xx++) {
            if (a == 0xFF) put_rgb(&row[xx], CONN_R(c), CONN_G(c), CONN_B(c));
            else           blend_px(&row[xx], c, 255);
        }
    }
}

static INTN glyph_index(UINT32 cp) {
    if (cp == 0x20) return -1;
    for (INTN i = 0; i < CONN_GLYPH_COUNT; i++)
        if (conn_glyph_cp[i] == cp) return i;
    return -1;
}

VOID conn_draw_glyph(ConnSurface *s, INTN x, INTN y, UINT32 cp,
                     BOOLEAN bold, ConnColor ink) {
    INTN gi = glyph_index(cp);
    if (gi < 0) return;
    const unsigned char *cell = conn_glyph_alpha[bold ? 1 : 0][gi];
    for (INTN gy = 0; gy < CONN_CELL_H; gy++) {
        INTN py = y + gy;
        if (py < 0 || py >= (INTN)s->h) continue;
        EFI_GRAPHICS_OUTPUT_BLT_PIXEL *row = s->px + (UINTN)py * s->w;
        const unsigned char *src = cell + (UINTN)gy * CONN_CELL_W;
        for (INTN gx = 0; gx < CONN_CELL_W; gx++) {
            UINT32 cov = src[gx];
            if (!cov) continue;
            INTN px = x + gx;
            if (px < 0 || px >= (INTN)s->w) continue;
            blend_px(&row[px], ink, cov);
        }
    }
}

VOID conn_scanlines(ConnSurface *s, UINT32 opacity_255) {
    ConnColor band = CONN_RGBA(0, 0, 0, opacity_255);
    for (INTN y = 0; y < (INTN)s->h; y += 4)
        conn_fill_rect(s, 0, y, (INTN)s->w, 2, band);
}

VOID conn_vignette(ConnSurface *s, UINT32 max_opacity_255) {
    INTN cx = (INTN)s->w / 2, cy = (INTN)s->h / 2;
    INTN rmax = cx > cy ? cx : cy;
    INTN r0 = (rmax * 58) / 100;
    INTN span = rmax - r0;
    if (span <= 0) return;
    for (INTN y = 0; y < (INTN)s->h; y++) {
        EFI_GRAPHICS_OUTPUT_BLT_PIXEL *row = s->px + (UINTN)y * s->w;
        for (INTN x = 0; x < (INTN)s->w; x++) {
            INTN dx = x - cx, dy = y - cy;
            INTN r = (INTN)isqrt32((UINT64)(dx * dx) + (UINT64)(dy * dy));
            if (r <= r0) continue;
            INTN t = r - r0; if (t > span) t = span;
            UINT32 a = (UINT32)((t * (INTN)max_opacity_255) / span);
            blend_px(&row[x], CONN_RGBA(0, 0, 0, a), 255);
        }
    }
}

enum { E_BLACK=0, E_BLUE=1, E_GREEN=2, E_CYAN=3, E_RED=4, E_MAGENTA=5,
       E_BROWN=6, E_LGRAY=7, E_DGRAY=8, E_LBLUE=9, E_LGREEN=10, E_LCYAN=11,
       E_LRED=12, E_LMAGENTA=13, E_YELLOW=14, E_WHITE=15 };

UINT8 conn_efi_fg(ConnColor c) {
    switch (c & 0x00FFFFFFu) {
    case 0x000000: return E_BLACK;   case 0x0A0A0A: return E_BLACK;
    case 0xFFFFFF: return E_WHITE;
    case 0xFF9933: return E_YELLOW;
    case 0xB9C0CC: return E_LGRAY;
    case 0x6B7280: return E_DGRAY;
    case 0x99CCEE: return E_LCYAN;
    case 0x7788DD: return E_LBLUE;
    case 0xCC99CC: return E_LMAGENTA;
    case 0xCC6677: return E_LRED;
    case 0xFFCC66: return E_YELLOW;
    case 0xFFCC99: return E_YELLOW;
    case 0xFF8866: return E_LRED;
    case 0x99DDBB: return E_LGREEN;
    case 0xDD8899: return E_LMAGENTA;
    case 0x66CCCC: return E_LCYAN;
    case 0xCC3333:
        return E_LRED;
    case 0x4488CC:
        return E_LBLUE;
    case 0xDDBB33:
        return E_YELLOW;
    default:       return E_LGRAY;
    }
}

UINT8 conn_efi_bg(ConnColor c) {
    switch (c & 0x00FFFFFFu) {
    case 0x000000: return E_BLACK;
    case 0xFF9933: return E_BROWN;
    case 0x99CCEE: return E_CYAN;
    case 0x7788DD: return E_BLUE;
    case 0xCC99CC: return E_MAGENTA;
    case 0xCC6677: return E_RED;
    case 0xFFCC66: return E_BROWN;
    case 0xFFCC99: return E_BROWN;
    case 0xFF8866: return E_RED;
    case 0x99DDBB: return E_GREEN;
    case 0xDD8899: return E_MAGENTA;
    case 0x66CCCC: return E_CYAN;
    case 0xCC3333:
        return E_RED;
    case 0x4488CC:
        return E_BLUE;
    case 0xDDBB33:
        return E_BROWN;
    default:       return E_BLACK;
    }
}

UINT8 conn_efi_inverse_fg(UINT8 bg_index) {
    return bg_index == E_LGRAY ? E_BLACK : E_WHITE;
}

ConnColor conn_contrast(ConnColor fill) {

    UINT32 lum = 299u * CONN_R(fill) + 587u * CONN_G(fill) + 114u * CONN_B(fill);
    return lum > 150000u ? CONN_RGB(0x0A, 0x0A, 0x0A) : CONN_RGB(0xFF, 0xFF, 0xFF);
}
