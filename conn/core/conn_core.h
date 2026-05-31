// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#ifndef CONN_CORE_H
#define CONN_CORE_H

#include <Uefi.h>
#include <Protocol/GraphicsOutput.h>
#include "state_machine.h"

#define STATE_MACHINE_DEF_HEADER                                                                   \
    .magic = STATE_MACHINE_MAGIC, .abi_epoch = STATE_MACHINE_ABI_EPOCH,                            \
    .abi_revision = STATE_MACHINE_ABI_REVISION, .struct_size = sizeof(state_machine_def_t),        \
    .flags = 0, .spec_version = STATE_MACHINE_SPEC_VERSION, ._reserved = 0

typedef UINT32 ConnColor;
#define CONN_RGB(r, g, b)                                                                          \
    ((ConnColor)(0xFF000000u | ((UINT32)(r) << 16) | ((UINT32)(g) << 8) | (UINT32)(b)))
#define CONN_RGBA(r, g, b, a)                                                                      \
    ((ConnColor)(((UINT32)(a) << 24) | ((UINT32)(r) << 16) | ((UINT32)(g) << 8) | (UINT32)(b)))
#define CONN_A(c) (((c) >> 24) & 0xFF)
#define CONN_R(c) (((c) >> 16) & 0xFF)
#define CONN_G(c) (((c) >> 8) & 0xFF)
#define CONN_B(c) ((c) & 0xFF)

typedef struct
{
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL *px;
    UINTN w;
    UINTN h;
} ConnSurface;

static inline UINTN conn_cp_bounded(char *dst, const char *src, UINTN cap)
{
    UINTN i = 0;
    if (cap == 0)
        return 0;
    if (src != NULL)
        for (; src[i] != '\0' && i + 1 < cap; i++)
            dst[i] = src[i];
    dst[i] = '\0';
    return i;
}

VOID conn_clear(ConnSurface *s, ConnColor c);
VOID conn_fill_rect(ConnSurface *s, INTN x, INTN y, INTN w, INTN h, ConnColor c);
VOID conn_draw_glyph(ConnSurface *s, INTN x, INTN y, UINT32 cp, BOOLEAN bold, ConnColor ink);
VOID conn_scanlines(ConnSurface *s, UINT32 opacity_255);
VOID conn_vignette(ConnSurface *s, UINT32 max_opacity_255);
ConnColor conn_contrast(ConnColor fill);

UINT8 conn_efi_fg(ConnColor c);
UINT8 conn_efi_bg(ConnColor c);

UINT8 conn_efi_inverse_fg(UINT8 bg_index);

typedef struct
{
    ConnColor bg, frame, text, dim, hi;
    const char *name;
    BOOLEAN accents;
    BOOLEAN crt;
} ConnTextTheme;

extern const ConnTextTheme CONN_TXT_EFI;
extern const ConnTextTheme CONN_TXT_ATARI;
extern const ConnTextTheme CONN_TXT_PDP;
extern const ConnTextTheme CONN_TXT_C64;

#define CONN_THEME_COUNT 3
extern const ConnTextTheme *const conn_themes[CONN_THEME_COUNT];

#define CONN_AMBER CONN_RGB(0xFF, 0x99, 0x33)
#define CONN_GOLD CONN_RGB(0xFF, 0xCC, 0x66)
#define CONN_SALMON CONN_RGB(0xCC, 0x66, 0x77)
#define CONN_LAV CONN_RGB(0xCC, 0x99, 0xCC)
#define CONN_SKY CONN_RGB(0x77, 0x88, 0xDD)
#define CONN_ICE CONN_RGB(0x99, 0xCC, 0xEE)
#define CONN_PEACH CONN_RGB(0xFF, 0xCC, 0x99)
#define CONN_CORAL CONN_RGB(0xFF, 0x88, 0x66)
#define CONN_MINT CONN_RGB(0x99, 0xDD, 0xBB)
#define CONN_ROSE CONN_RGB(0xDD, 0x88, 0x99)
#define CONN_TEAL CONN_RGB(0x66, 0xCC, 0xCC)
#define CONN_RED CONN_RGB(0xCC, 0x33, 0x33)
#define CONN_COBALT CONN_RGB(0x44, 0x88, 0xCC)
#define CONN_HONEY CONN_RGB(0xDD, 0xBB, 0x33)

#define CONN_OMA CONN_RGB(0x9E, 0xCE, 0x6A)

#define CONN_MAX_ENTRIES 24

typedef struct
{
    char name[32];
    char sub[48];
    char part[12];
    char loader[64];
    ConnColor accent;
    BOOLEAN locked;
} ConnEntry;

#define CONN_SYS_LINES 48
#define CONN_SYS_COLS 20
typedef struct
{
    char line[CONN_SYS_LINES][CONN_SYS_COLS];
    UINTN count;
} ConnSysInfo;

