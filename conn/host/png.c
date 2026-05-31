// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#include "png.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static uint32_t crc_table[256];
static int crc_ready = 0;
static void crc_init(void) {
    for (uint32_t n = 0; n < 256; n++) {
        uint32_t c = n;
        for (int k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        crc_table[n] = c;
    }
    crc_ready = 1;
}
static uint32_t crc32_upd(uint32_t c, const unsigned char *buf, size_t len) {
    if (!crc_ready) crc_init();
    c ^= 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) c = crc_table[(c ^ buf[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}
static uint32_t adler32(const unsigned char *d, size_t n) {
    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < n; i++) { a = (a + d[i]) % 65521; b = (b + a) % 65521; }
    return (b << 16) | a;
}

static void put_be32(unsigned char *p, uint32_t v) {
    p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v;
}

static int write_chunk(FILE *f, const char *type,
                       const unsigned char *data, uint32_t len) {
    unsigned char hdr[8];
    put_be32(hdr, len);
    memcpy(hdr + 4, type, 4);
    if (fwrite(hdr, 1, 8, f) != 8) return -1;
    if (len && fwrite(data, 1, len, f) != len) return -1;
    uint32_t crc = crc32_upd(0, (const unsigned char *)type, 4);
    crc = crc32_upd(crc, data, len);
    unsigned char cb[4]; put_be32(cb, crc);
    return fwrite(cb, 1, 4, f) == 4 ? 0 : -1;
}

int conn_write_png(const char *path, const unsigned char *rgba, int w, int h) {

    size_t stride = (size_t)w * 4;
    size_t raw_len = (size_t)h * (stride + 1);
    unsigned char *raw = (unsigned char *)malloc(raw_len);
    if (!raw) return -1;
    for (int y = 0; y < h; y++) {
        unsigned char *row = raw + (size_t)y * (stride + 1);
        row[0] = 0;
        memcpy(row + 1, rgba + (size_t)y * stride, stride);
    }

    size_t nblocks = (raw_len + 65534) / 65535;
    if (nblocks == 0) nblocks = 1;
    size_t z_len = 2 + nblocks * 5 + raw_len + 4;
    unsigned char *z = (unsigned char *)malloc(z_len);
    if (!z) { free(raw); return -1; }
    size_t zp = 0;
    z[zp++] = 0x78; z[zp++] = 0x01;
    size_t off = 0;
    while (off < raw_len || nblocks == 1) {
        size_t n = raw_len - off;
        if (n > 65535) n = 65535;
        int final = (off + n >= raw_len);
        z[zp++] = final ? 1 : 0;
        z[zp++] = n & 0xFF; z[zp++] = (n >> 8) & 0xFF;
        z[zp++] = ~n & 0xFF; z[zp++] = (~n >> 8) & 0xFF;
        memcpy(z + zp, raw + off, n); zp += n;
        off += n;
        if (final) break;
    }
    uint32_t ad = adler32(raw, raw_len);
    put_be32(z + zp, ad); zp += 4;

    FILE *f = fopen(path, "wb");
    if (!f) { free(raw); free(z); return -1; }

    static const unsigned char sig[8] = {137,80,78,71,13,10,26,10};
    int rc = (fwrite(sig, 1, 8, f) == 8) ? 0 : -1;

    unsigned char ihdr[13];
    put_be32(ihdr + 0, (uint32_t)w);
    put_be32(ihdr + 4, (uint32_t)h);
    ihdr[8] = 8;
    ihdr[9] = 6;
    ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
    if (!rc) rc = write_chunk(f, "IHDR", ihdr, 13);
    if (!rc) rc = write_chunk(f, "IDAT", z, (uint32_t)zp);
    if (!rc) rc = write_chunk(f, "IEND", NULL, 0);

    fclose(f);
    free(raw); free(z);
    return rc;
}
