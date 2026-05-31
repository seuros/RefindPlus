// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#ifndef CONN_TEST_HELPERS_H
#define CONN_TEST_HELPERS_H

static __attribute__((unused)) ConnEvent ev_key(ConnKey k)
{
    ConnEvent e;
    memset(&e, 0, sizeof e);
    e.type = CONN_EVENT_KEY;
    e.key = k;
    return e;
}
static __attribute__((unused)) ConnEvent ev_digit(INTN d)
{
    ConnEvent e;
    memset(&e, 0, sizeof e);
    e.type = CONN_EVENT_KEY;
    e.key = CONN_KEY_DIGIT;
    e.digit = d;
    return e;
}

#endif
