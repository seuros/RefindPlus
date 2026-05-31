// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#include "conn_core.h"
#include <stdio.h>
#include <string.h>

static int fails = 0;
#define CHECK(c, msg) do { if (!(c)) { printf("  FAIL: %s\n", msg); fails++; } } while (0)

#define BOOT_SCROLL_COL 58

static void mklist(ConnEntryList *l, UINTN n) {
    memset(l, 0, sizeof *l);
    l->count = n; l->default_index = 0; l->allow_autoboot = (n > 0);
    for (UINTN i = 0; i < n; i++) {
        snprintf(l->entry[i].name, sizeof l->entry[i].name, "OS %lu", (unsigned long)(i + 1));
        l->entry[i].sub[0] = 0; l->entry[i].part[0] = 0; l->entry[i].loader[0] = 0;
        l->entry[i].accent = CONN_ICE; l->entry[i].locked = FALSE;
    }
}

static int selected_screen_row(const ConnGrid *g) {
    int found = -1, hits = 0;
    for (int r = 4; r <= 4 + (CONN_ROWS - 7) - 1; r++) {
        if (CONN_A(g->cell[r][5].bg) != 0) { found = r; hits++; }
    }
    return hits == 1 ? found : -1;
}

static int row_contains_ascii(const ConnGrid *g, int row, const char *needle)
{
    int n = (int)strlen(needle);
    for (int col = 0; col <= CONN_COLS - n; col++) {
        int ok = 1;
        for (int i = 0; i < n; i++) {
            if (g->cell[row][col + i].ch != (unsigned char)needle[i]) {
                ok = 0;
                break;
            }
        }
        if (ok)
            return 1;
    }
    return 0;
}

static int grid_contains_ascii(const ConnGrid *g, const char *needle)
{
    for (int row = 0; row < CONN_ROWS; row++) {
        if (row_contains_ascii(g, row, needle))
            return 1;
    }
    return 0;
}

