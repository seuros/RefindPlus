// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#include "conn_core.h"
#include "widget_internal.h"

#define PANEL_BODY_TOP 5
#define PANEL_BODY_ROWS 15
#define PANEL_ACT_ROW (CONN_ROWS - 4)

typedef enum
{
    PANEL_ST_ACTIVE = 0,
    PANEL_ST_CHOSEN = 1,
    PANEL_ST_CANCELLED = 2,
} PanelSmState;

typedef enum
{
    PANEL_EV_KEY_UP = 0,
    PANEL_EV_KEY_DOWN = 1,
    PANEL_EV_KEY_HOME = 2,
    PANEL_EV_KEY_END = 3,
    PANEL_EV_KEY_LEFT = 4,
    PANEL_EV_KEY_RIGHT = 5,
    PANEL_EV_KEY_ENTER = 6,
    PANEL_EV_KEY_ESCAPE = 7,
    PANEL_EV_KEY_DIGIT = 8,
} PanelSmEvent;

typedef enum
{
    PANEL_G_HAS_ACTIONS = 0,
    PANEL_G_DIGIT_VALID = 1,
} PanelSmGuard;

typedef enum
{
    PANEL_A_SCROLL = 0,
    PANEL_A_ACT_NAV = 1,
    PANEL_A_CHOOSE_ENTER = 2,
    PANEL_A_CANCEL = 3,
    PANEL_A_CHOOSE_DIGIT = 4,
    PANEL_A_NOOP_REDRAW = 5,
} PanelSmAction;

static UINTN pn_len(const char *s)
{
    UINTN n = 0;
    if (s)
        while (s[n])
            n++;
    return n;
}

static ConnPanel *panel_from_machine(const state_machine_t *m)
{
    return (ConnPanel *)state_machine_userdata(m);
}

static bool guard_has_actions(const state_machine_t *m, state_machine_event_id_t ev,
                              const void *payload)
{
    const ConnPanel *p = (const ConnPanel *)state_machine_userdata(m);
    (void)ev;
    (void)payload;
    return p != NULL && p->act_count > 0;
}

static bool guard_digit_valid(const state_machine_t *m, state_machine_event_id_t ev,
                              const void *payload)
{
    const ConnPanel *p = (const ConnPanel *)state_machine_userdata(m);
    const ConnWidgetDispatch *ctx = (const ConnWidgetDispatch *)payload;
    (void)ev;
    if (p == NULL || ctx == NULL)
        return false;
    return ctx->digit >= 1 && (UINTN)ctx->digit <= p->act_count;
}

static void action_scroll(const state_machine_t *m, state_machine_event_id_t ev, void *payload)
{
    ConnPanel *p = panel_from_machine(m);
    ConnWidgetDispatch *ctx = (ConnWidgetDispatch *)payload;
    if (p == NULL || ctx == NULL)
        return;
    switch (ev) {
    case PANEL_EV_KEY_UP:
        if (p->body_first > 0)
            p->body_first--;
        break;
    case PANEL_EV_KEY_DOWN:
        if (p->body_first + (UINTN)PANEL_BODY_ROWS < p->body_count)
            p->body_first++;
        break;
    case PANEL_EV_KEY_HOME:
        p->body_first = 0;
        break;
    case PANEL_EV_KEY_END:
        p->body_first =
            (p->body_count > (UINTN)PANEL_BODY_ROWS) ? p->body_count - (UINTN)PANEL_BODY_ROWS : 0;
        break;
    default:
        break;
    }
    ctx->command = CONN_ACT_REDRAW;
}

static void action_act_nav(const state_machine_t *m, state_machine_event_id_t ev, void *payload)
{
    ConnPanel *p = panel_from_machine(m);
    ConnWidgetDispatch *ctx = (ConnWidgetDispatch *)payload;
    if (p == NULL || ctx == NULL)
        return;
    if (ev == PANEL_EV_KEY_LEFT) {
        if (p->selected > 0)
            p->selected--;
    }
    else {
        if (p->selected + 1 < p->act_count)
            p->selected++;
    }
    ctx->command = CONN_ACT_REDRAW;
}

static void action_choose_enter(const state_machine_t *m, state_machine_event_id_t ev,
                                void *payload)
{
    ConnPanel *p = panel_from_machine(m);
    ConnWidgetDispatch *ctx = (ConnWidgetDispatch *)payload;
    (void)ev;
    if (p == NULL || ctx == NULL)
        return;
    p->status = CONN_PANEL_CHOSEN;
    ctx->command = CONN_ACT_REDRAW;
}

static void action_cancel(const state_machine_t *m, state_machine_event_id_t ev, void *payload)
{
    ConnPanel *p = panel_from_machine(m);
    ConnWidgetDispatch *ctx = (ConnWidgetDispatch *)payload;
    (void)ev;
    if (p == NULL || ctx == NULL)
        return;
    p->status = CONN_PANEL_CANCELLED;
    ctx->command = CONN_ACT_REDRAW;
}

