// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#include "conn_core.h"

typedef enum
{
    CONN_UI_ST_BOOTSTRAP = 0,
    CONN_UI_ST_EMPTY = 1,
    CONN_UI_ST_AUTOBOOT = 2,
    CONN_UI_ST_MANUAL = 3,
    CONN_UI_ST_SAVER = 4,
    CONN_UI_ST_LAUNCH = 5,

} ConnUiSmState;

typedef enum
{
    CONN_UI_EV_INIT = 0,
    CONN_UI_EV_KEY_UP = 1,
    CONN_UI_EV_KEY_DOWN = 2,
    CONN_UI_EV_KEY_DIGIT = 3,
    CONN_UI_EV_KEY_ENTER = 4,
    CONN_UI_EV_KEY_OTHER = 5,
    CONN_UI_EV_TICK = 6,
    CONN_UI_EV_REMOTE = 7,
    CONN_UI_EV_KEY_PAGE_UP = 8,
    CONN_UI_EV_KEY_PAGE_DOWN = 9,
} ConnUiSmEvent;

typedef enum
{
    CONN_UI_G_LIST_EMPTY = 0,
    CONN_UI_G_LIST_AUTOBOOT = 1,
    CONN_UI_G_LIST_ENTRIES = 2,
    CONN_UI_G_TICK_LAUNCH = 3,
    CONN_UI_G_TICK_REDRAW = 4,
    CONN_UI_G_IDLE_READY = 5,
} ConnUiSmGuard;

typedef enum
{
    CONN_UI_A_INIT_EMPTY = 0,
    CONN_UI_A_INIT_AUTOBOOT = 1,
    CONN_UI_A_INIT_MANUAL = 2,
    CONN_UI_A_KEY = 3,
    CONN_UI_A_TICK_LAUNCH = 4,
    CONN_UI_A_TICK_REDRAW = 5,
    CONN_UI_A_TICK_IDLE = 6,
    CONN_UI_A_ENTER_SAVER = 7,
    CONN_UI_A_TICK_SAVER = 8,
    CONN_UI_A_WAKE = 9,
    CONN_UI_A_REMOTE = 10,
} ConnUiSmAction;

typedef struct
{
    const ConnEntryList *list;
    INTN digit;
    ConnWakeIntent wake;
    UINTN wake_index;
    ConnAction action;
} ConnUiDispatch;

static ConnUiState *ui_from_machine(const state_machine_t *m)
{
    return (ConnUiState *)state_machine_userdata(m);
}

static const ConnEntryList *list_from_payload(const void *payload)
{
    const ConnUiDispatch *ctx = (const ConnUiDispatch *)payload;
    return ctx != NULL ? ctx->list : NULL;
}

static BOOLEAN ui_tick_active(const ConnUiState *st, const ConnEntryList *list)
{
    return st != NULL && list != NULL && st->autoboot && !st->cancelled && list->count > 0;
}

static VOID ui_bump_idle(ConnUiState *st)
{
    if (st != NULL && st->idle_s < 0xFFFFFFFFu) {
        st->idle_s++;
    }
}

static VOID ui_reset_idle(ConnUiState *st)
{
    if (st != NULL) {
        st->idle_s = 0;
        st->saver_frame = 0;
    }
}

static bool guard_list_empty(const state_machine_t *m, state_machine_event_id_t event,
                             const void *payload)
{
    const ConnEntryList *list = list_from_payload(payload);
    (void)m;
    (void)event;
    return list != NULL && list->count == 0;
}

static bool guard_list_autoboot(const state_machine_t *m, state_machine_event_id_t event,
                                const void *payload)
{
    const ConnEntryList *list = list_from_payload(payload);
    (void)m;
    (void)event;
    return list != NULL && list->count > 0 && list->allow_autoboot;
}

static bool guard_list_entries(const state_machine_t *m, state_machine_event_id_t event,
                               const void *payload)
{
    const ConnEntryList *list = list_from_payload(payload);
    (void)m;
    (void)event;
    return list != NULL && list->count > 0;
}