typedef struct
{
    ConnEntry entry[CONN_MAX_ENTRIES];
    UINTN count;
    UINTN default_index;
    BOOLEAN allow_autoboot;
    char clock[24];
    char res[12];
    char rng_nonce[9];
    char rng_source[6];
    ConnSysInfo sys;
} ConnEntryList;

#define CONN_AUTOBOOT_SECONDS 5

typedef enum
{
    CONN_UI_OK,
    CONN_UI_EMPTY,
    CONN_UI_LAUNCH_FAILED
} ConnUiStatus;

typedef enum
{
    CONN_WAKE_DISMISS = 0,
    CONN_WAKE_SELECT,
    CONN_WAKE_LAUNCH,
} ConnWakeIntent;

typedef struct
{
    state_machine_t machine;
    UINTN selected;
    UINT32 countdown_s;
    UINT32 idle_s;
    UINT32 saver_frame;
    UINT32 screensaver_s;
    BOOLEAN autoboot;
    BOOLEAN cancelled;
    BOOLEAN saver_active;
    ConnUiStatus status;
} ConnUiState;

typedef enum
{
    CONN_KEY_NONE,
    CONN_KEY_UP,
    CONN_KEY_DOWN,
    CONN_KEY_PAGE_UP,
    CONN_KEY_PAGE_DOWN,
    CONN_KEY_ENTER,
    CONN_KEY_DIGIT,
    CONN_KEY_OPTIONS,

    CONN_KEY_LEFT,
    CONN_KEY_RIGHT,
    CONN_KEY_HOME,
    CONN_KEY_END,
    CONN_KEY_BACKSPACE,
    CONN_KEY_DELETE,
    CONN_KEY_TEXT,
    CONN_KEY_ESCAPE,
    CONN_KEY_OTHER
} ConnKey;

typedef enum
{
    CONN_ACT_NONE,
    CONN_ACT_REDRAW,
    CONN_ACT_LAUNCH,
    CONN_ACT_OPTIONS,
} ConnAction;
typedef ConnAction ConnCommand;

VOID conn_ui_init(ConnUiState *st, const ConnEntryList *list);
ConnAction conn_ui_on_key(ConnUiState *st, ConnKey key, INTN digit, const ConnEntryList *list);
ConnAction conn_ui_on_tick(ConnUiState *st, const ConnEntryList *list);
ConnAction conn_ui_on_remote(ConnUiState *st, ConnWakeIntent wake, UINTN wake_index,
                             const ConnEntryList *list);
VOID conn_ui_set_screensaver(ConnUiState *st, UINT32 seconds);
static inline BOOLEAN conn_ui_in_saver(const ConnUiState *st)
{
    return st != NULL && st->saver_active;
}

typedef struct
{
    ConnUiState ui;
    UINTN rail_top;
    UINT32 globe_frame;

} ConnModel;

typedef enum
{
    CONN_EVENT_INIT = 0,
    CONN_EVENT_KEY,
    CONN_EVENT_TICK,
    CONN_EVENT_REMOTE,
    CONN_EVENT_LAUNCH_RETURNED,
    CONN_EVENT_POINT,
} ConnEventType;

typedef struct
{
    ConnEventType type;
    ConnKey key;
    INTN digit;
    UINT32 ch;
    ConnWakeIntent wake;
    UINTN wake_index;
    EFI_STATUS launch_status;
    UINTN selected_hint;
} ConnEvent;

ConnEvent conn_event_init(VOID);
ConnEvent conn_event_key(ConnKey key, INTN digit);
ConnEvent conn_event_tick(VOID);
ConnEvent conn_event_remote(ConnWakeIntent wake, UINTN wake_index);
ConnEvent conn_event_launch_returned(EFI_STATUS launch_status, UINTN selected_hint);

ConnEvent conn_event_point(UINTN row, BOOLEAN click);
VOID conn_model_init(ConnModel *model, const ConnEntryList *list);
ConnCommand conn_update(ConnModel *model, const ConnEntryList *list, ConnEvent event);

#define CONN_COLS 80
#define CONN_ROWS 25

#define CONN_MENU_PAGE_ROWS (CONN_ROWS - 7)

static inline UINTN conn_page_step(UINTN selected, UINTN count, UINTN page, BOOLEAN forward)
{
    if (count == 0) {
        return 0;
    }
    if (forward) {
        return (selected + page < count) ? selected + page : count - 1;
    }
    return (selected >= page) ? selected - page : 0;
}

typedef struct
{
    UINT32 ch;
    ConnColor fg;
    ConnColor bg;
    BOOLEAN bold;
    UINT8 efi_fg;
    UINT8 efi_bg;
} ConnCell;

