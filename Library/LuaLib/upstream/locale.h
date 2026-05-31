// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#ifndef MLUA_LOCALE_H
#define MLUA_LOCALE_H

struct lconv {
  char *decimal_point;
  char *thousands_sep;
  char *grouping;
};

#define LC_ALL      0
#define LC_NUMERIC  4

struct lconv *localeconv(void);
char         *setlocale(int category, const char *locale);

#endif
