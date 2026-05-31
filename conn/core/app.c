// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#include "conn_core.h"

static UINTN clamp_selected(UINTN selected, const ConnEntryList *list) {
    if (list == NULL || list->count == 0) {
        return 0;
    }
    return selected < list->count ? selected : list->count - 1;
}

static ConnCommand open_options(ConnModel *model, const ConnEntryList *list) {
    if (list == NULL || list->count == 0) {
        return CONN_ACT_NONE;
    }

    model->ui.idle_s = 0;
    model->ui.saver_frame = 0;
    model->ui.autoboot = FALSE;
    model->ui.cancelled = TRUE;
    model->ui.status = CONN_UI_OK;
    return CONN_ACT_OPTIONS;
}

ConnEvent conn_event_init(VOID) {
    ConnEvent event;
    event.type = CONN_EVENT_INIT;
    event.key = CONN_KEY_NONE;
    event.digit = 0;
    event.ch = 0;
    event.wake = CONN_WAKE_DISMISS;
    event.wake_index = 0;
    event.launch_status = EFI_SUCCESS;
    event.selected_hint = 0;
    return event;
}

ConnEvent conn_event_key(ConnKey key, INTN digit) {
    ConnEvent event = conn_event_init();
    event.type = CONN_EVENT_KEY;
    event.key = key;
    event.digit = digit;
    return event;
}

ConnEvent conn_event_tick(VOID) {
    ConnEvent event = conn_event_init();
    event.type = CONN_EVENT_TICK;
    return event;
}

ConnEvent conn_event_remote(ConnWakeIntent wake, UINTN wake_index)
{
    ConnEvent event = conn_event_init();
    event.type = CONN_EVENT_REMOTE;
    event.wake = wake;
    event.wake_index = wake_index;
    return event;
}

ConnEvent conn_event_launch_returned(EFI_STATUS launch_status,
                                     UINTN selected_hint) {
    ConnEvent event = conn_event_init();
    event.type = CONN_EVENT_LAUNCH_RETURNED;
    event.launch_status = launch_status;
    event.selected_hint = selected_hint;
    return event;
}

ConnEvent conn_event_point(UINTN row, BOOLEAN click)
{
    ConnEvent event = conn_event_init();
    event.type = CONN_EVENT_POINT;

    event.wake = click ? CONN_WAKE_LAUNCH : CONN_WAKE_SELECT;
    event.wake_index = row;
    return event;
}

VOID conn_model_init(ConnModel *model, const ConnEntryList *list) {
    if (model == NULL) {
        return;
    }
    conn_ui_init(&model->ui, list);
    model->rail_top = 0;
}

ConnCommand conn_update(ConnModel *model, const ConnEntryList *list,
                        ConnEvent event) {
    if (model == NULL) {
        return CONN_ACT_NONE;
    }

    switch (event.type) {
    case CONN_EVENT_INIT: {
        UINT32 screensaver_s = model->ui.screensaver_s;
        conn_model_init(model, list);
        conn_ui_set_screensaver(&model->ui, screensaver_s);
        return CONN_ACT_REDRAW;
    }
    case CONN_EVENT_KEY:
        if (event.key == CONN_KEY_OPTIONS && !conn_ui_in_saver(&model->ui)) {
            return open_options(model, list);
        }
        return conn_ui_on_key(&model->ui, event.key, event.digit, list);
    case CONN_EVENT_TICK:
        return conn_ui_on_tick(&model->ui, list);
    case CONN_EVENT_REMOTE:
    case CONN_EVENT_POINT:

        return conn_ui_on_remote(&model->ui, event.wake, event.wake_index, list);
    case CONN_EVENT_LAUNCH_RETURNED: {
        UINT32 screensaver_s = model->ui.screensaver_s;
        conn_model_init(model, list);
        conn_ui_set_screensaver(&model->ui, screensaver_s);
        model->ui.autoboot = FALSE;
        model->ui.cancelled = TRUE;
        model->ui.selected = clamp_selected(event.selected_hint, list);
        model->ui.status = EFI_ERROR(event.launch_status)
                         ? CONN_UI_LAUNCH_FAILED
                         : CONN_UI_OK;
        return CONN_ACT_REDRAW;
    }
    default:
        return CONN_ACT_NONE;
    }
}
