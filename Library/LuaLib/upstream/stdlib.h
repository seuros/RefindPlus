// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#ifndef MLUA_STDLIB_H
#define MLUA_STDLIB_H

#include <stddef.h>

void  *malloc(size_t n);
void  *realloc(void *p, size_t n);
void  *calloc(size_t nmemb, size_t size);
void   free(void *p);

void   abort(void) __attribute__((noreturn));
void   exit(int status) __attribute__((noreturn));
char  *getenv(const char *name);

double             strtod(const char *s, char **endp);
long               strtol(const char *s, char **endp, int base);
unsigned long      strtoul(const char *s, char **endp, int base);
long long          strtoll(const char *s, char **endp, int base);
unsigned long long strtoull(const char *s, char **endp, int base);

int    abs(int n);
void   qsort(void *base, size_t n, size_t sz,
             int (*cmp)(const void *, const void *));

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define RAND_MAX     0x7fffffff

#endif