static bool guard_tick_launch(const state_machine_t *m, state_machine_event_id_t event,
                              const void *payload)
{
    const ConnEntryList *list = list_from_payload(payload);
    const ConnUiState *st = (const ConnUiState *)state_machine_userdata(m);
    (void)event;
    return ui_tick_active(st, list) && st->countdown_s <= 1;
}

static bool guard_tick_redraw(const state_machine_t *m, state_machine_event_id_t event,
                              const void *payload)
{
    const ConnEntryList *list = list_from_payload(payload);
    const ConnUiState *st = (const ConnUiState *)state_machine_userdata(m);
    (void)event;
    return ui_tick_active(st, list) && st->countdown_s > 1;
}

static bool guard_idle_ready(const state_machine_t *m, state_machine_event_id_t event,
                             const void *payload)
{
    const ConnUiState *st = (const ConnUiState *)state_machine_userdata(m);
    (void)event;
    (void)payload;

    return st != NULL && st->screensaver_s > 0 && st->idle_s >= st->screensaver_s - 1;
}

static void init_common(ConnUiState *st, const ConnEntryList *list)
{
    st->selected = list != NULL && list->default_index < list->count ? list->default_index : 0;
    st->cancelled = FALSE;
    st->status = CONN_UI_OK;
    st->idle_s = 0;
    st->saver_frame = 0;
    st->saver_active = FALSE;
}

static void action_init_empty(const state_machine_t *m, state_machine_event_id_t event,
                              void *payload)
{
    ConnUiState *st = ui_from_machine(m);
    const ConnEntryList *list = list_from_payload(payload);
    (void)event;
    init_common(st, list);
    st->autoboot = FALSE;
    st->countdown_s = 0;
    st->status = CONN_UI_EMPTY;
}

static void action_init_autoboot(const state_machine_t *m, state_machine_event_id_t event,
                                 void *payload)
{
    ConnUiState *st = ui_from_machine(m);
    const ConnEntryList *list = list_from_payload(payload);
    (void)event;
    init_common(st, list);
    st->autoboot = TRUE;
    st->countdown_s = CONN_AUTOBOOT_SECONDS;
}

static void action_init_manual(const state_machine_t *m, state_machine_event_id_t event,
                               void *payload)
{
    ConnUiState *st = ui_from_machine(m);
    const ConnEntryList *list = list_from_payload(payload);
    (void)event;
    init_common(st, list);
    st->autoboot = FALSE;
    st->countdown_s = 0;
}

static void action_key(const state_machine_t *m, state_machine_event_id_t event, void *payload)
{
    ConnUiState *st = ui_from_machine(m);
    ConnUiDispatch *ctx = (ConnUiDispatch *)payload;
    const ConnEntryList *list = ctx != NULL ? ctx->list : NULL;
    BOOLEAN was_running;

    if (st == NULL || ctx == NULL) {
        return;
    }
    ui_reset_idle(st);
    st->saver_active = FALSE;
    if (list == NULL || list->count == 0) {
        return;
    }

    was_running = st->autoboot;
    st->autoboot = FALSE;
    st->cancelled = TRUE;
    st->status = CONN_UI_OK;

    switch (event) {
    case CONN_UI_EV_KEY_UP:
        if (st->selected > 0) {
            st->selected--;
        }
        ctx->action = CONN_ACT_REDRAW;
        break;
    case CONN_UI_EV_KEY_DOWN:
        if (st->selected + 1 < list->count) {
            st->selected++;
        }
        ctx->action = CONN_ACT_REDRAW;
        break;
    case CONN_UI_EV_KEY_PAGE_UP:
        st->selected = conn_page_step(st->selected, list->count, CONN_MENU_PAGE_ROWS, FALSE);
        ctx->action = CONN_ACT_REDRAW;
        break;
    case CONN_UI_EV_KEY_PAGE_DOWN:
        st->selected = conn_page_step(st->selected, list->count, CONN_MENU_PAGE_ROWS, TRUE);
        ctx->action = CONN_ACT_REDRAW;
        break;
    case CONN_UI_EV_KEY_DIGIT:
        if (ctx->digit >= 1 && (UINTN)ctx->digit <= list->count) {
            st->selected = (UINTN)(ctx->digit - 1);
        }
        ctx->action = CONN_ACT_REDRAW;
        break;
    case CONN_UI_EV_KEY_ENTER:
        ctx->action = CONN_ACT_LAUNCH;
        break;
    default:
        ctx->action = was_running ? CONN_ACT_REDRAW : CONN_ACT_NONE;
        break;
    }
}

