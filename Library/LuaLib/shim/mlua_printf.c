// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>

typedef __SIZE_TYPE__ size_t;
typedef __builtin_va_list va_list;
#define va_start(a, l) __builtin_va_start(a, l)
#define va_arg(a, t)   __builtin_va_arg(a, t)
#define va_end(a)      __builtin_va_end(a)

extern double floor(double);
extern double fabs(double);
extern int    mlua_isnan(double);
extern int    mlua_isinf(double);

typedef struct { char *buf; size_t cap; size_t len; } Out;
static void oc(Out *o, char c) { if (o->len + 1 < o->cap) o->buf[o->len] = c; o->len++; }
static void os(Out *o, const char *s) { while (*s) oc(o, *s++); }

static int utoa(unsigned long long v, unsigned base, int upper, char *out) {
  const char *dig = upper ? "0123456789ABCDEF" : "0123456789abcdef";
  char tmp[24]; int n = 0;
  do { tmp[n++] = dig[v % base]; v /= base; } while (v);
  for (int i = 0; i < n; i++) out[i] = tmp[n - 1 - i];
  out[n] = 0; return n;
}

static void dtoa_f(double x, int prec, char *out) {
  int oi = 0;
  if (x < 0) { out[oi++] = '-'; x = -x; }
  if (prec > 17) prec = 17;

  double ip = floor(x);
  double fp = x - ip;
  unsigned long long scale = 1ULL; for (int i = 0; i < prec; i++) scale *= 10ULL;
  unsigned long long fpi = (unsigned long long)(fp * (double)scale + 0.5);
  if (fpi >= scale) { fpi -= scale; ip += 1.0; }

  char ib[330]; int n = 0;
  if (ip < 1.0) ib[n++] = '0';
  else while (ip >= 1.0) { double q = floor(ip / 10.0); int d = (int)(ip - q * 10.0); ib[n++] = (char)('0' + d); ip = q; }
  for (int i = 0; i < n; i++) out[oi++] = ib[n - 1 - i];

  if (prec > 0) {
    out[oi++] = '.';
    char fb[24]; int fn = 0;
    for (int i = 0; i < prec; i++) { fb[fn++] = (char)('0' + (int)(fpi % 10ULL)); fpi /= 10ULL; }
    for (int i = 0; i < prec; i++) out[oi++] = fb[prec - 1 - i];
  }
  out[oi] = 0;
}

static void dtoa_e(double x, int prec, int upper, char *out) {
  int oi = 0;
  if (x < 0) { out[oi++] = '-'; x = -x; }
  int exp = 0;
  if (x != 0.0) { while (x >= 10.0) { x /= 10.0; exp++; } while (x < 1.0) { x *= 10.0; exp--; } }
  char mant[400]; dtoa_f(x, prec, mant);
  for (int i = 0; mant[i]; i++) out[oi++] = mant[i];
  out[oi++] = upper ? 'E' : 'e';
  out[oi++] = exp < 0 ? '-' : '+';
  if (exp < 0) exp = -exp;
  out[oi++] = (char)('0' + (exp / 10) % 10);
  out[oi++] = (char)('0' + exp % 10);
  out[oi] = 0;
}

static void dtoa_g(double x, int prec, int upper, char *out) {
  if (prec <= 0) prec = 1;
  double ax = fabs(x);
  int exp = 0; double t = ax;
  if (t != 0.0) { while (t >= 10.0) { t /= 10.0; exp++; } while (t < 1.0) { t *= 10.0; exp--; } }
  if (exp < -4 || exp >= prec) dtoa_e(x, prec - 1, upper, out);
  else dtoa_f(x, prec - 1 - exp, out);

  int dot = -1, ei = -1;
  for (int i = 0; out[i]; i++) { if (out[i] == '.') dot = i; if (out[i] == 'e' || out[i] == 'E') { ei = i; break; } }
  if (dot >= 0) {
    int end = (ei >= 0) ? ei : 0; if (ei < 0) { while (out[end]) end++; }
    int j = end - 1;
    while (j > dot && out[j] == '0') j--;
    if (j == dot) j--;
    if (ei >= 0) { int k = j + 1, m = ei; while (out[m]) out[k++] = out[m++]; out[k] = 0; }
    else out[j + 1] = 0;
  }
}

