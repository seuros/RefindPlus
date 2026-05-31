// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#ifndef MLUA_TIME_H
#define MLUA_TIME_H

typedef long time_t;
typedef long clock_t;

#define CLOCKS_PER_SEC 1000000

time_t time(time_t *t);
clock_t clock(void);

#endif
