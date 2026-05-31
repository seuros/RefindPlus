// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#include "conn_core.h"

#define PE_COL 3
#define BUF_COL 8
#define PROMPT_ROW 12
#define VIEW (CONN_COLS - 3 - BUF_COL)

typedef enum
{
    EDIT_ST_ACTIVE = 0,
    EDIT_ST_COMMITTED = 1,
    EDIT_ST_CANCELLED = 2,
} EditSmState;

typedef enum
{
    EDIT_EV_TICK = 0,
    EDIT_EV_KEY_ENTER = 1,
    EDIT_EV_KEY_ESCAPE = 2,
    EDIT_EV_KEY_LEFT = 3,
    EDIT_EV_KEY_RIGHT = 4,
    EDIT_EV_KEY_HOME = 5,
    EDIT_EV_KEY_END = 6,
    EDIT_EV_KEY_BACKSPACE = 7,
    EDIT_EV_KEY_DELETE = 8,
    EDIT_EV_KEY_TEXT = 9,
} EditSmEvent;

typedef enum
{
    EDIT_G_TEXT_INSERTABLE = 0,
} EditSmGuard;

typedef enum
{
    EDIT_A_TICK = 0,
    EDIT_A_COMMIT = 1,
    EDIT_A_CANCEL = 2,
    EDIT_A_CURSOR = 3,
    EDIT_A_BACKSPACE = 4,
    EDIT_A_DELETE = 5,
    EDIT_A_INSERT = 6,
    EDIT_A_NOOP_REDRAW = 7,
} EditSmAction;

typedef struct
{
    UINT32 ch;
    ConnCommand command;
} ConnEditDispatch;

static UINTN sl_len(const char *s)
{
    UINTN n = 0;
    if (s != NULL)
        while (s[n])
            n++;
    return n;
}

static VOID clamp_scroll(ConnLineEditModel *m)
{
    if (m->cursor < m->first) {
        m->first = m->cursor;
    }
    else if (m->cursor >= m->first + (UINTN)VIEW) {
        m->first = m->cursor - (UINTN)VIEW + 1;
    }
}

static ConnLineEditModel *edit_from_machine(const state_machine_t *m)
{
    return (ConnLineEditModel *)state_machine_userdata(m);
}

static bool guard_text_insertable(const state_machine_t *m, state_machine_event_id_t ev,
                                  const void *payload)
{
    const ConnLineEditModel *em = (const ConnLineEditModel *)state_machine_userdata(m);
    const ConnEditDispatch *ctx = (const ConnEditDispatch *)payload;
    (void)ev;
    if (em == NULL || ctx == NULL)
        return false;
    return ctx->ch >= 0x20u && ctx->ch < 0x7Fu && em->len + 1 < CONN_EDIT_MAX;
}

static void action_tick(const state_machine_t *m, state_machine_event_id_t ev, void *payload)
{
    ConnLineEditModel *em = edit_from_machine(m);
    ConnEditDispatch *ctx = (ConnEditDispatch *)payload;
    (void)ev;
    if (em == NULL || ctx == NULL)
        return;
    em->cursor_on = (BOOLEAN)!em->cursor_on;
    ctx->command = CONN_ACT_REDRAW;
}

static void action_commit(const state_machine_t *m, state_machine_event_id_t ev, void *payload)
{
    ConnLineEditModel *em = edit_from_machine(m);
    ConnEditDispatch *ctx = (ConnEditDispatch *)payload;
    (void)ev;
    if (em == NULL || ctx == NULL)
        return;
    em->cursor_on = TRUE;
    em->status = CONN_EDIT_COMMITTED;
    ctx->command = CONN_ACT_REDRAW;
}

static void action_cancel(const state_machine_t *m, state_machine_event_id_t ev, void *payload)
{
    ConnLineEditModel *em = edit_from_machine(m);
    ConnEditDispatch *ctx = (ConnEditDispatch *)payload;
    (void)ev;
    if (em == NULL || ctx == NULL)
        return;
    em->cursor_on = TRUE;
    em->status = CONN_EDIT_CANCELLED;
    ctx->command = CONN_ACT_REDRAW;
}

static void action_cursor(const state_machine_t *m, state_machine_event_id_t ev, void *payload)
{
    ConnLineEditModel *em = edit_from_machine(m);
    ConnEditDispatch *ctx = (ConnEditDispatch *)payload;
    if (em == NULL || ctx == NULL)
        return;
    em->cursor_on = TRUE;
    switch (ev) {
    case EDIT_EV_KEY_LEFT:
        if (em->cursor > 0)
            em->cursor--;
        break;
    case EDIT_EV_KEY_RIGHT:
        if (em->cursor < em->len)
            em->cursor++;
        break;
    case EDIT_EV_KEY_HOME:
        em->cursor = 0;
        break;
    case EDIT_EV_KEY_END:
        em->cursor = em->len;
        break;
    default:
        break;
    }
    clamp_scroll(em);
    ctx->command = CONN_ACT_REDRAW;
}

