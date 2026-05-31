// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2026 Dayo Akanji
// SPDX-FileCopyrightText: 2010-2021 Free Software Foundation, Inc.

#ifndef _FSW_BTRFS_H_
#define _FSW_BTRFS_H_

#include "fsw_core.h"
#define uint8_t  fsw_u8
#define uint16_t fsw_u16
#define uint32_t fsw_u32
#define uint64_t fsw_u64
#define int64_t  fsw_s64
#define int32_t  fsw_s32

#define fsw_size_t       int
#define fsw_ssize_t      int

#define grub_off_t   int32_t
#define grub_size_t  int32_t
#define grub_ssize_t int32_t
#include "crc32c.c"
#include "gzio.c"
#define MINILZO_CFG_SKIP_LZO_PTR          1
#define MINILZO_CFG_SKIP_LZO_UTIL         1

#define MINILZO_CFG_SKIP_LZO_INIT         1
#define MINILZO_CFG_SKIP_LZO1X_DECOMPRESS 1
#define MINILZO_CFG_SKIP_LZO1X_1_COMPRESS 1

#ifndef LZO_CFG_FREESTANDING
#define LZO_CFG_FREESTANDING 1
#endif
#include "minilzo.c"
#include "scandisk.c"

#define BTRFS_DEFAULT_BLOCK_SIZE 4096
#define GRUB_BTRFS_SIGNATURE "_BHRfS_M"

#define GRUB_BTRFS_LZO_BLOCK_SIZE 4096
#define GRUB_BTRFS_LZO_BLOCK_MAX_CSIZE (GRUB_BTRFS_LZO_BLOCK_SIZE + \
        (GRUB_BTRFS_LZO_BLOCK_SIZE / 16) + 64 + 3)

typedef uint8_t btrfs_checksum_t[0x20];
typedef uint32_t btrfs_uuid_t[4];

struct btrfs_device {
    uint64_t         device_id;
    uint64_t              size;
    uint8_t dummy[0x62 - 0x10];
} __attribute__ ((__packed__));

struct btrfs_superblock {
    btrfs_checksum_t                            checksum;
    btrfs_uuid_t                                    uuid;
    uint8_t                                  dummy[0x10];
    uint8_t signature[sizeof (GRUB_BTRFS_SIGNATURE) - 1];
    uint64_t                                  generation;
    uint64_t                                   root_tree;
    uint64_t                                  chunk_tree;
    uint8_t                                 dummy2[0x10];
    uint64_t                                 total_bytes;
    uint64_t                                  bytes_used;
    uint64_t                           root_dir_objectid;
#define BTRFS_MAX_NUM_DEVICES                     0x10000
    uint64_t                                 num_devices;
    uint32_t                                  sectorsize;
    uint32_t                                    nodesize;
    uint8_t                                 dummy3[0x31];
    struct btrfs_device                      this_device;
    char                                    label[0x100];
    uint8_t                                dummy4[0x100];
    uint8_t                     bootstrap_mapping[0x800];
} __attribute__ ((__packed__));

struct btrfs_header {
    btrfs_checksum_t  checksum;
    btrfs_uuid_t          uuid;
    uint8_t        dummy[0x30];
    uint32_t            nitems;
    uint8_t              level;
} __attribute__ ((__packed__));

struct fsw_btrfs_device_desc {
    struct fsw_volume * dev;
    uint64_t             id;
};

#define RECOVER_CACHE_SIZE 17

struct fsw_btrfs_recover_cache {
    uint64_t device_id;
    uint64_t    offset;
    char       *buffer;
    BOOLEAN      valid;
};

struct fsw_btrfs_volume {
    struct fsw_volume                            g;

    uint8_t               bootstrap_mapping[0x800];
    btrfs_uuid_t                              uuid;
    uint64_t                           total_bytes;
    uint64_t                            bytes_used;
    uint64_t                            chunk_tree;
    uint64_t                             root_tree;
    uint64_t                              top_tree;
    unsigned                           num_devices;
    unsigned                           sectorshift;
    unsigned                            sectorsize;
    int                                  is_master;
    int                                rescan_once;

    struct fsw_btrfs_device_desc *devices_attached;
    unsigned                    n_devices_attached;
    unsigned                   n_devices_allocated;

    uint64_t                              extstart;
    uint64_t                                extend;
    uint64_t                                extino;
    uint64_t                               exttree;
    uint32_t                               extsize;

