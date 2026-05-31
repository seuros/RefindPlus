// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2024-2026 Dayo Akanji
// SPDX-FileCopyrightText: 2006 Christoph Pfisterer

#ifndef _FSW_EXT4_DISK_H_
#define _FSW_EXT4_DISK_H_

typedef fsw_s8  __s8;
typedef fsw_u8  __u8;
typedef fsw_s16 __s16;
typedef fsw_u16 __u16;
typedef fsw_s32 __s32;
typedef fsw_u32 __u32;
typedef fsw_s64 __s64;
typedef fsw_u64 __u64;

typedef __u16   __le16;
typedef __u32   __le32;
typedef __u64   __le64;

#define	EXT4_BAD_INO		 1
#define EXT4_ROOT_INO		 2
#define EXT4_USR_QUOTA_INO	 3
#define EXT4_GRP_QUOTA_INO	 4
#define EXT4_BOOT_LOADER_INO 5
#define EXT4_UNDEL_DIR_INO	 6
#define EXT4_RESIZE_INO		 7
#define EXT4_JOURNAL_INO	 8

#define EXT4_SUPER_MAGIC        0xEF53

#define EXT4_MIN_BLOCK_SIZE             1024
#define EXT4_MAX_BLOCK_SIZE             4096
#define EXT4_MIN_BLOCK_LOG_SIZE           10
#define EXT4_BLOCK_SIZE(s)              (EXT4_MIN_BLOCK_SIZE << (s)->s_log_block_size)
#define EXT4_ADDR_PER_BLOCK(s)          (EXT4_BLOCK_SIZE(s) / sizeof (__u32))
#define EXT4_BLOCK_SIZE_BITS(s)         ((s)->s_log_block_size + 10)
#define EXT4_INODE_SIZE(s)      (((s)->s_rev_level == EXT4_GOOD_OLD_REV) ? \
                                 EXT4_GOOD_OLD_INODE_SIZE : \
                                 (s)->s_inode_size)

struct ext4_group_desc
{
	__le32	bg_block_bitmap_lo;
	__le32	bg_inode_bitmap_lo;
	__le32	bg_inode_table_lo;
	__le16	bg_free_blocks_count_lo;
	__le16	bg_free_inodes_count_lo;
	__le16	bg_used_dirs_count_lo;
	__le16	bg_flags;
	__le32  bg_exclude_bitmap_lo;
	__le16  bg_block_bitmap_csum_lo;
	__le16  bg_inode_bitmap_csum_lo;
	__le16  bg_itable_unused_lo;
	__le16  bg_checksum;
	__le32	bg_block_bitmap_hi;
	__le32	bg_inode_bitmap_hi;
	__le32	bg_inode_table_hi;
	__le16	bg_free_blocks_count_hi;
	__le16	bg_free_inodes_count_hi;
	__le16	bg_used_dirs_count_hi;
	__le16  bg_itable_unused_hi;
	__le32  bg_exclude_bitmap_hi;
	__le16  bg_block_bitmap_csum_hi;
	__le16  bg_inode_bitmap_csum_hi;
	__u32   bg_reserved;
};

#define EXT4_MIN_DESC_SIZE              32
#define EXT4_MIN_DESC_SIZE_64BIT        64
#define EXT4_MAX_DESC_SIZE              EXT4_MIN_BLOCK_SIZE
#define EXT4_DESC_SIZE(s)              ((s)->s_desc_size)
#define EXT4_BLOCKS_PER_GROUP(s)       ((s)->s_blocks_per_group)
#define EXT4_DESC_PER_BLOCK(s)         (EXT4_BLOCK_SIZE(s) / EXT4_DESC_SIZE(s))
#define EXT4_INODES_PER_GROUP(s)       ((s)->s_inodes_per_group)

