// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2021-2026 Dayo Akanji
// SPDX-FileCopyrightText: 2021 Joe van Tunen

#if 0
#include <grub/err.h>
#include <grub/types.h>
#include <grub/mm.h>
#include <grub/misc.h>
#include <grub/fs.h>
#include <grub/file.h>
#include <grub/dl.h>
#include <grub/deflate.h>
#include <grub/i18n.h>

GRUB_MOD_LICENSE ("GPLv3+");
#endif

#define WSIZE   0x8000

#define INBUFSIZ  0x2000

struct grub_gzio
{
  int err;

  int mem_input_size, mem_input_off;
  uint8_t *mem_input;

  int data_offset;

  int block_type;

  int block_len;

  int last_block;

  int code_state;

  unsigned inflate_n;

  unsigned inflate_d;

  uint8_t inbuf[INBUFSIZ];
  int inbuf_d;

  unsigned long bb;

  unsigned bk;

  uint8_t slide[WSIZE];

  unsigned wp;

  struct huft *tl;

  struct huft *td;

  int bl;

  int bd;

  int saved_offset;
};
typedef struct grub_gzio *grub_gzio_t;

static void initialize_tables (grub_gzio_t);

#define GZIP_MAGIC      grub_le_to_cpu16 (0x8B1F)
#define OLD_GZIP_MAGIC  grub_le_to_cpu16 (0x9E1F)

#define STORED      0
#define COMPRESSED  1

#define LZHED       3

#define DEFLATED    8
#define MAX_METHODS 9

#define ASCII_FLAG   0x01
#define CONTINUATION 0x02
#define EXTRA_FIELD  0x04
#define ORIG_NAME    0x08
#define COMMENT      0x10
#define ENCRYPTED    0x20
#define RESERVED     0xC0

#define UNSUPPORTED_FLAGS       (CONTINUATION | ENCRYPTED | RESERVED)

#define INFLATE_STORED  0
#define INFLATE_FIXED   1
#define INFLATE_DYNAMIC 2

typedef unsigned char uch;
typedef unsigned short ush;
typedef unsigned long ulg;

struct huft
{
  uch e;
  uch b;
  union
    {
      ush n;
      struct huft *t;
    }
  v;
};