static void action_backspace(const state_machine_t *m, state_machine_event_id_t ev, void *payload)
{
    ConnLineEditModel *em = edit_from_machine(m);
    ConnEditDispatch *ctx = (ConnEditDispatch *)payload;
    UINTN i;
    (void)ev;
    if (em == NULL || ctx == NULL)
        return;
    em->cursor_on = TRUE;
    if (em->cursor > 0) {
        for (i = em->cursor - 1; i + 1 < em->len; i++)
            em->buf[i] = em->buf[i + 1];
        em->len--;
        em->cursor--;
        em->buf[em->len] = '\0';
    }
    clamp_scroll(em);
    ctx->command = CONN_ACT_REDRAW;
}

static void action_delete(const state_machine_t *m, state_machine_event_id_t ev, void *payload)
{
    ConnLineEditModel *em = edit_from_machine(m);
    ConnEditDispatch *ctx = (ConnEditDispatch *)payload;
    UINTN i;
    (void)ev;
    if (em == NULL || ctx == NULL)
        return;
    em->cursor_on = TRUE;
    if (em->cursor < em->len) {
        for (i = em->cursor; i + 1 < em->len; i++)
            em->buf[i] = em->buf[i + 1];
        em->len--;
        em->buf[em->len] = '\0';
    }
    clamp_scroll(em);
    ctx->command = CONN_ACT_REDRAW;
}

static void action_insert(const state_machine_t *m, state_machine_event_id_t ev, void *payload)
{
    ConnLineEditModel *em = edit_from_machine(m);
    ConnEditDispatch *ctx = (ConnEditDispatch *)payload;
    UINTN i;
    (void)ev;
    if (em == NULL || ctx == NULL)
        return;
    em->cursor_on = TRUE;
    for (i = em->len; i > em->cursor; i--)
        em->buf[i] = em->buf[i - 1];
    em->buf[em->cursor] = (char)ctx->ch;
    em->len++;
    em->cursor++;
    em->buf[em->len] = '\0';
    clamp_scroll(em);
    ctx->command = CONN_ACT_REDRAW;
}

static void action_noop_redraw(const state_machine_t *m, state_machine_event_id_t ev, void *payload)
{
    ConnLineEditModel *em = edit_from_machine(m);
    ConnEditDispatch *ctx = (ConnEditDispatch *)payload;
    (void)ev;
    if (em == NULL || ctx == NULL)
        return;
    em->cursor_on = TRUE;
    ctx->command = CONN_ACT_REDRAW;
}

static const state_machine_state_id_t SrcActive[] = {EDIT_ST_ACTIVE};

static const state_machine_guard_id_t GuardTextInsertable[] = {EDIT_G_TEXT_INSERTABLE};

static const state_machine_transition_def_t TickTransitions[] = {
    STATE_MACHINE_TRANSITION_NG(SrcActive, 1, EDIT_ST_ACTIVE, EDIT_A_TICK, 0),
};

static const state_machine_transition_def_t CommitTransitions[] = {
    STATE_MACHINE_TRANSITION_NG(SrcActive, 1, EDIT_ST_COMMITTED, EDIT_A_COMMIT, 0),
};

static const state_machine_transition_def_t CancelTransitions[] = {
    STATE_MACHINE_TRANSITION_NG(SrcActive, 1, EDIT_ST_CANCELLED, EDIT_A_CANCEL, 0),
};

static const state_machine_transition_def_t CursorTransitions[] = {
    STATE_MACHINE_TRANSITION_NG(SrcActive, 1, EDIT_ST_ACTIVE, EDIT_A_CURSOR, 0),
};

static const state_machine_transition_def_t BackspaceTransitions[] = {
    STATE_MACHINE_TRANSITION_NG(SrcActive, 1, EDIT_ST_ACTIVE, EDIT_A_BACKSPACE, 0),
};

static const state_machine_transition_def_t DeleteTransitions[] = {
    STATE_MACHINE_TRANSITION_NG(SrcActive, 1, EDIT_ST_ACTIVE, EDIT_A_DELETE, 0),
};

static const state_machine_transition_def_t TextTransitions[] = {
    STATE_MACHINE_TRANSITION(SrcActive, 1, GuardTextInsertable, 1, EDIT_ST_ACTIVE, EDIT_A_INSERT,
                             0),
    STATE_MACHINE_TRANSITION_NG(SrcActive, 1, EDIT_ST_ACTIVE, EDIT_A_NOOP_REDRAW, 1),
};

