// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

typedef union { double d; unsigned long long u; } mlua_dbits;

#define MLUA_EXP_MASK 0x7ff0000000000000ULL
#define MLUA_SGN_MASK 0x8000000000000000ULL

int mlua_isnan(double x) { mlua_dbits b = { x }; return ((b.u & MLUA_EXP_MASK) == MLUA_EXP_MASK) && (b.u & 0x000fffffffffffffULL); }
int mlua_isinf(double x) { mlua_dbits b = { x }; return ((b.u & ~MLUA_SGN_MASK) == MLUA_EXP_MASK); }

double fabs(double x) { mlua_dbits b = { x }; b.u &= ~MLUA_SGN_MASK; return b.d; }

static double mlua_trunc(double x) {
  if (mlua_isnan(x) || mlua_isinf(x)) return x;
  if (fabs(x) >= 4503599627370496.0) return x;
  long long i = (long long)x;
  return (double)i;
}

double floor(double x) {
  double t = mlua_trunc(x);
  return (t > x) ? t - 1.0 : t;
}
double ceil(double x) {
  double t = mlua_trunc(x);
  return (t < x) ? t + 1.0 : t;
}

double fmod(double x, double y) {
  if (y == 0.0 || mlua_isnan(x) || mlua_isnan(y) || mlua_isinf(x)) {
    mlua_dbits nan = { 0 }; nan.u = MLUA_EXP_MASK | 1; return nan.d;
  }
  if (mlua_isinf(y)) return x;
  double r = x - mlua_trunc(x / y) * y;
  return r;
}

double modf(double x, double *iptr) {
  double i = mlua_trunc(x);
  *iptr = i;
  return x - i;
}

double frexp(double x, int *e) {
  mlua_dbits b = { x };
  int ex = (int)((b.u & MLUA_EXP_MASK) >> 52);
  if (ex == 0) { *e = 0; return x; }
  if (ex == 0x7ff) { *e = 0; return x; }
  *e = ex - 1022;
  b.u = (b.u & ~MLUA_EXP_MASK) | (1022ULL << 52);
  return b.d;
}

double ldexp(double x, int e) {

  while (e > 1023) { x *= 8.98846567431158e307 ; e -= 1023; }
  while (e < -1022) { x *= 2.2250738585072014e-308 ; e += 1022; }
  mlua_dbits p = { 0 };
  p.u = (unsigned long long)(e + 1023) << 52;
  return x * p.d;
}

double sqrt(double x) {
  if (x < 0.0) { mlua_dbits nan = { 0 }; nan.u = MLUA_EXP_MASK | 1; return nan.d; }
  if (x == 0.0 || mlua_isinf(x) || mlua_isnan(x)) return x;
  double g = x;
  for (int i = 0; i < 60; i++) { double ng = 0.5 * (g + x / g); if (ng == g) break; g = ng; }
  return g;
}

#define MLUA_LN2 0.6931471805599453

static double mlua_log(double x) {
  if (x <= 0.0) { mlua_dbits nan = { 0 }; nan.u = MLUA_EXP_MASK | 1; return nan.d; }
  int e; double m = frexp(x, &e);
  if (m < 0.7071067811865476) { m *= 2.0; e -= 1; }
  double s = (m - 1.0) / (m + 1.0), s2 = s * s, term = s, sum = 0.0;
  for (int k = 1; k < 40; k += 2) { sum += term / k; term *= s2; }
  return 2.0 * sum + e * MLUA_LN2;
}

static double mlua_exp(double x) {
  if (x == 0.0) return 1.0;
  int k = (int)floor(x / MLUA_LN2 + 0.5);
  double r = x - k * MLUA_LN2;
  double term = 1.0, sum = 1.0;
  for (int n = 1; n < 30; n++) { term *= r / n; sum += term; if (fabs(term) < 1e-18) break; }
  return ldexp(sum, k);
}

double pow(double x, double y) {
  if (y == 0.0) return 1.0;
  if (x == 1.0) return 1.0;

  double yt = mlua_trunc(y);
  if (yt == y && fabs(y) < 1024.0) {
    int neg = y < 0.0; long long n = (long long)(neg ? -yt : yt);
    double r = 1.0, b = x;
    while (n) { if (n & 1) r *= b; b *= b; n >>= 1; }
    return neg ? 1.0 / r : r;
  }
  if (x <= 0.0) { mlua_dbits nan = { 0 }; nan.u = MLUA_EXP_MASK | 1; return nan.d; }
  return mlua_exp(y * mlua_log(x));
}

#define MLUA_PI    3.141592653589793
#define MLUA_PI_2  1.5707963267948966
#define MLUA_PI_4  0.7853981633974483
#define MLUA_LN10  2.302585092994046

double exp(double x) { return mlua_exp(x); }
double log(double x) { return mlua_log(x); }
double log2(double x) { return mlua_log(x) / MLUA_LN2; }
double log10(double x) { return mlua_log(x) / MLUA_LN10; }

static double mlua_sin_small(double x) {
  double x2 = x * x, term = x, sum = x;
  for (int n = 1; n < 12; n++) { term *= -x2 / ((2 * n) * (2 * n + 1)); sum += term; }
  return sum;
}
static double mlua_cos_small(double x) {
  double x2 = x * x, term = 1.0, sum = 1.0;
  for (int n = 1; n < 12; n++) { term *= -x2 / ((2 * n - 1) * (2 * n)); sum += term; }
  return sum;
}

double sin(double x) {
  if (mlua_isnan(x) || mlua_isinf(x)) { mlua_dbits n = { 0 }; n.u = MLUA_EXP_MASK | 1; return n.d; }
  int k = (int)floor(x / MLUA_PI_2 + 0.5);
  double r = x - k * MLUA_PI_2;
  switch (((k % 4) + 4) % 4) {
    case 0: return mlua_sin_small(r);
    case 1: return mlua_cos_small(r);
    case 2: return -mlua_sin_small(r);
    default: return -mlua_cos_small(r);
  }
}
double cos(double x) { return sin(x + MLUA_PI_2); }
double tan(double x) { double c = cos(x); return c == 0.0 ? __builtin_huge_val() : sin(x) / c; }

static double mlua_atan_small(double x) {
  double x2 = x * x, term = x, sum = x;
  for (int n = 1; n < 60; n++) { term *= -x2; sum += term / (2 * n + 1); }
  return sum;
}
double atan(double x) {
  if (x > 1.0) return MLUA_PI_2 - mlua_atan_small(1.0 / x);
  if (x < -1.0) return -MLUA_PI_2 - mlua_atan_small(1.0 / x);
  return mlua_atan_small(x);
}
double atan2(double y, double x) {
  if (x > 0.0) return atan(y / x);
  if (x < 0.0) return atan(y / x) + (y >= 0.0 ? MLUA_PI : -MLUA_PI);
  if (y > 0.0) return MLUA_PI_2;
  if (y < 0.0) return -MLUA_PI_2;
  return 0.0;
}
double asin(double x) { if (x <= -1.0) return -MLUA_PI_2; if (x >= 1.0) return MLUA_PI_2; return atan2(x, sqrt(1.0 - x * x)); }
double acos(double x) { if (x <= -1.0) return MLUA_PI; if (x >= 1.0) return 0.0; return atan2(sqrt(1.0 - x * x), x); }
