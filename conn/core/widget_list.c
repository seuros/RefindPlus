// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#include "conn_core.h"
#include "widget_internal.h"

#define LIST_TOP 6
#define LIST_VISIBLE (CONN_ROWS - 4 - LIST_TOP + 1)

typedef enum
{
    LIST_ST_ACTIVE = 0,
    LIST_ST_CHOSEN = 1,
    LIST_ST_CANCELLED = 2,
} ListSmState;

typedef enum
{
    LIST_EV_KEY_UP = 0,
    LIST_EV_KEY_DOWN = 1,
    LIST_EV_KEY_HOME = 2,
    LIST_EV_KEY_END = 3,
    LIST_EV_KEY_ENTER = 4,
    LIST_EV_KEY_ESCAPE = 5,
    LIST_EV_KEY_DIGIT = 6,
    LIST_EV_KEY_PAGE_UP = 7,
    LIST_EV_KEY_PAGE_DOWN = 8,
} ListSmEvent;

typedef enum
{
    LIST_G_HAS_ENTRIES = 0,
    LIST_G_DIGIT_VALID = 1,
} ListSmGuard;

typedef enum
{
    LIST_A_NAV = 0,
    LIST_A_CHOOSE_ENTER = 1,
    LIST_A_CANCEL = 2,
    LIST_A_CHOOSE_DIGIT = 3,
    LIST_A_DIGIT_CLAMP = 4,
    LIST_A_NOOP_REDRAW = 5,
} ListSmAction;

static VOID clamp_scroll(ConnList *l)
{
    if (l->selected < l->first) {
        l->first = l->selected;
    }
    else if (l->selected >= l->first + (UINTN)LIST_VISIBLE) {
        l->first = l->selected - (UINTN)LIST_VISIBLE + 1;
    }
}

static ConnList *list_from_machine(const state_machine_t *m)
{
    return (ConnList *)state_machine_userdata(m);
}

static bool guard_has_entries(const state_machine_t *m, state_machine_event_id_t ev,
                              const void *payload)
{
    const ConnList *l = (const ConnList *)state_machine_userdata(m);
    (void)ev;
    (void)payload;
    return l != NULL && l->count > 0;
}

static bool guard_digit_valid(const state_machine_t *m, state_machine_event_id_t ev,
                              const void *payload)
{
    const ConnList *l = (const ConnList *)state_machine_userdata(m);
    const ConnWidgetDispatch *ctx = (const ConnWidgetDispatch *)payload;
    (void)ev;
    if (l == NULL || ctx == NULL)
        return false;
    return ctx->digit >= 1 && (UINTN)ctx->digit <= l->count;
}

static void action_nav(const state_machine_t *m, state_machine_event_id_t ev, void *payload)
{
    ConnList *l = list_from_machine(m);
    ConnWidgetDispatch *ctx = (ConnWidgetDispatch *)payload;
    if (l == NULL || ctx == NULL)
        return;
    switch (ev) {
    case LIST_EV_KEY_UP:
        if (l->selected > 0)
            l->selected--;
        break;
    case LIST_EV_KEY_DOWN:
        if (l->selected + 1 < l->count)
            l->selected++;
        break;
    case LIST_EV_KEY_PAGE_UP:
        l->selected = conn_page_step(l->selected, l->count, (UINTN)LIST_VISIBLE, FALSE);
        break;
    case LIST_EV_KEY_PAGE_DOWN:
        l->selected = conn_page_step(l->selected, l->count, (UINTN)LIST_VISIBLE, TRUE);
        break;
    case LIST_EV_KEY_HOME:
        l->selected = 0;
        break;
    case LIST_EV_KEY_END:
        if (l->count > 0)
            l->selected = l->count - 1;
        break;
    default:
        break;
    }
    clamp_scroll(l);
    ctx->command = CONN_ACT_REDRAW;
}

