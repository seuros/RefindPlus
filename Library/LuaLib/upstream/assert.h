// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#ifndef MLUA_ASSERT_H
#define MLUA_ASSERT_H

void mlua_assert_fail(const char *expr, const char *file, int line);

#ifdef NDEBUG
#define assert(e) ((void)0)
#else
#define assert(e) ((e) ? (void)0 : mlua_assert_fail(#e, __FILE__, __LINE__))
#endif

#endif