    struct btrfs_extent_data               *extent;
    struct fsw_btrfs_recover_cache         *rcache;
};

enum {
    GRUB_BTRFS_ITEM_TYPE_INODE_ITEM         = 0x01,
    GRUB_BTRFS_ITEM_TYPE_INODE_REF          = 0x0c,
    GRUB_BTRFS_ITEM_TYPE_DIR_ITEM           = 0x54,
    GRUB_BTRFS_ITEM_TYPE_EXTENT_ITEM        = 0x6c,
    GRUB_BTRFS_ITEM_TYPE_ROOT_ITEM          = 0x84,
    GRUB_BTRFS_ITEM_TYPE_DEVICE             = 0xd8,
    GRUB_BTRFS_ITEM_TYPE_CHUNK              = 0xe4
};

struct btrfs_key {
    uint64_t         object_id;
    uint8_t               type;
    uint64_t            offset;
} __attribute__ ((__packed__));

struct btrfs_chunk_item {
    uint64_t              size;
    uint64_t             dummy;
    uint64_t     stripe_length;
    uint64_t              type;
#define GRUB_BTRFS_CHUNK_TYPE_BITS_DONTCARE 0x07
#define GRUB_BTRFS_CHUNK_TYPE_SINGLE        0x00
#define GRUB_BTRFS_CHUNK_TYPE_RAID0         0x08
#define GRUB_BTRFS_CHUNK_TYPE_RAID1         0x10
#define GRUB_BTRFS_CHUNK_TYPE_DUPLICATED    0x20
#define GRUB_BTRFS_CHUNK_TYPE_RAID10        0x40
#define GRUB_BTRFS_CHUNK_TYPE_RAID5         0x80
#define GRUB_BTRFS_CHUNK_TYPE_RAID6         0x100
#define GRUB_BTRFS_CHUNK_TYPE_RAID1C3       0x200
#define GRUB_BTRFS_CHUNK_TYPE_RAID1C4       0x400
    uint8_t        dummy2[0xc];
    uint16_t          nstripes;
    uint16_t       nsubstripes;
#define RAID5_TAG	   0x100000
} __attribute__ ((__packed__));

struct btrfs_chunk_stripe {
    uint64_t         device_id;
    uint64_t            offset;
    btrfs_uuid_t   device_uuid;
} __attribute__ ((__packed__));

struct btrfs_leaf_node {
    struct btrfs_key       key;
    uint32_t            offset;
    uint32_t              size;
} __attribute__ ((__packed__));

struct btrfs_internal_node {
    struct btrfs_key       key;
    uint64_t              addr;
    uint64_t             dummy;
} __attribute__ ((__packed__));

struct btrfs_dir_item {
    struct btrfs_key       key;
    uint64_t           transid;
    uint16_t                 m;
    uint16_t                 n;
#define GRUB_BTRFS_DIR_ITEM_TYPE_REGULAR   1
#define GRUB_BTRFS_DIR_ITEM_TYPE_DIRECTORY 2
#define GRUB_BTRFS_DIR_ITEM_TYPE_SYMLINK   7
    uint8_t               type;
    char               name[0];
} __attribute__ ((__packed__));

struct fsw_btrfs_leaf_descriptor {
    unsigned                depth;
    unsigned            allocated;
    struct {
        uint64_t             addr;
        unsigned             iter;
        unsigned          maxiter;
        int                  leaf;
    } *data;
};

struct btrfs_root_item {
    uint8_t        dummy[0xb0];
    uint64_t              tree;
    uint64_t             inode;
} __attribute__ ((__packed__));

struct btrfs_time {
    int64_t                sec;
    uint32_t           nanosec;
} __attribute__ ((__packed__));

struct btrfs_inode {
    uint64_t            gen_id;
    uint64_t          trans_id;
    uint64_t              size;
    uint64_t            nbytes;
    uint64_t       block_group;
    uint32_t             nlink;
    uint32_t               uid;
    uint32_t               gid;
    uint32_t              mode;
    uint64_t              rdev;
    uint64_t             flags;
    uint64_t               seq;
    uint64_t       reserved[4];
    struct btrfs_time    atime;
    struct btrfs_time    ctime;
    struct btrfs_time    mtime;
    struct btrfs_time    otime;
} __attribute__ ((__packed__));

struct fsw_btrfs_dnode {
    struct fsw_dnode         g;
    struct btrfs_inode    *raw;
};