static unsigned bitorder[] =
{
  16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
static ush cplens[] =
{
  3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
  35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258, 0, 0};

static ush cplext[] =
{
  0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
  3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0, 99, 99};
static ush cpdist[] =
{
  1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
  257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145,
  8193, 12289, 16385, 24577};
static ush cpdext[] =
{
  0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
  7, 7, 8, 8, 9, 9, 10, 10, 11, 11,
  12, 12, 13, 13};

static int lbits = 9;
static int dbits = 6;

#define BMAX 16
#define N_MAX 288

static ush mask_bits[] =
{
  0x0000,
  0x0001, 0x0003, 0x0007, 0x000f, 0x001f, 0x003f, 0x007f, 0x00ff,
  0x01ff, 0x03ff, 0x07ff, 0x0fff, 0x1fff, 0x3fff, 0x7fff, 0xffff
};

#if !defined(__clang__)
#   if defined(__GNUC__)
#       pragma GCC diagnostic ignored "-Wunsafe-loop-optimizations"
#   endif
#endif

#define NEEDBITS(n) do {while (k<(n)){b|=((ulg)get_byte(gzio))<<k;k+=8;}} while (0)
#define DUMPBITS(n) do {b>>=(n);k-=(n);} while (0)

static int
get_byte (grub_gzio_t gzio)
{
      if (gzio->mem_input_off < gzio->mem_input_size)
        return gzio->mem_input[gzio->mem_input_off++];
      return 0;
}

static void
gzio_seek (grub_gzio_t gzio, grub_off_t off)
{
      if (off > gzio->mem_input_size)
        gzio->err = -1;
      else
        gzio->mem_input_off = off;
}

static int huft_build (unsigned *, unsigned, unsigned, ush *, ush *,
                       struct huft **, int *);
static int huft_free (struct huft *);
static int inflate_codes_in_window (grub_gzio_t);

static int
huft_build (unsigned *b,
            unsigned n,
            unsigned s,
            ush * d,
            ush * e,
            struct huft **t,
            int *m)
{
  unsigned a;
  unsigned c[BMAX + 1];
  unsigned f;
  int g;
  int h;
  register unsigned i;
  register unsigned j;
  register int k;
  int l;
  register unsigned *p;
  register struct huft *q;
  struct huft r;
  struct huft *u[BMAX];
  unsigned v[N_MAX];
  register int w;
  unsigned x[BMAX + 1];
  unsigned *xp;
  int y;
  unsigned z;

  FSW_DO_MEMZERO((char *) c, sizeof (c));
  p = b;
  i = n;
  do
    {
      c[*p]++;
      p++;
    }
  while (--i);
  if (c[0] == n)
    {
      *t = (struct huft *) NULL;
      *m = 0;
      return 0;
    }

  l = *m;
  for (j = 1; j <= BMAX; j++)
    if (c[j])
      break;
  k = j;
  if ((unsigned) l < j)
    l = j;
  for (i = BMAX; i; i--)
    if (c[i])
      break;
  g = i;
  if ((unsigned) l > i)
    l = i;
  *m = l;

  for (y = 1 << j; j < i; j++, y <<= 1)
    if ((y -= c[j]) < 0)
      return 2;
  if ((y -= c[i]) < 0)
    return 2;
  c[i] += y;

  x[1] = j = 0;
  p = c + 1;
  xp = x + 2;
  while (--i)
    {
      *xp++ = (j += *p++);
    }

  p = b;
  i = 0;
  do
    {
      if ((j = *p++) != 0)
        v[x[j]++] = i;
    }
  while (++i < n);

  x[0] = i = 0;
  p = v;
  h = -1;
  w = -l;
  u[0] = (struct huft *) NULL;
  q = (struct huft *) NULL;
  z = 0;

  for (; k <= g; k++)
    {
      a = c[k];
      while (a--)
        {

          while (k > w + l)
            {
              h++;
              w += l;

              z = (z = (unsigned) (g - w)) > (unsigned) l ? (unsigned) l : z;
              if ((f = 1 << (j = k - w)) > a + 1)
                {
                  f -= a + 1;
                  xp = c + k;
                  while (++j < z)
                    {
                      if ((f <<= 1) <= *++xp)
                        break;
                      f -= *xp;
                    }
                }
              z = 1 << j;

              q = (struct huft *) AllocatePool ((z + 1) * sizeof (struct huft));
              if (! q)
                {
                  if (h)
                    huft_free (u[0]);
                  return 3;
                }

              *t = q + 1;
              *(t = &(q->v.t)) = (struct huft *) NULL;
              u[h] = ++q;

              if (h)
                {
                  x[h] = i;
                  r.b = (uch) l;
                  r.e = (uch) (16 + j);
                  r.v.t = q;
                  j = i >> (w - l);
                  u[h - 1][j] = r;
                }
            }

          r.b = (uch) (k - w);
          if (p >= v + n)
            r.e = 99;
          else if (*p < s)
            {
              r.e = (uch) (*p < 256 ? 16 : 15);
              r.v.n = (ush) (*p);
              p++;
            }
          else
            {
              r.e = (uch) e[*p - s];
              r.v.n = d[*p++ - s];
            }

          f = 1 << (k - w);
          for (j = i >> w; j < z; j += f)
            /* coverity[uninit_use: SUPPRESS] */
            q[j] = r;

          for (j = 1 << (k - 1); i & j; j >>= 1)
            i ^= j;
          i ^= j;

          while ((i & ((1 << w) - 1)) != x[h])
            {
              h--;
              w -= l;
            }
        }
    }

  return y != 0 && g != 1;
}

static int
huft_free (struct huft *t)
{
  register struct huft *p, *q;

  p = t;
  while (p != (struct huft *) NULL)
    {
      q = (--p)->v.t;
      FreePool ((char *) p);
      p = q;
    }
  return 0;
}

static int
inflate_codes_in_window (grub_gzio_t gzio)
{
  register unsigned e;
  unsigned n, d;
  unsigned w;
  struct huft *t;
  unsigned ml, md;
  register ulg b;
  register unsigned k;

  d = gzio->inflate_d;
  n = gzio->inflate_n;
  b = gzio->bb;
  k = gzio->bk;
  w = gzio->wp;

  ml = mask_bits[gzio->bl];
  md = mask_bits[gzio->bd];
  while (1)
    {
      if (! gzio->code_state)
        {
          NEEDBITS ((unsigned) gzio->bl);
          if ((e = (t = gzio->tl + ((unsigned) b & ml))->e) > 16)
            do
              {
                if (e == 99)
                  {
                    gzio->err = -1;
                    return 1;
                  }
                DUMPBITS (t->b);
                e -= 16;
                NEEDBITS (e);
              }
            while ((e = (t = t->v.t + ((unsigned) b & mask_bits[e]))->e) > 16);
          DUMPBITS (t->b);

          if (e == 16)
            {
              gzio->slide[w++] = (uch) t->v.n;
              if (w == WSIZE)
                break;
            }
          else

            {

              if (e == 15)
                {
                  gzio->block_len = 0;
                  break;
                }

              NEEDBITS (e);
              n = t->v.n + ((unsigned) b & mask_bits[e]);
              DUMPBITS (e);

              NEEDBITS ((unsigned) gzio->bd);
              if ((e = (t = gzio->td + ((unsigned) b & md))->e) > 16)
                do
                  {
                    if (e == 99)
                      {
                        gzio->err = -1;
                        return 1;
                      }
                    DUMPBITS (t->b);
                    e -= 16;
                    NEEDBITS (e);
                  }
                while ((e = (t = t->v.t + ((unsigned) b & mask_bits[e]))->e)
                       > 16);
              DUMPBITS (t->b);
              NEEDBITS (e);
              d = w - t->v.n - ((unsigned) b & mask_bits[e]);
              DUMPBITS (e);
              gzio->code_state++;
            }
        }

      if (gzio->code_state)
        {

          do
            {
              n -= (e = (e = WSIZE - ((d &= WSIZE - 1) > w ? d : w)) > n ? n
                    : e);

              if (w - d >= e)
                {
                  FSW_DO_MEMCPY(gzio->slide + w, gzio->slide + d, e);
                  w += e;
                  d += e;
                }
              else

                {
                  while (e--)
                    gzio->slide[w++] = gzio->slide[d++];
                }

              if (w == WSIZE)
                break;
            }
          while (n);

          if (! n)
            gzio->code_state--;

          if (w == WSIZE)
            break;
        }
    }

  gzio->inflate_d = d;
  gzio->inflate_n = n;
  gzio->wp = w;
  gzio->bb = b;
  gzio->bk = k;

  return ! gzio->block_len;
}

static void
init_stored_block (grub_gzio_t gzio)
{
  register ulg b;
  register unsigned k;

  b = gzio->bb;
  k = gzio->bk;

  DUMPBITS (k & 7);

  NEEDBITS (16);
  gzio->block_len = ((unsigned) b & 0xffff);
  DUMPBITS (16);
  NEEDBITS (16);
  if (gzio->block_len != (int) ((~b) & 0xffff))
    gzio->err = -1;
  DUMPBITS (16);

  gzio->bb = b;
  gzio->bk = k;
}

static void
init_fixed_block (grub_gzio_t gzio)
{
  int i;
  unsigned l[288];

  for (i = 0; i < 144; i++)
    l[i] = 8;
  for (; i < 256; i++)
    l[i] = 9;
  for (; i < 280; i++)
    l[i] = 7;
  for (; i < 288; i++)
    l[i] = 8;
  gzio->bl = 7;
  if (huft_build (l, 288, 257, cplens, cplext, &gzio->tl, &gzio->bl) != 0)
    {
        gzio->err = -1;
      return;
    }

  for (i = 0; i < 30; i++)
    l[i] = 5;
  gzio->bd = 5;
  if (huft_build (l, 30, 0, cpdist, cpdext, &gzio->td, &gzio->bd) > 1)
    {
        gzio->err = -1;
      huft_free (gzio->tl);
      gzio->tl = 0;
      return;
    }

  gzio->code_state = 0;
  gzio->block_len++;
}

static void
init_dynamic_block (grub_gzio_t gzio)
{
  int i;
  unsigned j;
  unsigned l;
  unsigned m;
  unsigned n;
  unsigned nb;
  unsigned nl;
  unsigned nd;
  unsigned ll[286 + 30];
  register ulg b;
  register unsigned k;

  b = gzio->bb;
  k = gzio->bk;

  NEEDBITS (5);
  nl = 257 + ((unsigned) b & 0x1f);
  DUMPBITS (5);
  NEEDBITS (5);
  nd = 1 + ((unsigned) b & 0x1f);
  DUMPBITS (5);
  NEEDBITS (4);
  nb = 4 + ((unsigned) b & 0xf);
  DUMPBITS (4);
  if (nl > 286 || nd > 30)
    {
      gzio->err = -1;
      return;
    }

  for (j = 0; j < nb; j++)
    {
      NEEDBITS (3);
      ll[bitorder[j]] = (unsigned) b & 7;
      DUMPBITS (3);
    }
  for (; j < 19; j++)
    ll[bitorder[j]] = 0;

  gzio->bl = 7;
  if (huft_build (ll, 19, 19, NULL, NULL, &gzio->tl, &gzio->bl) != 0)
    {
      gzio->err = -1;
      return;
    }

  n = nl + nd;
  m = mask_bits[gzio->bl];
  i = l = 0;
  while ((unsigned) i < n)
    {
      NEEDBITS ((unsigned) gzio->bl);
      j = (gzio->td = gzio->tl + ((unsigned) b & m))->b;
      DUMPBITS (j);
      j = gzio->td->v.n;
      if (j < 16)
        ll[i++] = l = j;
      else if (j == 16)
        {
          NEEDBITS (2);
          j = 3 + ((unsigned) b & 3);
          DUMPBITS (2);
          if ((unsigned) i + j > n)
            {
            gzio->err = -1;
              return;
            }
          while (j--)
            ll[i++] = l;
        }
      else if (j == 17)
        {
          NEEDBITS (3);
          j = 3 + ((unsigned) b & 7);
          DUMPBITS (3);
          if ((unsigned) i + j > n)
            {
              gzio->err = -1;
              return;
            }
          while (j--)
            ll[i++] = 0;
          l = 0;
        }
      else

        {
          NEEDBITS (7);
          j = 11 + ((unsigned) b & 0x7f);
          DUMPBITS (7);
          if ((unsigned) i + j > n)
            {
              gzio->err = -1;
              return;
            }
          while (j--)
            ll[i++] = 0;
          l = 0;
        }
    }

  huft_free (gzio->tl);
  gzio->td = 0;
  gzio->tl = 0;

  gzio->bb = b;
  gzio->bk = k;

  gzio->bl = lbits;
  if (huft_build (ll, nl, 257, cplens, cplext, &gzio->tl, &gzio->bl) != 0)
    {
      gzio->err = -1;
      return;
    }
  gzio->bd = dbits;
  if (huft_build (ll + nl, nd, 0, cpdist, cpdext, &gzio->td, &gzio->bd) != 0)
    {
      huft_free (gzio->tl);
      gzio->tl = 0;
      gzio->err = -1;
      return;
    }

  gzio->code_state = 0;
  gzio->block_len++;
}

static void
get_new_block (grub_gzio_t gzio)
{
  register ulg b;
  register unsigned k;

  b = gzio->bb;
  k = gzio->bk;

  NEEDBITS (1);
  gzio->last_block = (int) b & 1;
  DUMPBITS (1);

  NEEDBITS (2);
  gzio->block_type = (unsigned) b & 3;
  DUMPBITS (2);

  gzio->bb = b;
  gzio->bk = k;

  switch (gzio->block_type)
    {
    case INFLATE_STORED:
      init_stored_block (gzio);
      break;
    case INFLATE_FIXED:
      init_fixed_block (gzio);
      break;
    case INFLATE_DYNAMIC:
      init_dynamic_block (gzio);
      break;
    default:
      break;
    }
}

static void
inflate_window (grub_gzio_t gzio)
{

  gzio->wp = 0;

  while (gzio->wp < WSIZE && !gzio->err)
    {
      if (! gzio->block_len)
        {
          if (gzio->last_block)
            break;

          get_new_block (gzio);
        }

      if (gzio->block_type > INFLATE_DYNAMIC)
        gzio->err = -1;

      if (gzio->err)
        return;

      if (gzio->block_type == INFLATE_STORED)
        {
          int w = gzio->wp;

          while (gzio->block_len && w < WSIZE && !gzio->err)
            {
              gzio->slide[w++] = get_byte (gzio);
              gzio->block_len--;
            }

          gzio->wp = w;

          continue;
        }

      /* coverity[var_deref_model: SUPPRESS] */
      if (inflate_codes_in_window (gzio))
        {
          huft_free (gzio->tl);
          huft_free (gzio->td);
          gzio->tl = 0;
          gzio->td = 0;
        }
    }

  gzio->saved_offset += WSIZE;

}

static void
initialize_tables (grub_gzio_t gzio)
{
  gzio->saved_offset = 0;
  gzio_seek (gzio, gzio->data_offset);

  gzio->bk = 0;
  gzio->bb = 0;

  gzio->last_block = 0;
  gzio->block_len = 0;

  huft_free (gzio->tl);
  huft_free (gzio->td);
}

static int
test_zlib_header (grub_gzio_t gzio)
{
  uint8_t cmf, flg;

  cmf = get_byte (gzio);
  flg = get_byte (gzio);

  if ((cmf & 0xf) != DEFLATED)
    {
      return 0;
    }

  if ((cmf * 256 + flg) % 31)
    {
      return 0;
    }

  if (flg & 0x20)
    {
      return 0;
    }

  gzio->data_offset = 2;
  initialize_tables (gzio);

  return 1;
}

static grub_ssize_t
grub_gzio_read_real (grub_gzio_t gzio, grub_off_t offset,
                     char *buf, grub_size_t len)
{
  grub_ssize_t ret = 0;

  if (gzio->saved_offset > offset + WSIZE)
    initialize_tables (gzio);

  while (len > 0 && !gzio->err)
    {
      register grub_size_t size;
      register char *srcaddr;

      while (offset >= gzio->saved_offset)
        inflate_window (gzio);

      srcaddr = (char *) ((offset & (WSIZE - 1)) + gzio->slide);
      size = gzio->saved_offset - offset;
      if (size > len)
        size = len;

      FSW_DO_MEMCPY(buf, srcaddr, size);

      buf += size;
      len -= size;
      ret += size;
      offset += size;
    }

  if (gzio->err)
    ret = -1;

  return ret;
}

grub_ssize_t
grub_zlib_decompress (char *inbuf, grub_size_t insize, grub_off_t off,
                      char *outbuf, grub_size_t outsize)
{
  grub_gzio_t gzio = 0;
  grub_ssize_t ret;

  gzio = AllocatePool (sizeof (*gzio));
  if (! gzio)
    return -1;
  FSW_DO_MEMZERO(gzio, sizeof (*gzio));
  gzio->mem_input = (uint8_t *) inbuf;
  gzio->mem_input_size = insize;
  gzio->mem_input_off = 0;

  if (!test_zlib_header (gzio))
    {
      FreePool (gzio);
      return -1;
    }

  ret = grub_gzio_read_real (gzio, off, outbuf, outsize);
  FreePool (gzio);

  return ret;
}