#define EXT4_NDIR_BLOCKS                12
#define EXT4_IND_BLOCK                  EXT4_NDIR_BLOCKS
#define EXT4_DIND_BLOCK                 (EXT4_IND_BLOCK + 1)
#define EXT4_TIND_BLOCK                 (EXT4_DIND_BLOCK + 1)
#define EXT4_N_BLOCKS                   (EXT4_TIND_BLOCK + 1)

#define EXT4_SECRM_FL                   0x00000001
#define EXT4_UNRM_FL                    0x00000002
#define EXT4_COMPR_FL                   0x00000004
#define EXT4_SYNC_FL                    0x00000008
#define EXT4_IMMUTABLE_FL               0x00000010
#define EXT4_APPEND_FL                  0x00000020
#define EXT4_NODUMP_FL                  0x00000040
#define EXT4_NOATIME_FL                 0x00000080

#define EXT4_DIRTY_FL                   0x00000100
#define EXT4_COMPRBLK_FL                0x00000200
#define EXT4_NOCOMP_FL                  0x00000400
#define EXT4_ECOMPR_FL                  0x00000800

#define EXT4_INDEX_FL                   0x00001000
#define EXT4_IMAGIC_FL                  0x00002000
#define EXT4_JOURNAL_DATA_FL            0x00004000
#define EXT4_NOTAIL_FL                  0x00008000
#define EXT4_DIRSYNC_FL                 0x00010000
#define EXT4_TOPDIR_FL                  0x00020000
#define EXT4_HUGE_FILE_FL               0x00040000
#define EXT4_EXTENTS_FL                 0x00080000
#define EXT4_EA_INODE_FL                0x00200000
#define EXT4_EOFBLOCKS_FL               0x00400000
#define EXT4_INLINE_DATA_FL             0x20000000
#define EXT4_RESERVED_FL                0x80000000

#define EXT4_FL_USER_VISIBLE		0x004BDFFF
#define EXT4_FL_USER_MODIFIABLE		0x004B80FF

struct ext4_inode {
	__le16	i_mode;
	__le16	i_uid;
	__le32	i_size_lo;
	__le32	i_atime;
	__le32	i_ctime;
	__le32	i_mtime;
	__le32	i_dtime;
	__le16	i_gid;
	__le16	i_links_count;
	__le32	i_blocks_lo;
	__le32	i_flags;
	union {
		struct {
			__le32  l_i_version;
		} linux1;
		struct {
			__u32  h_i_translator;
		} hurd1;
		struct {
			__u32  m_i_reserved1;
		} masix1;
	} osd1;
	__le32	i_block[EXT4_N_BLOCKS];
	__le32	i_generation;
	__le32	i_file_acl_lo;
	__le32	i_size_high;
	__le32	i_obso_faddr;
	union {
		struct {
			__le16	l_i_blocks_high;
			__le16	l_i_file_acl_high;
			__le16	l_i_uid_high;
			__le16	l_i_gid_high;
			__le16	l_i_checksum_lo;
			__le16	l_i_reserved;
		} linux2;
		struct {
			__le16	h_i_reserved1;
			__u16	h_i_mode_high;
			__u16	h_i_uid_high;
			__u16	h_i_gid_high;
			__u32	h_i_author;
		} hurd2;
		struct {
			__le16	h_i_reserved1;
			__le16	m_i_file_acl_high;
			__u32	m_i_reserved2[2];
		} masix2;
	} osd2;
	__le16	i_extra_isize;
	__le16	i_checksum_hi;
	__le32  i_ctime_extra;
	__le32  i_mtime_extra;
	__le32  i_atime_extra;
	__le32  i_crtime;
	__le32  i_crtime_extra;
	__le32  i_version_hi;
};

enum {
	EXT4_INODE_SECRM	= 0,
	EXT4_INODE_UNRM		= 1,
	EXT4_INODE_COMPR	= 2,
	EXT4_INODE_SYNC		= 3,
	EXT4_INODE_IMMUTABLE	= 4,
	EXT4_INODE_APPEND	= 5,
	EXT4_INODE_NODUMP	= 6,
	EXT4_INODE_NOATIME	= 7,

