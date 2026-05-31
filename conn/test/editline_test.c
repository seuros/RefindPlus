// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "conn_core.h"
#include "conn_test_helpers.h"

static ConnEvent ev_text(UINT32 ch) {
    ConnEvent e; memset(&e, 0, sizeof e);
    e.type = CONN_EVENT_KEY; e.key = CONN_KEY_TEXT; e.ch = ch;
    return e;
}
static ConnEvent ev_tick(void) {
    ConnEvent e; memset(&e, 0, sizeof e);
    e.type = CONN_EVENT_TICK;
    return e;
}
static void type(ConnLineEditModel *m, const char *s) {
    for (const char *p = s; *p; p++) conn_line_edit_update(m, ev_text((UINT32)(unsigned char)*p));
}

int main(void) {
    ConnLineEditModel m;

    conn_line_edit_init(&m, "ro", "Arch Linux", "vmlinuz", "ESP P1");
    assert(strcmp(m.buf, "ro") == 0);
    assert(m.len == 2 && m.cursor == 2);
    assert(strcmp(m.title, "Arch Linux") == 0);
    assert(m.status == CONN_EDIT_ACTIVE);

    type(&m, " quiet");
    assert(strcmp(m.buf, "ro quiet") == 0 && m.len == 8 && m.cursor == 8);

    conn_line_edit_update(&m, ev_key(CONN_KEY_HOME));
    assert(m.cursor == 0);
    conn_line_edit_update(&m, ev_key(CONN_KEY_RIGHT));
    conn_line_edit_update(&m, ev_key(CONN_KEY_RIGHT));
    assert(m.cursor == 2);

    type(&m, "X");
    assert(strcmp(m.buf, "roX quiet") == 0 && m.cursor == 3);

    conn_line_edit_update(&m, ev_key(CONN_KEY_DELETE));
    assert(strcmp(m.buf, "roXquiet") == 0 && m.len == 8 && m.cursor == 3);

    conn_line_edit_update(&m, ev_key(CONN_KEY_BACKSPACE));
    assert(strcmp(m.buf, "roquiet") == 0 && m.len == 7 && m.cursor == 2);

    conn_line_edit_update(&m, ev_key(CONN_KEY_END));
    assert(m.cursor == 7);
    conn_line_edit_update(&m, ev_text(0x09));
    assert(m.len == 7);

    conn_line_edit_init(&m, "", "", "", "");
    for (int i = 0; i < CONN_EDIT_MAX + 50; i++) conn_line_edit_update(&m, ev_text('x'));
    assert(m.len == CONN_EDIT_MAX - 1);

    assert(m.first > 0 && m.cursor - m.first < (UINTN)(CONN_COLS - 3 - 8));

    conn_line_edit_update(&m, ev_key(CONN_KEY_ENTER));
    assert(m.status == CONN_EDIT_COMMITTED);
    assert(conn_line_edit_update(&m, ev_text('z')) == CONN_ACT_NONE);

    conn_line_edit_init(&m, "abc", "", "", "");
    conn_line_edit_update(&m, ev_key(CONN_KEY_ESCAPE));
    assert(m.status == CONN_EDIT_CANCELLED);

    conn_line_edit_init(&m, "abc", "", "", "");
    {
        int was = m.cursor_on;
        conn_line_edit_update(&m, ev_tick());
        assert(m.cursor_on == !was);
    }

    printf("PASS  editline_test (insert/edit/scroll/overflow/commit/cancel/blink)\n");
    return 0;
}
