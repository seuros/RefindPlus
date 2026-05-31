// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2020-2025 Dayo Akanji
// SPDX-FileCopyrightText: 2006 Christoph Pfisterer

#ifndef _FSW_HFS_H_
#define _FSW_HFS_H_

#define VOLSTRUCTNAME fsw_hfs_volume
#define DNODESTRUCTNAME fsw_hfs_dnode

#include "fsw_core.h"

#define HFS_BLOCKSIZE            512

#define HFS_SUPERBLOCK_BLOCKNO   2

#define __APPLE_API_PRIVATE
#define __APPLE_API_UNSTABLE

#define u_int8_t  fsw_u8
#define u_int16_t fsw_u16
#define u_int32_t fsw_u32
#define u_int64_t fsw_u64
#define int8_t    fsw_s8
#define int16_t   fsw_s16
#define int32_t   fsw_s32
#define int64_t   fsw_s64

#include "hfs_format.h"

#undef u_int8_t
#undef u_int16_t
#undef u_int32_t
#undef u_int64_t
#undef int8_t
#undef int16_t
#undef int32_t
#undef int64_t

#pragma pack(push, 1)
#ifdef _MSC_VER

# pragma warning (disable:4201)
# define inline __inline
#endif

struct hfs_dirrec {
    fsw_u8      _dummy;
};

struct fsw_hfs_key
{
  union
  {
    struct HFSPlusExtentKey  ext_key;
    struct HFSPlusCatalogKey cat_key;
    fsw_u16                  key_len;
  } HFS_ALIGNMENT;
} HFS_ALIGNMENT;

#pragma pack(pop)

typedef enum {

    FSW_HFS_PLAIN = 0,

    FSW_HFS_PLUS,

    FSW_HFS_PLUS_EMB
} fsw_hfs_kind;

struct fsw_hfs_dnode
{
  struct fsw_dnode          g;
  HFSPlusExtentRecord       extents;
  fsw_u32                   ctime;
  fsw_u32                   mtime;
  fsw_u64                   used_bytes;
};

struct fsw_hfs_btree
{
    fsw_u32                  root_node;
    fsw_u32                  node_size;
    struct fsw_hfs_dnode*    file;
};

struct fsw_hfs_volume
{
    struct fsw_volume            g;

    struct HFSPlusVolumeHeader   *primary_voldesc;
    struct fsw_hfs_btree          catalog_tree;
    struct fsw_hfs_btree          extents_tree;
    struct fsw_hfs_dnode          root_file;
    int                           case_sensitive;
    fsw_u32                       block_size_shift;
    fsw_hfs_kind                  hfs_kind;
    fsw_u32                       emb_block_off;
};

static inline fsw_u16
swab16(fsw_u16 x)
{
    /* coverity[tainted_data: SUPPRESS] */
    return (x<<8 | ((x & 0xff00)>>8));
}

static inline fsw_u32
swab32(fsw_u32 x)
{
    return x<<24 | x>>24 |
            (x & (fsw_u32)0x0000ff00UL)<<8 |
            (x & (fsw_u32)0x00ff0000UL)>>8;
}

static inline fsw_u64
swab64(fsw_u64 x)
{
    return x<<56 | x>>56 |
            (x & (fsw_u64)0x000000000000ff00ULL)<<40 |
            (x & (fsw_u64)0x0000000000ff0000ULL)<<24 |
            (x & (fsw_u64)0x00000000ff000000ULL)<< 8 |
            (x & (fsw_u64)0x000000ff00000000ULL)>> 8 |
            (x & (fsw_u64)0x0000ff0000000000ULL)>>24 |
            (x & (fsw_u64)0x00ff000000000000ULL)>>40;
}

static inline fsw_u16
be16_to_cpu(fsw_u16 x)
{
    return swab16(x);
}

static inline fsw_u16
cpu_to_be16(fsw_u16 x)
{
    return swab16(x);
}

static inline fsw_u32
cpu_to_be32(fsw_u32 x)
{
    return swab32(x);
}

static inline fsw_u32
be32_to_cpu(fsw_u32 x)
{
    return swab32(x);
}

static inline fsw_u64
be64_to_cpu(fsw_u64 x)
{
    return swab64(x);
}

#endif