static void action_choose_digit(const state_machine_t *m, state_machine_event_id_t ev,
                                void *payload)
{
    ConnPanel *p = panel_from_machine(m);
    ConnWidgetDispatch *ctx = (ConnWidgetDispatch *)payload;
    (void)ev;
    if (p == NULL || ctx == NULL)
        return;
    p->selected = (UINTN)(ctx->digit - 1);
    p->status = CONN_PANEL_CHOSEN;
    ctx->command = CONN_ACT_REDRAW;
}

static const state_machine_state_id_t SrcActive[] = {PANEL_ST_ACTIVE};

static const state_machine_guard_id_t GuardHasActions[] = {PANEL_G_HAS_ACTIONS};
static const state_machine_guard_id_t GuardDigitValid[] = {PANEL_G_DIGIT_VALID};

static const state_machine_transition_def_t ScrollTransitions[] = {
    STATE_MACHINE_TRANSITION_NG(SrcActive, 1, PANEL_ST_ACTIVE, PANEL_A_SCROLL, 0),
};

static const state_machine_transition_def_t ActNavTransitions[] = {
    STATE_MACHINE_TRANSITION_NG(SrcActive, 1, PANEL_ST_ACTIVE, PANEL_A_ACT_NAV, 0),
};

static const state_machine_transition_def_t EnterTransitions[] = {
    STATE_MACHINE_TRANSITION(SrcActive, 1, GuardHasActions, 1, PANEL_ST_CHOSEN,
                             PANEL_A_CHOOSE_ENTER, 0),
    STATE_MACHINE_TRANSITION_NG(SrcActive, 1, PANEL_ST_ACTIVE, PANEL_A_NOOP_REDRAW, 1),
};

static const state_machine_transition_def_t EscapeTransitions[] = {
    STATE_MACHINE_TRANSITION_NG(SrcActive, 1, PANEL_ST_CANCELLED, PANEL_A_CANCEL, 0),
};

static const state_machine_transition_def_t DigitTransitions[] = {
    STATE_MACHINE_TRANSITION(SrcActive, 1, GuardDigitValid, 1, PANEL_ST_CHOSEN,
                             PANEL_A_CHOOSE_DIGIT, 0),
    STATE_MACHINE_TRANSITION_NG(SrcActive, 1, PANEL_ST_ACTIVE, PANEL_A_NOOP_REDRAW, 1),
};

static const state_machine_event_def_t PanelEvents[] = {
    {.id = PANEL_EV_KEY_UP,
     .transition_count = 1,
     .transitions = ScrollTransitions,
     .name = "key_up"},
    {.id = PANEL_EV_KEY_DOWN,
     .transition_count = 1,
     .transitions = ScrollTransitions,
     .name = "key_down"},
    {.id = PANEL_EV_KEY_HOME,
     .transition_count = 1,
     .transitions = ScrollTransitions,
     .name = "key_home"},
    {.id = PANEL_EV_KEY_END,
     .transition_count = 1,
     .transitions = ScrollTransitions,
     .name = "key_end"},
    {.id = PANEL_EV_KEY_LEFT,
     .transition_count = 1,
     .transitions = ActNavTransitions,
     .name = "key_left"},
    {.id = PANEL_EV_KEY_RIGHT,
     .transition_count = 1,
     .transitions = ActNavTransitions,
     .name = "key_right"},
    {.id = PANEL_EV_KEY_ENTER,
     .transition_count = 2,
     .transitions = EnterTransitions,
     .name = "key_enter"},
    {.id = PANEL_EV_KEY_ESCAPE,
     .transition_count = 1,
     .transitions = EscapeTransitions,
     .name = "key_escape"},
    {.id = PANEL_EV_KEY_DIGIT,
     .transition_count = 2,
     .transitions = DigitTransitions,
     .name = "key_digit"},
};

static const state_machine_guard_def_t PanelGuards[] = {
    {.fn = guard_has_actions, .name = "has_actions"},
    {.fn = guard_digit_valid, .name = "digit_valid"},
};

static const state_machine_action_def_t PanelActions[] = {
    {.fn = action_scroll, .name = "scroll"},
    {.fn = action_act_nav, .name = "act_nav"},
    {.fn = action_choose_enter, .name = "choose_enter"},
    {.fn = action_cancel, .name = "cancel"},
    {.fn = action_choose_digit, .name = "choose_digit"},
    {.fn = conn_widget_action_noop_redraw, .name = "noop_redraw"},
};

static const state_machine_def_t ConnPanelMachine = {
    STATE_MACHINE_DEF_HEADER, .name = "conn_panel",       .states = ConnWidgetStates3,
    .events = PanelEvents,    .guards = PanelGuards,      .actions = PanelActions,
    .state_count = 3,         .initial = PANEL_ST_ACTIVE, .event_count = 9,
    .guard_count = 2,         .action_count = 6,
};