static const state_machine_state_def_t EditStates[] = {
    {.id = EDIT_ST_ACTIVE,
     .parent = STATE_MACHINE_ID_NONE,
     .initial_child = STATE_MACHINE_ID_NONE,
     .depth = 0,
     .flags = 0,
     .name = "active"},
    {.id = EDIT_ST_COMMITTED,
     .parent = STATE_MACHINE_ID_NONE,
     .initial_child = STATE_MACHINE_ID_NONE,
     .depth = 0,
     .flags = 0,
     .name = "committed"},
    {.id = EDIT_ST_CANCELLED,
     .parent = STATE_MACHINE_ID_NONE,
     .initial_child = STATE_MACHINE_ID_NONE,
     .depth = 0,
     .flags = 0,
     .name = "cancelled"},
};

static const state_machine_event_def_t EditEvents[] = {
    {.id = EDIT_EV_TICK, .transition_count = 1, .transitions = TickTransitions, .name = "tick"},
    {.id = EDIT_EV_KEY_ENTER,
     .transition_count = 1,
     .transitions = CommitTransitions,
     .name = "key_enter"},
    {.id = EDIT_EV_KEY_ESCAPE,
     .transition_count = 1,
     .transitions = CancelTransitions,
     .name = "key_escape"},
    {.id = EDIT_EV_KEY_LEFT,
     .transition_count = 1,
     .transitions = CursorTransitions,
     .name = "key_left"},
    {.id = EDIT_EV_KEY_RIGHT,
     .transition_count = 1,
     .transitions = CursorTransitions,
     .name = "key_right"},
    {.id = EDIT_EV_KEY_HOME,
     .transition_count = 1,
     .transitions = CursorTransitions,
     .name = "key_home"},
    {.id = EDIT_EV_KEY_END,
     .transition_count = 1,
     .transitions = CursorTransitions,
     .name = "key_end"},
    {.id = EDIT_EV_KEY_BACKSPACE,
     .transition_count = 1,
     .transitions = BackspaceTransitions,
     .name = "key_backspace"},
    {.id = EDIT_EV_KEY_DELETE,
     .transition_count = 1,
     .transitions = DeleteTransitions,
     .name = "key_delete"},
    {.id = EDIT_EV_KEY_TEXT,
     .transition_count = 2,
     .transitions = TextTransitions,
     .name = "key_text"},
};

static const state_machine_guard_def_t EditGuards[] = {
    {.fn = guard_text_insertable, .name = "text_insertable"},
};

static const state_machine_action_def_t EditActions[] = {
    {.fn = action_tick, .name = "tick"},
    {.fn = action_commit, .name = "commit"},
    {.fn = action_cancel, .name = "cancel"},
    {.fn = action_cursor, .name = "cursor"},
    {.fn = action_backspace, .name = "backspace"},
    {.fn = action_delete, .name = "delete"},
    {.fn = action_insert, .name = "insert"},
    {.fn = action_noop_redraw, .name = "noop_redraw"},
};

static const state_machine_def_t ConnEditMachine = {
    STATE_MACHINE_DEF_HEADER, .name = "conn_edit",       .states = EditStates,
    .events = EditEvents,     .guards = EditGuards,      .actions = EditActions,
    .state_count = 3,         .initial = EDIT_ST_ACTIVE, .event_count = 10,
    .guard_count = 1,         .action_count = 8,
};

static state_machine_event_id_t event_for_ev(ConnEvent ev)
{
    if (ev.type == CONN_EVENT_TICK)
        return EDIT_EV_TICK;
    if (ev.type != CONN_EVENT_KEY)
        return STATE_MACHINE_ID_NONE;
    switch (ev.key) {
    case CONN_KEY_ENTER:
        return EDIT_EV_KEY_ENTER;
    case CONN_KEY_ESCAPE:
        return EDIT_EV_KEY_ESCAPE;
    case CONN_KEY_LEFT:
        return EDIT_EV_KEY_LEFT;
    case CONN_KEY_RIGHT:
        return EDIT_EV_KEY_RIGHT;
    case CONN_KEY_HOME:
        return EDIT_EV_KEY_HOME;
    case CONN_KEY_END:
        return EDIT_EV_KEY_END;
    case CONN_KEY_BACKSPACE:
        return EDIT_EV_KEY_BACKSPACE;
    case CONN_KEY_DELETE:
        return EDIT_EV_KEY_DELETE;
    case CONN_KEY_TEXT:
        return EDIT_EV_KEY_TEXT;
    default:
        return STATE_MACHINE_ID_NONE;
    }
}

