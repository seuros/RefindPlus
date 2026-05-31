// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2021-2026 Dayo Akanji
// SPDX-FileCopyrightText: 2021 Joe van Tunen
// SPDX-FileCopyrightText: 2010-2021 Free Software Foundation, Inc.

#include "fsw_btrfs.h"

#define GET_UNALIGNED32(x) (*(uint32_t *) (x))
#define DEPTH_2_CACHE(x) ((x) >= 4 ? 1 : 5-(x))

typedef fsw_ssize_t (*decompressor_t) (
    char       *ibuf,
    fsw_size_t  isize,
    grub_off_t  off,
    char       *obuf,
    fsw_size_t  osize
);

decompressor_t fsw_btrfs_decompressor_func[GRUB_BTRFS_COMPRESSION_MAX] = {
	grub_zlib_decompress,
	grub_btrfs_lzo_decompress,
	zstd_decompress,
};

struct fsw_fstype_table   FSW_FSTYPE_TABLE_NAME(btrfs) = {
    { FSW_STRING_TYPE_UTF08, 5, 5, "btrfs" },
    sizeof (struct fsw_btrfs_volume),
    sizeof (struct fsw_btrfs_dnode),

    fsw_btrfs_volume_mount,
    fsw_btrfs_volume_free,
    fsw_btrfs_volume_stat,
    fsw_btrfs_dnode_fill,
    fsw_btrfs_dnode_free,
    fsw_btrfs_dnode_stat,
    fsw_btrfs_get_extent,
    fsw_btrfs_dir_lookup,
    fsw_btrfs_dir_read,
    fsw_btrfs_readlink,
};

struct fsw_fstype_table *fsw_active_fstype_table = &FSW_FSTYPE_TABLE_NAME(btrfs);
CONST CHAR16            *fsw_active_fstype_name  = L"btrfs";

int fsw_btrfs_uuid_eq (
    btrfs_uuid_t u1,
    btrfs_uuid_t u2
) {
    return u1[0]==u2[0] && u1[1]==u2[1] && u1[2]==u2[2] && u1[3]==u2[3];
}

int fsw_btrfs_master_uuid_add (
    struct fsw_btrfs_volume  *vol,
    struct fsw_btrfs_volume **master_out
) {
    struct fsw_btrfs_uuid_list *l;

    for (l = master_uuid_list; l; l=l->next)
        if (fsw_btrfs_uuid_eq (l->master->uuid, vol->uuid)) {
            if (master_out) *master_out = l->master;
            return 0;
        }

    l = AllocatePool (sizeof (struct fsw_btrfs_uuid_list));
    l->master = vol;
    l->next = master_uuid_list;
    master_uuid_list = l;
    return 1;
}

void fsw_btrfs_master_uuid_remove (
    struct fsw_btrfs_volume *vol
) {
    struct fsw_btrfs_uuid_list **lp;

    for (lp = &master_uuid_list; *lp; lp=&(*lp)->next) {
        if ((*lp)->master == vol) {
            struct fsw_btrfs_uuid_list *n = *lp;
            *lp = n->next;

            FreePool(n);
            break;
        }
    }
}

fsw_status_t fsw_btrfs_set_superblock_info (
    struct fsw_btrfs_volume *vol,
    struct btrfs_superblock *sb
) {
    int              i;
    uint64_t temp_swap;

    vol->uuid[0] = sb->uuid[0];
    vol->uuid[1] = sb->uuid[1];
    vol->uuid[2] = sb->uuid[2];
    vol->uuid[3] = sb->uuid[3];

    vol->root_tree   = sb->root_tree;
    vol->chunk_tree  = sb->chunk_tree;
    vol->bytes_used  = FSW_U64_LE_SWAP(sb->bytes_used);
    vol->total_bytes = FSW_U64_LE_SWAP(sb->total_bytes);

    vol->sectorshift = 0;
    vol->sectorsize = FSW_U32_LE_SWAP(sb->sectorsize);
    for (i = 9; i < 20; i++) {
        if (vol->sectorsize == 1UL << i) {
            vol->sectorshift = i;
            break;
        }
    }

    temp_swap = FSW_U64_LE_SWAP(sb->num_devices);
    vol->num_devices = (
        temp_swap <= BTRFS_MAX_NUM_DEVICES
    ) ? temp_swap :  BTRFS_MAX_NUM_DEVICES;

    FSW_DO_MEMCPY(
        vol->bootstrap_mapping,
        sb->bootstrap_mapping,
        sizeof (vol->bootstrap_mapping)
    );

    return FSW_SUCCESS;
}

fsw_status_t fsw_btrfs_read_superblock (
    struct fsw_volume       *vol,
    struct btrfs_superblock *sb_out
) {
    unsigned i;
    uint64_t total_blocks = 1024;
    fsw_status_t err = FSW_SUCCESS;

    fsw_set_blocksize (
        vol,
        BTRFS_DEFAULT_BLOCK_SIZE,
        BTRFS_DEFAULT_BLOCK_SIZE
    );

    for (i = 0; i < 4; i++) {
        uint8_t *buffer;
        struct btrfs_superblock *sb;

        if (total_blocks <= superblock_pos[i]) break;

        err = fsw_block_get (
            vol, superblock_pos[i], 0,
            (void **) &buffer
        );
        if (err) {
            fsw_block_release (
                vol, superblock_pos[i], buffer
            );

            break;
        }

        sb = (struct btrfs_superblock *)buffer;
        if (!FSW_DO_MEMEQ(
                sb->signature, GRUB_BTRFS_SIGNATURE,
                sizeof (GRUB_BTRFS_SIGNATURE) - 1
            )
        ) {
            fsw_block_release (
                vol, superblock_pos[i], buffer
            );

            break;
        }
        if (i == 0 ||
            (
                FSW_U64_LE_SWAP(sb->generation) >
                FSW_U64_LE_SWAP(sb_out->generation)
            )
        ) {
            FSW_DO_MEMCPY(sb_out, sb, sizeof (*sb));
            total_blocks = FSW_U64_LE_SWAP(
                sb->this_device.size
            ) >> 12;
        }
        fsw_block_release (
            vol, superblock_pos[i], buffer
        );
    }

    if ((err == FSW_UNSUPPORTED || !err) && i == 0) {
        return  FSW_UNSUPPORTED;
    }

    if (err == FSW_UNSUPPORTED) {
        err =  FSW_SUCCESS;
    }

    #if FSW_DEBUG_LEVEL >= 3
    if (err == FSW_SUCCESS) {
        FSW_MSG_L03((
            FSW_MSG_STR(
                "FSW_BTRFS: fsw_btrfs_read_superblock ... UUID: %08x-%08x-%08x-%08x - device id: %d\n"
            ),
            sb_out->uuid[0],
            sb_out->uuid[1],
            sb_out->uuid[2],
            sb_out->uuid[3],
            sb_out->this_device.device_id
        ));
    }
    #endif

    return err;
}

int fsw_btrfs_key_cmp (
    const struct btrfs_key *a,
    const struct btrfs_key *b
) {
    uint64_t a_val, b_val;

    a_val = FSW_U64_LE_SWAP(a->object_id);
    b_val = FSW_U64_LE_SWAP(b->object_id);
    if (a_val < b_val) return -1;
    if (a_val > b_val) return  1;

    if (a->type < b->type) return -1;
    if (a->type > b->type) return  1;

    a_val = FSW_U64_LE_SWAP(a->offset);
    b_val = FSW_U64_LE_SWAP(b->offset);
    if (a_val < b_val) return -1;
    if (a_val > b_val) return  1;

    return 0;
}

void fsw_btrfs_free_iterator (
    struct fsw_btrfs_leaf_descriptor *desc
) {
    FSW_DO_FREE(desc->data);
}

fsw_status_t fsw_btrfs_save_ref (
    struct fsw_btrfs_leaf_descriptor *desc,
    uint64_t                          addr,
    unsigned                          i,
    unsigned                          m,
    int                               l
) {
    desc->depth++;
    if (desc->allocated < desc->depth) {
        void *newdata;
        int oldsize = sizeof (desc->data[0]) * desc->allocated;
        desc->allocated *= 2;

        newdata = AllocatePool (
            sizeof (desc->data[0]) * desc->allocated
        );
        if (!newdata) {
            return FSW_OUT_OF_MEMORY;
        }

        FSW_DO_MEMCPY(
            newdata,
            desc->data,
            oldsize
        );
        FreePool(desc->data);
        desc->data = newdata;
    }

    desc->data[desc->depth - 1].addr = addr;
    desc->data[desc->depth - 1].iter = i;
    desc->data[desc->depth - 1].maxiter = m;
    desc->data[desc->depth - 1].leaf = l;

    return FSW_SUCCESS;
}

int fsw_btrfs_next_leaf (
    struct fsw_btrfs_volume          *vol,
    struct fsw_btrfs_leaf_descriptor *desc,
    uint64_t                         *outaddr,
    fsw_size_t                       *outsize,
    struct btrfs_key                 *key_out
) {
    fsw_status_t err;
    struct btrfs_leaf_node leaf;

    for (; desc->depth > 0; desc->depth--) {
        desc->data[desc->depth - 1].iter++;
        if (desc->data[desc->depth - 1].iter <
            desc->data[desc->depth - 1].maxiter
        ) {
            break;
        }
    }
    if (desc->depth == 0) {
        return 0;
    }

    while (!desc->data[desc->depth - 1].leaf) {
        struct btrfs_internal_node node;
        struct btrfs_header        head;
        FSW_DO_MEMZERO(&node, sizeof (node));

        err = fsw_btrfs_read_logical (
            vol, desc->data[desc->depth - 1].iter *
            sizeof (node)                         +
            sizeof (struct btrfs_header)          +
            desc->data[desc->depth - 1].addr,
            &node, sizeof (node), 0, 1
        );
        if (err) return -err;

        err = fsw_btrfs_read_logical (
            vol, FSW_U64_LE_SWAP(node.addr),
            &head, sizeof (head), 0, 1
        );
        if (err) return -err;

        fsw_btrfs_save_ref (
            desc, FSW_U64_LE_SWAP(node.addr), 0,
            FSW_U32_LE_SWAP(head.nitems), !head.level
        );
    }

    err = fsw_btrfs_read_logical (
        vol, desc->data[desc->depth - 1].iter *
        sizeof (leaf)                         +
        sizeof (struct btrfs_header)          +
        desc->data[desc->depth - 1].addr, &leaf,
        sizeof (leaf), 0, 1
    );
    if (err) return -err;

    *key_out = leaf.key;
    *outsize = FSW_U32_LE_SWAP(leaf.size);
    *outaddr = (
        desc->data[desc->depth - 1].addr +
        sizeof (struct btrfs_header)     +
        FSW_U32_LE_SWAP(leaf.offset)
    );

    return 1;
}