static void action_choose_enter(const state_machine_t *m, state_machine_event_id_t ev,
                                void *payload)
{
    ConnList *l = list_from_machine(m);
    ConnWidgetDispatch *ctx = (ConnWidgetDispatch *)payload;
    (void)ev;
    if (l == NULL || ctx == NULL)
        return;
    l->status = CONN_LIST_CHOSEN;
    ctx->command = CONN_ACT_REDRAW;
}

static void action_cancel(const state_machine_t *m, state_machine_event_id_t ev, void *payload)
{
    ConnList *l = list_from_machine(m);
    ConnWidgetDispatch *ctx = (ConnWidgetDispatch *)payload;
    (void)ev;
    if (l == NULL || ctx == NULL)
        return;
    l->status = CONN_LIST_CANCELLED;
    ctx->command = CONN_ACT_REDRAW;
}

static void action_choose_digit(const state_machine_t *m, state_machine_event_id_t ev,
                                void *payload)
{
    ConnList *l = list_from_machine(m);
    ConnWidgetDispatch *ctx = (ConnWidgetDispatch *)payload;
    (void)ev;
    if (l == NULL || ctx == NULL)
        return;
    l->selected = (UINTN)(ctx->digit - 1);
    l->status = CONN_LIST_CHOSEN;
    clamp_scroll(l);
    ctx->command = CONN_ACT_REDRAW;
}

static void action_digit_clamp(const state_machine_t *m, state_machine_event_id_t ev, void *payload)
{
    ConnList *l = list_from_machine(m);
    ConnWidgetDispatch *ctx = (ConnWidgetDispatch *)payload;
    (void)ev;
    if (l == NULL || ctx == NULL)
        return;
    clamp_scroll(l);
    ctx->command = CONN_ACT_REDRAW;
}

static const state_machine_state_id_t SrcActive[] = {LIST_ST_ACTIVE};

static const state_machine_guard_id_t GuardHasEntries[] = {LIST_G_HAS_ENTRIES};
static const state_machine_guard_id_t GuardDigitValid[] = {LIST_G_DIGIT_VALID};

static const state_machine_transition_def_t NavTransitions[] = {
    STATE_MACHINE_TRANSITION_NG(SrcActive, 1, LIST_ST_ACTIVE, LIST_A_NAV, 0),
};

static const state_machine_transition_def_t EnterTransitions[] = {
    STATE_MACHINE_TRANSITION(SrcActive, 1, GuardHasEntries, 1, LIST_ST_CHOSEN, LIST_A_CHOOSE_ENTER,
                             0),
    STATE_MACHINE_TRANSITION_NG(SrcActive, 1, LIST_ST_ACTIVE, LIST_A_NOOP_REDRAW, 1),
};

static const state_machine_transition_def_t EscapeTransitions[] = {
    STATE_MACHINE_TRANSITION_NG(SrcActive, 1, LIST_ST_CANCELLED, LIST_A_CANCEL, 0),
};

static const state_machine_transition_def_t DigitTransitions[] = {
    STATE_MACHINE_TRANSITION(SrcActive, 1, GuardDigitValid, 1, LIST_ST_CHOSEN, LIST_A_CHOOSE_DIGIT,
                             0),
    STATE_MACHINE_TRANSITION_NG(SrcActive, 1, LIST_ST_ACTIVE, LIST_A_DIGIT_CLAMP, 1),
};