VOID conn_line_edit_init(ConnLineEditModel *m, const char *initial, const char *title,
                         const char *loader, const char *volume)
{
    char *p = (char *)m;
    UINTN i;

    for (i = 0; i < sizeof(*m); i++)
        p[i] = 0;

    conn_cp_bounded(m->title, title, sizeof m->title);
    conn_cp_bounded(m->loader, loader, sizeof m->loader);
    conn_cp_bounded(m->volume, volume, sizeof m->volume);
    m->len = conn_cp_bounded(m->buf, initial, sizeof m->buf);
    m->cursor = m->len;
    m->first = 0;
    m->cursor_on = TRUE;
    m->status = CONN_EDIT_ACTIVE;
    clamp_scroll(m);
    (void)state_machine_init(&m->machine, &ConnEditMachine, m);
}

ConnCommand conn_line_edit_update(ConnLineEditModel *m, ConnEvent ev)
{
    state_machine_event_id_t sm_ev;
    ConnEditDispatch ctx;

    if (m == NULL)
        return CONN_ACT_NONE;

    sm_ev = event_for_ev(ev);
    if (sm_ev == STATE_MACHINE_ID_NONE)
        return CONN_ACT_NONE;

    ctx.ch = ev.ch;
    ctx.command = CONN_ACT_NONE;
    (void)state_machine_dispatch(&m->machine, sm_ev, &ctx);
    return ctx.command;
}

static VOID put_u4(char *d, UINTN v)
{
    d[0] = (char)('0' + (v / 1000) % 10);
    d[1] = (char)('0' + (v / 100) % 10);
    d[2] = (char)('0' + (v / 10) % 10);
    d[3] = (char)('0' + v % 10);
}

VOID conn_line_edit_render(ConnGrid *g, const ConnLineEditModel *m, const ConnTextTheme *th)
{
    char off[24];
    const char *stat;
    UINTN i, j = 0;

    conn_grid_clear(g, th);
    conn_grid_frame(g, th);

    conn_grid_puts(g, 3, 1, "MERIDIAN", th->frame, CONN_TRANSPARENT, TRUE);
    conn_grid_puts(g, 12, 1, "BOOT MANAGER \xC2\xB7 CONN", th->text, CONN_TRANSPARENT, FALSE);

    conn_grid_puts(g, 3, 4, "BOOT PARAMETER CONSOLE", th->hi, CONN_TRANSPARENT, TRUE);
    conn_grid_puts(g, 3, 6, "TARGET", th->dim, CONN_TRANSPARENT, FALSE);
    conn_grid_puts(g, 12, 6, m->title, th->text, CONN_TRANSPARENT, FALSE);
    conn_grid_puts(g, 3, 7, "LOADER", th->dim, CONN_TRANSPARENT, FALSE);
    conn_grid_puts(g, 12, 7, m->loader, th->text, CONN_TRANSPARENT, FALSE);
    conn_grid_puts(g, 3, 8, "VOLUME", th->dim, CONN_TRANSPARENT, FALSE);
    conn_grid_puts(g, 12, 8, m->volume, th->text, CONN_TRANSPARENT, FALSE);

    conn_grid_puts(g, 3, 10, "PARAMETER BUFFER", th->dim, CONN_TRANSPARENT, FALSE);
    conn_grid_puts(g, PE_COL, PROMPT_ROW, "opt>", th->frame, CONN_TRANSPARENT, TRUE);

    for (i = 0; i < (UINTN)VIEW && m->first + i < m->len; i++) {
        conn_grid_set(g, BUF_COL + (INTN)i, PROMPT_ROW, (UINT32)(unsigned char)m->buf[m->first + i],
                      th->text, CONN_TRANSPARENT, FALSE);
    }

    if (m->cursor_on) {
        INTN cs = BUF_COL + (INTN)(m->cursor - m->first);
        UINT32 cch = (m->cursor < m->len) ? (UINT32)(unsigned char)m->buf[m->cursor] : 0x20;
        conn_grid_set(g, cs, PROMPT_ROW, cch, th->bg, th->hi, TRUE);
    }

    {
        const char *pfx = "OFFSET ";
        for (i = 0; pfx[i]; i++)
            off[j++] = pfx[i];
        put_u4(off + j, m->cursor);
        j += 4;
        off[j++] = ' ';
        off[j++] = '/';
        off[j++] = ' ';
        put_u4(off + j, m->len);
        j += 4;
        off[j] = '\0';
    }
    conn_grid_puts(g, 3, 14, off, th->dim, CONN_TRANSPARENT, FALSE);

    stat = (m->status == CONN_EDIT_COMMITTED)   ? "COMMITTED"
           : (m->status == CONN_EDIT_CANCELLED) ? "CANCELLED"
                                                : "EDIT";
    conn_grid_puts(g, CONN_COLS - 3 - (INTN)sl_len(stat), 14, stat, th->hi, CONN_TRANSPARENT, TRUE);

    conn_grid_puts(g, 3, CONN_ROWS - 2,
                   "ENTER commit+boot   ESC cancel   \xE2\x86\x90\xE2\x86\x92 move   BKSP/DEL edit",
                   th->dim, CONN_TRANSPARENT, FALSE);
}