fsw_status_t fsw_btrfs_lower_bound (
    struct       fsw_btrfs_volume    *vol,
    const struct btrfs_key           *key_in,
    struct       btrfs_key           *key_out,
    uint64_t                          root,
    uint64_t                         *outaddr,
    fsw_size_t                       *outsize,
    struct fsw_btrfs_leaf_descriptor *desc,
    int rdepth
) {
    uint64_t addr = FSW_U64_LE_SWAP(root);

    if (desc) {
        desc->allocated = 16;
        desc->depth = 0;
        desc->data = AllocatePool (
            sizeof (desc->data[0]) * desc->allocated
        );
        if (!desc->data) {
            return FSW_OUT_OF_MEMORY;
        }
    }

    if (rdepth > 10) {
        return FSW_VOLUME_CORRUPTED;
    }

    FSW_MSG_L03((
        FSW_MSG_STR(
            "FSW_BTRFS: fsw_btrfs_lower_bound ... retrieving %llx %x %llx\n"
        ),
        (unsigned long long) key_in->object_id,
        (unsigned) key_in->type,
        (unsigned long long) key_in->offset
    ));

    while (1) {
        fsw_status_t err;
        struct btrfs_header head;
        FSW_DO_MEMZERO(&head, sizeof (head));

reiter:

        err = fsw_btrfs_read_logical (
            vol, addr, &head, sizeof (head),
            rdepth + 1, DEPTH_2_CACHE(rdepth)
        );
        if (err) return err;

        addr += sizeof (head);
        if (head.level) {
            unsigned i;
            struct btrfs_internal_node node, node_last;
            int have_last = 0;
            FSW_DO_MEMZERO(&node_last, sizeof (node_last));
            for (i = 0; i < FSW_U32_LE_SWAP(head.nitems); i++) {
                err = fsw_btrfs_read_logical (
                    vol, addr + i * sizeof (node),
                    &node, sizeof (node), rdepth + 1,
                    DEPTH_2_CACHE(rdepth)
                );
                if (err) return err;

                FSW_MSG_L03((
                    FSW_MSG_STR(
                        "FSW_BTRFS: fsw_btrfs_lower_bound ... internal node %llx %x %llx\n"
                    ),
                    (unsigned long long) node.key.object_id,
                    (unsigned) node.key.type,
                    (unsigned long long) node.key.offset
                ));

                if (fsw_btrfs_key_cmp (&node.key, key_in) == 0) {
                    err = FSW_SUCCESS;
                    if (desc) {
                        err = fsw_btrfs_save_ref (
                            desc, addr - sizeof (head), i,
                            FSW_U32_LE_SWAP(head.nitems), 0
                        );
                        if (err) return err;
                    }
                    addr = FSW_U64_LE_SWAP(node.addr);
                    goto reiter;
                }
                if (fsw_btrfs_key_cmp (&node.key, key_in) > 0) break;

                FSW_DO_MEMCPY(
                    &node_last, &node,
                    sizeof (node_last)
                );
                have_last = 1;
            }
            if (have_last) {
                err = FSW_SUCCESS;
                if (desc) {
                    err = fsw_btrfs_save_ref (
                        desc, addr - sizeof (head), i - 1,
                        FSW_U32_LE_SWAP(head.nitems), 0
                    );
                    if (err) return err;
                }
                addr = FSW_U64_LE_SWAP(node_last.addr);
                goto reiter;
            }
            *outsize = 0;
            *outaddr = 0;
            FSW_DO_MEMZERO(key_out, sizeof (*key_out));
            if (desc) {
                return fsw_btrfs_save_ref (
                    desc, addr - sizeof (head), -1,
                    FSW_U32_LE_SWAP(head.nitems), 0
                );
            }
            return FSW_SUCCESS;
        }

        {
            unsigned i;
            struct btrfs_leaf_node leaf, leaf_last;
            int have_last    = 0;
            leaf_last.size   = 0;
            leaf_last.offset = 0;
            for (i = 0; i < FSW_U32_LE_SWAP(head.nitems); i++) {
                err = fsw_btrfs_read_logical (
                    vol, addr + i * sizeof (leaf),
                    &leaf, sizeof (leaf), rdepth + 1,
                    DEPTH_2_CACHE(rdepth)
                );
                if (err) {
                    FSW_MSG_L03((
                        FSW_MSG_STR(
                            "FSW_BTRFS: fsw_btrfs_lower_bound ... Failed to Read Logical Nodes\n"
                        )
                    ));

                    return err;
                }

                FSW_MSG_L03((
                    FSW_MSG_STR(
                        "FSW_BTRFS: fsw_btrfs_lower_bound ... Leaf %llx %x %llx\n"
                    ),
                    (unsigned long long) leaf.key.object_id,
                    (unsigned) leaf.key.type,
                    (unsigned long long) leaf.key.offset
                ));

                if (fsw_btrfs_key_cmp (&leaf.key, key_in) == 0) {
                    FSW_DO_MEMCPY(
                        key_out,
                        &leaf.key,
                        sizeof (*key_out)
                    );
                    *outsize = FSW_U32_LE_SWAP(leaf.size);
                    *outaddr = addr + FSW_U32_LE_SWAP(leaf.offset);
                    if (desc) {
                        return fsw_btrfs_save_ref (
                            desc, addr - sizeof (head), i,
                            FSW_U32_LE_SWAP(head.nitems), 1
                        );
                    }

                    return FSW_SUCCESS;
                }

                if (fsw_btrfs_key_cmp (&leaf.key, key_in) > 0) break;

                have_last = 1;
                leaf_last = leaf;
            }

            if (have_last) {
                FSW_DO_MEMCPY(
                    key_out,
                    &leaf_last.key,
                    sizeof (*key_out)
                );
                *outsize =        FSW_U32_LE_SWAP(leaf_last.size);
                *outaddr = addr + FSW_U32_LE_SWAP(leaf_last.offset);
                if (desc) {
                    return fsw_btrfs_save_ref (
                        desc, addr - sizeof (head), i - 1,
                        FSW_U32_LE_SWAP(head.nitems), 1
                    );
                }

                return FSW_SUCCESS;
            }
            *outsize = 0;
            *outaddr = 0;
            FSW_DO_MEMZERO(
                key_out,
                sizeof (*key_out)
            );

            if (desc) {
                return fsw_btrfs_save_ref (
                    desc, addr - sizeof (head), -1,
                    FSW_U32_LE_SWAP(head.nitems), 1
                );
            }

            return FSW_SUCCESS;
        }
    }
}

int fsw_btrfs_add_multi_device (
    struct fsw_btrfs_volume *master,
    struct fsw_volume       *slave,
    struct btrfs_superblock *sb
) {
    int i;

    for (i = 0; i < master->n_devices_attached; i++) {
        if (sb->this_device.device_id == master->devices_attached[i].id) {
            FSW_MSG_L03((
                FSW_MSG_STR(
                    "FSW_BTRFS: fsw_btrfs_add_multi_device ... Returned FSW_UNSUPPORTED on Device: %d\n"
                ), sb->this_device.device_id
            ));

            return FSW_UNSUPPORTED;
        }
    }

    slave = dsk_btrfs_clone_dummy_volume (slave);
    if (slave == NULL) {
        FSW_MSG_L03((
            FSW_MSG_STR(
                "FSW_BTRFS: fsw_btrfs_add_multi_device ... Returned FSW_OUT_OF_MEMORY\n"
            )
        ));

        return FSW_OUT_OF_MEMORY;
    }

    fsw_set_blocksize (
        slave,
        master->sectorsize,
        master->sectorsize
    );

    master->devices_attached[i].id = sb->this_device.device_id;
    master->devices_attached[i].dev = slave;
    master->n_devices_attached++;

    FSW_MSG_L03((
        FSW_MSG_STR(
            "FSW_BTRFS: fsw_btrfs_add_multi_device ... Found Device: %d\n"
        ), sb->this_device.device_id
    ));

    return FSW_SUCCESS;
}

int fsw_btrfs_scan_disks_hook (
    struct fsw_volume *volg,
    struct fsw_volume *slave
) {
    struct fsw_btrfs_volume *vol = (struct fsw_btrfs_volume *) volg;
    struct btrfs_superblock sb;
    fsw_status_t err;
    btrfs_uuid_t u;

    if (vol->n_devices_attached >= vol->n_devices_allocated) {
        return FSW_UNSUPPORTED;
    }

    err = fsw_btrfs_read_superblock (
        slave, &sb
    );
    if (err) {
        FSW_MSG_L03((
            FSW_MSG_STR(
                "FSW_BTRFS: fsw_btrfs_scan_disks_hook ... Could Not Read Superblock\n"
            )
        ));

        return FSW_UNSUPPORTED;
    }

    u[0] = sb.uuid[0];
    u[1] = sb.uuid[1];
    u[2] = sb.uuid[2];
    u[3] = sb.uuid[3];

    if (!fsw_btrfs_uuid_eq (vol->uuid, u)) {
        return FSW_UNSUPPORTED;
    }

    return fsw_btrfs_add_multi_device (
        vol, slave, &sb
    );
}

int fsw_btrfs_do_rescan_once (
    struct fsw_btrfs_volume *vol
) {
    if (vol->rescan_once == 0 || vol->n_devices_attached >= vol->n_devices_allocated) {
        return 0;
    }
    vol->rescan_once = 0;

    return dsk_btrfs_scan_disks (
        fsw_btrfs_scan_disks_hook, &vol->g
    );
}

struct fsw_volume * fsw_btrfs_find_device (
    struct fsw_btrfs_volume *vol,
    uint64_t                 id
) {
    int i;

    for (i = 0; i < vol->n_devices_attached; i++) {
        if (id == vol->devices_attached[i].id) {
            return vol->devices_attached[i].dev;
        }
    }

    FSW_MSG_L03((
        FSW_MSG_STR(
            "FSW_BTRFS: fsw_btrfs_find_device ... Device '%d' Not Found\n"
        ), id
    ));

    return NULL;
}

void fsw_btrfs_block_xor (
    char       *dst,
    const char *src,
    uint32_t    blocksize
) {
    UINTN *d = (UINTN *)dst;
    const UINTN *s = (const UINTN *)src;
    blocksize /= sizeof (UINTN);
    uint32_t i;

    for ( i = 0; i < blocksize; i++) d[i] ^= s[i];
}

void fsw_btrfs_stripe_xor (
    char                *dst,
    struct btrfs_stripe_table *stripe,
    int                  data_stripes,
    uint32_t             blocksize
) {
    UINTN c;
    unsigned i, j;

    for (j = 0; j < blocksize; j += sizeof (UINTN)) {

        for (c=0, i=0; i <= data_stripes; i++) {
            if (stripe[i].ptr) c ^= *(UINTN *) (stripe[i].ptr + j);
        }
        *(UINTN *) (dst + j) = c;
    }
}

void fsw_btrfs_stripe_release (
    struct btrfs_stripe_table *stripe,
    int                  count,
    uint32_t             offset
) {
    unsigned i;

    for (i = 0; i < count; i++) {
        if (stripe[i].ptr) {
            fsw_block_release (
                stripe[i].dev,
                stripe[i].off + offset,
                (void *)stripe[i].ptr
            );
        }
    }
}

void fsw_btrfs_block_mulx (
    unsigned  mul,
    char     *buf,
    uint32_t  size
) {
    uint32_t i;
    uint8_t *p = (uint8_t *) buf;

    for (i = 0; i < size; i++, p++) {
        if (*p) *p = powx[mul + powx_inv[*p]];
    }
}

void fsw_btrfs_block_mulx_xor (
    char       *dst,
    unsigned    mul,
    const char *buf,
    uint32_t    size
) {
    uint32_t i;
    const uint8_t *p = (const uint8_t *) buf;
    uint8_t *q = (uint8_t *) dst;

    for (i = 0; i < size; i++, p++, q++) {
        if (*p) *q ^= powx[mul + powx_inv[*p]];
    }
}

void fsw_btrfs_raid6_init_table (void) {
    static int initialized = 0;
    unsigned i;

    if (initialized) {
        return;
    }

    uint8_t cur = 1;
    for (i = 0; i < 255; i++) {
        powx[i] = cur;
        powx[i + 255] = cur;
        powx_inv[cur] = i;
        if (cur & 0x80) {
            cur = (cur << 1) ^ poly;
        }
        else {
            cur <<= 1;
        }
    }
    initialized = 1;
}

struct fsw_btrfs_recover_cache * fsw_btrfs_get_recover_cache (
    struct fsw_btrfs_volume *vol,
    uint64_t device_id,
    uint64_t offset
) {
    UINT32 hash;

    if (vol->rcache == NULL) {
        if (fsw_alloc_zero (
                sizeof (struct fsw_btrfs_recover_cache) * RECOVER_CACHE_SIZE,
                (void **) &vol->rcache
            ) != FSW_SUCCESS
        ) {
            return NULL;
        }
    }

    DivU64x32Remainder (
        ((device_id >> 32) | device_id | (offset >> 32) | offset),
        RECOVER_CACHE_SIZE, &hash
    );

    struct fsw_btrfs_recover_cache *rc = &vol->rcache[hash];
    if (rc->buffer == NULL) {
        if (fsw_alloc_zero (
                vol->sectorsize,
                (void **) &rc->buffer
            ) != FSW_SUCCESS
        ) {
            return NULL;
        }
    }
    if (rc->device_id != device_id || rc->offset != offset) {
        rc->valid = FALSE;
        rc->device_id = device_id;
        rc->offset = offset;
    }

    return rc;
}