static const state_machine_event_def_t ListEvents[] = {
    {.id = LIST_EV_KEY_UP, .transition_count = 1, .transitions = NavTransitions, .name = "key_up"},
    {.id = LIST_EV_KEY_DOWN,
     .transition_count = 1,
     .transitions = NavTransitions,
     .name = "key_down"},
    {.id = LIST_EV_KEY_HOME,
     .transition_count = 1,
     .transitions = NavTransitions,
     .name = "key_home"},
    {.id = LIST_EV_KEY_END,
     .transition_count = 1,
     .transitions = NavTransitions,
     .name = "key_end"},
    {.id = LIST_EV_KEY_ENTER,
     .transition_count = 2,
     .transitions = EnterTransitions,
     .name = "key_enter"},
    {.id = LIST_EV_KEY_ESCAPE,
     .transition_count = 1,
     .transitions = EscapeTransitions,
     .name = "key_escape"},
    {.id = LIST_EV_KEY_DIGIT,
     .transition_count = 2,
     .transitions = DigitTransitions,
     .name = "key_digit"},
    {.id = LIST_EV_KEY_PAGE_UP,
     .transition_count = 1,
     .transitions = NavTransitions,
     .name = "key_page_up"},
    {.id = LIST_EV_KEY_PAGE_DOWN,
     .transition_count = 1,
     .transitions = NavTransitions,
     .name = "key_page_down"},
};

static const state_machine_guard_def_t ListGuards[] = {
    {.fn = guard_has_entries, .name = "has_entries"},
    {.fn = guard_digit_valid, .name = "digit_valid"},
};

static const state_machine_action_def_t ListActions[] = {
    {.fn = action_nav, .name = "nav"},
    {.fn = action_choose_enter, .name = "choose_enter"},
    {.fn = action_cancel, .name = "cancel"},
    {.fn = action_choose_digit, .name = "choose_digit"},
    {.fn = action_digit_clamp, .name = "digit_clamp"},
    {.fn = conn_widget_action_noop_redraw, .name = "noop_redraw"},
};

static const state_machine_def_t ConnListMachine = {
    STATE_MACHINE_DEF_HEADER, .name = "conn_list",       .states = ConnWidgetStates3,
    .events = ListEvents,     .guards = ListGuards,      .actions = ListActions,
    .state_count = 3,         .initial = LIST_ST_ACTIVE, .event_count = 9,
    .guard_count = 2,         .action_count = 6,
};

static state_machine_event_id_t event_for_key(ConnKey key)
{
    switch (key) {
    case CONN_KEY_UP:
        return LIST_EV_KEY_UP;
    case CONN_KEY_DOWN:
        return LIST_EV_KEY_DOWN;
    case CONN_KEY_PAGE_UP:
        return LIST_EV_KEY_PAGE_UP;
    case CONN_KEY_PAGE_DOWN:
        return LIST_EV_KEY_PAGE_DOWN;
    case CONN_KEY_HOME:
        return LIST_EV_KEY_HOME;
    case CONN_KEY_END:
        return LIST_EV_KEY_END;
    case CONN_KEY_ENTER:
        return LIST_EV_KEY_ENTER;
    case CONN_KEY_ESCAPE:
        return LIST_EV_KEY_ESCAPE;
    case CONN_KEY_DIGIT:
        return LIST_EV_KEY_DIGIT;
    default:
        return STATE_MACHINE_ID_NONE;
    }
}

VOID conn_list_init(ConnList *l, const char *title)
{
    char *p = (char *)l;
    UINTN i;
    for (i = 0; i < sizeof(*l); i++)
        p[i] = 0;
    conn_cp_bounded(l->title, title, sizeof l->title);
    l->status = CONN_LIST_ACTIVE;
    (void)state_machine_init(&l->machine, &ConnListMachine, l);
}

VOID conn_list_add_info(ConnList *l, const char *line)
{
    if (l->info_count < CONN_LIST_INFO) {
        conn_cp_bounded(l->info[l->info_count++], line, CONN_LIST_COLS);
    }
}

BOOLEAN conn_list_add(ConnList *l, const char *item)
{
    if (l->count >= CONN_LIST_MAX)
        return FALSE;
    conn_cp_bounded(l->item[l->count++], item, CONN_LIST_COLS);
    return TRUE;
}

VOID conn_list_set_default(ConnList *l, UINTN idx)
{
    if (l == NULL || l->count == 0)
        return;
    l->selected = (idx < l->count) ? idx : l->count - 1;
    clamp_scroll(l);
}