static void test_copy_line(char *dst, UINTN cap, const char *src)
{
    UINTN i = 0;

    if (cap == 0)
        return;

    while (i + 1 < cap && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

int main(void) {
    ConnEntryList l; ConnModel model;

    mklist(&l, 5); l.default_index = 2;
    conn_model_init(&model, &l);
    CHECK(model.ui.selected == 2, "init selects default_index");
    CHECK(model.ui.autoboot && !model.ui.cancelled, "init arms auto-boot");
    CHECK(model.ui.countdown_s == CONN_AUTOBOOT_SECONDS, "init countdown = 5");

    conn_model_init(&model, &l);
    CHECK(conn_update(&model, &l, conn_event_key(CONN_KEY_DOWN, 0)) == CONN_ACT_REDRAW, "down redraws");
    CHECK(model.ui.selected == 3, "down moves selection");
    CHECK(!model.ui.autoboot && model.ui.cancelled, "any key cancels auto-boot (sticky)");
    conn_update(&model, &l, conn_event_key(CONN_KEY_DOWN, 0));
    conn_update(&model, &l, conn_event_key(CONN_KEY_DOWN, 0));
    CHECK(model.ui.selected == 4, "down clamps at bottom");
    for (int i = 0; i < 9; i++) conn_update(&model, &l, conn_event_key(CONN_KEY_UP, 0));
    CHECK(model.ui.selected == 0, "up clamps at top");

    conn_model_init(&model, &l);
    conn_update(&model, &l, conn_event_key(CONN_KEY_DIGIT, 4));
    CHECK(model.ui.selected == 3, "digit 4 selects entry index 3");
    CHECK(conn_update(&model, &l, conn_event_key(CONN_KEY_ENTER, 0)) == CONN_ACT_LAUNCH, "enter launches");

    conn_model_init(&model, &l);
    model.ui.selected = 1;
    CHECK(conn_update(&model, &l, conn_event_key(CONN_KEY_OPTIONS, 0)) == CONN_ACT_OPTIONS,
          "options key returns options command");
    CHECK(model.ui.selected == 1, "options key preserves selection");
    CHECK(!model.ui.autoboot && model.ui.cancelled, "options key cancels auto-boot");

    conn_model_init(&model, &l);
    ConnCommand a = CONN_ACT_NONE;
    for (int i = 0; i < CONN_AUTOBOOT_SECONDS; i++) a = conn_update(&model, &l, conn_event_tick());
    CHECK(a == CONN_ACT_LAUNCH, "countdown expiry launches");
    CHECK(model.ui.selected == l.default_index, "expiry launches default");

    CHECK(strcmp(state_machine_state_name(state_machine_def(&model.ui.machine),
                                          state_machine_current(&model.ui.machine)),
                 "launching") == 0,
          "countdown expiry -> launching state");

    conn_model_init(&model, &l);
    conn_update(&model, &l, conn_event_key(CONN_KEY_DOWN, 0));
    CHECK(conn_update(&model, &l, conn_event_tick()) == CONN_ACT_NONE, "no tick after cancel");

    mklist(&l, 5);
    l.allow_autoboot = FALSE;
    conn_model_init(&model, &l);
    conn_ui_set_screensaver(&model.ui, 3);
    CHECK(conn_update(&model, &l, conn_event_tick()) == CONN_ACT_NONE,
          "idle tick 1 does not redraw");
    CHECK(model.ui.idle_s == 1 && !conn_ui_in_saver(&model.ui), "idle tick 1 bumps idle");
    CHECK(conn_update(&model, &l, conn_event_tick()) == CONN_ACT_NONE,
          "idle tick 2 does not redraw");
    CHECK(model.ui.idle_s == 2 && !conn_ui_in_saver(&model.ui), "idle tick 2 bumps idle");
    CHECK(conn_update(&model, &l, conn_event_tick()) == CONN_ACT_REDRAW,
          "idle threshold enters saver");
    CHECK(conn_ui_in_saver(&model.ui) && model.ui.idle_s == 3,
          "saver entered at configured threshold");
    CHECK(conn_update(&model, &l, conn_event_tick()) == CONN_ACT_REDRAW,
          "saver tick redraws frame");
    CHECK(model.ui.saver_frame == 1, "saver tick advances frame");

    mklist(&l, 5);
    l.allow_autoboot = FALSE;
    conn_model_init(&model, &l);
    conn_ui_set_screensaver(&model.ui, 1);
    CHECK(conn_update(&model, &l, conn_event_tick()) == CONN_ACT_REDRAW,
          "screensaver=1 enters on first idle tick");
    CHECK(conn_update(&model, &l, conn_event_key(CONN_KEY_DOWN, 0)) == CONN_ACT_REDRAW,
          "key from saver wakes and redraws");
    CHECK(!conn_ui_in_saver(&model.ui), "key wake leaves saver");
    CHECK(model.ui.selected == 0, "key wake is swallowed, selection unchanged");
    CHECK(model.ui.idle_s == 0 && model.ui.saver_frame == 0, "key wake resets idle counters");
    CHECK(conn_update(&model, &l, conn_event_key(CONN_KEY_DOWN, 0)) == CONN_ACT_REDRAW,
          "second down after wake navigates");
    CHECK(model.ui.selected == 1, "second down moves selection");

    mklist(&l, 5);
    l.allow_autoboot = FALSE;
    conn_model_init(&model, &l);
    conn_ui_set_screensaver(&model.ui, 1);
    conn_update(&model, &l, conn_event_tick());
    CHECK(conn_update(&model, &l, conn_event_key(CONN_KEY_ENTER, 0)) == CONN_ACT_REDRAW,
          "enter from saver wakes, does not launch");
    mklist(&l, 5);
    l.allow_autoboot = FALSE;
    conn_model_init(&model, &l);
    conn_ui_set_screensaver(&model.ui, 1);
    conn_update(&model, &l, conn_event_tick());
    CHECK(conn_update(&model, &l, conn_event_key(CONN_KEY_OPTIONS, 0)) == CONN_ACT_REDRAW,
          "options key from saver wakes, does not open options");

    mklist(&l, 5);
    conn_model_init(&model, &l);
    conn_ui_set_screensaver(&model.ui, 1);
    CHECK(conn_update(&model, &l, conn_event_tick()) == CONN_ACT_REDRAW,
          "autoboot tick still redraws countdown");
    CHECK(!conn_ui_in_saver(&model.ui), "autoboot state is not saver-eligible");

    mklist(&l, 5);
    l.allow_autoboot = FALSE;
    conn_model_init(&model, &l);
    conn_ui_set_screensaver(&model.ui, 1);
    conn_update(&model, &l, conn_event_tick());
    CHECK(conn_update(&model, &l, conn_event_remote(CONN_WAKE_SELECT, 3)) == CONN_ACT_REDRAW,
          "remote select wakes and redraws");
    CHECK(!conn_ui_in_saver(&model.ui) && model.ui.selected == 3,
          "remote select carries target index");
    conn_update(&model, &l, conn_event_tick());
    CHECK(conn_update(&model, &l, conn_event_remote(CONN_WAKE_LAUNCH, 2)) == CONN_ACT_LAUNCH,
          "remote launch wakes and launches");
    CHECK(model.ui.selected == 2, "remote launch carries target index");

    mklist(&l, 5);
    conn_model_init(&model, &l);
    CHECK(conn_update(&model, &l, conn_event_point(3, FALSE)) == CONN_ACT_REDRAW,
          "point hover redraws");
    CHECK(model.ui.selected == 3, "point hover selects the row");
    CHECK(!model.ui.autoboot && model.ui.cancelled, "point hover cancels auto-boot");
    CHECK(conn_update(&model, &l, conn_event_point(1, TRUE)) == CONN_ACT_LAUNCH,
          "point click launches");
    CHECK(model.ui.selected == 1, "point click selects the clicked row");

    conn_model_init(&model, &l);
    CHECK(conn_update(&model, &l, conn_event_point(99, FALSE)) == CONN_ACT_REDRAW,
          "point to out-of-range row is a harmless redraw");
    CHECK(model.ui.selected == l.default_index, "out-of-range point does not move selection");

    mklist(&l, 5);
    l.allow_autoboot = FALSE;
    conn_model_init(&model, &l);
    conn_ui_set_screensaver(&model.ui, 1);
    conn_update(&model, &l, conn_event_tick());
    CHECK(conn_update(&model, &l, conn_event_point(2, FALSE)) == CONN_ACT_REDRAW,
          "point from saver wakes and redraws");
    CHECK(!conn_ui_in_saver(&model.ui), "point wake leaves saver");
    CHECK(model.ui.selected == 2, "point wake carries the hovered row");

    mklist(&l, 5);
    CHECK(conn_menu_hit_index(&l, 0, 5, 4) == 0, "hit row 4 -> entry 0");
    CHECK(conn_menu_hit_index(&l, 0, 5, 6) == 2, "hit row 6 -> entry 2");
    CHECK(conn_menu_hit_index(&l, 0, 5, 8) == 4, "hit row 8 -> last entry");
    CHECK(conn_menu_hit_index(&l, 0, 5, 9) == -1, "row below the list -> no hit");
    CHECK(conn_menu_hit_index(&l, 0, 5, 3) == -1, "header row -> no hit");
    CHECK(conn_menu_hit_index(&l, 0, 59, 4) == -1, "rail column -> no hit");
    CHECK(conn_menu_hit_index(&l, 0, 0, 4) == -1, "index gutter column 0 -> no hit");
    mklist(&l, 24);
    CHECK(conn_menu_hit_index(&l, 12, 5, 4) == 3, "overflow: top visible row -> start index");
    CHECK(conn_menu_hit_index(&l, 12, 5, 21) == 20, "overflow: bottom visible row -> start+17");
    CHECK(conn_menu_hit_index(&l, 12, 5, 22) == -1, "overflow: below viewport -> no hit");

    mklist(&l, 5);
    l.allow_autoboot = FALSE;
    test_copy_line(l.sys.line[0], sizeof l.sys.line[0], "MODEL");
    test_copy_line(l.sys.line[1], sizeof l.sys.line[1], " ThinkPad X200");
    test_copy_line(l.sys.line[2], sizeof l.sys.line[2], "FIRMWARE");
    test_copy_line(l.sys.line[3], sizeof l.sys.line[3], " Gigabyte UEFI");
    l.sys.count = 4;
    test_copy_line(l.rng_nonce, sizeof l.rng_nonce, "DEADBEEF");
    conn_model_init(&model, &l);
    conn_ui_set_screensaver(&model.ui, 1);
    conn_update(&model, &l, conn_event_tick());
    model.ui.saver_frame = 7;
    ConnGrid saver;
    conn_build_text_grid(&saver, &l, &model, &CONN_TXT_EFI);
    CHECK(grid_contains_ascii(&saver, "DORMANT CRT"), "saver grid shows dormant title");
    CHECK(grid_contains_ascii(&saver, "NODE IDENTITY"), "saver grid labels identity panel");
    CHECK(grid_contains_ascii(&saver, "MODEL"), "saver grid reuses sysinfo heading");
    CHECK(grid_contains_ascii(&saver, "ThinkPad X200"), "saver grid reuses sysinfo value");
    CHECK(grid_contains_ascii(&saver, "LIVENESS"), "saver grid shows liveness panel");
    CHECK(grid_contains_ascii(&saver, "RNG DEADBEEF"), "saver grid shows firmware RNG nonce");
    CHECK(selected_screen_row(&saver) == -1, "saver grid does not draw selected boot row");

    mklist(&l, 0); conn_model_init(&model, &l);
    CHECK(model.ui.status == CONN_UI_EMPTY && !model.ui.autoboot, "empty list -> EMPTY, no auto-boot");
    CHECK(conn_update(&model, &l, conn_event_key(CONN_KEY_DOWN, 0)) == CONN_ACT_NONE, "empty list ignores keys");
    CHECK(conn_update(&model, &l, conn_event_key(CONN_KEY_OPTIONS, 0)) == CONN_ACT_NONE,
          "empty list ignores options key");

    mklist(&l, 1); l.allow_autoboot = FALSE; conn_model_init(&model, &l);
    CHECK(!model.ui.autoboot && model.ui.status == CONN_UI_OK, "only-self -> menu, no auto-boot");
    CHECK(conn_update(&model, &l, conn_event_tick()) == CONN_ACT_NONE, "only-self never auto-launches");

    mklist(&l, 5); conn_model_init(&model, &l);
    CHECK(conn_update(&model, &l, conn_event_launch_returned(EFI_LOAD_ERROR, 4)) == CONN_ACT_REDRAW,
          "launch return redraws");
    CHECK(model.ui.selected == 4, "launch return preserves selected hint");
    CHECK(model.ui.status == CONN_UI_LAUNCH_FAILED, "launch error sets failed status");
    CHECK(!model.ui.autoboot && model.ui.cancelled, "launch return disables autoboot");

    ConnGrid g;
    mklist(&l, 24);
    conn_model_init(&model, &l);
    model.ui.selected = 0;  conn_build_text_grid(&g, &l, &model, &CONN_TXT_EFI);
    CHECK(selected_screen_row(&g) == 4, "sel 0 renders at top row");
    model.ui.selected = 23; conn_build_text_grid(&g, &l, &model, &CONN_TXT_EFI);
    int r = selected_screen_row(&g);
    CHECK(r >= 4 && r <= 21, "sel 23 stays visible when scrolled");
    model.ui.selected = 12; conn_build_text_grid(&g, &l, &model, &CONN_TXT_EFI);
    CHECK(selected_screen_row(&g) >= 4, "mid selection visible");

    CHECK(g.cell[4][BOOT_SCROLL_COL].ch == 0x2191, "up chevron when rows above");
    CHECK(g.cell[21][BOOT_SCROLL_COL].ch == 0x2193, "down chevron when rows below");

    mklist(&l, 5); model.ui.selected = 0; conn_build_text_grid(&g, &l, &model, &CONN_TXT_EFI);
    CHECK(g.cell[4][BOOT_SCROLL_COL].ch != 0x2191, "no up chevron for small list");

    mklist(&l, 18); model.ui.selected = 17; conn_build_text_grid(&g, &l, &model, &CONN_TXT_EFI);
    CHECK(selected_screen_row(&g) == 21, "count=18 sel=17 at bottom row, no scroll");
    CHECK(g.cell[21][BOOT_SCROLL_COL].ch != 0x2193, "count=18 has no down chevron");

    mklist(&l, 19); model.ui.selected = 18; conn_build_text_grid(&g, &l, &model, &CONN_TXT_EFI);
    CHECK(selected_screen_row(&g) == 21, "count=19 sel=18 visible at bottom");
    CHECK(g.cell[4][BOOT_SCROLL_COL].ch == 0x2191, "count=19 scrolled -> up chevron");

    if (fails == 0) printf("PASS  ui_test (update + scroll, all assertions)\n");
    else            printf("FAIL  ui_test: %d assertion(s)\n", fails);
    return fails ? 1 : 0;
}
