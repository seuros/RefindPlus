// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#ifndef __MERIDIAN_DISPLAY_TYPES_H_
#define __MERIDIAN_DISPLAY_TYPES_H_

#include "tiano_includes.h"

typedef enum ColorTypes
{
    white,
    black
} Colors;

typedef struct
{
    UINT8 b, g, r, a;
} EG_PIXEL;

#endif