fsw_status_t fsw_btrfs_read_logical (
    struct fsw_btrfs_volume *vol,
    uint64_t                 addr,
    void                    *buf,
    fsw_size_t               size,
    int                      rdepth,
    int                      cache_level
) {
    struct btrfs_chunk_item   *chunk       = NULL;
    struct btrfs_stripe_table *stripe_data = NULL;
    int challoc = 0;
    fsw_status_t err;
    uint64_t raw_off = 0;

    while (size > 0) {
        uint8_t          *ptr;
        struct btrfs_key *key;
        struct btrfs_key  key_out;
        struct btrfs_key  key_in;
        fsw_size_t        chsize;
        uint64_t          chaddr;
        uint64_t          csize;

        err = 0;
        for (
            ptr = vol->bootstrap_mapping;
            ptr < vol->bootstrap_mapping + sizeof (vol->bootstrap_mapping) - sizeof (struct btrfs_key);
        ) {
            key = (struct btrfs_key *) ptr;
            if (key->type != GRUB_BTRFS_ITEM_TYPE_CHUNK) break;

            chunk = (struct btrfs_chunk_item *) (key + 1);

            raw_off = FSW_U64_LE_SWAP(key->offset);
            if (addr >= raw_off &&
                addr <  raw_off + FSW_U64_LE_SWAP(chunk->size)
            ) {
                goto chunk_found;
            }

            ptr += sizeof (*key) + sizeof (*chunk)
            + sizeof (struct btrfs_chunk_stripe)
            * FSW_U16_LE_SWAP(chunk->nstripes);
        }

        key_in.object_id = FSW_U64_LE_SWAP(
            GRUB_BTRFS_OBJECT_ID_CHUNK
        );
        key_in.type = GRUB_BTRFS_ITEM_TYPE_CHUNK;
        key_in.offset = FSW_U64_LE_SWAP(addr);

        err = fsw_btrfs_lower_bound (
            vol, &key_in, &key_out, vol->chunk_tree,
            &chaddr, &chsize, NULL, rdepth
        );
        if (err) return err;

        key = &key_out;
        raw_off = FSW_U64_LE_SWAP(key->offset);
        if (raw_off > addr ||
            key->type != GRUB_BTRFS_ITEM_TYPE_CHUNK
        ) {
            return FSW_VOLUME_CORRUPTED;
        }

        chunk = AllocatePool (chsize);
        if (!chunk) return FSW_OUT_OF_MEMORY;

        challoc = 1;
        err = fsw_btrfs_read_logical (
            vol, chaddr, chunk, chsize, rdepth,
            cache_level < 5 ? cache_level+1 : 5
        );
        if (err) goto io_error;

chunk_found:
        {
            #undef DivU64x32
            #define DivU64x32 DivU64x32Remainder
            #define UINTREM UINT32

            uint64_t middle, high;
            uint64_t off = addr - raw_off;
            uint64_t chunk_len     = FSW_U64_LE_SWAP(chunk->size);
            uint64_t chunk_type    = FSW_U64_LE_SWAP(chunk->type);
            uint16_t nstripes      = FSW_U16_LE_SWAP(chunk->nstripes);
            uint64_t nsubstripes   = FSW_U16_LE_SWAP(chunk->nsubstripes);
            uint64_t stripe_length = FSW_U64_LE_SWAP(chunk->stripe_length);

            UINTREM low;
            UINTREM stripen;
            UINTREM stripeq = 0;
            UINTREM stripe_offset;

            unsigned redundancy = 1;
            unsigned i;

            if (chunk_len <= off) {
                goto volume_corrupted;
            }

            uint64_t type_check = chunk_type & ~GRUB_BTRFS_CHUNK_TYPE_BITS_DONTCARE;
            FSW_MSG_L03((
                FSW_MSG_STR(
                    "FSW_BTRFS: fsw_btrfs_read_logical ... btrfs chunk 0x%llx+0x%llx %d stripes (%d substripes) of %llx\n"
                ),
                (unsigned long long) raw_off,
                (unsigned long long) chunk_len,
                (unsigned) nstripes,
                (unsigned) nsubstripes,
                (unsigned long long) stripe_length
            ));

            switch (type_check) {
                case GRUB_BTRFS_CHUNK_TYPE_SINGLE: {
                    stripe_length = DivU64x32(chunk_len, nstripes, NULL);
                    if (stripe_length >= 1ULL<<32) return FSW_VOLUME_CORRUPTED;

                    stripen = DivU64x32(off, (uint32_t)stripe_length, &stripe_offset);
                    csize = (stripen + 1) * stripe_length - off;

                    FSW_MSG_L03((
                        FSW_MSG_STR(
                            "FSW_BTRFS: fsw_btrfs_read_logical ... Line#: %d chunk_found single csize==%d\n"
                        ), __LINE__, csize
                    ));

                    break;
                }

                case GRUB_BTRFS_CHUNK_TYPE_RAID1C4:    redundancy += 1; // Fall through
                case GRUB_BTRFS_CHUNK_TYPE_RAID1C3:    redundancy += 1; // Fall through
                case GRUB_BTRFS_CHUNK_TYPE_DUPLICATED:
                case GRUB_BTRFS_CHUNK_TYPE_RAID1: {
                    stripen = 0;
                    stripe_offset = off;
                    csize = chunk_len - off;
                    redundancy += 1;

                    FSW_MSG_L03((
                        FSW_MSG_STR(
                            "FSW_BTRFS: fsw_btrfs_read_logical ... Line#: %d chunk_found dup/raid1 off==%llx csize==%d redundancy==%d\n"
                        ), __LINE__, (unsigned long long) stripe_offset, csize, redundancy
                    ));

                    break;
                }

                case GRUB_BTRFS_CHUNK_TYPE_RAID0: {
                    if (stripe_length > 1UL<<30) return FSW_VOLUME_CORRUPTED;

                    middle = DivU64x32(off, (uint32_t)stripe_length, &low);
                    high   = DivU64x32(middle, nstripes, &stripen);

                    stripe_offset = low + stripe_length * high;
                    csize         = stripe_length - low;

                    FSW_MSG_L03((
                        FSW_MSG_STR(
                            "FSW_BTRFS: fsw_btrfs_read_logical ... Line#: %d chunk_found raid0 csize==%d\n"
                        ), __LINE__, csize
                    ));

                    break;
                }

                case GRUB_BTRFS_CHUNK_TYPE_RAID10: {
                    if (stripe_length > 1UL<<30) {
                        return FSW_VOLUME_CORRUPTED;
                    }

                    middle = DivU64x32(off, stripe_length, &low);
                    high   = DivU64x32(
                        middle,
                        nstripes / nsubstripes,
                        &stripen
                    );

                    stripen    *= nsubstripes;
                    redundancy  = nsubstripes;

                    stripe_offset = low + stripe_length * high;
                    csize         =       stripe_length - low;

                    FSW_MSG_L03((
                        FSW_MSG_STR(
                            "FSW_BTRFS: fsw_btrfs_read_logical ... Line#: %d chunk_found raid01 csize==%d\n"
                        ), __LINE__, csize
                    ));

                    break;
                }

                case GRUB_BTRFS_CHUNK_TYPE_RAID5:
                case GRUB_BTRFS_CHUNK_TYPE_RAID6: {
                    uint16_t nparities = (
                        chunk_type & GRUB_BTRFS_CHUNK_TYPE_RAID6
                    ) ? 2 : 1;

                    if (stripe_length > 1UL<<30 || nstripes > 255) {
                        goto volume_corrupted;
                    }

                    middle = DivU64x32(off, stripe_length, &low);
                    high   = DivU64x32(middle, nstripes - nparities, &stripen);
                    middle = DivU64x32(high + stripen, nstripes, &stripen);
                    if (nparities == 1) {
                        stripeq = RAID5_TAG;
                    }
                    else {
                        middle = DivU64x32(high + nstripes -1, nstripes, &stripeq);
                    }
                    redundancy = RAID5_TAG;
                    stripe_offset = low + stripe_length * high;
                    csize = stripe_length - low;

                    FSW_MSG_L03((
                        FSW_MSG_STR(
                            "FSW_BTRFS: fsw_btrfs_read_logical ... Line#: %d chunk_found raid01 csize==%d\n"
                        ), __LINE__, csize
                    ));

                    break;
                }

                default: {
                    FSW_MSG_L03((
                        FSW_MSG_STR(
                            "FSW_BTRFS: fsw_btrfs_read_logical ... Unsupported RAID\n"
                        )
                    ));

                    err = FSW_UNSUPPORTED;
                    goto io_error;
                }
            }

            if (csize == 0) goto volume_corrupted;
            if (csize > (uint64_t) size) csize = size;

            if (redundancy < RAID5_TAG) {
begin_direct_read:
                err = 0;
                for (i = 0; !err && i < redundancy; i++) {
                    struct btrfs_chunk_stripe *stripe;
                    uint64_t paddr;
                    struct fsw_volume *dev;

                    stripe = (struct btrfs_chunk_stripe *) (chunk + 1);

                    stripe += stripen + i;

                    paddr = FSW_U64_LE_SWAP(stripe->offset) + stripe_offset;

                    FSW_MSG_L03((
                        FSW_MSG_STR(
                            "FSW_BTRFS: fsw_btrfs_read_logical ... chunk 0x%llx+0x%llx (%d stripes (%d substripes) of %llx) stripe %llx maps to 0x%llx\n"
                        ),
                        (unsigned long long) raw_off,
                        (unsigned long long) chunk_len,
                        (unsigned) nstripes,
                        (unsigned) nsubstripes,
                        (unsigned long long) FSW_U64_LE_SWAP(chunk->stripe_length),
                        (unsigned long long) stripen,
                        (unsigned long long) stripe->offset
                    ));

                    FSW_MSG_L03((
                        FSW_MSG_STR(
                            "FSW_BTRFS: fsw_btrfs_read_logical ... reading paddr 0x%llx for laddr 0x%llx\n"
                        ), (unsigned long long) paddr, (unsigned long long) addr
                    ));

                    dev = fsw_btrfs_find_device (vol, stripe->device_id);
                    if (!dev) continue;

                    uint32_t off = paddr & (vol->sectorsize - 1);
                    paddr >>= vol->sectorshift;
                    uint64_t n = 0;
                    while (n < csize) {
                        char *buffer;
                        err = fsw_block_get (dev, paddr, cache_level, (void **) &buffer);
                        if (err) break;

                        int s = vol->sectorsize - off;
                        if (s > csize - n) s = csize - n;

                        FSW_DO_MEMCPY(
                            (char *)buf + n,
                            buffer + off, s
                        );
                        fsw_block_release (
                            dev, paddr,
                            (void *)buffer
                        );

                        n += s;
                        off = 0;
                        paddr++;
                    }

                    FSW_MSG_L03((
                        FSW_MSG_STR(
                            "FSW_BTRFS: fsw_btrfs_read_logical ... err %d, csize %d, got %d\n"
                        ), err, csize, n
                    ));
                    if (n>=csize) break;
                }
                if (i == redundancy) {
                    if (fsw_btrfs_do_rescan_once (vol) > 0) goto begin_direct_read;
                    if (err == 0) goto volume_corrupted;
                }
                if (err) goto io_error;

            }
            else {

                struct btrfs_chunk_stripe *stripe = (struct btrfs_chunk_stripe *) (chunk + 1);
                unsigned sectorsize = vol->sectorsize;

                {
                    uint64_t sectormask = FSW_U64_LE_SWAP(sectorsize - 1);
                    for (i = 0; i < nstripes; i++) {
                        if (stripe[i].offset & sectormask) goto volume_corrupted;
                    }
                }

                struct fsw_volume *dev = fsw_btrfs_find_device (
                    vol, stripe[stripen].device_id
                );
                if (dev == NULL && fsw_btrfs_do_rescan_once (vol) > 0) {
                    dev = fsw_btrfs_find_device (
                        vol, stripe[stripen].device_id
                    );
                }

                uint32_t posN = stripen;
                BOOLEAN is_raid5 = (stripeq == RAID5_TAG);
                uint32_t dstripes = nstripes - (is_raid5 ? 1 : 2);

                uint64_t n = 0;
                uint32_t off = stripe_offset & (sectorsize - 1);
                stripe_offset >>= vol->sectorshift;
                while (n < csize) {
                    int used_bytes = sectorsize - off;
                    if (used_bytes > csize - n) used_bytes = csize - n;
                    char *buffer;
                    struct fsw_btrfs_recover_cache *rcache = NULL;
                    uint64_t paddrN = (
                        FSW_U64_LE_SWAP(stripe[stripen].offset) >> vol->sectorshift
                    ) + stripe_offset;

                    err = (dev) ? fsw_block_get (
                        dev, paddrN, cache_level, (void **) &buffer
                    ) : 0;
                    if (dev && !err) {

                        FSW_DO_MEMCPY(
                            (char *)buf + n,
                            buffer + off,
                            used_bytes
                        );

                        fsw_block_release (
                            dev, paddrN,
                            (void *)buffer
                        );

                    }
                    else if (
                        (rcache = fsw_btrfs_get_recover_cache (
                                vol, stripe[stripen].device_id, paddrN
                        )) == NULL
                    ) {
                        err = FSW_OUT_OF_MEMORY;
                        goto io_error;
                    }
                    else if (rcache->valid) {

                        FSW_DO_MEMCPY(
                            (char *)buf + n,
                            rcache->buffer + off,
                            used_bytes
                        );
                    }
                    else {

                        if (!stripe_data) {

                            err = fsw_alloc_zero (
                                sizeof (struct btrfs_stripe_table) * nstripes,
                                (void **) &stripe_data
                            );
                            if (err) goto io_error;

                            unsigned dev_count = 0;
                            uint32_t stripeI = is_raid5 ? 0 : (stripeq + 1) % nstripes;
                            for (i = 0; i < nstripes; i++) {
                                if (stripeI == stripen) {
                                    stripe_data[i].dev = NULL;
                                    posN = i;
                                }
                                else {
                                    stripe_data[i].off = FSW_U64_LE_SWAP(
                                        stripe[stripeI].offset
                                    ) >> vol->sectorshift;
                                    stripe_data[i].dev = fsw_btrfs_find_device (
                                        vol, stripe[stripeI].device_id
                                    );

                                    if (stripe_data[i].dev == NULL &&
                                        fsw_btrfs_do_rescan_once (vol) > 0
                                    ) {
                                        stripe_data[i].dev = fsw_btrfs_find_device (
                                            vol, stripe[stripeI].device_id
                                        );
                                    }
                                    if (stripe_data[i].dev) dev_count ++;
                                }
                                stripeI = stripeI == nstripes -1 ? 0 : stripeI + 1;
                            }

                            if (dev_count < dstripes) {

                                goto volume_corrupted;
                            }
                        }

                        uint32_t bad2 = RAID5_TAG;
                        for (i = 0; i < nstripes; i++) {
                            stripe_data[i].ptr = NULL;
                            if (i == posN) continue;

                            err = (stripe_data[i].dev == NULL)
                            ? FSW_IO_ERROR
                            : fsw_block_get (
                                stripe_data[i].dev,
                                stripe_data[i].off + stripe_offset,
                                cache_level,
                                (void **) &(stripe_data[i].ptr)
                            );
                            if (!err) {
                                if (bad2 == RAID5_TAG && dstripes == i) break;
                            }
                            else {
                                if (bad2 != RAID5_TAG || is_raid5) break;

                                bad2 = i;
                                err  = 0;
                            }
                        }

                        char *pbuf;
                        if (err) {
                            fsw_btrfs_stripe_release (
                                stripe_data, i,
                                stripe_offset
                            );
                        }
                        else if (bad2 == RAID5_TAG) {

                            fsw_btrfs_stripe_xor (
                                rcache->buffer,
                                stripe_data, i,
                                sectorsize
                            );
                            fsw_btrfs_stripe_release (
                                stripe_data, i+1,
                                stripe_offset
                            );
                        }
                        else {
                            fsw_btrfs_raid6_init_table();

                            FSW_DO_MEMZERO(rcache->buffer, sectorsize);
                            for ( i = 0; i < nstripes - 2; i++) {
                                if (stripe_data[i].ptr) {
                                    fsw_btrfs_block_mulx_xor (
                                        rcache->buffer, i,
                                        stripe_data[i].ptr,
                                        sectorsize
                                    );
                                }
                            }

                            fsw_btrfs_block_xor (
                                rcache->buffer,
                                stripe_data[nstripes - 1].ptr,
                                sectorsize
                            );

                            if (bad2 == nstripes - 2) {

                                fsw_btrfs_block_mulx (
                                    255 - posN,
                                    rcache->buffer,
                                    sectorsize
                                );
                            }
                            else if (
                                (
                                    err = FSW_DO_ALLOC(
                                        sectorsize,
                                        (void **) &pbuf
                                    )
                                ) == FSW_SUCCESS
                            ) {

                                unsigned int c = (
                                    (255 ^ posN) +
                                    (255 ^ powx_inv[
                                        (powx[bad2 + (posN ^ 255)
                                    ] ^ 1)])
                                ) % 255;
                                fsw_btrfs_block_mulx (
                                    c, rcache->buffer,
                                    sectorsize
                                );
                                fsw_btrfs_stripe_xor (
                                    pbuf, stripe_data,
                                    dstripes, sectorsize
                                );
                                fsw_btrfs_block_mulx_xor (
                                    rcache->buffer,
                                    (bad2 + c) % 255,
                                    pbuf, sectorsize
                                );
                                FSW_DO_FREE(pbuf);
                            }
                            fsw_btrfs_stripe_release (
                                stripe_data,
                                nstripes,
                                stripe_offset
                            );
                        }

                        if (err) goto io_error;

                        FSW_DO_MEMCPY(
                            (char *)buf + n,
                            rcache->buffer + off,
                            used_bytes
                        );
                        rcache->valid = TRUE;
                    }

                    off = 0;
                    n += used_bytes;
                    stripe_offset++;

                    FSW_MSG_L03((
                        FSW_MSG_STR(
                            "FSW_BTRFS: fsw_btrfs_read_logical ... err %d, csize %d, got %d\n"
                        ), err, csize, n
                    ));
                }
            }
        }

        size -= csize;
        buf   = (uint8_t *)buf + csize;
        addr += csize;

        if (challoc) {
            FreePool(chunk);
            chunk = NULL;
        }
        challoc = 0;

        if (stripe_data) FSW_DO_FREE(stripe_data);
        stripe_data = NULL;
    }

    return FSW_SUCCESS;

volume_corrupted:
    FSW_MSG_L03((
        FSW_MSG_STR(
            "FSW_BTRFS: fsw_btrfs_read_logical ... Volume Corrupted\n"
        )
    ));
    err = FSW_VOLUME_CORRUPTED;

io_error:
    FSW_MSG_L03((
        FSW_MSG_STR(
            "FSW_BTRFS: fsw_btrfs_read_logical ... Return on Error\n"
        )
    ));

    if (stripe_data) FreePool(stripe_data);
    return err;
}

