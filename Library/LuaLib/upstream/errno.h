// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#ifndef MLUA_ERRNO_H
#define MLUA_ERRNO_H

extern int *mlua_errno_location(void);
#define errno (*mlua_errno_location())

#define EDOM   33
#define ERANGE 34
#define EILSEQ 84

#endif