ConnCommand conn_list_update(ConnList *l, ConnEvent ev)
{
    state_machine_event_id_t sm_ev;
    ConnWidgetDispatch ctx;

    if (l == NULL || ev.type != CONN_EVENT_KEY)
        return CONN_ACT_NONE;

    sm_ev = event_for_key(ev.key);
    if (sm_ev == STATE_MACHINE_ID_NONE)
        return CONN_ACT_NONE;

    ctx.digit = ev.digit;
    ctx.command = CONN_ACT_NONE;
    (void)state_machine_dispatch(&l->machine, sm_ev, &ctx);
    return ctx.command;
}

static VOID put_item(ConnGrid *g, INTN row, UINTN num, const char *title, const ConnTextTheme *th,
                     BOOLEAN sel)
{
    char nb[3];
    nb[0] = (num >= 10 && num < 100) ? (char)('0' + (num / 10) % 10) : ' ';
    nb[1] = (num < 100) ? (char)('0' + num % 10) : ' ';
    nb[2] = '\0';

    if (sel) {
        ConnColor bg = th->frame, ink = conn_contrast(bg);
        INTN c;
        for (c = 1; c <= CONN_COLS - 2; c++)
            conn_grid_set(g, c, row, 0x20, ink, bg, TRUE);
        conn_grid_puts(g, 2, row, nb, ink, bg, TRUE);
        conn_grid_puts(g, 5, row, title, ink, bg, TRUE);
    }
    else {
        conn_grid_puts(g, 2, row, nb, th->frame, CONN_TRANSPARENT, TRUE);
        conn_grid_puts(g, 5, row, title, th->text, CONN_TRANSPARENT, FALSE);
    }
}

VOID conn_list_render(ConnGrid *g, const ConnList *l, const ConnTextTheme *th)
{
    INTN start, shown, i;
    UINTN k;

    conn_grid_clear(g, th);
    conn_grid_frame(g, th);

    conn_grid_puts(g, 3, 1, "MERIDIAN", th->frame, CONN_TRANSPARENT, TRUE);
    conn_grid_puts(g, 12, 1, "BOOT MANAGER \xC2\xB7 CONN", th->text, CONN_TRANSPARENT, FALSE);
    conn_grid_puts(g, 3, 3, l->title, th->hi, CONN_TRANSPARENT, TRUE);

    for (k = 0; k < l->info_count && k < 2; k++) {
        conn_grid_puts(g, 3, 4 + (INTN)k, l->info[k], th->dim, CONN_TRANSPARENT, FALSE);
    }

    start = 0;
    if ((INTN)l->count > LIST_VISIBLE) {
        start = (INTN)l->first;
        if (start > (INTN)l->count - LIST_VISIBLE)
            start = (INTN)l->count - LIST_VISIBLE;
        if (start < 0)
            start = 0;
    }
    shown = (INTN)l->count - start;
    if (shown > LIST_VISIBLE)
        shown = LIST_VISIBLE;
    for (i = 0; i < shown; i++) {
        INTN idx = start + i;
        put_item(g, LIST_TOP + i, (UINTN)idx + 1, l->item[idx], th, (UINTN)idx == l->selected);
    }
    if (start > 0)
        conn_grid_set(g, CONN_COLS - 2, LIST_TOP, 0x2191, th->dim, CONN_TRANSPARENT, FALSE);
    if (start + LIST_VISIBLE < (INTN)l->count)
        conn_grid_set(g, CONN_COLS - 2, LIST_TOP + LIST_VISIBLE - 1, 0x2193, th->dim,
                      CONN_TRANSPARENT, FALSE);
    if (l->count == 0)
        conn_grid_puts(g, 5, LIST_TOP, "(no entries)", th->dim, CONN_TRANSPARENT, FALSE);

    conn_grid_puts(g, 3, CONN_ROWS - 2, "\xE2\x86\x91\xE2\x86\x93 select   ENTER choose   ESC back",
                   th->dim, CONN_TRANSPARENT, FALSE);
}
