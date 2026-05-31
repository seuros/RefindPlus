// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#ifndef MLUA_STDIO_H
#define MLUA_STDIO_H

#include <stddef.h>
#include <stdarg.h>

typedef struct mlua_FILE FILE;

extern FILE *mlua_stdout;
extern FILE *mlua_stderr;
extern FILE *mlua_stdin;
#define stdout mlua_stdout
#define stderr mlua_stderr
#define stdin  mlua_stdin

#define EOF      (-1)
#define BUFSIZ   512

int snprintf(char *buf, size_t n, const char *fmt, ...);
int vsnprintf(char *buf, size_t n, const char *fmt, va_list ap);
int sprintf(char *buf, const char *fmt, ...);

int    fprintf(FILE *stream, const char *fmt, ...);
int    fputs(const char *s, FILE *stream);
int    fputc(int c, FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
int    fflush(FILE *stream);

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#define _IOFBF   0
#define _IOLBF   1
#define _IONBF   2
#define L_tmpnam 32

FILE  *fopen(const char *path, const char *mode);
FILE  *freopen(const char *path, const char *mode, FILE *stream);
int    fclose(FILE *stream);
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
int    getc(FILE *stream);
int    ungetc(int c, FILE *stream);
char  *fgets(char *s, int size, FILE *stream);
int    feof(FILE *stream);
int    ferror(FILE *stream);
void   clearerr(FILE *stream);
int    fseek(FILE *stream, long off, int whence);
long   ftell(FILE *stream);
int    setvbuf(FILE *stream, char *buf, int mode, size_t size);

#endif