fsw_status_t fsw_btrfs_volume_mount (
    struct fsw_volume *volg
) {
    struct btrfs_superblock sblock;
    struct fsw_btrfs_volume *vol = (struct fsw_btrfs_volume *)volg;
    struct fsw_btrfs_volume *master_out = NULL;
    struct fsw_string s;
    fsw_status_t err;
    int i;

    init_crc32c_table();

    err = fsw_btrfs_read_superblock (volg, &sblock);
    if (err) return err;

    fsw_btrfs_set_superblock_info (vol, &sblock);

    if (vol->sectorshift == 0) {
        return FSW_UNSUPPORTED;
    }

    if (vol->num_devices >= BTRFS_MAX_NUM_DEVICES) {
        return FSW_UNSUPPORTED;
    }

    vol->is_master = fsw_btrfs_master_uuid_add (
        vol, &master_out
    );
    if (vol->is_master == 0) {

#define FAKE_LABEL "btrfs.multi.device"
        s.type = FSW_STRING_TYPE_UTF08;
        s.size = s.len = sizeof (FAKE_LABEL) - 1;
        s.data = FAKE_LABEL;
        err = fsw_strdup_coerce (
            &volg->label,
            volg->host_string_type, &s
        );
        if (err) return err;

        fsw_btrfs_add_multi_device (
            master_out,
            volg, &sblock
        );

        return fsw_dnode_create_root_with_tree (
            volg, 0, 0,
            &volg->root
        );
    }

    fsw_set_blocksize (
        volg, vol->sectorsize,
        vol->sectorsize
    );
    vol->n_devices_allocated = vol->num_devices;
    vol->rescan_once = vol->num_devices > 1;
    err = FSW_DO_ALLOC(
        sizeof (struct fsw_btrfs_device_desc) * vol->n_devices_allocated,
        (void **) &vol->devices_attached
    );
    if (err) return err;

    vol->n_devices_attached = 1;
    vol->devices_attached[0].dev = volg;
    vol->devices_attached[0].id = sblock.this_device.device_id;

    for (i = 0; i < 0x100; i++) {
        if (sblock.label[i] == 0) break;
    }

    s.type = FSW_STRING_TYPE_UTF08;
    s.size = s.len = i;
    s.data = sblock.label;

    err = fsw_strdup_coerce (
        &volg->label,
        volg->host_string_type, &s
    );
    if (err) {
        FreePool(vol->devices_attached);
        vol->devices_attached = NULL;
        return err;
    }

    err = fsw_btrfs_get_default_root (
        vol, sblock.root_dir_objectid
    );
    if (err) {
        FSW_MSG_L03((
            FSW_MSG_STR(
                "FSW_BTRFS: fsw_btrfs_volume_mount ... Root not Found!\n"
            )
        ));

        FreePool(vol->devices_attached);
        vol->devices_attached = NULL;
        return err;
    }

    return FSW_SUCCESS;
}

void fsw_btrfs_volume_free (
    struct fsw_volume *volg
) {
    unsigned i;
    struct fsw_btrfs_volume *vol = (struct fsw_btrfs_volume *)volg;

    if (!vol) return;
    if (vol->is_master) fsw_btrfs_master_uuid_remove (vol);
    if (vol->devices_attached) {

        for (i = 1; i < vol->n_devices_attached; i++) {
            if (vol->devices_attached[i].dev) {
                fsw_unmount (vol->devices_attached[i].dev);
            }
        }
        FreePool(vol->devices_attached);
        vol->devices_attached = NULL;
    }

    if (vol->extent) FreePool(vol->extent);
    if (vol->rcache) {
	for (i = 0; i < RECOVER_CACHE_SIZE; i++)
        if (vol->rcache->buffer) {
            FreePool(vol->rcache->buffer);
            vol->rcache->buffer = NULL;
        }
        FreePool(vol->rcache);
        vol->rcache = NULL;
    }
}

fsw_status_t fsw_btrfs_volume_stat (
    struct fsw_volume      *volg,
    struct fsw_volume_stat *sb
) {
    struct fsw_btrfs_volume *vol = (struct fsw_btrfs_volume *)volg;

    sb->total_bytes = vol->total_bytes;
    sb->free_bytes  = vol->bytes_used;

    return FSW_SUCCESS;
}

fsw_status_t fsw_btrfs_read_inode (
    struct fsw_btrfs_volume *vol,
    struct btrfs_inode      *inode,
    uint64_t                 num,
    uint64_t                 tree
) {
    struct btrfs_key key_in, key_out;
    uint64_t elemaddr;
    fsw_size_t elemsize;
    fsw_status_t err;

    key_in.object_id = num;
    key_in.type = GRUB_BTRFS_ITEM_TYPE_INODE_ITEM;
    key_in.offset = 0;

    err = fsw_btrfs_lower_bound (
        vol, &key_in, &key_out, tree,
        &elemaddr, &elemsize, NULL, 0
    );
    if (err) return err;

    if (key_out.object_id != num ||
        key_out.type != GRUB_BTRFS_ITEM_TYPE_INODE_ITEM
    ) {
        return FSW_NOT_FOUND;
    }

    return fsw_btrfs_read_logical (
        vol, elemaddr, inode,
        sizeof (*inode), 0, 2
    );
}

fsw_status_t fsw_btrfs_dnode_fill (
    struct fsw_volume *volg,
    struct fsw_dnode  *dnog
) {
    struct fsw_btrfs_volume *vol = (struct fsw_btrfs_volume *)volg;
    struct fsw_btrfs_dnode  *dno = (struct fsw_btrfs_dnode  *)dnog;
    fsw_status_t    err;
    uint32_t        mode;

    if (!vol->is_master) {
        dno->g.size = 0;
        dno->g.type = FSW_DNODE_TYPE_DIR;

        return FSW_SUCCESS;
    }

    if (dno->raw) {
        return FSW_SUCCESS;
    }

    dno->raw = AllocatePool (sizeof (struct btrfs_inode));
    if (dno->raw == NULL) {
        return FSW_OUT_OF_MEMORY;
    }

    err = fsw_btrfs_read_inode (
        vol, dno->raw,
        dno->g.dnode_id,
        dno->g.tree_id
    );
    if (err) {
        FreePool(dno->raw);
        dno->raw = NULL;

        return err;
    }

    dno->g.size = FSW_U64_LE_SWAP(dno->raw->size);

    mode = FSW_U32_LE_SWAP(dno->raw->mode);
    if (0);
    else if (S_ISREG (mode)) dno->g.type = FSW_DNODE_TYPE_FILE;
    else if (S_ISDIR (mode)) dno->g.type = FSW_DNODE_TYPE_DIR;
    else if (S_ISLNK (mode)) dno->g.type = FSW_DNODE_TYPE_SYMLINK;
    else                     dno->g.type = FSW_DNODE_TYPE_SPECIAL;

    return FSW_SUCCESS;
}

