// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

typedef __SIZE_TYPE__ size_t;

static int sp(int c) { return c == ' ' || (c >= '\t' && c <= '\r'); }
static int dv(int c, int base) {
  int d;
  if (c >= '0' && c <= '9') d = c - '0';
  else if (c >= 'a' && c <= 'z') d = c - 'a' + 10;
  else if (c >= 'A' && c <= 'Z') d = c - 'A' + 10;
  else return -1;
  return d < base ? d : -1;
}

double strtod(const char *s, char **endp) {
  const char *p = s;
  while (sp(*p)) p++;
  int sign = 1;
  if (*p == '+') p++; else if (*p == '-') { sign = -1; p++; }

  double val = 0.0; int any = 0;
  while (*p >= '0' && *p <= '9') { val = val * 10.0 + (*p - '0'); p++; any = 1; }
  if (*p == '.') {
    p++; double f = 0.1;
    while (*p >= '0' && *p <= '9') { val += (*p - '0') * f; f *= 0.1; p++; any = 1; }
  }
  if (any && (*p == 'e' || *p == 'E')) {
    const char *save = p; p++;
    int es = 1; if (*p == '+') p++; else if (*p == '-') { es = -1; p++; }
    int ev = 0, ed = 0;
    while (*p >= '0' && *p <= '9') { ev = ev * 10 + (*p - '0'); p++; ed = 1; }
    if (ed) { double pw = 1.0; for (int i = 0; i < ev; i++) pw *= 10.0; if (es > 0) val *= pw; else val /= pw; }
    else p = save;
  }
  if (!any) { if (endp) *endp = (char *)s; return 0.0; }
  if (endp) *endp = (char *)p;
  return sign * val;
}

static unsigned long long str2u(const char *s, char **endp, int base, int *neg) {
  const char *p = s;
  while (sp(*p)) p++;
  *neg = 0;
  if (*p == '+') p++; else if (*p == '-') { *neg = 1; p++; }
  if ((base == 0 || base == 16) && p[0] == '0' && (p[1] == 'x' || p[1] == 'X') && dv(p[2], 16) >= 0) { p += 2; base = 16; }
  else if (base == 0 && p[0] == '0') base = 8;
  else if (base == 0) base = 10;
  unsigned long long v = 0; int any = 0, d;
  while ((d = dv(*p, base)) >= 0) { v = v * (unsigned)base + (unsigned)d; p++; any = 1; }
  if (endp) *endp = (char *)(any ? p : s);
  return v;
}

unsigned long long strtoull(const char *s, char **endp, int base) { int neg; unsigned long long v = str2u(s, endp, base, &neg); return neg ? 0ULL - v : v; }
unsigned long      strtoul(const char *s, char **endp, int base) { return (unsigned long)strtoull(s, endp, base); }
long long          strtoll(const char *s, char **endp, int base) { int neg; unsigned long long v = str2u(s, endp, base, &neg); return neg ? -(long long)v : (long long)v; }
long               strtol(const char *s, char **endp, int base) { return (long)strtoll(s, endp, base); }