static void action_wake(const state_machine_t *m, state_machine_event_id_t event, void *payload)
{
    ConnUiState *st = ui_from_machine(m);
    ConnUiDispatch *ctx = (ConnUiDispatch *)payload;
    const ConnEntryList *list = ctx != NULL ? ctx->list : NULL;
    (void)event;

    if (st == NULL || ctx == NULL) {
        return;
    }

    ui_reset_idle(st);
    st->saver_active = FALSE;
    st->autoboot = FALSE;
    st->cancelled = TRUE;
    st->status = (list != NULL && list->count == 0) ? CONN_UI_EMPTY : CONN_UI_OK;
    ctx->action = CONN_ACT_REDRAW;
}

static void action_remote(const state_machine_t *m, state_machine_event_id_t event, void *payload)
{
    ConnUiState *st = ui_from_machine(m);
    ConnUiDispatch *ctx = (ConnUiDispatch *)payload;
    const ConnEntryList *list = ctx != NULL ? ctx->list : NULL;
    (void)event;

    if (st == NULL || ctx == NULL) {
        return;
    }

    ui_reset_idle(st);
    st->saver_active = FALSE;
    st->autoboot = FALSE;
    st->cancelled = TRUE;
    st->status = (list != NULL && list->count == 0) ? CONN_UI_EMPTY : CONN_UI_OK;

    if (ctx->wake == CONN_WAKE_SELECT || ctx->wake == CONN_WAKE_LAUNCH) {
        if (list == NULL || ctx->wake_index >= list->count) {
            ctx->action = CONN_ACT_REDRAW;
            return;
        }
        st->selected = ctx->wake_index;
        ctx->action = (ctx->wake == CONN_WAKE_LAUNCH) ? CONN_ACT_LAUNCH : CONN_ACT_REDRAW;
        return;
    }

    ctx->action = CONN_ACT_REDRAW;
}

static void action_tick_launch(const state_machine_t *m, state_machine_event_id_t event,
                               void *payload)
{
    ConnUiState *st = ui_from_machine(m);
    ConnUiDispatch *ctx = (ConnUiDispatch *)payload;
    const ConnEntryList *list = ctx != NULL ? ctx->list : NULL;
    (void)event;

    if (st == NULL || ctx == NULL || list == NULL) {
        return;
    }

    st->countdown_s = 0;
    st->autoboot = FALSE;
    st->saver_active = FALSE;
    st->selected = list->default_index < list->count ? list->default_index : 0;
    ctx->action = CONN_ACT_LAUNCH;
}

static void action_tick_redraw(const state_machine_t *m, state_machine_event_id_t event,
                               void *payload)
{
    ConnUiState *st = ui_from_machine(m);
    ConnUiDispatch *ctx = (ConnUiDispatch *)payload;
    (void)event;

    if (st == NULL || ctx == NULL) {
        return;
    }
    if (st->countdown_s > 0) {
        st->countdown_s--;
    }
    ctx->action = CONN_ACT_REDRAW;
}

static void action_tick_idle(const state_machine_t *m, state_machine_event_id_t event,
                             void *payload)
{
    ConnUiState *st = ui_from_machine(m);
    (void)event;
    (void)payload;

    ui_bump_idle(st);
}

static void action_enter_saver(const state_machine_t *m, state_machine_event_id_t event,
                               void *payload)
{
    ConnUiState *st = ui_from_machine(m);
    ConnUiDispatch *ctx = (ConnUiDispatch *)payload;
    (void)event;

    if (st == NULL || ctx == NULL) {
        return;
    }
    ui_bump_idle(st);
    st->saver_frame = 0;
    st->saver_active = TRUE;
    st->autoboot = FALSE;
    ctx->action = CONN_ACT_REDRAW;
}

