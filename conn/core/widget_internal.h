// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#ifndef CONN_WIDGET_INTERNAL_H
#define CONN_WIDGET_INTERNAL_H

#include "conn_core.h"

typedef struct
{
    INTN digit;
    ConnCommand command;
} ConnWidgetDispatch;

static const state_machine_state_def_t ConnWidgetStates3[] = {
    {.id = 0,
     .parent = STATE_MACHINE_ID_NONE,
     .initial_child = STATE_MACHINE_ID_NONE,
     .depth = 0,
     .flags = 0,
     .name = "active"},
    {.id = 1,
     .parent = STATE_MACHINE_ID_NONE,
     .initial_child = STATE_MACHINE_ID_NONE,
     .depth = 0,
     .flags = 0,
     .name = "chosen"},
    {.id = 2,
     .parent = STATE_MACHINE_ID_NONE,
     .initial_child = STATE_MACHINE_ID_NONE,
     .depth = 0,
     .flags = 0,
     .name = "cancelled"},
};

static inline void conn_widget_action_noop_redraw(const state_machine_t *m,
                                                  state_machine_event_id_t ev, void *payload)
{
    ConnWidgetDispatch *ctx = (ConnWidgetDispatch *)payload;
    (void)m;
    (void)ev;
    if (ctx != NULL)
        ctx->command = CONN_ACT_REDRAW;
}

#endif
