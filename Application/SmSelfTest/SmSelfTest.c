// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiApplicationEntryPoint.h>

#include "state_machine.h"

static const state_machine_state_def_t mAirlockStates[] = {
    { .id = 0, .parent = STATE_MACHINE_ID_NONE, .initial_child = STATE_MACHINE_ID_NONE,
      .depth = 0, .flags = 0, .name = "pressurized" },
    { .id = 1, .parent = STATE_MACHINE_ID_NONE, .initial_child = STATE_MACHINE_ID_NONE,
      .depth = 0, .flags = 0, .name = "vacuum" },
};

static const state_machine_state_id_t mDepressSrc[]   = { 0 };
static const state_machine_state_id_t mRepressSrc[]    = { 1 };

static const state_machine_transition_def_t mDepressT[] = {
    { .sources = mDepressSrc, .guards = NULL, .target = 1,
      .source_count = 1, .guard_count = 0, .ordinal = 0, .flags = 0, ._reserved = 0 },
};
static const state_machine_transition_def_t mRepressT[] = {
    { .sources = mRepressSrc, .guards = NULL, .target = 0,
      .source_count = 1, .guard_count = 0, .ordinal = 0, .flags = 0, ._reserved = 0 },
};

static const state_machine_event_def_t mAirlockEvents[] = {
    { .id = 0, .transition_count = 1, .transitions = mDepressT, .name = "depressurize" },
    { .id = 1, .transition_count = 1, .transitions = mRepressT, .name = "repressurize" },
};

static const state_machine_def_t mAirlock = {
    .magic        = STATE_MACHINE_MAGIC,
    .abi_epoch    = STATE_MACHINE_ABI_EPOCH,
    .abi_revision = STATE_MACHINE_ABI_REVISION,
    .struct_size  = sizeof(state_machine_def_t),
    .flags        = 0,
    .spec_version = STATE_MACHINE_SPEC_VERSION,
    ._reserved    = 0,
    .name         = "airlock",
    .states       = mAirlockStates,
    .events       = mAirlockEvents,
    .guards       = NULL,
    .state_count  = 2,
    .initial      = 0,
    .event_count  = 2,
    .guard_count  = 0,
};

#define CHECK(cond, label)                                  \
    do {                                                    \
        if (cond) {                                         \
            Print(L"  [ ok ] %a\n", (label));               \
        } else {                                            \
            Print(L"  [FAIL] %a\n", (label));               \
            Pass = FALSE;                                   \
        }                                                   \
    } while (0)

EFI_STATUS
EFIAPI
UefiMain (
    IN EFI_HANDLE        ImageHandle,
    IN EFI_SYSTEM_TABLE *SystemTable
    )
{
    state_machine_t        m = STATE_MACHINE_INITIALIZER;
    state_machine_result_t r;
    BOOLEAN                Pass = TRUE;
    int                    rc;

    Print(L"SmSelfTest: state-machines-c under EDK2 (ABI %u, spec %u.%u)\n",
          (UINT32)state_machine_abi_version(),
          (UINT32)STATE_MACHINE_SPEC_VERSION_MAJOR,
          (UINT32)STATE_MACHINE_SPEC_VERSION_MINOR);

    rc = state_machine_init(&m, &mAirlock, NULL);
    CHECK(rc == 0, "init -> 0");
    CHECK(state_machine_current(&m) == 0, "initial state = pressurized");

    rc = state_machine_dispatch_ex(&m, 0, NULL, &r);
    CHECK(rc == 0 && r.outcome == STATE_MACHINE_OUTCOME_ACCEPTED
              && r.from == 0 && r.to == 1,
          "depressurize accepted (0 -> 1)");
    CHECK(state_machine_current(&m) == 1, "now vacuum");

    rc = state_machine_dispatch_ex(&m, 0, NULL, &r);
    CHECK(rc == EOPNOTSUPP && r.outcome == STATE_MACHINE_OUTCOME_NO_TRANSITION,
          "depressurize@vacuum -> NO_TRANSITION");

    rc = state_machine_dispatch(&m, 1, NULL);
    CHECK(rc == 0 && state_machine_current(&m) == 0, "repressurize accepted (1 -> 0)");

    rc = state_machine_dispatch_ex(&m, 99, NULL, &r);
    CHECK(rc == EINVAL && r.outcome == STATE_MACHINE_OUTCOME_INVALID_ARG,
          "event 99 -> INVALID_ARG");

    state_machine_destroy(&m);

    Print(L"SmSelfTest: %a\n", Pass ? "ALL PASS" : "FAILURES");
    return Pass ? EFI_SUCCESS : EFI_ABORTED;
}