	EXT4_INODE_DIRTY	= 8,
	EXT4_INODE_COMPRBLK	= 9,
	EXT4_INODE_NOCOMPR	= 10,
	EXT4_INODE_ECOMPR	= 11,

	EXT4_INODE_INDEX	= 12,
	EXT4_INODE_IMAGIC	= 13,
	EXT4_INODE_JOURNAL_DATA	= 14,
	EXT4_INODE_NOTAIL	= 15,
	EXT4_INODE_DIRSYNC	= 16,
	EXT4_INODE_TOPDIR	= 17,
	EXT4_INODE_HUGE_FILE	= 18,
	EXT4_INODE_EXTENTS	= 19,
	EXT4_INODE_EA_INODE	= 21,
	EXT4_INODE_EOFBLOCKS	= 22,
	EXT4_INODE_RESERVED	= 31,
};

struct ext4_super_block {
	__le32	s_inodes_count;
	__le32	s_blocks_count_lo;
	__le32	s_r_blocks_count_lo;
	__le32	s_free_blocks_count_lo;
	__le32	s_free_inodes_count;
	__le32	s_first_data_block;
	__le32	s_log_block_size;
	__le32	s_log_cluster_size;
	__le32	s_blocks_per_group;
	__le32	s_clusters_per_group;
	__le32	s_inodes_per_group;
	__le32	s_mtime;
	__le32	s_wtime;
	__le16	s_mnt_count;
	__le16	s_max_mnt_count;
	__le16	s_magic;
	__le16	s_state;
	__le16	s_errors;
	__le16	s_minor_rev_level;
	__le32	s_lastcheck;
	__le32	s_checkinterval;
	__le32	s_creator_os;
	__le32	s_rev_level;
	__le16	s_def_resuid;
	__le16	s_def_resgid;

	__le32	s_first_ino;
	__le16  s_inode_size;
	__le16	s_block_group_nr;
	__le32	s_feature_compat;
	__le32	s_feature_incompat;
	__le32	s_feature_ro_compat;
	__u8	s_uuid[16];
	char	s_volume_name[16];
	char	s_last_mounted[64];
	__le32	s_algorithm_usage_bitmap;

	__u8	s_prealloc_blocks;
	__u8	s_prealloc_dir_blocks;
	__le16	s_reserved_gdt_blocks;

	__u8	s_journal_uuid[16];
	__le32	s_journal_inum;
	__le32	s_journal_dev;
	__le32	s_last_orphan;
	__le32	s_hash_seed[4];
	__u8	s_def_hash_version;
	__u8	s_jnl_backup_type;
	__le16  s_desc_size;
	__le32	s_default_mount_opts;
	__le32	s_first_meta_bg;
	__le32	s_mkfs_time;
	__le32	s_jnl_blocks[17];

	__le32	s_blocks_count_hi;
	__le32	s_r_blocks_count_hi;
	__le32	s_free_blocks_count_hi;
	__le16	s_min_extra_isize;
	__le16	s_want_extra_isize;
	__le32	s_flags;
	__le16  s_raid_stride;
	__le16  s_mmp_update_interval;
	__le64  s_mmp_block;
	__le32  s_raid_stripe_width;
	__u8	s_log_groups_per_flex;
	__u8	s_checksum_type;
	__le16  s_reserved_pad;
	__le64	s_kbytes_written;
	__le32	s_snapshot_inum;
	__le32	s_snapshot_id;
	__le64	s_snapshot_r_blocks_count;

	__le32	s_snapshot_list;

#define EXT4_S_ERR_START offsetof(struct ext4_super_block, s_error_count)
	__le32	s_error_count;
	__le32	s_first_error_time;
	__le32	s_first_error_ino;
	__le64	s_first_error_block;
	__u8	s_first_error_func[32];
	__le32	s_first_error_line;
	__le32	s_last_error_time;
	__le32	s_last_error_ino;
	__le32	s_last_error_line;
	__le64	s_last_error_block;
	__u8	s_last_error_func[32];
#define EXT4_S_ERR_END offsetof(struct ext4_super_block, s_mount_opts)
	__u8	s_mount_opts[64];
	__le32	s_usr_quota_inum;
	__le32	s_grp_quota_inum;
	__le32	s_overhead_clusters;
	__le32	s_reserved[108];
	__le32	s_checksum;
};