void fsw_btrfs_dnode_free (
    struct fsw_volume *volg,
    struct fsw_dnode  *dnog
) {
    struct fsw_btrfs_dnode *dno = (struct fsw_btrfs_dnode  *)dnog;

    if (dno->raw) FreePool(dno->raw);
}

fsw_status_t fsw_btrfs_dnode_stat (
    struct fsw_volume     *volg,
    struct fsw_dnode      *dnog,
    struct fsw_dnode_stat *sb
) {
    struct fsw_btrfs_dnode *dno = (struct fsw_btrfs_dnode  *)dnog;

    if (dno->raw == NULL) {
        sb->used_bytes = 0;
        fsw_store_time_posix (sb, FSW_DNODE_STAT_CTIME, 0);
        fsw_store_time_posix (sb, FSW_DNODE_STAT_ATIME, 0);
        fsw_store_time_posix (sb, FSW_DNODE_STAT_MTIME, 0);
        return FSW_SUCCESS;
    }

    sb->used_bytes = FSW_U64_LE_SWAP(dno->raw->nbytes);
    fsw_store_time_posix (sb, FSW_DNODE_STAT_ATIME, FSW_U64_LE_SWAP(dno->raw->atime.sec));
    fsw_store_time_posix (sb, FSW_DNODE_STAT_CTIME, FSW_U64_LE_SWAP(dno->raw->otime.sec));
    fsw_store_time_posix (sb, FSW_DNODE_STAT_MTIME, FSW_U64_LE_SWAP(dno->raw->mtime.sec));
    fsw_store_attr_posix (sb,                       FSW_U32_LE_SWAP(dno->raw->mode));

    return FSW_SUCCESS;
}

fsw_ssize_t grub_btrfs_lzo_decompress (
    char       *ibuf,
    fsw_size_t  isize,
    grub_off_t  off,
    char       *obuf,
    fsw_size_t  osize
) {
    uint32_t total_size, cblock_size;
    fsw_size_t ret = 0;
    unsigned char buf[GRUB_BTRFS_LZO_BLOCK_SIZE];
    char *ibuf0 = ibuf;

    total_size = FSW_U32_LE_SWAP(
        GET_UNALIGNED32(ibuf)
    );
    ibuf += sizeof (total_size);

    if (isize < total_size) {
        return -1;
    }

    while (off >= GRUB_BTRFS_LZO_BLOCK_SIZE) {

        if (((ibuf - ibuf0) & 0xffc) == 0xffc) {
            ibuf = ((ibuf - ibuf0 + 3) & ~3) + ibuf0;
        }

        cblock_size = FSW_U32_LE_SWAP(
            GET_UNALIGNED32(ibuf)
        );
        ibuf += sizeof (cblock_size);

        if (cblock_size > GRUB_BTRFS_LZO_BLOCK_MAX_CSIZE) {
            return -1;
        }

        off -= GRUB_BTRFS_LZO_BLOCK_SIZE;
        ibuf += cblock_size;
    }

    while (osize > 0) {
        lzo_uint usize = GRUB_BTRFS_LZO_BLOCK_SIZE;

        if (((ibuf - ibuf0) & 0xffc) == 0xffc) {
            ibuf = ((ibuf - ibuf0 + 3) & ~3) + ibuf0;
        }

        cblock_size = FSW_U32_LE_SWAP(
            GET_UNALIGNED32(ibuf)
        );
        ibuf += sizeof (cblock_size);

        if (cblock_size > GRUB_BTRFS_LZO_BLOCK_MAX_CSIZE) {
            return -1;
        }

        if (off > 0 || osize < GRUB_BTRFS_LZO_BLOCK_SIZE) {
            fsw_size_t to_copy = GRUB_BTRFS_LZO_BLOCK_SIZE - off;

            if (to_copy > osize) to_copy = osize;
            if (lzo1x_decompress_safe (
                    (lzo_bytep)ibuf, cblock_size,
                    (lzo_bytep)buf, &usize, NULL
                ) != 0
            ) {
                return -1;
            }

            if (to_copy > usize) to_copy = usize;

            FSW_DO_MEMCPY(
                obuf,
                buf + off,
                to_copy
            );

            osize -= to_copy;
            ret   += to_copy;
            obuf  += to_copy;
            ibuf  += cblock_size;
            off    = 0;

            continue;
        }

        if (lzo1x_decompress_safe (
                (lzo_bytep)ibuf, cblock_size,
                (lzo_bytep)obuf, &usize, NULL
            ) != 0
        ) {
            return -1;
        }

        osize -= usize;
        ret   += usize;
        obuf  += usize;
        ibuf  += cblock_size;
    }

    return ret;
}

fsw_status_t fsw_btrfs_log_inflate (
    fsw_ssize_t              ret,
    fsw_size_t               csize,
    char                    *buf,
    struct fsw_btrfs_volume *vol,
    fsw_size_t               log_flag
) {
    #if FSW_DEBUG_LEVEL >= 2
    switch (vol->extent->compression) {
        case GRUB_BTRFS_COMPRESSION_NONE: {
            FSW_MSG_L02((
                FSW_MSG_STR(
                    "FSW_BTRFS: fsw_btrfs_log_inflate ... Compression Type:- 'None'\n"
                )
            ));

            break;
        }

        case GRUB_BTRFS_COMPRESSION_ZLIB: {
            FSW_MSG_L02((
                FSW_MSG_STR(
                    "FSW_BTRFS: fsw_btrfs_log_inflate ... Compression Type:- 'ZLIB'\n"
                )
            ));

            break;
        }

        case GRUB_BTRFS_COMPRESSION_LZO: {
            FSW_MSG_L02((
                FSW_MSG_STR(
                    "FSW_BTRFS: fsw_btrfs_log_inflate ... Compression Type:- 'LZO'\n"
                )
            ));

            break;
        }

        case GRUB_BTRFS_COMPRESSION_ZSTD: {
            FSW_MSG_L02((
                FSW_MSG_STR(
                    "FSW_BTRFS: fsw_btrfs_log_inflate ... Compression Type:- 'ZSTD'\n"
                )
            ));

            break;
        }

        default: {
            FSW_MSG_L02((
                FSW_MSG_STR(
                    "FSW_BTRFS: fsw_btrfs_log_inflate ... Compression Type:- 'Other'\n"
                )
            ));
        }
    }
    #endif

    if (vol->extent->compression == GRUB_BTRFS_COMPRESSION_NONE) {
        return FSW_NOT_FOUND;
    }

    if (ret == (fsw_ssize_t) csize) {
        FSW_MSG_L02((
            FSW_MSG_STR(
                "FSW_BTRFS: fsw_btrfs_log_inflate ... Decompression Status:- 'Success' (Tag_%02u)\n"
            ), (unsigned) log_flag
        ));

        return FSW_SUCCESS;
    }

    #if FSW_DEBUG_LEVEL >= 2
    if (ret < 0) {
        FSW_MSG_L02((
            FSW_MSG_STR(
                "FSW_BTRFS: fsw_btrfs_log_inflate ... Decompression Status:- Bad Input (Type '%d' Error ... Tag_%02u)\n"
            ), ret, (unsigned) log_flag
        ));
    }
    #endif

    FSW_MSG_L01((
        FSW_MSG_STR(
            "FSW_BTRFS: fsw_btrfs_log_inflate ... Leaving with Status: 'FSW_VOLUME_CORRUPTED' (Tag_%02u)\n"
        ), (unsigned) log_flag
    ));

    return FSW_VOLUME_CORRUPTED;
}

fsw_ssize_t fsw_btrfs_decompress (
    uint8_t     comp,
    char       *ibuf,
    fsw_size_t  isize,
	grub_off_t  off,
    char       *obuf,
    fsw_size_t  osize
) {
	return fsw_btrfs_decompressor_func[comp-1] (
        ibuf, isize,
        off, obuf, osize
    );
}