static void action_tick_saver(const state_machine_t *m, state_machine_event_id_t event,
                              void *payload)
{
    ConnUiState *st = ui_from_machine(m);
    ConnUiDispatch *ctx = (ConnUiDispatch *)payload;
    (void)event;

    if (st == NULL || ctx == NULL) {
        return;
    }
    ui_bump_idle(st);
    st->saver_active = TRUE;
    if (st->saver_frame < 0xFFFFFFFFu) {
        st->saver_frame++;
    }
    ctx->action = CONN_ACT_REDRAW;
}

static const state_machine_state_def_t ConnUiStates[] = {
    {.id = CONN_UI_ST_BOOTSTRAP,
     .parent = STATE_MACHINE_ID_NONE,
     .initial_child = STATE_MACHINE_ID_NONE,
     .depth = 0,
     .flags = 0,
     .name = "bootstrap"},
    {.id = CONN_UI_ST_EMPTY,
     .parent = STATE_MACHINE_ID_NONE,
     .initial_child = STATE_MACHINE_ID_NONE,
     .depth = 0,
     .flags = 0,
     .name = "empty"},
    {.id = CONN_UI_ST_AUTOBOOT,
     .parent = STATE_MACHINE_ID_NONE,
     .initial_child = STATE_MACHINE_ID_NONE,
     .depth = 0,
     .flags = 0,
     .name = "autoboot"},
    {.id = CONN_UI_ST_MANUAL,
     .parent = STATE_MACHINE_ID_NONE,
     .initial_child = STATE_MACHINE_ID_NONE,
     .depth = 0,
     .flags = 0,
     .name = "manual"},
    {.id = CONN_UI_ST_SAVER,
     .parent = STATE_MACHINE_ID_NONE,
     .initial_child = STATE_MACHINE_ID_NONE,
     .depth = 0,
     .flags = 0,
     .name = "saver"},
    {.id = CONN_UI_ST_LAUNCH,
     .parent = STATE_MACHINE_ID_NONE,
     .initial_child = STATE_MACHINE_ID_NONE,
     .depth = 0,
     .flags = 0,
     .name = "launching"},
};

static const state_machine_state_id_t SrcBootstrap[] = {
    CONN_UI_ST_BOOTSTRAP,
};
static const state_machine_state_id_t SrcEmpty[] = {
    CONN_UI_ST_EMPTY,
};
static const state_machine_state_id_t SrcManual[] = {
    CONN_UI_ST_MANUAL,
};
static const state_machine_state_id_t SrcSaver[] = {
    CONN_UI_ST_SAVER,
};
static const state_machine_state_id_t SrcInteractive[] = {
    CONN_UI_ST_AUTOBOOT,
    CONN_UI_ST_MANUAL,
};
static const state_machine_state_id_t SrcAutoboot[] = {
    CONN_UI_ST_AUTOBOOT,
};
static const state_machine_state_id_t SrcRemote[] = {
    CONN_UI_ST_EMPTY,
    CONN_UI_ST_AUTOBOOT,
    CONN_UI_ST_MANUAL,
    CONN_UI_ST_SAVER,
};

static const state_machine_guard_id_t GuardListEmpty[] = {
    CONN_UI_G_LIST_EMPTY,
};
static const state_machine_guard_id_t GuardListAutoboot[] = {
    CONN_UI_G_LIST_AUTOBOOT,
};
static const state_machine_guard_id_t GuardListEntries[] = {
    CONN_UI_G_LIST_ENTRIES,
};
static const state_machine_guard_id_t GuardTickLaunch[] = {
    CONN_UI_G_TICK_LAUNCH,
};
static const state_machine_guard_id_t GuardTickRedraw[] = {
    CONN_UI_G_TICK_REDRAW,
};
static const state_machine_guard_id_t GuardIdleReady[] = {
    CONN_UI_G_IDLE_READY,
};

static const state_machine_transition_def_t InitTransitions[] = {
    STATE_MACHINE_TRANSITION(SrcBootstrap, 1, GuardListEmpty, 1, CONN_UI_ST_EMPTY,
                             CONN_UI_A_INIT_EMPTY, 0),
    STATE_MACHINE_TRANSITION(SrcBootstrap, 1, GuardListAutoboot, 1, CONN_UI_ST_AUTOBOOT,
                             CONN_UI_A_INIT_AUTOBOOT, 1),
    STATE_MACHINE_TRANSITION(SrcBootstrap, 1, GuardListEntries, 1, CONN_UI_ST_MANUAL,
                             CONN_UI_A_INIT_MANUAL, 2),
};

