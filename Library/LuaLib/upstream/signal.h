// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#ifndef MLUA_SIGNAL_H
#define MLUA_SIGNAL_H

// lstate.h wants sig_atomic_t for l_signalT. Nothing delivers a signal to a
// UEFI image, so the atomicity it asks for is not a requirement here.
typedef volatile int sig_atomic_t;

#endif