static state_machine_event_id_t event_for_key(ConnKey key)
{
    switch (key) {
    case CONN_KEY_UP:
        return PANEL_EV_KEY_UP;
    case CONN_KEY_DOWN:
        return PANEL_EV_KEY_DOWN;
    case CONN_KEY_HOME:
        return PANEL_EV_KEY_HOME;
    case CONN_KEY_END:
        return PANEL_EV_KEY_END;
    case CONN_KEY_LEFT:
        return PANEL_EV_KEY_LEFT;
    case CONN_KEY_RIGHT:
        return PANEL_EV_KEY_RIGHT;
    case CONN_KEY_ENTER:
        return PANEL_EV_KEY_ENTER;
    case CONN_KEY_ESCAPE:
        return PANEL_EV_KEY_ESCAPE;
    case CONN_KEY_DIGIT:
        return PANEL_EV_KEY_DIGIT;
    default:
        return STATE_MACHINE_ID_NONE;
    }
}

VOID conn_panel_init(ConnPanel *p, const char *title)
{
    char *b = (char *)p;
    UINTN i;
    for (i = 0; i < sizeof(*p); i++)
        b[i] = 0;
    conn_cp_bounded(p->title, title, sizeof p->title);
    p->status = CONN_PANEL_ACTIVE;
    (void)state_machine_init(&p->machine, &ConnPanelMachine, p);
}

VOID conn_panel_add_body(ConnPanel *p, const char *line)
{
    if (p->body_count < CONN_PANEL_BODY)
        conn_cp_bounded(p->body[p->body_count++], line, CONN_PANEL_COLS);
}

BOOLEAN conn_panel_add_action(ConnPanel *p, const char *label)
{
    if (p->act_count >= CONN_PANEL_ACTS)
        return FALSE;
    conn_cp_bounded(p->act[p->act_count++], label, CONN_PANEL_ALBL);
    return TRUE;
}

VOID conn_panel_set_default(ConnPanel *p, UINTN idx)
{
    if (p == NULL || p->act_count == 0)
        return;
    p->selected = (idx < p->act_count) ? idx : p->act_count - 1;
}

ConnCommand conn_panel_update(ConnPanel *p, ConnEvent ev)
{
    state_machine_event_id_t sm_ev;
    ConnWidgetDispatch ctx;

    if (p == NULL || ev.type != CONN_EVENT_KEY)
        return CONN_ACT_NONE;

    sm_ev = event_for_key(ev.key);
    if (sm_ev == STATE_MACHINE_ID_NONE)
        return CONN_ACT_NONE;

    ctx.digit = ev.digit;
    ctx.command = CONN_ACT_NONE;
    (void)state_machine_dispatch(&p->machine, sm_ev, &ctx);
    return ctx.command;
}

VOID conn_panel_render(ConnGrid *g, const ConnPanel *p, const ConnTextTheme *th)
{
    UINTN shown, i;
    INTN col;
    UINTN total = 0;

    conn_grid_clear(g, th);
    conn_grid_frame(g, th);

    conn_grid_puts(g, 3, 1, "MERIDIAN", th->frame, CONN_TRANSPARENT, TRUE);
    conn_grid_puts(g, 12, 1, "BOOT MANAGER \xC2\xB7 CONN", th->text, CONN_TRANSPARENT, FALSE);
    conn_grid_puts(g, 3, 3, p->title, th->hi, CONN_TRANSPARENT, TRUE);

    shown = p->body_count - p->body_first;
    if (shown > (UINTN)PANEL_BODY_ROWS)
        shown = (UINTN)PANEL_BODY_ROWS;
    for (i = 0; i < shown; i++) {
        conn_grid_puts(g, 3, PANEL_BODY_TOP + (INTN)i, p->body[p->body_first + i], th->text,
                       CONN_TRANSPARENT, FALSE);
    }
    if (p->body_first > 0)
        conn_grid_set(g, CONN_COLS - 2, PANEL_BODY_TOP, 0x2191, th->dim, CONN_TRANSPARENT, FALSE);
    if (p->body_first + (UINTN)PANEL_BODY_ROWS < p->body_count)
        conn_grid_set(g, CONN_COLS - 2, PANEL_BODY_TOP + PANEL_BODY_ROWS - 1, 0x2193, th->dim,
                      CONN_TRANSPARENT, FALSE);

    for (i = 0; i < p->act_count; i++)
        total += pn_len(p->act[i]) + 4 + (i ? 1 : 0);
    col = (INTN)((CONN_COLS > total) ? (CONN_COLS - total) / 2 : 1);
    if (col < 1)
        col = 1;
    for (i = 0; i < p->act_count; i++) {
        UINTN wlen = pn_len(p->act[i]) + 4;
        BOOLEAN sel = (i == p->selected);
        ConnColor bg = sel ? th->frame : CONN_TRANSPARENT;
        ConnColor fg = sel ? conn_contrast(th->frame) : th->text;
        INTN c;
        for (c = 0; c < (INTN)wlen; c++)
            conn_grid_set(g, col + c, PANEL_ACT_ROW, 0x20, fg, bg, sel);
        conn_grid_puts(g, col + 2, PANEL_ACT_ROW, p->act[i], fg, bg, sel);
        col += (INTN)wlen + 1;
    }

    conn_grid_puts(g, 3, CONN_ROWS - 2,
                   "\xE2\x86\x91\xE2\x86\x93 scroll   \xE2\x86\x90\xE2\x86\x92 select   ENTER "
                   "confirm   ESC cancel",
                   th->dim, CONN_TRANSPARENT, FALSE);
}