fsw_status_t fsw_btrfs_get_extent (
    struct fsw_volume *volg,
    struct fsw_dnode  *dnog,
    struct fsw_extent *extent
) {
    struct fsw_btrfs_volume *vol = (struct fsw_btrfs_volume *)volg;
    struct btrfs_key key_in  = {0};
    struct btrfs_key key_out = {0};
    uint64_t ino = dnog->dnode_id;
    uint64_t tree = dnog->tree_id;
    uint64_t pos = extent->log_start << vol->sectorshift;
    uint64_t count;
    uint64_t extoff;
    fsw_size_t csize;
    fsw_size_t logtag;
    fsw_ssize_t ret = 0;
    fsw_status_t err;
    char *buf = NULL;
    char *tmp = NULL;

    if (!vol->is_master) {
        FSW_MSG_L01((
            FSW_MSG_STR(
                "FSW_BTRFS: fsw_btrfs_get_extent ... Leaving with Status: 'FSW_NOT_FOUND' (Slave Device)\n"
            )
        ));

        return FSW_NOT_FOUND;
    }

    extent->log_count = 1;
    extent->type = FSW_EXTENT_TYPE_INVALID;

    BOOLEAN no_cache = (
        !vol->extent         ||
        vol->extino  != ino  ||
        vol->exttree != tree ||
        pos < vol->extstart  ||
        pos >= vol->extend
    );

    if (no_cache) {
        uint64_t   elemaddr;
        fsw_size_t elemsize;

        if (vol->extent) {
            FreePool(vol->extent);
            vol->extent = NULL;
        }

        key_in.object_id = ino;
        key_in.offset    = FSW_U64_LE_SWAP(pos);
        key_in.type      = GRUB_BTRFS_ITEM_TYPE_EXTENT_ITEM;

        err = fsw_btrfs_lower_bound (
            vol, &key_in, &key_out, tree,
            &elemaddr, &elemsize, NULL, 0
        );
        if (err) {
            FSW_MSG_L01((
                FSW_MSG_STR(
                    "FSW_BTRFS: fsw_btrfs_get_extent ... Leaving with Status: 'FSW_VOLUME_CORRUPTED' ('fsw_btrfs_lower_bound' Failure ... Tag_01)\n"
                )
            ));

            return FSW_VOLUME_CORRUPTED;
        }

        if (key_out.object_id != ino) {
            FSW_MSG_L01((
                FSW_MSG_STR(
                    "FSW_BTRFS: fsw_btrfs_get_extent ... Leaving with Status: 'FSW_VOLUME_CORRUPTED' (key_out.object_id != ino)\n"
                )
            ));

            return FSW_VOLUME_CORRUPTED;
        }

        if (key_out.type != GRUB_BTRFS_ITEM_TYPE_EXTENT_ITEM) {
            FSW_MSG_L01((
                FSW_MSG_STR(
                    "FSW_BTRFS: fsw_btrfs_get_extent ... Leaving with Status: 'FSW_VOLUME_CORRUPTED' (key_out.type != TYPE_EXTENT_ITEM)\n"
                )
            ));

            return FSW_VOLUME_CORRUPTED;
        }

        size_t inl_need = offsetof (
            struct btrfs_extent_data, inl
        );
        if (elemsize < inl_need) {
            FSW_MSG_L01((
                FSW_MSG_STR(
                    "FSW_BTRFS: fsw_btrfs_get_extent ... Leaving with Status: 'FSW_VOLUME_CORRUPTED' (elemsize < inline header)\n"
                )
            ));

            return FSW_VOLUME_CORRUPTED;
        }

        vol->extent = AllocatePool (elemsize);
        if (!vol->extent) {
            FSW_MSG_L01((
                FSW_MSG_STR(
                    "FSW_BTRFS: fsw_btrfs_get_extent ... Leaving with Status: 'FSW_OUT_OF_MEMORY' ('vol->extent' Memory Allocation Failure)\n"
                )
            ));

            return FSW_OUT_OF_MEMORY;
        }

        vol->extstart = FSW_U64_LE_SWAP(
            key_out.offset
        );
        if (pos < vol->extstart) {
            FSW_MSG_L01((
                FSW_MSG_STR(
                    "FSW_BTRFS: fsw_btrfs_get_extent ... Leaving with Status: 'FSW_VOLUME_CORRUPTED' (Cache Corruption ... Tag_01)\n"
                )
            ));

            return FSW_VOLUME_CORRUPTED;
        }

        vol->extino  = ino;
        vol->exttree = tree;
        vol->extsize = elemsize;

        err = fsw_btrfs_read_logical (
            vol,
            elemaddr, vol->extent,
            elemsize, 0, 1
        );
        if (err) {
            FSW_MSG_L01((
                FSW_MSG_STR(
                    "FSW_BTRFS: fsw_btrfs_get_extent ... Leaving with Status '%d' Error ('fsw_btrfs_read_logical' Failure ... Tag_01)\n"
                ), err
            ));

            return err;
        }

        if (vol->extent->type != GRUB_BTRFS_EXTENT_INLINE &&
            vol->extent->type != GRUB_BTRFS_EXTENT_REGULAR
        ) {
            FSW_MSG_L01((
                FSW_MSG_STR(
                    "FSW_BTRFS: fsw_btrfs_get_extent ... Leaving with Status: 'FSW_UNSUPPORTED' (Bad Extent Type)\n"
                )
            ));

            return FSW_UNSUPPORTED;
        }

        size_t UseSizePhys = 0;
        size_t filled_need = (
            sizeof (
                vol->extent->filled
            ) + offsetof (
                struct btrfs_extent_data, filled
            )
        );

        BOOLEAN filled_present = (
            elemsize >= filled_need &&
            vol->extent->type != GRUB_BTRFS_EXTENT_INLINE
        );

        uint64_t extsize_filled = (
            filled_present
        ) ? FSW_U64_LE_SWAP(vol->extent->filled) : 0;

        uint64_t extsize_physical = FSW_U64_LE_SWAP(
            vol->extent->size
        );

        FSW_MSG_L02((
            FSW_MSG_STR(
                "FSW_BTRFS: fsw_btrfs_get_extent ... elemsize==%llu filled_present==%u\n"
            ),
            (unsigned long long) elemsize,
            (unsigned) filled_present
        ));

        FSW_MSG_L02((
            FSW_MSG_STR(
                "FSW_BTRFS: fsw_btrfs_get_extent ... extsize_physical==%llu extsize_filled==%llu\n"
            ),
            (unsigned long long) extsize_physical,
            (unsigned long long) extsize_filled
        ));

        if (vol->extent->type == GRUB_BTRFS_EXTENT_INLINE) {
            FSW_MSG_L02((
                FSW_MSG_STR(
                    "FSW_BTRFS: fsw_btrfs_get_extent ... Extent Type:- 'Inline'\n"
                )
            ));

            UseSizePhys = 1;

            if (pos != vol->extstart) {
                FSW_MSG_L02((
                    FSW_MSG_STR(
                        "FSW_BTRFS: fsw_btrfs_get_extent ... Reset 'pos' from %llu to %llu\n"
                    ),
                    (unsigned long long) pos,
                    (unsigned long long) vol->extstart
                ));

                pos = vol->extstart;
            }
        }
        else {

            FSW_MSG_L02((
                FSW_MSG_STR(
                    "FSW_BTRFS: fsw_btrfs_get_extent ... Extent Type:- 'Regular'\n"
                )
            ));

            if (filled_present &&
                extsize_filled == 0
            ) {
                UseSizePhys = 1;
            }
            else {
                if (extsize_filled > UINTN_MAX - vol->extstart) {
                    UseSizePhys = 1;
                }
                else {
                    FSW_MSG_L02((
                        FSW_MSG_STR(
                            "FSW_BTRFS: fsw_btrfs_get_extent ... Use Size:- 'Filled'\n"
                        )
                    ));
                    vol->extend = vol->extstart + extsize_filled;
                }
            }
        }

        if (UseSizePhys) {
            FSW_MSG_L02((
                FSW_MSG_STR(
                    "FSW_BTRFS: fsw_btrfs_get_extent ... Use Size:- 'Physical'\n"
                )
            ));

            if (extsize_physical > UINTN_MAX - vol->extstart) {
                FSW_MSG_L01((
                    FSW_MSG_STR(
                        "FSW_BTRFS: fsw_btrfs_get_extent ... Leaving with Status: 'FSW_VOLUME_CORRUPTED' (extsize_physical > Max)\n"
                    )
                ));

                return FSW_VOLUME_CORRUPTED;
            }

            vol->extend = vol->extstart + extsize_physical;
        }

        FSW_MSG_L02((
            FSW_MSG_STR(
                "FSW_BTRFS: fsw_btrfs_get_extent ... extend==%llu extstart==%llu\n"
            ),
            (unsigned long long) vol->extend,
            (unsigned long long) vol->extstart
        ));
    }

    if (pos < vol->extstart) {
        FSW_MSG_L01((
            FSW_MSG_STR(
                "FSW_BTRFS: fsw_btrfs_get_extent ... Leaving with Status: 'FSW_VOLUME_CORRUPTED' (Cache Corruption ... Tag_02)\n"
            )
        ));

        return FSW_VOLUME_CORRUPTED;
    }

    if (pos >= vol->extend) {
        FSW_MSG_L02((
            FSW_MSG_STR(
                "FSW_BTRFS: fsw_btrfs_get_extent ... Chunk Type:- 'Void'\n"
            )
        ));

        extent->log_count = 1;
        if (no_cache) {
            uint64_t swap_offset = FSW_U64_LE_SWAP(key_out.offset);

            FSW_MSG_L02((
                FSW_MSG_STR(
                    "FSW_BTRFS: fsw_btrfs_get_extent ... ('%llu'>'%llu') && ('%llu'=='%llu')??\n"
                ),
                (unsigned long long) swap_offset,
                (unsigned long long) pos,
                (unsigned long long) key_out.object_id,
                (unsigned long long) ino
            ));

            if (swap_offset > pos &&
                key_out.object_id == ino
            ) {
                extent->log_count = (
                    swap_offset - pos
                ) >> vol->sectorshift;

                if (extent->log_count < 1) extent->log_count = 1;
                if (extent->log_count > 1) {
                    FSW_MSG_L02((
                        FSW_MSG_STR(
                            "FSW_BTRFS: fsw_btrfs_get_extent ... Aggregated %llu Void Blocks\n"
                        ), (unsigned long long) extent->log_count
                    ));
                }
            }
        }

         extent->type = FSW_EXTENT_TYPE_INVALID;
         return FSW_IO_ERROR;
    }

    csize = vol->extend - pos;
    if (csize > (UINTN_MAX >> vol->sectorshift)) {
        FSW_MSG_L01((
            FSW_MSG_STR(
                "FSW_BTRFS: fsw_btrfs_get_extent ... Leaving with Status: 'FSW_VOLUME_CORRUPTED' (Invalid Chunk Size)\n"
            )
        ));

        return FSW_VOLUME_CORRUPTED;
    }

    if (vol->extent->encryption) {
        FSW_MSG_L01((
            FSW_MSG_STR(
                "FSW_BTRFS: fsw_btrfs_get_extent ... Leaving with Status: 'FSW_UNSUPPORTED' (Encrypted)\n"
            )
        ));

        return FSW_UNSUPPORTED;
    }

    if (vol->extent->encoding) {
        FSW_MSG_L01((
            FSW_MSG_STR(
                "FSW_BTRFS: fsw_btrfs_get_extent ... Leaving with Status: 'FSW_UNSUPPORTED' (Encoded)\n"
            )
        ));

        return FSW_UNSUPPORTED;
    }

    switch (vol->extent->compression) {
        case GRUB_BTRFS_COMPRESSION_NONE:
        case GRUB_BTRFS_COMPRESSION_ZLIB:
        case GRUB_BTRFS_COMPRESSION_LZO:
        case GRUB_BTRFS_COMPRESSION_ZSTD:
            break;

        default: {
            FSW_MSG_L01((
                FSW_MSG_STR(
                    "FSW_BTRFS: fsw_btrfs_get_extent ... Leaving with Status: 'FSW_UNSUPPORTED' (Bad Compression Type ... Tag_01)\n"
                )
            ));

            return FSW_UNSUPPORTED;
        }
    }

    extoff = pos - vol->extstart;
    count  = (csize + vol->sectorsize - 1) >> vol->sectorshift;

    buf = AllocatePool (count << vol->sectorshift);
    if (!buf) {
        FSW_MSG_L01((
            FSW_MSG_STR(
                "FSW_BTRFS: fsw_btrfs_get_extent ... Leaving with Status: 'FSW_OUT_OF_MEMORY' ('buf' Memory Allocation Failure)\n"
            )
        ));

        return FSW_OUT_OF_MEMORY;
    }

    switch (vol->extent->type) {
        case GRUB_BTRFS_EXTENT_INLINE: {
            if (vol->extent->compression == GRUB_BTRFS_COMPRESSION_NONE) {
                #if FSW_DEBUG_LEVEL >= 2
                logtag = 0;
                fsw_btrfs_log_inflate (
                    ret, csize, buf,
                    vol, logtag
                );
                #endif

                FSW_DO_MEMCPY(
                    buf,
                    vol->extent->inl + extoff,
                    csize
                );
            }
            else {
                size_t inl_len = vol->extsize - (
                    (uint8_t *)vol->extent->inl -
                    (uint8_t *)vol->extent
                );
                ret = fsw_btrfs_decompress (
                    vol->extent->compression,
                    vol->extent->inl,
                    inl_len,
                    extoff, buf, csize
                );

                logtag = 1;
                err = fsw_btrfs_log_inflate (
                    ret, csize, buf,
                    vol, logtag
                );
                if (err) {
                    if (buf) { FreePool(buf); buf = NULL; }
                    return err;
                }
            }

            break;
        }

        default: {

            if (!vol->extent->laddr) break;

            if (vol->extent->compression > GRUB_BTRFS_COMPRESSION_MAX) {
                FSW_MSG_L01((
                    FSW_MSG_STR(
                        "FSW_BTRFS: fsw_btrfs_get_extent ... Leaving with Status: 'FSW_VOLUME_CORRUPTED' (Bad Compression Type ... Tag_02)\n"
                    )
                ));

                if (buf) { FreePool(buf); buf = NULL; }
                return FSW_VOLUME_CORRUPTED;
            }

            uint64_t phys_off  = FSW_U64_LE_SWAP(vol->extent->offset);
            uint64_t phys_base = FSW_U64_LE_SWAP(vol->extent->laddr);

            if (phys_off > UINTN_MAX -  phys_base ||
                extoff   > UINTN_MAX - (phys_base + phys_off)
            ) {
                FSW_MSG_L01((
                    FSW_MSG_STR(
                        "FSW_BTRFS: fsw_btrfs_get_extent ... Leaving with Status: 'FSW_VOLUME_CORRUPTED' ('Invalid Extent Offset)\n"
                    )
                ));

                if (buf) { FreePool(buf); buf = NULL; }
                return FSW_VOLUME_CORRUPTED;
            }

            if (vol->extent->compression == GRUB_BTRFS_COMPRESSION_NONE) {
                if (count > 128) {
                    count = 128;
                    csize = count << vol->sectorshift;
                }

                uint64_t phys_addr = (
                    phys_base +
                    phys_off +
                    extoff
                );
                err = fsw_btrfs_read_logical (
                    vol, phys_addr,
                    buf, csize, 0, 0
                );
                if (err) {
                    FSW_MSG_L01((
                        FSW_MSG_STR(
                            "FSW_BTRFS: fsw_btrfs_get_extent ... Leaving with Status '%d' Error ('fsw_btrfs_read_logical' Failure ... Tag_02)\n"
                        ), err
                    ));

                    if (buf) { FreePool(buf); buf = NULL; }
                    return err;
                }

                break;
            }

            uint64_t zsize = FSW_U64_LE_SWAP(
                vol->extent->compressed_size
            );

            tmp = AllocatePool (zsize);
            if (!tmp) {
                FSW_MSG_L01((
                    FSW_MSG_STR(
                        "FSW_BTRFS: fsw_btrfs_get_extent ... Leaving with Status: 'FSW_OUT_OF_MEMORY' ('tmp' Memory Allocation Failure)\n"
                    )
                ));

                if (buf) { FreePool(buf); buf = NULL; }
                return FSW_OUT_OF_MEMORY;
            }

            err = fsw_btrfs_read_logical (
                vol,
                FSW_U64_LE_SWAP(vol->extent->laddr),
                tmp, zsize, 0, 0
            );
            if (err) {
                FSW_MSG_L01((
                    FSW_MSG_STR(
                        "FSW_BTRFS: fsw_btrfs_get_extent ... Leaving with Status: 'FSW_VOLUME_CORRUPTED' ('fsw_btrfs_read_logical' Failure ... Tag_03)\n"
                    )
                ));

                if (tmp) { FreePool(tmp); tmp = NULL; }
                if (buf) { FreePool(buf); buf = NULL; }
                return FSW_VOLUME_CORRUPTED;
            }

            if (extoff > UINTN_MAX - phys_off) {
                FSW_MSG_L01((
                    FSW_MSG_STR(
                        "FSW_BTRFS: fsw_btrfs_get_extent ... Leaving with Status: 'FSW_VOLUME_CORRUPTED' (Path Offset Overflow)\n"
                    )
                ));

                if (tmp) { FreePool(tmp); tmp = NULL; }
                if (buf) { FreePool(buf); buf = NULL; }
                return FSW_VOLUME_CORRUPTED;
            }

            ret = fsw_btrfs_decompress (
                vol->extent->compression,
                tmp, zsize,
                phys_off + extoff,
                buf, csize
            );

            if (tmp) { FreePool(tmp); tmp = NULL; }

            logtag = 2;
            err = fsw_btrfs_log_inflate (
                ret, csize, buf,
                vol, logtag
            );
            if (err) {
                if (buf) { FreePool(buf); buf = NULL; }
                return err;
            }

            break;
        }
    }

    extent->log_count = count;
    extent->buffer = buf;
    if (extent->buffer) {
        extent->type = FSW_EXTENT_TYPE_BUFFER;
        if (csize < (count << vol->sectorshift)) {
            FSW_DO_MEMZERO(
                buf + csize,
                (count << vol->sectorshift) - csize
            );
        }

        FSW_MSG_L02((
            FSW_MSG_STR(
                "FSW_BTRFS: fsw_btrfs_get_extent ... Chunk Type:- 'Buffer'\n"
            )
        ));
    }
    else {
        extent->type = FSW_EXTENT_TYPE_SPARSE;

        FSW_MSG_L02((
            FSW_MSG_STR(
                "FSW_BTRFS: fsw_btrfs_get_extent ... Chunk Type:- 'Sparse'\n"
            )
        ));
    }

    FSW_MSG_L03((
        FSW_MSG_STR(
            "FSW_BTRFS: fsw_btrfs_get_extent ... Leaving with Status: 'FSW_SUCCESS'\n"
        )
    ));

    return FSW_SUCCESS;
}

