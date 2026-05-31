// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>

typedef __SIZE_TYPE__ size_t;

void *memcpy(void *dst, const void *src, size_t n) { CopyMem(dst, (void *)src, n); return dst; }
void *memmove(void *dst, const void *src, size_t n) { CopyMem(dst, (void *)src, n); return dst; }
void *memset(void *s, int c, size_t n) { SetMem(s, n, (UINT8)c); return s; }
int   memcmp(const void *a, const void *b, size_t n) { return (int)CompareMem(a, b, n); }

void *memchr(const void *s, int c, size_t n) {
  const unsigned char *p = s;
  while (n--) { if (*p == (unsigned char)c) return (void *)p; p++; }
  return (void *)0;
}

size_t strlen(const char *s) { const char *p = s; while (*p) p++; return (size_t)(p - s); }

int strcmp(const char *a, const char *b) {
  while (*a && *a == *b) { a++; b++; }
  return (int)(unsigned char)*a - (int)(unsigned char)*b;
}
int strncmp(const char *a, const char *b, size_t n) {
  while (n && *a && *a == *b) { a++; b++; n--; }
  if (!n) return 0;
  return (int)(unsigned char)*a - (int)(unsigned char)*b;
}
int strcoll(const char *a, const char *b) { return strcmp(a, b); }

char *strcpy(char *dst, const char *src) { char *d = dst; while ((*d++ = *src++)) ; return dst; }
char *strcat(char *dst, const char *src) { char *d = dst; while (*d) d++; while ((*d++ = *src++)) ; return dst; }

char *strchr(const char *s, int c) {
  for (;; s++) { if (*s == (char)c) return (char *)s; if (!*s) return (void *)0; }
}
char *strrchr(const char *s, int c) {
  const char *last = (void *)0;
  for (;; s++) { if (*s == (char)c) last = s; if (!*s) return (char *)last; }
}
char *strstr(const char *h, const char *n) {
  if (!*n) return (char *)h;
  for (; *h; h++) {
    const char *a = h, *b = n;
    while (*a && *b && *a == *b) { a++; b++; }
    if (!*b) return (char *)h;
  }
  return (void *)0;
}
size_t strspn(const char *s, const char *set) {
  const char *p = s;
  for (; *p; p++) { const char *q = set; while (*q && *q != *p) q++; if (!*q) break; }
  return (size_t)(p - s);
}
size_t strcspn(const char *s, const char *set) {
  const char *p = s;
  for (; *p; p++) { const char *q = set; while (*q && *q != *p) q++; if (*q) break; }
  return (size_t)(p - s);
}
char *strpbrk(const char *s, const char *set) {
  for (; *s; s++) { const char *q = set; while (*q) { if (*q == *s) return (char *)s; q++; } }
  return (void *)0;
}
char *strerror(int e) { (void)e; return (char *)"Meridian-Lua error"; }

#define MLUA_HDR 16

void *malloc(size_t n) {
  UINT8 *base = AllocatePool(n + MLUA_HDR);
  if (!base) return (void *)0;
  *(UINT64 *)base = (UINT64)n;
  return base + MLUA_HDR;
}
void free(void *p) {
  if (p) FreePool((UINT8 *)p - MLUA_HDR);
}
void *realloc(void *p, size_t n) {
  if (!p) return malloc(n);
  if (n == 0) { free(p); return (void *)0; }
  UINT64 old = *(UINT64 *)((UINT8 *)p - MLUA_HDR);
  void *np = malloc(n);
  if (!np) return (void *)0;
  CopyMem(np, p, old < n ? old : n);
  free(p);
  return np;
}
void *calloc(size_t nmemb, size_t size) {
  size_t total = nmemb * size;
  void *p = malloc(total);
  if (p) SetMem(p, total, 0);
  return p;
}

void abort(void) { CpuDeadLoop(); __builtin_unreachable(); }
void exit(int status) { (void)status; CpuDeadLoop(); __builtin_unreachable(); }
void mlua_assert_fail(const char *expr, const char *file, int line) {
  (void)expr; (void)file; (void)line; CpuDeadLoop(); __builtin_unreachable();
}
char *getenv(const char *name) { (void)name; return (void *)0; }

static int mlua_errno_storage = 0;
int *mlua_errno_location(void) { return &mlua_errno_storage; }

struct lconv { char *decimal_point; char *thousands_sep; char *grouping; };
static struct lconv mlua_c_locale = { (char *)".", (char *)"", (char *)"" };
struct lconv *localeconv(void) { return &mlua_c_locale; }
char *setlocale(int category, const char *locale) { (void)category; (void)locale; return (char *)"C"; }

int isalpha(int c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); }
int isdigit(int c) { return c >= '0' && c <= '9'; }
int isalnum(int c) { return isalpha(c) || isdigit(c); }
int isspace(int c) { return c == ' ' || (c >= '\t' && c <= '\r'); }
int iscntrl(int c) { return (c >= 0 && c < 32) || c == 127; }
int isgraph(int c) { return c > 32 && c < 127; }
int islower(int c) { return c >= 'a' && c <= 'z'; }
int isupper(int c) { return c >= 'A' && c <= 'Z'; }
int ispunct(int c) { return isgraph(c) && !isalnum(c); }
int isxdigit(int c) { return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
int tolower(int c) { return isupper(c) ? c + 32 : c; }
int toupper(int c) { return islower(c) ? c - 32 : c; }

long time(long *t) { if (t) *t = 0; return 0; }
long clock(void) { return 0; }

int abs(int n) { return n < 0 ? -n : n; }

void qsort(void *base, size_t n, size_t sz, int (*cmp)(const void *, const void *)) {

  UINT8 *a = base, *tmp = AllocatePool(sz);
  if (!tmp) return;
  for (size_t i = 1; i < n; i++) {
    CopyMem(tmp, a + i * sz, sz);
    size_t j = i;
    while (j > 0 && cmp(a + (j - 1) * sz, tmp) > 0) {
      CopyMem(a + j * sz, a + (j - 1) * sz, sz); j--;
    }
    CopyMem(a + j * sz, tmp, sz);
  }
  FreePool(tmp);
}

typedef struct __attribute__((aligned(16))) { unsigned long long _opaque[40]; } mlua_jmp_buf[1];
STATIC_ASSERT(sizeof(BASE_LIBRARY_JUMP_BUFFER) <= sizeof(mlua_jmp_buf),
              "jmp_buf shim smaller than BASE_LIBRARY_JUMP_BUFFER");

int MluaSetJump(void *env) __attribute__((returns_twice));
int MluaSetJump(void *env) { return (int)SetJump((BASE_LIBRARY_JUMP_BUFFER *)env); }

void MluaLongJump(void *env, int val) __attribute__((noreturn));
void MluaLongJump(void *env, int val) {
  LongJump((BASE_LIBRARY_JUMP_BUFFER *)env, (UINTN)(val == 0 ? 1 : val));
  __builtin_unreachable();
}
