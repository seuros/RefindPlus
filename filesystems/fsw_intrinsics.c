// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#include "fsw_base.h"

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

__attribute__((weak)) void *memmove (void *dest, const void *src, __SIZE_TYPE__ count) {
    UINT8       *d = (UINT8 *) dest;
    const UINT8 *s = (const UINT8 *) src;

    if (d == s || count == 0) {
        return dest;
    }
    if (d < s) {
        while (count--) {
            *d++ = *s++;
        }
    }
    else {
        d += count;
        s += count;
        while (count--) {
            *--d = *--s;
        }
    }
    return dest;
}

__attribute__((weak)) int memcmp (const void *a, const void *b, __SIZE_TYPE__ count) {
    const UINT8 *pa = (const UINT8 *) a;
    const UINT8 *pb = (const UINT8 *) b;
    while (count--) {
        if (*pa != *pb) {
            return (int) *pa - (int) *pb;
        }
        pa++;
        pb++;
    }
    return 0;
}
