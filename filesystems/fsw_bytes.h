// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#ifndef _FSW_BYTES_H_
#define _FSW_BYTES_H_

static inline fsw_u16 fsw_le16 (const fsw_u8 *p) {
    return (fsw_u16) (
        ((fsw_u16) p[0]) |
        ((fsw_u16) p[1] << 8)
    );
}

static inline fsw_u32 fsw_le32 (const fsw_u8 *p) {
    return (fsw_u32) (
        ((fsw_u32) p[0])       |
        ((fsw_u32) p[1] <<  8) |
        ((fsw_u32) p[2] << 16) |
        ((fsw_u32) p[3] << 24)
    );
}

static inline fsw_u64 fsw_le64 (const fsw_u8 *p) {
    return ((fsw_u64) fsw_le32 (p)) | ((fsw_u64) fsw_le32 (p + 4) << 32);
}

static inline fsw_u16 fsw_le16_at (const void *buf, fsw_u32 off) {
    return fsw_le16 ((const fsw_u8 *) buf + off);
}

static inline fsw_u32 fsw_le32_at (const void *buf, fsw_u32 off) {
    return fsw_le32 ((const fsw_u8 *) buf + off);
}

static inline fsw_u64 fsw_le64_at (const void *buf, fsw_u32 off) {
    return fsw_le64 ((const fsw_u8 *) buf + off);
}

static inline fsw_s32 fsw_les32_at (const void *buf, fsw_u32 off) {
    return (fsw_s32) fsw_le32_at (buf, off);
}

#endif
