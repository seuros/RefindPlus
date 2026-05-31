// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#ifndef MLUA_CTYPE_H
#define MLUA_CTYPE_H

int isalpha(int c);
int isdigit(int c);
int isalnum(int c);
int isspace(int c);
int iscntrl(int c);
int isgraph(int c);
int islower(int c);
int isupper(int c);
int ispunct(int c);
int isxdigit(int c);
int tolower(int c);
int toupper(int c);

#endif
