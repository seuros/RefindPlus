// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#ifndef MLUA_SETJMP_H
#define MLUA_SETJMP_H

typedef struct __attribute__((aligned(16))) { unsigned long long _opaque[40]; } jmp_buf[1];

int  MluaSetJump(void *env) __attribute__((returns_twice));
void MluaLongJump(void *env, int val) __attribute__((noreturn));

#define setjmp(env)        MluaSetJump(env)
#define longjmp(env, val)  MluaLongJump((env), (val))

#endif
