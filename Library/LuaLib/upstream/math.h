// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#ifndef MLUA_MATH_H
#define MLUA_MATH_H

double floor(double x);
double ceil(double x);
double fabs(double x);
double fmod(double x, double y);
double pow(double x, double y);
double sqrt(double x);
double frexp(double x, int *exp);
double ldexp(double x, int exp);
double modf(double x, double *iptr);

double exp(double x);
double log(double x);
double log2(double x);
double log10(double x);
double sin(double x);
double cos(double x);
double tan(double x);
double asin(double x);
double acos(double x);
double atan(double x);
double atan2(double y, double x);

int    mlua_isnan(double x);
int    mlua_isinf(double x);

#define HUGE_VAL (__builtin_huge_val())
#define INFINITY (__builtin_inff())
#define NAN      (__builtin_nanf(""))

#define isnan(x) mlua_isnan((double)(x))
#define isinf(x) mlua_isinf((double)(x))

#endif