fsw_status_t fsw_btrfs_readlink (
    struct fsw_volume *volg,
    struct fsw_dnode  *dnog,
    struct fsw_string *link_target
) {
    int                      i;
    char                    *tmp;
    fsw_status_t             status;
    struct fsw_string        link_str;
    struct fsw_btrfs_volume *vol = (struct fsw_btrfs_volume *)volg;
    struct fsw_btrfs_dnode  *dno = (struct fsw_btrfs_dnode  *)dnog;

    if (dno->g.size > FSW_PATH_MAX) {
        return FSW_VOLUME_CORRUPTED;
    }

    tmp = AllocatePool (dno->g.size);
    if (!tmp) {
        return FSW_OUT_OF_MEMORY;
    }

    i = 0;
    do {
        struct fsw_extent extent;
        extent.type       =    0;
        extent.log_start  =    0;
        extent.log_count  =    0;
        extent.phys_start =    0;
        extent.buffer     = NULL;
        status = fsw_btrfs_get_extent (
            volg, dnog, &extent
        );
        if (status || extent.type != FSW_EXTENT_TYPE_BUFFER) {
            FreePool(tmp);
            if (extent.buffer) FreePool(extent.buffer);

            return FSW_VOLUME_CORRUPTED;
        }

        int size =  extent.log_count << vol->sectorshift;
        if (size > (dno->g.size - (i << vol->sectorshift))) {
            size = (dno->g.size - (i << vol->sectorshift));
        }
        FSW_DO_MEMCPY(
            tmp + (i<<vol->sectorshift),
            extent.buffer,
            size
        );
        FreePool(extent.buffer);
        extent.buffer = NULL;

        extent.log_start = i;
        i += extent.log_count;
    } while ((i << vol->sectorshift) < dno->g.size);

    link_str.data = tmp;
    link_str.type = FSW_STRING_TYPE_UTF08;
    link_str.size = link_str.len = (int)dno->g.size;

    fsw_strdup_coerce (
        link_target,
        volg->host_string_type, &link_str
    );
    FreePool(tmp);

    return FSW_SUCCESS;
}

fsw_status_t fsw_btrfs_lookup_dir_item (
    struct fsw_btrfs_volume  *vol,
    uint64_t                  tree_id,
    uint64_t                  object_id,
    struct fsw_string        *lookup_name,
    struct btrfs_dir_item   **direl_buf,
    struct btrfs_dir_item   **direl_out
) {
    fsw_status_t           err;
    uint64_t               elemaddr;
    fsw_size_t             elemsize;
    fsw_size_t             allocated;
    struct btrfs_key       key;
    struct btrfs_key       key_out;
    struct btrfs_dir_item *cdirel;

    *direl_buf = NULL;

    key.object_id = object_id;
    key.type = GRUB_BTRFS_ITEM_TYPE_DIR_ITEM;
    key.offset = FSW_U64_LE_SWAP(
        ~grub_getcrc32c (
            1, lookup_name->data, lookup_name->size
        )
    );

    err = fsw_btrfs_lower_bound (
        vol, &key, &key_out, tree_id,
        &elemaddr, &elemsize, NULL, 0
    );
    if (err) {
        FSW_MSG_L03((
            FSW_MSG_STR(
                "FSW_BTRFS: fsw_btrfs_lookup_dir_item ... Returned 'FSW_NOT_FOUND' at Main Check 01\n"
            )
        ));

        return err;
    }

    if (elemsize == 0) {
        FSW_MSG_L03((
            FSW_MSG_STR(
                "FSW_BTRFS: fsw_btrfs_lookup_dir_item ... Returned 'FSW_NOT_FOUND' at Main Check 02 (Zero Element Size)\n"
            )
        ));

        return FSW_NOT_FOUND;
    }

    if (fsw_btrfs_key_cmp (&key, &key_out) != 0) {
        FSW_MSG_L03((
            FSW_MSG_STR(
                "FSW_BTRFS: fsw_btrfs_lookup_dir_item ... Returned 'FSW_NOT_FOUND' at Main Check 03\n"
            )
        ));

        return FSW_NOT_FOUND;
    }

    allocated = elemsize * 2;
    if (*direl_buf) FreePool(*direl_buf);
    *direl_buf = AllocatePool (allocated + 1);
    if (!*direl_buf) {
        FSW_MSG_L03((
            FSW_MSG_STR(
                "FSW_BTRFS: fsw_btrfs_lookup_dir_item ... Returned 'FSW_OUT_OF_MEMORY' at Main Check 04\n"
            )
        ));

        return FSW_OUT_OF_MEMORY;
    }

    err = fsw_btrfs_read_logical (
        vol, elemaddr, *direl_buf,
        elemsize, 0, 1
    );
    if (err) {
        FSW_MSG_L03((
            FSW_MSG_STR(
                "FSW_BTRFS: fsw_btrfs_lookup_dir_item ... Returned with Error at Main Check 05 (Failed to Read Leaf Data)\n"
            )
        ));

        return err;
    }

    for (
        cdirel = *direl_buf;
        (uint8_t *) cdirel - (uint8_t *) *direl_buf < (fsw_ssize_t) elemsize;
        cdirel = (struct btrfs_dir_item *) (
            (uint8_t *) cdirel             +
            sizeof (struct btrfs_dir_item) +
            FSW_U16_LE_SWAP(cdirel->n)     +
            FSW_U16_LE_SWAP(cdirel->m)
        )
    ) {
        uint16_t n = FSW_U16_LE_SWAP(cdirel->n);
        uint16_t m = FSW_U16_LE_SWAP(cdirel->m);
        size_t entry_size = sizeof (struct btrfs_dir_item) + (size_t) n + (size_t) m;

        if (n == 0 ||
            entry_size > (size_t)((uint8_t *) *direl_buf - (uint8_t *)cdirel + elemsize)
        ) {
            FSW_MSG_L03((
                FSW_MSG_STR(
                    "FSW_BTRFS: fsw_btrfs_lookup_dir_item ... Returned 'FSW_NOT_FOUND' from Main Loop (Malformed/Overflow)\n"
                )
            ));

            return FSW_NOT_FOUND;
        }

        if (lookup_name->size == n &&
            FSW_DO_MEMEQ(
                cdirel->name,
                lookup_name->data,
                lookup_name->size
            )
        ) {
            FSW_MSG_L03((
                FSW_MSG_STR(
                    "FSW_BTRFS: fsw_btrfs_lookup_dir_item ... Found Entry\n"
                )
            ));

            break;
        }
    }

    if ((uint8_t *)cdirel - (uint8_t *) *direl_buf >= (fsw_ssize_t)elemsize) {
        FSW_MSG_L03((
            FSW_MSG_STR(
                "FSW_BTRFS: fsw_btrfs_lookup_dir_item ... Returned 'FSW_NOT_FOUND' at Main Check 06 (Invalid Entry Size)\n"
            )
        ));

        return FSW_NOT_FOUND;
    }

    FSW_MSG_L03((
        FSW_MSG_STR(
            "FSW_BTRFS: fsw_btrfs_lookup_dir_item ... Leaving with Status: FSW_SUCCESS\n"
        )
    ));

    *direl_out = cdirel;
    return FSW_SUCCESS;
}

fsw_status_t fsw_btrfs_get_root_tree (
    struct fsw_btrfs_volume *vol,
    struct btrfs_key        *key_in,
    uint64_t                *tree_out
) {
    fsw_status_t err;
    uint64_t elemaddr;
    fsw_size_t elemsize;
    struct btrfs_root_item ri;
    struct btrfs_key key_out;

    err = fsw_btrfs_lower_bound (
        vol, key_in,
        &key_out, vol->root_tree,
        &elemaddr, &elemsize, NULL, 0
    );
    if (err) return err;

    if (key_in->type != key_out.type ||
        key_in->object_id != key_out.object_id
    ) {
        return FSW_NOT_FOUND;
    }

    err = fsw_btrfs_read_logical (
        vol, elemaddr, &ri,
        sizeof (ri), 0, 1
    );
    if (err) return err;

    *tree_out = ri.tree;
    return FSW_SUCCESS;
}

fsw_status_t fsw_btrfs_get_sub_dnode (
    struct fsw_btrfs_volume  *vol,
    struct fsw_btrfs_dnode   *dno,
    struct btrfs_dir_item    *cdirel,
    struct fsw_string        *name,
    struct fsw_dnode        **child_dno_out
) {
    fsw_status_t err;
    int child_type;
    uint64_t tree_id = dno->g.tree_id;
    uint64_t child_id;

    switch (cdirel->key.type) {
        case GRUB_BTRFS_ITEM_TYPE_ROOT_ITEM: {
            err = fsw_btrfs_get_root_tree (
                vol, &cdirel->key, &tree_id
            );
            if (err) {
                FSW_MSG_L03((
                    FSW_MSG_STR(
                        "FSW_BTRFS: fsw_btrfs_get_sub_dnode ... Exit Func with Err:- '%d'\n"
                    ), err
                ));

                return err;
            }

            child_type = GRUB_BTRFS_DIR_ITEM_TYPE_DIRECTORY;
            child_id = FSW_U64_LE_SWAP(
                GRUB_BTRFS_OBJECT_ID_CHUNK
            );

            break;
        }

        case GRUB_BTRFS_ITEM_TYPE_INODE_ITEM: {
            child_type = cdirel->type;
            child_id   = cdirel->key.object_id;

            break;
        }

        default: {
            FSW_MSG_L03((
                FSW_MSG_STR(
                    "FSW_BTRFS: fsw_btrfs_get_sub_dnode ... Exit Func with FSW_VOLUME_CORRUPTED\n"
                ), cdirel->key.type
            ));

            return FSW_VOLUME_CORRUPTED;
        }
    }

    switch (child_type) {
        case GRUB_BTRFS_DIR_ITEM_TYPE_DIRECTORY: child_type = FSW_DNODE_TYPE_DIR    ; break;
        case GRUB_BTRFS_DIR_ITEM_TYPE_REGULAR:   child_type = FSW_DNODE_TYPE_FILE   ; break;
        case GRUB_BTRFS_DIR_ITEM_TYPE_SYMLINK:   child_type = FSW_DNODE_TYPE_SYMLINK; break;
        default:                                 child_type = FSW_DNODE_TYPE_SPECIAL; break;
    }

    return fsw_dnode_create_with_tree (
        &dno->g, tree_id,
        child_id, child_type,
        name, child_dno_out
    );
}