static void emit_float(Out *o, double x, char conv, int prec, int hasprec) {
  if (!hasprec) prec = 6;
  int upper = (conv >= 'A' && conv <= 'Z');
  char low = upper ? conv + 32 : conv;
  char tmp[420];
  if (mlua_isnan(x)) { os(o, upper ? "NAN" : "nan"); return; }
  if (mlua_isinf(x)) { if (x < 0) oc(o, '-'); os(o, upper ? "INF" : "inf"); return; }
  if (low == 'f') dtoa_f(x, prec, tmp);
  else if (low == 'e') dtoa_e(x, prec, upper, tmp);
  else dtoa_g(x, prec ? prec : 1, upper, tmp);
  os(o, tmp);
}

int vsnprintf(char *buf, size_t cap, const char *fmt, va_list ap) {
  Out o = { buf, cap, 0 };
  for (; *fmt; fmt++) {
    if (*fmt != '%') { oc(&o, *fmt); continue; }
    fmt++;
    int left = 0, zero = 0, plus = 0, space = 0, alt = 0;
    for (;; fmt++) {
      if (*fmt == '-') left = 1; else if (*fmt == '0') zero = 1;
      else if (*fmt == '+') plus = 1; else if (*fmt == ' ') space = 1;
      else if (*fmt == '#') alt = 1; else break;
    }
    int width = 0;
    if (*fmt == '*') { width = va_arg(ap, int); fmt++; }
    else while (*fmt >= '0' && *fmt <= '9') width = width * 10 + (*fmt++ - '0');
    int prec = 0, hasprec = 0;
    if (*fmt == '.') { hasprec = 1; fmt++; if (*fmt == '*') { prec = va_arg(ap, int); fmt++; } else while (*fmt >= '0' && *fmt <= '9') prec = prec * 10 + (*fmt++ - '0'); }
    int lng = 0;
    for (;;) { if (*fmt == 'l') { lng++; fmt++; } else if (*fmt == 'h' || *fmt == 'z' || *fmt == 'j' || *fmt == 't' || *fmt == 'L') fmt++; else break; }

    char c = *fmt;
    char num[64]; int isneg = 0; const char *prefix = "";
    char body[440]; const char *out = body; int bodylen = -1;

    switch (c) {
      case 'd': case 'i': {
        long long v = lng ? va_arg(ap, long long) : (long long)va_arg(ap, int);
        unsigned long long m;
        if (v < 0) { isneg = 1; m = (unsigned long long)(-(v + 1)) + 1; } else m = (unsigned long long)v;
        utoa(m, 10, 0, num); out = num; break;
      }
      case 'u': { unsigned long long v = lng ? va_arg(ap, unsigned long long) : (unsigned long long)va_arg(ap, unsigned int); utoa(v, 10, 0, num); out = num; break; }
      case 'o': { unsigned long long v = lng ? va_arg(ap, unsigned long long) : (unsigned long long)va_arg(ap, unsigned int); utoa(v, 8, 0, num); out = num; if (alt) prefix = "0"; break; }
      case 'x': case 'X': { unsigned long long v = lng ? va_arg(ap, unsigned long long) : (unsigned long long)va_arg(ap, unsigned int); utoa(v, 16, c == 'X', num); out = num; if (alt && v) prefix = (c == 'X') ? "0X" : "0x"; break; }
      case 'p': { unsigned long long v = (unsigned long long)(__SIZE_TYPE__)va_arg(ap, void *); num[0] = '0'; num[1] = 'x'; utoa(v, 16, 0, num + 2); out = num; break; }
      case 'c': { num[0] = (char)va_arg(ap, int); num[1] = 0; out = num; break; }
      case 's': { out = va_arg(ap, const char *); if (!out) out = "(null)"; break; }
      case '%': { num[0] = '%'; num[1] = 0; out = num; break; }
      case 'f': case 'F': case 'e': case 'E': case 'g': case 'G': case 'a': case 'A': {
        double dv = va_arg(ap, double);
        Out fo = { body, sizeof(body), 0 };
        if (dv < 0 || (dv == 0 && (1.0 / dv) < 0)) { }
        emit_float(&fo, dv, c, prec, hasprec);
        body[fo.len < sizeof(body) ? fo.len : sizeof(body) - 1] = 0;
        if (body[0] == '-') { isneg = 1; out = body + 1; } else out = body;
        bodylen = -1; break;
      }
      default: { oc(&o, '%'); oc(&o, c); continue; }
    }

    int len = 0; while (out[len]) len++;
    if (c == 's' && hasprec && prec < len) len = prec;
    if (bodylen >= 0) len = bodylen;

    char sign = 0;
    if (isneg) sign = '-'; else if (plus && (c == 'd' || c == 'i' || c == 'f' || c == 'e' || c == 'g' || c == 'F' || c == 'E' || c == 'G')) sign = '+'; else if (space && (c == 'd' || c == 'i')) sign = ' ';

    int plen = 0; while (prefix[plen]) plen++;
    int total = len + (sign ? 1 : 0) + plen;
    int pad = width > total ? width - total : 0;

    if (!left && !zero) for (int i = 0; i < pad; i++) oc(&o, ' ');
    if (sign) oc(&o, sign);
    os(&o, prefix);
    if (!left && zero) for (int i = 0; i < pad; i++) oc(&o, '0');
    for (int i = 0; i < len; i++) oc(&o, out[i]);
    if (left) for (int i = 0; i < pad; i++) oc(&o, ' ');
  }
  if (o.cap) o.buf[o.len < o.cap ? o.len : o.cap - 1] = 0;
  return (int)o.len;
}