static const state_machine_transition_def_t KeyTransition[] = {
    STATE_MACHINE_TRANSITION_NG(SrcInteractive, 2, CONN_UI_ST_MANUAL, CONN_UI_A_KEY, 0),
    STATE_MACHINE_TRANSITION_NG(SrcSaver, 1, CONN_UI_ST_MANUAL, CONN_UI_A_WAKE, 1),
};

static const state_machine_transition_def_t TickTransitions[] = {
    STATE_MACHINE_TRANSITION(SrcAutoboot, 1, GuardTickLaunch, 1, CONN_UI_ST_LAUNCH,
                             CONN_UI_A_TICK_LAUNCH, 0),
    STATE_MACHINE_TRANSITION(SrcAutoboot, 1, GuardTickRedraw, 1, CONN_UI_ST_AUTOBOOT,
                             CONN_UI_A_TICK_REDRAW, 1),
    STATE_MACHINE_TRANSITION(SrcManual, 1, GuardIdleReady, 1, CONN_UI_ST_SAVER,
                             CONN_UI_A_ENTER_SAVER, 2),
    STATE_MACHINE_TRANSITION(SrcEmpty, 1, GuardIdleReady, 1, CONN_UI_ST_SAVER,
                             CONN_UI_A_ENTER_SAVER, 3),
    STATE_MACHINE_TRANSITION_NG(SrcSaver, 1, CONN_UI_ST_SAVER, CONN_UI_A_TICK_SAVER, 4),
    STATE_MACHINE_TRANSITION_NG(SrcManual, 1, CONN_UI_ST_MANUAL, CONN_UI_A_TICK_IDLE, 5),
    STATE_MACHINE_TRANSITION_NG(SrcEmpty, 1, CONN_UI_ST_EMPTY, CONN_UI_A_TICK_IDLE, 6),
};

static const state_machine_transition_def_t RemoteTransition[] = {
    STATE_MACHINE_TRANSITION_NG(SrcRemote, 4, CONN_UI_ST_MANUAL, CONN_UI_A_REMOTE, 0),
};

static const state_machine_event_def_t ConnUiEvents[] = {
    {.id = CONN_UI_EV_INIT, .transition_count = 3, .transitions = InitTransitions, .name = "init"},
    {.id = CONN_UI_EV_KEY_UP,
     .transition_count = 2,
     .transitions = KeyTransition,
     .name = "key_up"},
    {.id = CONN_UI_EV_KEY_DOWN,
     .transition_count = 2,
     .transitions = KeyTransition,
     .name = "key_down"},
    {.id = CONN_UI_EV_KEY_DIGIT,
     .transition_count = 2,
     .transitions = KeyTransition,
     .name = "key_digit"},
    {.id = CONN_UI_EV_KEY_ENTER,
     .transition_count = 2,
     .transitions = KeyTransition,
     .name = "key_enter"},
    {.id = CONN_UI_EV_KEY_OTHER,
     .transition_count = 2,
     .transitions = KeyTransition,
     .name = "key_other"},
    {.id = CONN_UI_EV_TICK, .transition_count = 7, .transitions = TickTransitions, .name = "tick"},
    {.id = CONN_UI_EV_REMOTE,
     .transition_count = 1,
     .transitions = RemoteTransition,
     .name = "remote"},
    {.id = CONN_UI_EV_KEY_PAGE_UP,
     .transition_count = 2,
     .transitions = KeyTransition,
     .name = "key_page_up"},
    {.id = CONN_UI_EV_KEY_PAGE_DOWN,
     .transition_count = 2,
     .transitions = KeyTransition,
     .name = "key_page_down"},
};

static const state_machine_guard_def_t ConnUiGuards[] = {
    {.fn = guard_list_empty, .name = "list_empty"},
    {.fn = guard_list_autoboot, .name = "list_autoboot"},
    {.fn = guard_list_entries, .name = "list_entries"},
    {.fn = guard_tick_launch, .name = "tick_launch"},
    {.fn = guard_tick_redraw, .name = "tick_redraw"},
    {.fn = guard_idle_ready, .name = "idle_ready"},
};