fsw_status_t fsw_btrfs_dir_lookup (
    struct fsw_volume  *volg,
    struct fsw_dnode   *dnog,
    struct fsw_string  *lookup_name,
    struct fsw_dnode  **child_dno_out
) {
    struct fsw_btrfs_volume *vol = (struct fsw_btrfs_volume *)volg;
    struct fsw_btrfs_dnode  *dno = (struct fsw_btrfs_dnode  *)dnog;
    fsw_status_t             err;
    struct fsw_string        s;

    *child_dno_out = NULL;

    if (!vol->is_master) {
        return FSW_NOT_FOUND;
    }

    err = fsw_strdup_coerce (
        &s, FSW_STRING_TYPE_UTF08,
        lookup_name
    );
    if (err) return err;

    if (s.size == 3              &&
        dnog == volg->root       &&
        ((char *)s.data)[0]=='.' &&
        ((char *)s.data)[1]=='.' &&
        ((char *)s.data)[2]=='.'
    ) {

        fsw_strfree (&s);
        if (dnog->tree_id == vol->top_tree) {
            fsw_dnode_retain (dnog);
            *child_dno_out = dnog;
            return FSW_SUCCESS;
        }
        return fsw_dnode_create_with_tree (
            dnog, vol->top_tree,
            FSW_U64_LE_SWAP(GRUB_BTRFS_OBJECT_ID_CHUNK),
            FSW_DNODE_TYPE_DIR, lookup_name, child_dno_out
        );
    }
    struct btrfs_dir_item *direl=NULL, *cdirel;
    err = fsw_btrfs_lookup_dir_item (
        vol,
        dnog->tree_id, dnog->dnode_id,
        &s, &direl, &cdirel
    );
    if (!err) {
        err = fsw_btrfs_get_sub_dnode (
            vol, dno, cdirel,
            lookup_name, child_dno_out
        );
    }
    if (direl) FreePool(direl);
    fsw_strfree (&s);

    return err;
}

fsw_status_t fsw_btrfs_get_default_root (
    struct fsw_btrfs_volume *vol,
    uint64_t                 root_dir_objectid
) {
    fsw_status_t err;
    struct fsw_string s;
    struct btrfs_dir_item *direl=NULL, *cdirel;
    struct btrfs_key top_root_key;

    top_root_key.object_id = FSW_U64_LE_SWAP(5UL);
    top_root_key.type = GRUB_BTRFS_ITEM_TYPE_ROOT_ITEM;
    top_root_key.offset = -1LL;

    err = fsw_btrfs_get_root_tree (
        vol, &top_root_key, &vol->top_tree
    );
    if (err) return err;

    uint64_t default_tree_id = vol->top_tree;

    s.type = FSW_STRING_TYPE_UTF08;
    s.data = "default";
    s.size = 7;

    err = fsw_btrfs_lookup_dir_item (
        vol, vol->root_tree,
        root_dir_objectid, &s,
        &direl, &cdirel
    );
    if (!err                                               &&
        cdirel->type == GRUB_BTRFS_DIR_ITEM_TYPE_DIRECTORY &&
        cdirel->key.type == GRUB_BTRFS_ITEM_TYPE_ROOT_ITEM &&
        cdirel->key.object_id != FSW_U64_LE_SWAP(5UL)
    ) {

        fsw_btrfs_get_root_tree (
            vol, &cdirel->key,
            &default_tree_id
        );
    }

    err = fsw_dnode_create_root_with_tree (
        &vol->g,
        default_tree_id,
	    FSW_U64_LE_SWAP(GRUB_BTRFS_OBJECT_ID_CHUNK),
        &vol->g.root
    );
    if (direl) FreePool(direl);
    return err;
}

fsw_status_t fsw_btrfs_dir_read (
    struct fsw_volume   *volg,
    struct fsw_dnode    *dnog,
    struct fsw_shandle  *shand,
    struct fsw_dnode   **child_dno_out
) {
    int r;
    uint64_t tree;
    uint64_t elemaddr;
    fsw_status_t err;
    fsw_size_t elemsize;
    fsw_size_t allocated;
    struct btrfs_key key_in, key_out;
    struct btrfs_dir_item *direl = NULL;
    struct btrfs_dir_item *cdirel = NULL;
    struct fsw_btrfs_leaf_descriptor desc;
    struct fsw_btrfs_volume *vol = (struct fsw_btrfs_volume *)volg;
    struct fsw_btrfs_dnode  *dno = (struct fsw_btrfs_dnode  *)dnog;

    if (!vol->is_master) {
        FSW_MSG_L03((
            FSW_MSG_STR(
                "FSW_BTRFS: fsw_btrfs_dir_read ... Returned 'FSW_NOT_FOUND' at Main Check 01\n"
            )
        ));

        return FSW_NOT_FOUND;
    }

    key_in.object_id = dnog->dnode_id;
    key_in.type = GRUB_BTRFS_ITEM_TYPE_DIR_ITEM;
    key_in.offset = shand->pos;

    if ((int64_t) key_in.offset == -1LL) {
        FSW_MSG_L03((
            FSW_MSG_STR(
                "FSW_BTRFS: fsw_btrfs_dir_read ... Returned 'FSW_NOT_FOUND' at Main Check 02\n"
            )
        ));

        return FSW_NOT_FOUND;
    }

    tree = dnog->tree_id;
    err = fsw_btrfs_lower_bound (
        vol, &key_in, &key_out, tree,
        &elemaddr, &elemsize, &desc, 0
    );
    if (err) {
        FSW_MSG_L03((
            FSW_MSG_STR(
                "FSW_BTRFS: fsw_btrfs_dir_read ... Returned '%llu' at Main Check 03\n"
            ), (unsigned long long) err
        ));

        return err;
    }

    if (elemsize == 0) {
        FSW_MSG_L03((
            FSW_MSG_STR(
                "FSW_BTRFS: fsw_btrfs_dir_read ... Returned 'FSW_NOT_FOUND' at Main Check 04 (Zero Element Size)\n"
            )
        ));

        return FSW_NOT_FOUND;
    }

    FSW_MSG_L03((
        FSW_MSG_STR(
            "FSW_BTRFS: fsw_btrfs_dir_read ... KEY IN==%llx:%x:%llx OUT==%llx:%x:%llx ELEM==%llu+%llu\n"
        ),
        (unsigned long long) key_in.object_id, (unsigned) key_in.type, (unsigned long long) key_in.offset,
        (unsigned long long) key_out.object_id, (unsigned) key_out.type, (unsigned long long) key_out.offset,
        (unsigned long long) elemaddr, (unsigned long long) elemsize
    ));

    if (key_out.object_id == key_in.object_id &&
        key_out.type == GRUB_BTRFS_ITEM_TYPE_DIR_ITEM
    ) {
        r = 0;
    }
    else {
        r = fsw_btrfs_next_leaf (
            vol, &desc, &elemaddr,
            &elemsize, &key_out
        );
        if (r <= 0) {
            FSW_MSG_L03((
                FSW_MSG_STR(
                    "FSW_BTRFS: fsw_btrfs_dir_read ... Hit 'goto out' at Main Check 05\n"
                )
            ));

            goto out;
        }

        FSW_MSG_L03((
            FSW_MSG_STR(
                "FSW_BTRFS: fsw_btrfs_dir_read ... NEXT OUT==%llx:%x:%llx ELEM==%llu+%llu\n"
            ),
            (unsigned long long) key_out.object_id, (unsigned) key_out.type,
            (unsigned long long) key_out.offset, (unsigned long long) elemaddr, (unsigned long long) elemsize
        ));
    }

    if (key_out.object_id == key_in.object_id &&
        key_out.type == GRUB_BTRFS_ITEM_TYPE_DIR_ITEM &&
        FSW_U64_LE_SWAP(key_out.offset) <= FSW_U64_LE_SWAP(key_in.offset)
    ) {
        r = fsw_btrfs_next_leaf (
            vol, &desc, &elemaddr,
            &elemsize, &key_out
        );
        if (r <= 0) {
            FSW_MSG_L03((
                FSW_MSG_STR(
                    "FSW_BTRFS: fsw_btrfs_dir_read ... Hit 'goto out' at Main Check 06\n"
                )
            ));

            goto out;
        }

        FSW_MSG_L03((
            FSW_MSG_STR(
                "FSW_BTRFS: fsw_btrfs_dir_read ... NEXT_2A OUT==%llx:%x:%llx ELEM==%llu+%llu\n"
            ),
            (unsigned long long) key_out.object_id, (unsigned) key_out.type,
            (unsigned long long) key_out.offset, (unsigned long long) elemaddr, (unsigned long long) elemsize
        ));
    }

    allocated = 0;
    do {

        if (key_out.object_id != key_in.object_id ||
            key_out.type != GRUB_BTRFS_ITEM_TYPE_DIR_ITEM
        ) {
            FSW_MSG_L03((
                FSW_MSG_STR(
                    "FSW_BTRFS: fsw_btrfs_dir_read ... Break at Outer Loop Check 01 (Beyond Dir Item Range)\n"
                )
            ));

            r = 0;
            break;
        }

        if (elemsize > allocated) {
            allocated = elemsize * 2;
            if (direl) FreePool(direl);
            direl = AllocatePool (allocated + 1);
            if (!direl) {
                FSW_MSG_L03((
                    FSW_MSG_STR(
                        "FSW_BTRFS: fsw_btrfs_dir_read ... Break at Outer Loop Check 02 (Out of Memory)\n"
                    )
                ));

                r = -FSW_OUT_OF_MEMORY;
                break;
            }
        }

        err = fsw_btrfs_read_logical (
            vol, elemaddr, direl,
            elemsize, 0, 1
        );
        if (err) {
            FSW_MSG_L03((
                FSW_MSG_STR(
                    "FSW_BTRFS: fsw_btrfs_dir_read ... Break at Outer Loop Check 03 (Failed to Read Leaf Data)\n"
                )
            ));

            r = -err;
            break;
        }

        for (
            cdirel = direl;
            (uint8_t *) cdirel - (uint8_t *) direl < (fsw_ssize_t) elemsize;
            cdirel = (void *) (
                (uint8_t *) cdirel             +
                sizeof (struct btrfs_dir_item) +
                FSW_U16_LE_SWAP(cdirel->n)     +
                FSW_U16_LE_SWAP(cdirel->m)
            )
        ) {

            if (shand->pos != 0 &&
                shand->pos == cdirel->key.offset
            ) {
                FSW_MSG_L03((
                    FSW_MSG_STR(
                        "FSW_BTRFS: fsw_btrfs_dir_read ... Skip Previously Returned Entry at LE Offset %llx\n"
                    ), (unsigned long long) cdirel->key.offset
                ));

                continue;
            }

            struct fsw_string s;
            s.type = FSW_STRING_TYPE_UTF08;
            s.size = s.len = FSW_U16_LE_SWAP(cdirel->n);
            s.data = cdirel->name;

            FSW_MSG_L03((
                FSW_MSG_STR(
                    "FSW_BTRFS: fsw_btrfs_dir_read ... ITEM KEY==%llx:%x:%llx TYPE==%llx, NAME_LEN==%u\n"
                ),
                (unsigned long long) cdirel->key.object_id, (unsigned) cdirel->key.type,
                (unsigned long long) cdirel->key.offset, (unsigned long long) cdirel->type, (unsigned) s.size
            ));

            err = fsw_btrfs_get_sub_dnode (
                vol, dno,
                cdirel, &s,
                child_dno_out
            );
            if (!err) {
                if (direl) {
                    FreePool(direl);
                    direl = NULL;
                }
                fsw_btrfs_free_iterator (&desc);

                shand->pos = key_out.offset;

                FSW_MSG_L03((
                    FSW_MSG_STR(
                        "FSW_BTRFS: fsw_btrfs_dir_read ... Exit Func with FSW_SUCCESS at Offset==%llx\n"
                    ), (unsigned long long) shand->pos
                ));

                return FSW_SUCCESS;
            }
        }

        r = fsw_btrfs_next_leaf (
            vol, &desc, &elemaddr,
            &elemsize, &key_out
        );

        FSW_MSG_L03((
            FSW_MSG_STR(
                "FSW_BTRFS: fsw_btrfs_dir_read ... NEXT_2B OUT==%llx:%x:%llx ELEM==%llu+%llu\n"
            ),
            (unsigned long long) key_out.object_id, (unsigned) key_out.type,
            (unsigned long long) key_out.offset, (unsigned long long) elemaddr, (unsigned long long) elemsize
        ));
    } while (r > 0);

out:
    if (direl) {
        FreePool(direl);
        direl = NULL;
    }

    fsw_btrfs_free_iterator (&desc);

    if (r < 0) {
        r *= -1;

        FSW_MSG_L03((
            FSW_MSG_STR(
                "FSW_BTRFS: fsw_btrfs_dir_read ... Exit Func with Status '%d' Error\n"
            ), r
        ));
    }
    else {
        r = FSW_NOT_FOUND;

        FSW_MSG_L03((
            FSW_MSG_STR(
                "FSW_BTRFS: fsw_btrfs_dir_read ... Exit Func with FSW_NOT_FOUND\n"
            )
        ));
    }

    return r;
}