int snprintf(char *buf, size_t n, const char *fmt, ...) {
  va_list ap; va_start(ap, fmt); int r = vsnprintf(buf, n, fmt, ap); va_end(ap); return r;
}
int sprintf(char *buf, const char *fmt, ...) {
  va_list ap; va_start(ap, fmt); int r = vsnprintf(buf, (size_t)0x7fffffff, fmt, ap); va_end(ap); return r;
}

struct mlua_FILE { int which; };
static struct mlua_FILE f_out = { 1 }, f_err = { 2 }, f_in = { 0 };
struct mlua_FILE *mlua_stdout = &f_out;
struct mlua_FILE *mlua_stderr = &f_err;
struct mlua_FILE *mlua_stdin  = &f_in;

static void console_write(const char *s, size_t n) {
  CHAR16 w[129]; UINTN wi = 0;
  for (size_t i = 0; i < n; i++) {
    if (s[i] == '\n') w[wi++] = L'\r';
    w[wi++] = (CHAR16)(unsigned char)s[i];
    if (wi >= 126) { w[wi] = 0; gST->ConOut->OutputString(gST->ConOut, w); wi = 0; }
  }
  if (wi) { w[wi] = 0; gST->ConOut->OutputString(gST->ConOut, w); }
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb, struct mlua_FILE *stream) {
  (void)stream; console_write((const char *)ptr, size * nmemb); return nmemb;
}
int fputs(const char *s, struct mlua_FILE *stream) { (void)stream; size_t n = 0; while (s[n]) n++; console_write(s, n); return 0; }
int fputc(int ch, struct mlua_FILE *stream) { (void)stream; char c = (char)ch; console_write(&c, 1); return ch; }
int fflush(struct mlua_FILE *stream) { (void)stream; return 0; }
int fprintf(struct mlua_FILE *stream, const char *fmt, ...) {
  char b[512]; va_list ap; va_start(ap, fmt); int r = vsnprintf(b, sizeof(b), fmt, ap); va_end(ap);
  (void)stream; console_write(b, (size_t)(r < (int)sizeof(b) ? r : (int)sizeof(b) - 1)); return r;
}

struct mlua_FILE *fopen(const char *p, const char *m) { (void)p; (void)m; return (void *)0; }
struct mlua_FILE *freopen(const char *p, const char *m, struct mlua_FILE *s) { (void)p; (void)m; (void)s; return (void *)0; }
int    fclose(struct mlua_FILE *s) { (void)s; return 0; }
size_t fread(void *p, size_t sz, size_t n, struct mlua_FILE *s) { (void)p; (void)sz; (void)n; (void)s; return 0; }
int    getc(struct mlua_FILE *s) { (void)s; return -1; }
int    ungetc(int c, struct mlua_FILE *s) { (void)s; return c; }
char  *fgets(char *b, int n, struct mlua_FILE *s) { (void)b; (void)n; (void)s; return (void *)0; }
int    feof(struct mlua_FILE *s) { (void)s; return 1; }
int    ferror(struct mlua_FILE *s) { (void)s; return 0; }
void   clearerr(struct mlua_FILE *s) { (void)s; }
int    fseek(struct mlua_FILE *s, long o, int w) { (void)s; (void)o; (void)w; return -1; }
long   ftell(struct mlua_FILE *s) { (void)s; return -1; }
int    setvbuf(struct mlua_FILE *s, char *b, int m, size_t sz) { (void)s; (void)b; (void)m; (void)sz; return 0; }
