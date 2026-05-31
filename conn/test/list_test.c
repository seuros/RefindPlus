// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "conn_core.h"
#include "conn_test_helpers.h"

int main(void) {
    ConnList l;

    conn_list_init(&l, "BOOT OPTIONS");
    assert(strcmp(l.title, "BOOT OPTIONS") == 0);
    assert(l.count == 0 && l.selected == 0 && l.status == CONN_LIST_ACTIVE);

    conn_list_add_info(&l, "NOTE: example");
    assert(l.info_count == 1);

    assert(conn_list_add(&l, "Boot normal"));
    assert(conn_list_add(&l, "Boot single-user"));
    assert(conn_list_add(&l, "Return"));
    assert(l.count == 3);

    conn_list_update(&l, ev_key(CONN_KEY_DOWN));
    conn_list_update(&l, ev_key(CONN_KEY_DOWN));
    assert(l.selected == 2);
    conn_list_update(&l, ev_key(CONN_KEY_DOWN));
    assert(l.selected == 2);
    conn_list_update(&l, ev_key(CONN_KEY_HOME));
    assert(l.selected == 0);
    conn_list_update(&l, ev_key(CONN_KEY_END));
    assert(l.selected == 2);
    conn_list_update(&l, ev_key(CONN_KEY_UP));
    assert(l.selected == 1);

    conn_list_update(&l, ev_key(CONN_KEY_ENTER));
    assert(l.status == CONN_LIST_CHOSEN && l.selected == 1);

    assert(conn_list_update(&l, ev_key(CONN_KEY_DOWN)) == CONN_ACT_NONE);

    conn_list_init(&l, "T");
    conn_list_add(&l, "a"); conn_list_add(&l, "b"); conn_list_add(&l, "c");
    conn_list_update(&l, ev_digit(3));
    assert(l.status == CONN_LIST_CHOSEN && l.selected == 2);

    conn_list_init(&l, "T");
    conn_list_add(&l, "a");
    conn_list_update(&l, ev_digit(5));
    assert(l.status == CONN_LIST_ACTIVE);

    conn_list_init(&l, "T");
    conn_list_add(&l, "a");
    conn_list_update(&l, ev_key(CONN_KEY_ESCAPE));
    assert(l.status == CONN_LIST_CANCELLED);

    conn_list_init(&l, "T");
    for (int i = 0; i < CONN_LIST_MAX + 10; i++) {
        char b[8]; snprintf(b, sizeof b, "i%d", i);
        conn_list_add(&l, b);
    }
    assert(l.count == CONN_LIST_MAX);

    conn_list_init(&l, "T");
    for (int i = 0; i < 30; i++) { char b[8]; snprintf(b,sizeof b,"i%d",i); conn_list_add(&l,b); }
    conn_list_update(&l, ev_key(CONN_KEY_END));
    assert(l.selected == 29);
    assert(l.first <= l.selected && l.selected < l.first + (UINTN)(CONN_ROWS - 4 - 6 + 1));

    conn_list_update(&l, ev_digit(1));
    assert(l.selected == 0 && l.first == 0);

    conn_list_init(&l, "T");
    conn_list_add(&l, "Yes"); conn_list_add(&l, "No");
    conn_list_set_default(&l, 9999);
    assert(l.selected == 1);
    conn_list_set_default(&l, 0);
    assert(l.selected == 0);

    printf("PASS  list_test (add/nav/digit/choose/cancel/overflow/scroll/default)\n");
    return 0;
}