struct btrfs_extent_data {
    uint64_t                   dummy;
    uint64_t                    size;
    uint8_t              compression;
    uint8_t               encryption;
    uint16_t                encoding;
    uint8_t                     type;
    union {
        char inl[0];
        struct {
            uint64_t           laddr;
            uint64_t compressed_size;
            uint64_t          offset;
            uint64_t          filled;
        };
    };
} __attribute__ ((__packed__));

struct btrfs_stripe_table {
    struct fsw_volume *dev;
    uint64_t           off;
    char              *ptr;
};

#define GRUB_BTRFS_EXTENT_INLINE     0
#define GRUB_BTRFS_EXTENT_REGULAR    1

#define GRUB_BTRFS_COMPRESSION_NONE  0
#define GRUB_BTRFS_COMPRESSION_ZLIB  1
#define GRUB_BTRFS_COMPRESSION_LZO   2
#define GRUB_BTRFS_COMPRESSION_ZSTD  3
#define GRUB_BTRFS_COMPRESSION_MAX   3

#define GRUB_BTRFS_OBJECT_ID_CHUNK   0x100

#ifndef UINTN_MAX
#define UINTN_MAX  ((UINTN)~0)
#endif

struct fsw_btrfs_uuid_list {
    struct fsw_btrfs_volume  *master;
    struct fsw_btrfs_uuid_list *next;
};

#include "fsw_btrfs_zstd.h"

static uint8_t powx[255 * 2];

static unsigned powx_inv[256];
static const uint8_t poly = 0x1d;

uint64_t superblock_pos[4] = {
    (64                     ) >> 2,
    (64         * 1024      ) >> 2,
    (256        * 1048576   ) >> 2,
    (1048576ULL * 1048576ULL) >> 2
};

struct fsw_btrfs_uuid_list *master_uuid_list = NULL;

void fsw_btrfs_raid6_init_table (void);

