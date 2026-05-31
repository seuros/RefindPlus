// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2026 Dayo Akanji
// SPDX-FileCopyrightText: 2006 Christoph Pfisterer

#ifndef _FSW_EXT4_H_
#define _FSW_EXT4_H_

#define VOLSTRUCTNAME fsw_ext4_volume
#define DNODESTRUCTNAME fsw_ext4_dnode
#include "fsw_core.h"

#include "fsw_ext4_disk.h"

#define EXT4_SUPERBLOCK_BLOCKSIZE  1024

#define EXT4_SUPERBLOCK_BLOCKNO       1

struct fsw_ext4_volume {
    struct fsw_volume g;

    struct ext4_super_block *sb;
    fsw_u64     *inotab_bno;
    fsw_u32     ind_bcnt;
    fsw_u32     dind_bcnt;
    fsw_u32     inode_size;
};

struct fsw_ext4_dnode {
    struct fsw_dnode g;

    struct ext4_inode *raw;
};

void fsw_ext4_volume_free (
    struct fsw_ext4_volume  *vol
);
void fsw_ext4_dnode_free (
    struct fsw_ext4_volume  *vol,
    struct fsw_ext4_dnode   *dno
);
fsw_status_t fsw_ext4_volume_mount (
    struct fsw_ext4_volume  *vol
);
fsw_status_t fsw_ext4_volume_stat (
    struct fsw_ext4_volume  *vol,
    struct fsw_volume_stat  *sb
);
fsw_status_t fsw_ext4_dnode_fill (
    struct fsw_ext4_volume  *vol,
    struct fsw_ext4_dnode   *dno
);
fsw_status_t fsw_ext4_dnode_stat (
    struct fsw_ext4_volume  *vol,
    struct fsw_ext4_dnode   *dno,
    struct fsw_dnode_stat   *sb
);
fsw_status_t fsw_ext4_get_extent (
    struct fsw_ext4_volume  *vol,
    struct fsw_ext4_dnode   *dno,
    struct fsw_extent       *extent
);
fsw_status_t fsw_ext4_dir_lookup (
    struct fsw_ext4_volume  *vol,
    struct fsw_ext4_dnode   *dno,
    struct fsw_string       *lookup_name,
    struct fsw_ext4_dnode  **child_dno
);
fsw_status_t fsw_ext4_dir_read (
    struct fsw_ext4_volume  *vol,
    struct fsw_ext4_dnode   *dno,
    struct fsw_shandle      *shand,
    struct fsw_ext4_dnode  **child_dno
);
fsw_status_t fsw_ext4_read_dentry (
    struct fsw_shandle      *shand,
    struct ext4_dir_entry   *entry
);
fsw_status_t fsw_ext4_readlink (
    struct fsw_ext4_volume  *vol,
    struct fsw_ext4_dnode   *dno,
    struct fsw_string       *link
);

#endif
