// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "conn_core.h"
#include "conn_test_helpers.h"

int main(void) {
    ConnPanel p;
    char b[16];

    conn_panel_init(&p, "CONFIRM HIDE");
    assert(strcmp(p.title, "CONFIRM HIDE") == 0 && p.status == CONN_PANEL_ACTIVE);

    for (int i = 0; i < 30; i++) { snprintf(b, sizeof b, "line %d", i); conn_panel_add_body(&p, b); }
    assert(p.body_count == 30);
    assert(conn_panel_add_action(&p, "Yes"));
    assert(conn_panel_add_action(&p, "No"));
    conn_panel_set_default(&p, 9999);
    assert(p.selected == 1);

    conn_panel_update(&p, ev_key(CONN_KEY_DOWN));
    assert(p.body_first == 1);
    conn_panel_update(&p, ev_key(CONN_KEY_END));
    assert(p.body_first == 30 - 15);
    conn_panel_update(&p, ev_key(CONN_KEY_HOME));
    assert(p.body_first == 0);

    for (int i = 0; i < 50; i++) conn_panel_update(&p, ev_key(CONN_KEY_DOWN));
    assert(p.body_first == 30 - 15);

    conn_panel_update(&p, ev_key(CONN_KEY_LEFT));
    assert(p.selected == 0);
    conn_panel_update(&p, ev_key(CONN_KEY_RIGHT));
    assert(p.selected == 1);

    conn_panel_update(&p, ev_key(CONN_KEY_ENTER));
    assert(p.status == CONN_PANEL_CHOSEN && p.selected == 1);
    assert(conn_panel_update(&p, ev_key(CONN_KEY_LEFT)) == CONN_ACT_NONE);

    conn_panel_init(&p, "T");
    conn_panel_add_action(&p, "A"); conn_panel_add_action(&p, "B"); conn_panel_add_action(&p, "C");
    conn_panel_update(&p, ev_digit(2));
    assert(p.status == CONN_PANEL_CHOSEN && p.selected == 1);

    conn_panel_init(&p, "T");
    conn_panel_add_action(&p, "OK");
    conn_panel_update(&p, ev_key(CONN_KEY_ESCAPE));
    assert(p.status == CONN_PANEL_CANCELLED);

    printf("PASS  panel_test (body-scroll/action-nav/default/digit/choose/cancel)\n");
    return 0;
}
