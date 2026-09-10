// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#include <Uefi.h>
#include <Library/BaseMemoryLib.h>

__attribute__((weak)) void *memset(void *dest, int ch, __SIZE_TYPE__ count)
{
    SetMem(dest, (UINTN)count, (UINT8)ch);
    return dest;
}

__attribute__((weak)) void *memcpy(void *dest, const void *src, __SIZE_TYPE__ count)
{
    CopyMem(dest, (VOID *)src, (UINTN)count);
    return dest;
}

__attribute__((weak)) void *memmove(void *dest, const void *src, __SIZE_TYPE__ count)
{
    CopyMem(dest, (VOID *)src, (UINTN)count);
    return dest;
}

__attribute__((weak)) int memcmp(const void *a, const void *b, __SIZE_TYPE__ count)
{
    return (int)CompareMem(a, b, (UINTN)count);
}