int fsw_btrfs_uuid_eq (
    btrfs_uuid_t u1,
    btrfs_uuid_t u2
);
int fsw_btrfs_master_uuid_add (
    struct fsw_btrfs_volume  *vol,
    struct fsw_btrfs_volume **master_out
);
void fsw_btrfs_master_uuid_remove (
    struct fsw_btrfs_volume *vol
);
fsw_status_t fsw_btrfs_set_superblock_info (
    struct fsw_btrfs_volume *vol,
    struct btrfs_superblock *sb
);
fsw_status_t fsw_btrfs_read_superblock (
    struct fsw_volume       *vol,
    struct btrfs_superblock *sb_out
);
int fsw_btrfs_key_cmp (
    const struct btrfs_key *a,
    const struct btrfs_key *b
);
void fsw_btrfs_free_iterator (
    struct fsw_btrfs_leaf_descriptor *desc
);
fsw_status_t fsw_btrfs_save_ref (
    struct fsw_btrfs_leaf_descriptor *desc,
    uint64_t                          addr,
    unsigned                          i,
    unsigned                          m,
    int                               l
);
int fsw_btrfs_next_leaf (
    struct fsw_btrfs_volume          *vol,
    struct fsw_btrfs_leaf_descriptor *desc,
    uint64_t                         *outaddr,
    fsw_size_t                       *outsize,
    struct btrfs_key                 *key_out
);
fsw_status_t fsw_btrfs_lower_bound (
    struct       fsw_btrfs_volume    *vol,
    const struct btrfs_key           *key_in,
    struct       btrfs_key           *key_out,
    uint64_t                          root,
    uint64_t                         *outaddr,
    fsw_size_t                       *outsize,
    struct fsw_btrfs_leaf_descriptor *desc,
    int                               rdepth
);
int fsw_btrfs_add_multi_device (
    struct fsw_btrfs_volume *master,
    struct fsw_volume       *slave,
    struct btrfs_superblock *sb
);
int fsw_btrfs_scan_disks_hook (
    struct fsw_volume *volg,
    struct fsw_volume *slave
);
int fsw_btrfs_do_rescan_once (
    struct fsw_btrfs_volume *vol
);
struct fsw_volume * fsw_btrfs_find_device (
    struct fsw_btrfs_volume *vol,
    uint64_t                 id
);
void fsw_btrfs_block_xor (
    char       *dst,
    const char *src,
    uint32_t    blocksize
);
void fsw_btrfs_stripe_xor (
    char                      *dst,
    struct btrfs_stripe_table *stripe,
    int                        data_stripes,
    uint32_t                   blocksize
);
void fsw_btrfs_stripe_release (
    struct btrfs_stripe_table *stripe,
    int                        count,
    uint32_t                   offset
);
void fsw_btrfs_block_mulx (
    unsigned  mul,
    char     *buf,
    uint32_t  size
);
void fsw_btrfs_block_mulx_xor (
    char       *dst,
    unsigned    mul,
    const char *buf,
    uint32_t    size
);
struct fsw_btrfs_recover_cache * fsw_btrfs_get_recover_cache (
    struct fsw_btrfs_volume *vol,
    uint64_t                 device_id,
    uint64_t                 offset
);
fsw_status_t fsw_btrfs_read_logical (
    struct fsw_btrfs_volume *vol,
    uint64_t                 addr,
    void                    *buf,
    fsw_size_t               size,
    int                      rdepth,
    int                      cache_level
);
fsw_status_t fsw_btrfs_volume_mount (
    struct fsw_volume *volg
);
void fsw_btrfs_volume_free (
    struct fsw_volume *volg
);
fsw_status_t fsw_btrfs_volume_stat (
    struct fsw_volume      *volg,
    struct fsw_volume_stat *sb
);
fsw_status_t fsw_btrfs_read_inode (
    struct fsw_btrfs_volume *vol,
    struct btrfs_inode      *inode,
    uint64_t                 num,
    uint64_t                 tree
);
fsw_status_t fsw_btrfs_dnode_fill (
    struct fsw_volume *volg,
    struct fsw_dnode  *dnog
);
void fsw_btrfs_dnode_free (
    struct fsw_volume *volg,
    struct fsw_dnode  *dnog
);
fsw_status_t fsw_btrfs_dnode_stat (
    struct fsw_volume     *volg,
    struct fsw_dnode      *dnog,
    struct fsw_dnode_stat *sb
);
fsw_ssize_t grub_btrfs_lzo_decompress (
    char       *ibuf,
    fsw_size_t  isize,
    grub_off_t  off,
    char       *obuf,
    fsw_size_t  osize
);
fsw_status_t fsw_btrfs_log_inflate (
    fsw_ssize_t              ret,
    fsw_size_t               csize,
    char                    *buf,
    struct fsw_btrfs_volume *vol,
    fsw_size_t               log_flag
);
fsw_ssize_t fsw_btrfs_decompress (
    uint8_t     comp,
    char       *ibuf,
    fsw_size_t  isize,
	grub_off_t  off,
    char       *obuf,
    fsw_size_t  osize
);
fsw_status_t fsw_btrfs_get_extent (
    struct fsw_volume *volg,
    struct fsw_dnode  *dnog,
    struct fsw_extent *extent
);
fsw_status_t fsw_btrfs_readlink (
    struct fsw_volume *volg,
    struct fsw_dnode  *dnog,
    struct fsw_string *link_target
);
fsw_status_t fsw_btrfs_lookup_dir_item (
    struct fsw_btrfs_volume  *vol,
    uint64_t                  tree_id,
    uint64_t                  object_id,
    struct fsw_string        *lookup_name,
    struct btrfs_dir_item   **direl_buf,
    struct btrfs_dir_item   **direl_out
);
fsw_status_t fsw_btrfs_get_root_tree (
    struct fsw_btrfs_volume *vol,
    struct btrfs_key        *key_in,
    uint64_t                *tree_out
);
fsw_status_t fsw_btrfs_get_sub_dnode (
    struct fsw_btrfs_volume  *vol,
    struct fsw_btrfs_dnode   *dno,
    struct btrfs_dir_item    *cdirel,
    struct fsw_string        *name,
    struct fsw_dnode        **child_dno_out
);
fsw_status_t fsw_btrfs_dir_lookup (
    struct fsw_volume  *volg,
    struct fsw_dnode   *dnog,
    struct fsw_string  *lookup_name,
    struct fsw_dnode  **child_dno_out
);
fsw_status_t fsw_btrfs_get_default_root (
    struct fsw_btrfs_volume *vol,
    uint64_t                 root_dir_objectid
);
fsw_status_t fsw_btrfs_dir_read (
    struct fsw_volume   *volg,
    struct fsw_dnode    *dnog,
    struct fsw_shandle  *shand,
    struct fsw_dnode   **child_dno_out
);

#endif