typedef struct
{
    ConnCell cell[CONN_ROWS][CONN_COLS];
} ConnGrid;

VOID conn_build_text_grid(ConnGrid *g, const ConnEntryList *list, const ConnModel *model,
                          const ConnTextTheme *theme);
VOID conn_text_to_gop(ConnSurface *s, const ConnGrid *g, const ConnTextTheme *theme);

INTN conn_menu_hit_index(const ConnEntryList *list, UINTN selected, INTN gcol, INTN grow);

VOID conn_render_to_surface(ConnSurface *s, const ConnEntryList *list, const ConnModel *model,
                            const ConnTextTheme *theme);

#define CONN_TRANSPARENT ((ConnColor)0u)

typedef struct
{
    INTN col, row, w, h;
} ConnRect;

VOID conn_grid_puts(ConnGrid *g, INTN col, INTN row, const char *s, ConnColor fg, ConnColor bg,
                    BOOLEAN bold);
VOID conn_grid_set(ConnGrid *g, INTN col, INTN row, UINT32 cp, ConnColor fg, ConnColor bg,
                   BOOLEAN bold);
VOID conn_grid_clear(ConnGrid *g, const ConnTextTheme *theme);
VOID conn_grid_frame(ConnGrid *g, const ConnTextTheme *theme);

VOID conn_widget_clock(ConnGrid *g, ConnRect area, const char *clock, const ConnTextTheme *theme);

#define CONN_EDIT_MAX 1024

typedef enum
{
    CONN_EDIT_ACTIVE = 0,
    CONN_EDIT_COMMITTED,
    CONN_EDIT_CANCELLED,
} ConnEditStatus;

typedef struct
{
    char title[32];
    char loader[64];
    char volume[48];
    char buf[CONN_EDIT_MAX];
    UINTN len;
    UINTN cursor;
    UINTN first;
    BOOLEAN cursor_on;
    ConnEditStatus status;
    state_machine_t machine;
} ConnLineEditModel;

VOID conn_line_edit_init(ConnLineEditModel *m, const char *initial, const char *title,
                         const char *loader, const char *volume);

ConnCommand conn_line_edit_update(ConnLineEditModel *m, ConnEvent ev);
VOID conn_line_edit_render(ConnGrid *g, const ConnLineEditModel *m, const ConnTextTheme *theme);

#define CONN_LIST_MAX 32
#define CONN_LIST_COLS 72
#define CONN_LIST_INFO 14

typedef enum
{
    CONN_LIST_ACTIVE = 0,
    CONN_LIST_CHOSEN,
    CONN_LIST_CANCELLED,
} ConnListStatus;

typedef struct
{
    char title[48];
    char info[CONN_LIST_INFO][CONN_LIST_COLS];
    UINTN info_count;
    char item[CONN_LIST_MAX][CONN_LIST_COLS];
    UINTN count;
    UINTN selected;
    UINTN first;
    ConnListStatus status;
    state_machine_t machine;
} ConnList;

VOID conn_list_init(ConnList *l, const char *title);
VOID conn_list_add_info(ConnList *l, const char *line);
BOOLEAN conn_list_add(ConnList *l, const char *item);
VOID conn_list_set_default(ConnList *l, UINTN idx);

ConnCommand conn_list_update(ConnList *l, ConnEvent ev);
VOID conn_list_render(ConnGrid *g, const ConnList *l, const ConnTextTheme *theme);

#define CONN_PANEL_BODY 64
#define CONN_PANEL_ACTS 6
#define CONN_PANEL_COLS 72
#define CONN_PANEL_ALBL 28

typedef enum
{
    CONN_PANEL_ACTIVE = 0,
    CONN_PANEL_CHOSEN,
    CONN_PANEL_CANCELLED,
} ConnPanelStatus;

typedef struct
{
    char title[48];
    char body[CONN_PANEL_BODY][CONN_PANEL_COLS];
    UINTN body_count;
    UINTN body_first;
    char act[CONN_PANEL_ACTS][CONN_PANEL_ALBL];
    UINTN act_count;
    UINTN selected;
    ConnPanelStatus status;
    state_machine_t machine;
} ConnPanel;

VOID conn_panel_init(ConnPanel *p, const char *title);
VOID conn_panel_add_body(ConnPanel *p, const char *line);
BOOLEAN conn_panel_add_action(ConnPanel *p, const char *label);
VOID conn_panel_set_default(ConnPanel *p, UINTN idx);
ConnCommand conn_panel_update(ConnPanel *p, ConnEvent ev);
VOID conn_panel_render(ConnGrid *g, const ConnPanel *p, const ConnTextTheme *theme);

UINTN conn_text_width(VOID);
UINTN conn_text_height(VOID);
INTN conn_col_x(INTN col);
INTN conn_row_y(INTN row);

#endif
