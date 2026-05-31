// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#ifndef _FSW_BCACHEFS_H_
#define _FSW_BCACHEFS_H_

#define VOLSTRUCTNAME fsw_bcachefs_volume
#define DNODESTRUCTNAME fsw_bcachefs_dnode
#include "fsw_core.h"

#define BCACHEFS_ROOT_INO              4096
#define BCACHEFS_SECTOR_SIZE           512
#define BCACHEFS_SUPER_SECTOR          8
#define BCACHEFS_SUPER_READ_SECTORS    128
#define BCACHEFS_SB_FIELD_OFFSET       752
#define BCACHEFS_SB_FIELD_CLEAN        6
#define BCACHEFS_JSET_ENTRY_BTREE_ROOT 1
#define BCACHEFS_BTREE_EXTENTS         0
#define BCACHEFS_BTREE_INODES          1
#define BCACHEFS_BTREE_DIRENTS         2
#define BCACHEFS_BTREE_REFLINK         7
#define BCACHEFS_BTREE_ID_NR           28

#define BCACHEFS_KEY_TYPE_DELETED      0
#define BCACHEFS_KEY_TYPE_WHITEOUT     1
#define BCACHEFS_KEY_TYPE_HASH_WHITEOUT 4
#define BCACHEFS_KEY_TYPE_BTREE_PTR    5
#define BCACHEFS_KEY_TYPE_EXTENT       6
#define BCACHEFS_KEY_TYPE_INODE        8
#define BCACHEFS_KEY_TYPE_DIRENT       10
#define BCACHEFS_KEY_TYPE_REFLINK_P    15
#define BCACHEFS_KEY_TYPE_REFLINK_V    16
#define BCACHEFS_KEY_TYPE_INLINE_DATA  17
#define BCACHEFS_KEY_TYPE_BTREE_PTR_V2 18
#define BCACHEFS_KEY_TYPE_INDIRECT_INLINE_DATA 19
#define BCACHEFS_KEY_TYPE_INODE_V2     23
#define BCACHEFS_KEY_TYPE_INODE_V3     29

#define BCACHEFS_KEY_FORMAT_LOCAL      0
#define BCACHEFS_KEY_FORMAT_CURRENT    1
#define BCACHEFS_BKEY_U64S             5
#define BCACHEFS_MAX_VALUE_BYTES       2048
#define BCACHEFS_MAX_NODE_SECTORS      2048

struct bcachefs_pos {
    fsw_u64 inode;
    fsw_u64 offset;
    fsw_u32 snapshot;
};

struct bcachefs_key {
    fsw_u8  u64s;
    fsw_u8  format;
    fsw_u8  type;
    fsw_u32 size;
    struct bcachefs_pos p;
};

struct bcachefs_bkey_format {
    fsw_u8  key_u64s;
    fsw_u8  nr_fields;
    fsw_u8  bits_per_field[6];
    fsw_u64 field_offset[6];
};

struct bcachefs_btree_ptr {
    fsw_u64 offset;
    fsw_u16 sectors_written;
};

struct bcachefs_btree_root {
    int alive;
    fsw_u8 level;
    struct bcachefs_btree_ptr ptr;
};

struct bcachefs_lookup_result {
    struct bcachefs_key key;
    fsw_u32 value_bytes;
    fsw_u8  value[BCACHEFS_MAX_VALUE_BYTES];
};

struct fsw_bcachefs_volume {
    struct fsw_volume g;
    fsw_u16 block_size_sectors;
    fsw_u32 block_bytes;
    fsw_u32 btree_node_sectors;
    struct bcachefs_btree_root roots[BCACHEFS_BTREE_ID_NR];
};

struct fsw_bcachefs_dnode {
    struct fsw_dnode g;
    fsw_u16 mode;
    fsw_u64 sectors;
};

fsw_status_t fsw_bcachefs_volume_mount(struct fsw_bcachefs_volume *vol);
void fsw_bcachefs_volume_free(struct fsw_bcachefs_volume *vol);
fsw_status_t fsw_bcachefs_volume_stat(struct fsw_bcachefs_volume *vol, struct fsw_volume_stat *sb);
fsw_status_t fsw_bcachefs_dnode_fill(struct fsw_bcachefs_volume *vol, struct fsw_bcachefs_dnode *dno);
void fsw_bcachefs_dnode_free(struct fsw_bcachefs_volume *vol, struct fsw_bcachefs_dnode *dno);
fsw_status_t fsw_bcachefs_dnode_stat(struct fsw_bcachefs_volume *vol, struct fsw_bcachefs_dnode *dno,
                                     struct fsw_dnode_stat *sb);
fsw_status_t fsw_bcachefs_get_extent(struct fsw_bcachefs_volume *vol, struct fsw_bcachefs_dnode *dno,
                                     struct fsw_extent *extent);
fsw_status_t fsw_bcachefs_dir_lookup(struct fsw_bcachefs_volume *vol, struct fsw_bcachefs_dnode *dno,
                                     struct fsw_string *lookup_name, struct fsw_bcachefs_dnode **child_dno);
fsw_status_t fsw_bcachefs_dir_read(struct fsw_bcachefs_volume *vol, struct fsw_bcachefs_dnode *dno,
                                   struct fsw_shandle *shand, struct fsw_bcachefs_dnode **child_dno);
fsw_status_t fsw_bcachefs_readlink(struct fsw_bcachefs_volume *vol, struct fsw_bcachefs_dnode *dno,
                                   struct fsw_string *link);

#endif