#define EXT4_GOOD_OLD_REV       0
#define EXT4_DYNAMIC_REV        1

#define EXT4_CURRENT_REV        EXT4_GOOD_OLD_REV
#define EXT4_MAX_SUPP_REV       EXT4_DYNAMIC_REV

#define EXT4_GOOD_OLD_INODE_SIZE 128

#define EXT4_FEATURE_RO_COMPAT_SPARSE_SUPER     0x0001

#define EXT4_FEATURE_INCOMPAT_COMPRESSION       0x0001
#define EXT4_FEATURE_INCOMPAT_FILETYPE          0x0002
#define EXT4_FEATURE_INCOMPAT_RECOVER           0x0004
#define EXT4_FEATURE_INCOMPAT_JOURNAL_DEV       0x0008
#define EXT4_FEATURE_INCOMPAT_META_BG           0x0010
#define EXT4_FEATURE_INCOMPAT_EXTENTS           0x0040
#define EXT4_FEATURE_INCOMPAT_64BIT             0x0080
#define EXT4_FEATURE_INCOMPAT_MMP               0x0100
#define EXT4_FEATURE_INCOMPAT_FLEX_BG           0x0200
#define EXT4_FEATURE_INCOMPAT_EA_INODE          0x0400
#define EXT4_FEATURE_INCOMPAT_DIRDATA           0x1000
#define EXT4_FEATURE_INCOMPAT_BG_USE_META_CSUM  0x2000
#define EXT4_FEATURE_INCOMPAT_LARGEDIR          0x4000
#define EXT4_FEATURE_INCOMPAT_INLINEDATA        0x8000
#define EXT4_FEATURE_INCOMPAT_ENCRYPT           0x10000

#define EXT4_FEATURE_INCOMPAT_SUPP	(EXT4_FEATURE_INCOMPAT_FILETYPE| \
					 EXT4_FEATURE_INCOMPAT_RECOVER| \
					 EXT4_FEATURE_INCOMPAT_META_BG| \
					 EXT4_FEATURE_INCOMPAT_EXTENTS| \
					 EXT4_FEATURE_INCOMPAT_64BIT| \
					 EXT4_FEATURE_INCOMPAT_FLEX_BG| \
					 EXT4_FEATURE_INCOMPAT_MMP)

#define EXT4_NAME_LEN 255

struct ext4_dir_entry {
    __le32  inode;
    __le16  rec_len;
    __u8    name_len;
    __u8    file_type;
    char    name[EXT4_NAME_LEN];
};

enum {
    EXT4_FT_UNKNOWN,
    EXT4_FT_REG_FILE,
    EXT4_FT_DIR,
    EXT4_FT_CHRDEV,
    EXT4_FT_BLKDEV,
    EXT4_FT_FIFO,
    EXT4_FT_SOCK,
    EXT4_FT_SYMLINK,
    EXT4_FT_MAX
};

struct ext4_extent_tail {
	__le32	et_checksum;
};

struct ext4_extent {
	__le32	ee_block;
	__le16	ee_len;
	__le16	ee_start_hi;
	__le32	ee_start_lo;
};

struct ext4_extent_idx {
	__le32	ei_block;
	__le32	ei_leaf_lo;

	__le16	ei_leaf_hi;
	__u16	ei_unused;
};

struct ext4_extent_header {
	__le16	eh_magic;
	__le16	eh_entries;
	__le16	eh_max;
	__le16	eh_depth;
	__le32	eh_generation;
};

#define EXT4_EXT_MAGIC		(0xf30a)

#endif