static const state_machine_action_def_t ConnUiActions[] = {
    {.fn = action_init_empty, .name = "init_empty"},
    {.fn = action_init_autoboot, .name = "init_autoboot"},
    {.fn = action_init_manual, .name = "init_manual"},
    {.fn = action_key, .name = "key"},
    {.fn = action_tick_launch, .name = "tick_launch"},
    {.fn = action_tick_redraw, .name = "tick_redraw"},
    {.fn = action_tick_idle, .name = "tick_idle"},
    {.fn = action_enter_saver, .name = "enter_saver"},
    {.fn = action_tick_saver, .name = "tick_saver"},
    {.fn = action_wake, .name = "wake"},
    {.fn = action_remote, .name = "remote"},
};

static const state_machine_def_t ConnUiMachine = {
    STATE_MACHINE_DEF_HEADER, .name = "conn_ui",
    .states = ConnUiStates,   .events = ConnUiEvents,
    .guards = ConnUiGuards,   .actions = ConnUiActions,
    .state_count = 6,         .initial = CONN_UI_ST_BOOTSTRAP,
    .event_count = 10,        .guard_count = 6,
    .action_count = 11,
};

static state_machine_event_id_t event_for_key(ConnKey key)
{
    switch (key) {
    case CONN_KEY_UP:
        return CONN_UI_EV_KEY_UP;
    case CONN_KEY_DOWN:
        return CONN_UI_EV_KEY_DOWN;
    case CONN_KEY_PAGE_UP:
        return CONN_UI_EV_KEY_PAGE_UP;
    case CONN_KEY_PAGE_DOWN:
        return CONN_UI_EV_KEY_PAGE_DOWN;
    case CONN_KEY_DIGIT:
        return CONN_UI_EV_KEY_DIGIT;
    case CONN_KEY_ENTER:
        return CONN_UI_EV_KEY_ENTER;
    default:
        return CONN_UI_EV_KEY_OTHER;
    }
}

VOID conn_ui_init(ConnUiState *st, const ConnEntryList *list)
{
    ConnUiDispatch ctx = {list, 0, CONN_WAKE_DISMISS, 0, CONN_ACT_NONE};

    st->selected = 0;
    st->countdown_s = 0;
    st->idle_s = 0;
    st->saver_frame = 0;
    st->screensaver_s = 0;
    st->saver_active = FALSE;
    st->autoboot = FALSE;
    st->cancelled = FALSE;
    st->status = CONN_UI_EMPTY;

    if (state_machine_init(&st->machine, &ConnUiMachine, st) != 0) {
        return;
    }
    (void)state_machine_dispatch(&st->machine, CONN_UI_EV_INIT, &ctx);
}

ConnAction conn_ui_on_key(ConnUiState *st, ConnKey key, INTN digit, const ConnEntryList *list)
{
    ConnUiDispatch ctx = {list, digit, CONN_WAKE_DISMISS, 0, CONN_ACT_NONE};
    (void)state_machine_dispatch(&st->machine, event_for_key(key), &ctx);
    return ctx.action;
}

ConnAction conn_ui_on_tick(ConnUiState *st, const ConnEntryList *list)
{
    ConnUiDispatch ctx = {list, 0, CONN_WAKE_DISMISS, 0, CONN_ACT_NONE};
    (void)state_machine_dispatch(&st->machine, CONN_UI_EV_TICK, &ctx);
    return ctx.action;
}

ConnAction conn_ui_on_remote(ConnUiState *st, ConnWakeIntent wake, UINTN wake_index,
                             const ConnEntryList *list)
{
    ConnUiDispatch ctx = {list, 0, wake, wake_index, CONN_ACT_NONE};
    (void)state_machine_dispatch(&st->machine, CONN_UI_EV_REMOTE, &ctx);
    return ctx.action;
}

VOID conn_ui_set_screensaver(ConnUiState *st, UINT32 seconds)
{
    if (st != NULL) {
        st->screensaver_s = seconds;
        if (seconds == 0) {
            st->idle_s = 0;
            st->saver_frame = 0;
        }
    }
}
