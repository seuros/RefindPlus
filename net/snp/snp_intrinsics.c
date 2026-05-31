// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#include <Uefi.h>

__attribute__((weak)) void *memset (void *dest, int ch, __SIZE_TYPE__ count) {
    UINT8 *p = (UINT8 *) dest;
    while (count--) {
        *p++ = (UINT8) ch;
    }
    return dest;
}

__attribute__((weak)) void *memcpy (void *dest, const void *src, __SIZE_TYPE__ count) {
    UINT8       *d = (UINT8 *) dest;
    const UINT8 *s = (const UINT8 *) src;
    while (count--) {
        *d++ = *s++;
    }
    return dest;
}
