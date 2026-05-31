// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#ifndef _FSW_UFS_H_
#define _FSW_UFS_H_

#define VOLSTRUCTNAME fsw_ufs_volume
#define DNODESTRUCTNAME fsw_ufs_dnode
#include "fsw_core.h"

#define UFS1_MAGIC              0x00011954U
#define UFS2_MAGIC              0x19540119U

#define UFS_SBLOCK_FLOPPY       0U
#define UFS_SBLOCK_UFS1         8192U
#define UFS_SBLOCK_UFS2         65536U
#define UFS_SBLOCK_PIGGY        262144U
#define UFS_SBLOCK_SIZE         8192U

#define UFS_ROOT_INO            2U
#define UFS_NDADDR              12U
#define UFS_NIADDR              3U
#define UFS_MAXNAMLEN           255U
#define UFS_DIRENT_HEADER_SIZE  8U

enum fsw_ufs_variant {
    FSW_UFS_VARIANT_NONE = 0,
    FSW_UFS_VARIANT_UFS1,
    FSW_UFS_VARIANT_UFS2
};

struct ufs_direct {
    fsw_u32 d_ino;
    fsw_u16 d_reclen;
    fsw_u8  d_type;
    fsw_u8  d_namlen;
    char    d_name[UFS_MAXNAMLEN + 1];
};

struct fsw_ufs_volume {
    struct fsw_volume g;

    fsw_u8              *sb_raw;
    enum fsw_ufs_variant variant;
    const char          *variant_name;
    fsw_u32              superblock_offset;

    fsw_u32              bsize;
    fsw_u32              fsize;
    fsw_u32              frag_per_block;
    fsw_u32              inode_size;
    fsw_u32              pointer_size;
    fsw_u32              inodes_per_block;
    fsw_u64              ind_bcnt;
    int                  use_cg_offset;
};

struct fsw_ufs_dnode {
    struct fsw_dnode g;

    fsw_u8 *raw;
};

fsw_status_t fsw_ufs_volume_mount (
    struct fsw_ufs_volume *vol
);
void fsw_ufs_volume_free (
    struct fsw_ufs_volume *vol
);
fsw_status_t fsw_ufs_volume_stat (
    struct fsw_ufs_volume *vol,
    struct fsw_volume_stat *sb
);

fsw_status_t fsw_ufs_dnode_fill (
    struct fsw_ufs_volume *vol,
    struct fsw_ufs_dnode *dno
);
void fsw_ufs_dnode_free (
    struct fsw_ufs_volume *vol,
    struct fsw_ufs_dnode *dno
);
fsw_status_t fsw_ufs_dnode_stat (
    struct fsw_ufs_volume *vol,
    struct fsw_ufs_dnode *dno,
    struct fsw_dnode_stat *sb
);
fsw_status_t fsw_ufs_get_extent (
    struct fsw_ufs_volume *vol,
    struct fsw_ufs_dnode *dno,
    struct fsw_extent *extent
);

fsw_status_t fsw_ufs_dir_lookup (
    struct fsw_ufs_volume *vol,
    struct fsw_ufs_dnode *dno,
    struct fsw_string *lookup_name,
    struct fsw_ufs_dnode **child_dno_out
);
fsw_status_t fsw_ufs_dir_read (
    struct fsw_ufs_volume *vol,
    struct fsw_ufs_dnode *dno,
    struct fsw_shandle *shand,
    struct fsw_ufs_dnode **child_dno_out
);
fsw_status_t fsw_ufs_read_dentry (
    struct fsw_shandle *shand,
    struct ufs_direct *entry
);
fsw_status_t fsw_ufs_readlink (
    struct fsw_ufs_volume *vol,
    struct fsw_ufs_dnode *dno,
    struct fsw_string *link_target
);

#endif
