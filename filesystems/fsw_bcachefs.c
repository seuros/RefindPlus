// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#include "fsw_bcachefs.h"
#include "fsw_bytes.h"

struct fsw_fstype_table FSW_FSTYPE_TABLE_NAME(bcachefs) = {
    { FSW_STRING_TYPE_ISO88591, 8, 8, "bcachefs" },
    sizeof (struct fsw_bcachefs_volume),
    sizeof (struct fsw_bcachefs_dnode),

    fsw_bcachefs_volume_mount,
    fsw_bcachefs_volume_free,
    fsw_bcachefs_volume_stat,
    fsw_bcachefs_dnode_fill,
    fsw_bcachefs_dnode_free,
    fsw_bcachefs_dnode_stat,
    fsw_bcachefs_get_extent,
    fsw_bcachefs_dir_lookup,
    fsw_bcachefs_dir_read,
    fsw_bcachefs_readlink,
};

struct fsw_fstype_table *fsw_active_fstype_table = &FSW_FSTYPE_TABLE_NAME(bcachefs);
CONST CHAR16            *fsw_active_fstype_name  = L"bcachefs";

static const fsw_u8 bcachefs_magic[16] = {
    0xc6, 0x85, 0x73, 0xf6, 0x66, 0xce, 0x90, 0xa9,
    0xd9, 0x6a, 0x60, 0xcf, 0x80, 0x3d, 0xf7, 0xef
};

static int bcachefs_pos_cmp(struct bcachefs_pos a, struct bcachefs_pos b)
{
    if (a.inode < b.inode) return -1;
    if (a.inode > b.inode) return 1;
    if (a.offset < b.offset) return -1;
    if (a.offset > b.offset) return 1;
    if (a.snapshot < b.snapshot) return -1;
    if (a.snapshot > b.snapshot) return 1;
    return 0;
}

static struct bcachefs_pos bcachefs_pos(fsw_u64 inode, fsw_u64 offset, fsw_u32 snapshot)
{
    struct bcachefs_pos p;

    p.inode = inode;
    p.offset = offset;
    p.snapshot = snapshot;
    return p;
}

static void bcachefs_read_format(const fsw_u8 *p, struct bcachefs_bkey_format *format)
{
    int i;

    format->key_u64s = p[0];
    format->nr_fields = p[1];
    for (i = 0; i < 6; i++) {
        format->bits_per_field[i] = p[2 + i];
        format->field_offset[i] = fsw_le64(p + 8 + i * 8);
    }
}

static void bcachefs_current_format(struct bcachefs_bkey_format *format)
{
    int i;

    format->key_u64s = BCACHEFS_BKEY_U64S;
    format->nr_fields = 6;
    format->bits_per_field[0] = 64;
    format->bits_per_field[1] = 64;
    format->bits_per_field[2] = 32;
    format->bits_per_field[3] = 32;
    format->bits_per_field[4] = 32;
    format->bits_per_field[5] = 64;
    for (i = 0; i < 6; i++) {
        format->field_offset[i] = 0;
    }
}

static fsw_status_t bcachefs_unpack_key(const fsw_u8 *p, const struct bcachefs_bkey_format *format,
                                        const fsw_u8 *end, struct bcachefs_key *key,
                                        fsw_u32 *key_bytes)
{
    fsw_u64 words[8];
    fsw_u64 vals[6];
    fsw_u64 w;
    fsw_u32 i;
    fsw_u32 bits_left;
    int word;

    if (p + 8 > end) return FSW_VOLUME_CORRUPTED;

    key->u64s = p[0];
    key->format = p[1] & 0x7f;
    key->type = p[2];
    if (key->u64s == 0 || p + key->u64s * 8 > end) {
        return FSW_VOLUME_CORRUPTED;
    }

    if (key->format == BCACHEFS_KEY_FORMAT_CURRENT) {
        if (key->u64s < BCACHEFS_BKEY_U64S) return FSW_VOLUME_CORRUPTED;

        key->size = fsw_le32(p + 16);
        key->p.snapshot = fsw_le32(p + 20);
        key->p.offset = fsw_le64(p + 24);
        key->p.inode = fsw_le64(p + 32);
        *key_bytes = BCACHEFS_BKEY_U64S * 8;

        return FSW_SUCCESS;
    }

    if (key->format != BCACHEFS_KEY_FORMAT_LOCAL ||
        format->key_u64s == 0 ||
        format->key_u64s > 8 ||
        format->nr_fields != 6 ||
        key->u64s < format->key_u64s) {
        return FSW_VOLUME_CORRUPTED;
    }

    for (i = 0; i < format->key_u64s; i++) {
        words[i] = fsw_le64(p + i * 8);
    }

    word = format->key_u64s - 1;
    bits_left = 64;
    w = words[word];

    for (i = 0; i < 6; i++) {
        fsw_u32 bits = format->bits_per_field[i];
        fsw_u64 v = 0;

        if (bits > 64) return FSW_VOLUME_CORRUPTED;

        if (bits >= bits_left) {
            if (bits != 0) {
                v = w >> (64 - bits);
            }
            bits -= bits_left;
            word--;
            w = (word >= 0) ? words[word] : 0;
            bits_left = 64;
        }

        if (bits != 0) {
            v |= (w >> 1) >> (63 - bits);
            w <<= bits;
            bits_left -= bits;
        }

        vals[i] = v + format->field_offset[i];
    }

    key->p.inode = vals[0];
    key->p.offset = vals[1];
    key->p.snapshot = (fsw_u32)vals[2];
    key->size = (fsw_u32)vals[3];
    *key_bytes = format->key_u64s * 8;

    return FSW_SUCCESS;
}

static int bcachefs_key_is_deleted(const struct bcachefs_key *key)
{
    return key->type == BCACHEFS_KEY_TYPE_DELETED ||
           key->type == BCACHEFS_KEY_TYPE_WHITEOUT ||
           key->type == BCACHEFS_KEY_TYPE_HASH_WHITEOUT;
}

static void bcachefs_copy_key(struct bcachefs_key *dst, const struct bcachefs_key *src)
{
    dst->u64s = src->u64s;
    dst->format = src->format;
    dst->type = src->type;
    dst->size = src->size;
    dst->p.inode = src->p.inode;
    dst->p.offset = src->p.offset;
    dst->p.snapshot = src->p.snapshot;
}

static void bcachefs_copy_result(struct bcachefs_lookup_result *dst,
                                 const struct bcachefs_lookup_result *src)
{
    bcachefs_copy_key(&dst->key, &src->key);
    dst->value_bytes = src->value_bytes;
    if (src->value_bytes > 0) {
        FSW_DO_MEMCPY(dst->value, src->value, src->value_bytes);
    }
}

static fsw_status_t bcachefs_copy_lookup(const struct bcachefs_key *key, const fsw_u8 *value,
                                         fsw_u32 value_bytes,
                                         struct bcachefs_lookup_result *out)
{
    bcachefs_copy_key(&out->key, key);
    out->value_bytes = value_bytes;

    if (value_bytes > BCACHEFS_MAX_VALUE_BYTES) {
        return FSW_UNSUPPORTED;
    }
    if (value_bytes > 0) {
        FSW_DO_MEMCPY(out->value, value, value_bytes);
    }

    return FSW_SUCCESS;
}

static fsw_u32 bcachefs_round_up(fsw_u32 v, fsw_u32 block)
{
    return (v + block - 1) & ~(block - 1);
}

static fsw_status_t bcachefs_read_sectors(struct fsw_bcachefs_volume *vol, fsw_u64 sector,
                                          fsw_u32 sectors, fsw_u8 **buffer_out)
{
    fsw_status_t status;
    fsw_u8 *buffer;
    fsw_u8 *block;
    fsw_u32 i;

    if (sectors == 0 || sectors > BCACHEFS_MAX_NODE_SECTORS) {
        return FSW_VOLUME_CORRUPTED;
    }

    status = FSW_DO_ALLOC(sectors * BCACHEFS_SECTOR_SIZE, &buffer);
    if (status) return status;

    for (i = 0; i < sectors; i++) {
        status = fsw_block_get(vol, sector + i, 2, (void **)&block);
        if (status) {
            FSW_DO_FREE(buffer);
            return status;
        }

        FSW_DO_MEMCPY(buffer + i * BCACHEFS_SECTOR_SIZE, block, BCACHEFS_SECTOR_SIZE);
        fsw_block_release(vol, sector + i, block);
    }

    *buffer_out = buffer;
    return FSW_SUCCESS;
}

static fsw_status_t bcachefs_parse_btree_ptr(const struct bcachefs_lookup_result *in,
                                             struct bcachefs_btree_ptr *ptr)
{
    fsw_u64 raw;

    if (in->key.type == BCACHEFS_KEY_TYPE_BTREE_PTR_V2) {
        if (in->value_bytes < 48) return FSW_VOLUME_CORRUPTED;

        ptr->sectors_written = fsw_le16(in->value + 16);
        raw = fsw_le64(in->value + 40);
    }
    else if (in->key.type == BCACHEFS_KEY_TYPE_BTREE_PTR) {
        if (in->value_bytes < 8) return FSW_VOLUME_CORRUPTED;

        ptr->sectors_written = 0;
        raw = fsw_le64(in->value);
    }
    else {
        return FSW_VOLUME_CORRUPTED;
    }

    if ((raw & 1) == 0 || ((raw >> 48) & 0xff) != 0) {
        return FSW_UNSUPPORTED;
    }

    ptr->offset = (raw >> 4) & 0x0fffffffffffULL;
    return FSW_SUCCESS;
}

static fsw_status_t bcachefs_node_lower_bound(struct fsw_bcachefs_volume *vol,
                                              struct bcachefs_btree_ptr ptr,
                                              struct bcachefs_pos search,
                                              struct bcachefs_lookup_result *out)
{
    fsw_status_t status;
    fsw_u8 *node;
    fsw_u8 *end;
    fsw_u8 *p;
    struct bcachefs_bkey_format format;
    struct bcachefs_lookup_result best;
    fsw_u32 sectors;
    fsw_u32 bytes;
    fsw_u32 written;
    fsw_u64 seq;
    int found;
    int best_valid;

    sectors = ptr.sectors_written ? ptr.sectors_written : vol->btree_node_sectors;
    if (sectors == 0 || sectors > vol->btree_node_sectors) {
        return FSW_VOLUME_CORRUPTED;
    }

    status = bcachefs_read_sectors(vol, ptr.offset, sectors, &node);
    if (status) return status;

    bytes = sectors * BCACHEFS_SECTOR_SIZE;
    if (bytes < 160) {
        FSW_DO_FREE(node);
        return FSW_VOLUME_CORRUPTED;
    }

    bcachefs_read_format(node + 80, &format);
    if (format.key_u64s == 0 || format.key_u64s > 8) {
        FSW_DO_FREE(node);
        return FSW_VOLUME_CORRUPTED;
    }

    seq = fsw_le64(node + 136);
    found = 0;
    best_valid = 0;
    written = 0;

    while (written < sectors) {
        fsw_u32 hbase;
        fsw_u32 bset;
        fsw_u32 data;
        fsw_u32 u64s;
        fsw_u32 bset_bytes;
        fsw_u32 bset_sectors;

        hbase = written * BCACHEFS_SECTOR_SIZE;
        if (written == 0) {
            bset = 136;
            data = 160;
        }
        else {
            if (hbase + 40 > bytes) break;
            bset = hbase + 16;
            data = hbase + 40;
            if (fsw_le64(node + bset) != seq) break;
        }

        if (bset + 24 > bytes) break;
        u64s = fsw_le16(node + bset + 22);
        if (data + u64s * 8 > bytes) {
            FSW_DO_FREE(node);
            return FSW_VOLUME_CORRUPTED;
        }

        p = node + data;
        end = p + u64s * 8;
        while (p < end) {
            struct bcachefs_key key;
            fsw_u32 key_bytes;
            fsw_u32 value_bytes;
            fsw_u8 *value;
            int cmp_search;
            int replace;

            status = bcachefs_unpack_key(p, &format, end, &key, &key_bytes);
            if (status) {
                FSW_DO_FREE(node);
                return status;
            }
            if (key.u64s * 8 < key_bytes) {
                FSW_DO_FREE(node);
                return FSW_VOLUME_CORRUPTED;
            }

            cmp_search = bcachefs_pos_cmp(key.p, search);
            if (cmp_search >= 0) {
                replace = 0;
                if (!found) {
                    replace = 1;
                }
                else {
                    int cmp_best = bcachefs_pos_cmp(key.p, best.key.p);
                    if (cmp_best < 0 || cmp_best == 0) replace = 1;
                }

                if (replace) {
                    found = 1;
                    best_valid = !bcachefs_key_is_deleted(&key);
                    value = p + key_bytes;
                    value_bytes = key.u64s * 8 - key_bytes;
                    if (best_valid) {
                        status = bcachefs_copy_lookup(&key, value, value_bytes, &best);
                        if (status) {
                            FSW_DO_FREE(node);
                            return status;
                        }
                    }
                    else {
                        bcachefs_copy_key(&best.key, &key);
                        best.value_bytes = 0;
                    }
                }
            }

            p += key.u64s * 8;
        }

        bset_bytes = (data - hbase) + u64s * 8;
        bset_sectors = bcachefs_round_up(bset_bytes, vol->block_bytes) / BCACHEFS_SECTOR_SIZE;
        if (bset_sectors == 0) break;
        written += bset_sectors;
    }

    FSW_DO_FREE(node);

    if (!found || !best_valid) return FSW_NOT_FOUND;

    bcachefs_copy_result(out, &best);
    return FSW_SUCCESS;
}

static fsw_status_t bcachefs_btree_lookup(struct fsw_bcachefs_volume *vol, fsw_u8 btree_id,
                                          struct bcachefs_pos search,
                                          struct bcachefs_lookup_result *out)
{
    fsw_status_t status;
    struct bcachefs_btree_ptr ptr;
    fsw_u8 level;

    if (btree_id >= BCACHEFS_BTREE_ID_NR || !vol->roots[btree_id].alive) {
        return FSW_NOT_FOUND;
    }

    ptr.offset = vol->roots[btree_id].ptr.offset;
    ptr.sectors_written = vol->roots[btree_id].ptr.sectors_written;
    level = vol->roots[btree_id].level;

    while (1) {
        status = bcachefs_node_lower_bound(vol, ptr, search, out);
        if (status) return status;

        if (level == 0) return FSW_SUCCESS;

        status = bcachefs_parse_btree_ptr(out, &ptr);
        if (status) return status;
        level--;
    }
}

static fsw_status_t bcachefs_parse_extent_ptr(const fsw_u8 *value, fsw_u32 value_bytes,
                                              fsw_u64 *ptr_offset)
{
    fsw_u32 pos;
    fsw_u64 raw;

    pos = 0;
    while (pos + 8 <= value_bytes) {
        raw = fsw_le64(value + pos);

        if (raw & 1) {
            if (((raw >> 1) & 1) || ((raw >> 3) & 1) || ((raw >> 48) & 0xff) != 0) {
                return FSW_UNSUPPORTED;
            }

            *ptr_offset = (raw >> 4) & 0x0fffffffffffULL;
            return FSW_SUCCESS;
        }
        else if (raw & 2) {
            if (((raw >> 28) & 0xf) != 0) return FSW_UNSUPPORTED;
            pos += 8;
        }
        else if (raw & 4) {
            if (((raw >> 44) & 0xf) != 0) return FSW_UNSUPPORTED;
            pos += 16;
        }
        else if (raw & 8) {
            if (((raw >> 60) & 0xf) != 0) return FSW_UNSUPPORTED;
            pos += 24;
        }
        else {
            return FSW_UNSUPPORTED;
        }
    }

    return FSW_NOT_FOUND;
}

static fsw_status_t bcachefs_map_extent_value(struct bcachefs_lookup_result *lookup,
                                              fsw_u64 logical_sector,
                                              fsw_u64 *phys_sector,
                                              fsw_u32 *sector_count)
{
    fsw_status_t status;
    fsw_u64 start;
    fsw_u64 ptr_offset;

    if (lookup->key.size == 0 || logical_sector >= lookup->key.p.offset) {
        return FSW_NOT_FOUND;
    }

    start = lookup->key.p.offset - lookup->key.size;
    if (logical_sector < start) {
        return FSW_NOT_FOUND;
    }

    status = bcachefs_parse_extent_ptr(lookup->value, lookup->value_bytes, &ptr_offset);
    if (status) return status;

    *phys_sector = ptr_offset + (logical_sector - start);
    *sector_count = (fsw_u32)(lookup->key.p.offset - logical_sector);
    return FSW_SUCCESS;
}

static fsw_status_t bcachefs_map_reflink(struct fsw_bcachefs_volume *vol,
                                         struct bcachefs_lookup_result *lookup,
                                         fsw_u64 logical_sector,
                                         fsw_u64 *phys_sector,
                                         fsw_u32 *sector_count)
{
    fsw_status_t status;
    struct bcachefs_lookup_result indirect;
    fsw_u64 start;
    fsw_u64 idx_flags;
    fsw_u64 reflink_sector;
    fsw_u64 indirect_start;
    fsw_u64 ptr_offset;
    fsw_u32 direct_count;
    fsw_u32 indirect_count;

    if (lookup->value_bytes < 16 || lookup->key.size == 0 || logical_sector >= lookup->key.p.offset) {
        return FSW_VOLUME_CORRUPTED;
    }

    start = lookup->key.p.offset - lookup->key.size;
    if (logical_sector < start) return FSW_NOT_FOUND;

    idx_flags = fsw_le64(lookup->value);
    if ((idx_flags >> 56) & 1) return FSW_UNSUPPORTED;

    reflink_sector = (idx_flags & 0x00ffffffffffffffULL) + (logical_sector - start);
    status = bcachefs_btree_lookup(vol, BCACHEFS_BTREE_REFLINK,
                                   bcachefs_pos(0, reflink_sector + 1, 0),
                                   &indirect);
    if (status) return status;

    if (indirect.key.type != BCACHEFS_KEY_TYPE_REFLINK_V) {
        return FSW_UNSUPPORTED;
    }
    if (indirect.value_bytes < 8) return FSW_VOLUME_CORRUPTED;

    indirect_start = indirect.key.p.offset - indirect.key.size;
    if (reflink_sector < indirect_start || reflink_sector >= indirect.key.p.offset) {
        return FSW_NOT_FOUND;
    }

    status = bcachefs_parse_extent_ptr(indirect.value + 8, indirect.value_bytes - 8, &ptr_offset);
    if (status) return status;

    *phys_sector = ptr_offset + (reflink_sector - indirect_start);
    direct_count = (fsw_u32)(lookup->key.p.offset - logical_sector);
    indirect_count = (fsw_u32)(indirect.key.p.offset - reflink_sector);
    *sector_count = (direct_count < indirect_count) ? direct_count : indirect_count;

    return FSW_SUCCESS;
}

static int bcachefs_dirent_type(fsw_u8 dtype)
{
    switch (dtype & 0x1f) {
        case 4:  return FSW_DNODE_TYPE_DIR;
        case 8:  return FSW_DNODE_TYPE_FILE;
        case 10: return FSW_DNODE_TYPE_SYMLINK;
        default: return FSW_DNODE_TYPE_UNKNOWN;
    }
}

static fsw_status_t bcachefs_dirent_to_dnode(struct fsw_bcachefs_dnode *dno,
                                             struct bcachefs_lookup_result *lookup,
                                             struct fsw_bcachefs_dnode **child_dno)
{
    fsw_u64 ino;
    fsw_u32 name_len;
    struct fsw_string name;

    if (lookup->key.type != BCACHEFS_KEY_TYPE_DIRENT || lookup->value_bytes < 10) {
        return FSW_VOLUME_CORRUPTED;
    }

    ino = fsw_le64(lookup->value);
    name_len = lookup->value_bytes - 9;
    while (name_len > 0 && lookup->value[9 + name_len - 1] == 0) {
        name_len--;
    }
    if (name_len == 0) return FSW_VOLUME_CORRUPTED;

    name.type = FSW_STRING_TYPE_ISO88591;
    name.len = name_len;
    name.size = name_len;
    name.data = lookup->value + 9;

    return fsw_dnode_create(dno, ino,
                            bcachefs_dirent_type(lookup->value[8]),
                            &name, child_dno);
}

static fsw_status_t bcachefs_next_dirent(struct fsw_bcachefs_volume *vol,
                                         fsw_u64 dir_ino,
                                         fsw_u64 start_offset,
                                         struct bcachefs_lookup_result *lookup)
{
    fsw_status_t status;

    status = bcachefs_btree_lookup(vol, BCACHEFS_BTREE_DIRENTS,
                                   bcachefs_pos(dir_ino, start_offset, 0),
                                   lookup);
    if (status) return status;

    if (lookup->key.p.inode != dir_ino || lookup->key.type != BCACHEFS_KEY_TYPE_DIRENT) {
        return FSW_NOT_FOUND;
    }

    return FSW_SUCCESS;
}

fsw_status_t fsw_bcachefs_volume_mount(struct fsw_bcachefs_volume *vol)
{
    fsw_status_t status;
    fsw_u8 *sb;
    fsw_u32 sb_u64s;
    fsw_u32 sb_bytes;
    fsw_u64 flags0;
    fsw_u64 flags1;
    fsw_u64 flags3;
    fsw_u32 pos;
    struct fsw_string label;
    struct bcachefs_bkey_format current_format;
    int i;

    fsw_set_blocksize(vol, BCACHEFS_SECTOR_SIZE, BCACHEFS_SECTOR_SIZE);

    status = bcachefs_read_sectors(vol, BCACHEFS_SUPER_SECTOR, BCACHEFS_SUPER_READ_SECTORS, &sb);
    if (status) return status;

    if (!FSW_DO_MEMEQ(sb + 24, bcachefs_magic, sizeof (bcachefs_magic))) {
        FSW_DO_FREE(sb);
        return FSW_UNSUPPORTED;
    }

    sb_u64s = fsw_le32(sb + 124);
    sb_bytes = sb_u64s * 8;
    if (sb_bytes < BCACHEFS_SB_FIELD_OFFSET || sb_bytes > BCACHEFS_SUPER_READ_SECTORS * BCACHEFS_SECTOR_SIZE) {
        FSW_DO_FREE(sb);
        return FSW_VOLUME_CORRUPTED;
    }

    flags0 = fsw_le64(sb + 144);
    flags1 = fsw_le64(sb + 152);
    flags3 = fsw_le64(sb + 168);

    if (((flags0 >> 1) & 1) == 0 || sb[123] != 1 || ((flags3 >> 63) & 1)) {
        FSW_DO_FREE(sb);
        return FSW_UNSUPPORTED;
    }
    if (((flags1 >> 10) & 0xf) != 0 || (flags3 & 0xffff) != 0) {
        FSW_DO_FREE(sb);
        return FSW_UNSUPPORTED;
    }

    vol->block_size_sectors = fsw_le16(sb + 120);
    vol->block_bytes = vol->block_size_sectors * BCACHEFS_SECTOR_SIZE;
    vol->btree_node_sectors = (fsw_u32)((flags0 >> 12) & 0xffff);
    if (vol->block_size_sectors == 0 ||
        vol->btree_node_sectors == 0 ||
        vol->btree_node_sectors > BCACHEFS_MAX_NODE_SECTORS) {
        FSW_DO_FREE(sb);
        return FSW_UNSUPPORTED;
    }

    for (i = 0; i < BCACHEFS_BTREE_ID_NR; i++) {
        vol->roots[i].alive = 0;
    }
    bcachefs_current_format(&current_format);

    pos = BCACHEFS_SB_FIELD_OFFSET;
    while (pos + 8 <= sb_bytes) {
        fsw_u32 field_u64s = fsw_le32(sb + pos);
        fsw_u32 field_type = fsw_le32(sb + pos + 4);
        fsw_u32 field_end;

        if (field_u64s == 0) break;
        field_end = pos + field_u64s * 8;
        if (field_end > sb_bytes || field_end <= pos) {
            FSW_DO_FREE(sb);
            return FSW_VOLUME_CORRUPTED;
        }

        if (field_type == BCACHEFS_SB_FIELD_CLEAN) {
            fsw_u32 epos = pos + 24;

            while (epos + 8 <= field_end) {
                fsw_u16 entry_u64s = fsw_le16(sb + epos);
                fsw_u8 btree_id = sb[epos + 2];
                fsw_u8 level = sb[epos + 3];
                fsw_u8 type = sb[epos + 4];
                fsw_u32 next = epos + 8 + entry_u64s * 8;

                if (entry_u64s == 0) break;
                if (next > field_end) {
                    FSW_DO_FREE(sb);
                    return FSW_VOLUME_CORRUPTED;
                }

                if (type == BCACHEFS_JSET_ENTRY_BTREE_ROOT &&
                    btree_id < BCACHEFS_BTREE_ID_NR) {
                    struct bcachefs_lookup_result root;
                    fsw_u32 key_bytes;

                    status = bcachefs_unpack_key(sb + epos + 8, &current_format,
                                                 sb + next, &root.key, &key_bytes);
                    if (status) {
                        FSW_DO_FREE(sb);
                        return status;
                    }
                    if (root.key.u64s != entry_u64s || root.key.u64s * 8 < key_bytes ||
                        root.key.u64s * 8 - key_bytes > BCACHEFS_MAX_VALUE_BYTES) {
                        FSW_DO_FREE(sb);
                        return FSW_VOLUME_CORRUPTED;
                    }

                    root.value_bytes = root.key.u64s * 8 - key_bytes;
                    FSW_DO_MEMCPY(root.value, sb + epos + 8 + key_bytes, root.value_bytes);
                    status = bcachefs_parse_btree_ptr(&root, &vol->roots[btree_id].ptr);
                    if (status) {
                        FSW_DO_FREE(sb);
                        return status;
                    }

                    vol->roots[btree_id].level = level;
                    vol->roots[btree_id].alive = 1;
                }

                epos = next;
            }
        }

        pos = field_end;
    }

    label.type = FSW_STRING_TYPE_ISO88591;
    label.data = sb + 72;
    label.len = 0;
    while (label.len < 32 && ((fsw_u8 *)label.data)[label.len] != 0) {
        label.len++;
    }
    label.size = label.len;
    status = fsw_strdup_coerce(&vol->g.label, vol->g.host_string_type, &label);
    FSW_DO_FREE(sb);
    if (status) return status;

    if (!vol->roots[BCACHEFS_BTREE_EXTENTS].alive ||
        !vol->roots[BCACHEFS_BTREE_INODES].alive ||
        !vol->roots[BCACHEFS_BTREE_DIRENTS].alive) {
        return FSW_UNSUPPORTED;
    }

    return fsw_dnode_create_root(vol, BCACHEFS_ROOT_INO, &vol->g.root);
}

void fsw_bcachefs_volume_free(struct fsw_bcachefs_volume *vol)
{
    (void)vol;
}

fsw_status_t fsw_bcachefs_volume_stat(struct fsw_bcachefs_volume *vol, struct fsw_volume_stat *sb)
{
    (void)vol;
    sb->total_bytes = 0;
    sb->free_bytes = 0;
    return FSW_SUCCESS;
}

fsw_status_t fsw_bcachefs_dnode_fill(struct fsw_bcachefs_volume *vol, struct fsw_bcachefs_dnode *dno)
{
    fsw_status_t status;
    struct bcachefs_lookup_result lookup;
    fsw_u64 flags;
    fsw_u16 mode;

    if (dno->g.type != FSW_DNODE_TYPE_UNKNOWN && dno->g.size != 0) {
        return FSW_SUCCESS;
    }

    status = bcachefs_btree_lookup(vol, BCACHEFS_BTREE_INODES,
                                   bcachefs_pos(0, dno->g.dnode_id, 0),
                                   &lookup);
    if (status) return status;

    if (lookup.key.p.inode != 0 || lookup.key.p.offset != dno->g.dnode_id) {
        return FSW_NOT_FOUND;
    }

    if (lookup.key.type == BCACHEFS_KEY_TYPE_INODE_V3) {
        if (lookup.value_bytes < 48) return FSW_VOLUME_CORRUPTED;

        flags = fsw_le64(lookup.value + 16);
        mode = (fsw_u16)((flags >> 36) & 0xffff);
        dno->sectors = fsw_le64(lookup.value + 24);
        dno->g.size = fsw_le64(lookup.value + 32);
    }
    else if (lookup.key.type == BCACHEFS_KEY_TYPE_INODE_V2) {
        if (lookup.value_bytes < 26) return FSW_VOLUME_CORRUPTED;

        flags = fsw_le64(lookup.value + 16);
        mode = fsw_le16(lookup.value + 24);
        dno->sectors = 0;
        dno->g.size = 0;
    }
    else if (lookup.key.type == BCACHEFS_KEY_TYPE_INODE) {
        if (lookup.value_bytes < 14) return FSW_VOLUME_CORRUPTED;

        mode = fsw_le16(lookup.value + 12);
        dno->sectors = 0;
        dno->g.size = 0;
    }
    else {
        return FSW_VOLUME_CORRUPTED;
    }

    dno->mode = mode;
    switch (mode & 0170000) {
        case 0040000:
            dno->g.type = FSW_DNODE_TYPE_DIR;
            break;
        case 0100000:
            dno->g.type = FSW_DNODE_TYPE_FILE;
            break;
        case 0120000:
            dno->g.type = FSW_DNODE_TYPE_SYMLINK;
            break;
        default:
            dno->g.type = FSW_DNODE_TYPE_SPECIAL;
            break;
    }

    return FSW_SUCCESS;
}

void fsw_bcachefs_dnode_free(struct fsw_bcachefs_volume *vol, struct fsw_bcachefs_dnode *dno)
{
    (void)vol;
    (void)dno;
}

fsw_status_t fsw_bcachefs_dnode_stat(struct fsw_bcachefs_volume *vol, struct fsw_bcachefs_dnode *dno,
                                     struct fsw_dnode_stat *sb)
{
    (void)vol;
    sb->used_bytes = dno->sectors * BCACHEFS_SECTOR_SIZE;
    return FSW_SUCCESS;
}

fsw_status_t fsw_bcachefs_get_extent(struct fsw_bcachefs_volume *vol, struct fsw_bcachefs_dnode *dno,
                                     struct fsw_extent *extent)
{
    fsw_status_t status;
    struct bcachefs_lookup_result lookup;
    fsw_u64 logical_sector;
    fsw_u64 phys_sector;
    fsw_u32 sector_count;

    if (dno->g.type != FSW_DNODE_TYPE_FILE) return FSW_UNSUPPORTED;

    logical_sector = extent->log_start;
    status = bcachefs_btree_lookup(vol, BCACHEFS_BTREE_EXTENTS,
                                   bcachefs_pos(dno->g.dnode_id, logical_sector + 1, 0),
                                   &lookup);
    if (status) return status;

    if (lookup.key.p.inode != dno->g.dnode_id) return FSW_NOT_FOUND;

    if (lookup.key.type == BCACHEFS_KEY_TYPE_EXTENT) {
        status = bcachefs_map_extent_value(&lookup, logical_sector, &phys_sector, &sector_count);
    }
    else if (lookup.key.type == BCACHEFS_KEY_TYPE_REFLINK_P) {
        status = bcachefs_map_reflink(vol, &lookup, logical_sector, &phys_sector, &sector_count);
    }
    else {
        return FSW_UNSUPPORTED;
    }
    if (status) return status;

    extent->type = FSW_EXTENT_TYPE_PHYSBLOCK;
    extent->phys_start = phys_sector;
    extent->log_count = sector_count;

    return FSW_SUCCESS;
}

fsw_status_t fsw_bcachefs_dir_lookup(struct fsw_bcachefs_volume *vol, struct fsw_bcachefs_dnode *dno,
                                     struct fsw_string *lookup_name, struct fsw_bcachefs_dnode **child_dno)
{
    fsw_status_t status;
    struct bcachefs_lookup_result lookup;
    fsw_u64 offset;

    offset = 0;
    while (1) {
        struct fsw_string entry_name;
        fsw_u32 name_len;

        status = bcachefs_next_dirent(vol, dno->g.dnode_id, offset, &lookup);
        if (status) return status;

        name_len = lookup.value_bytes - 9;
        while (name_len > 0 && lookup.value[9 + name_len - 1] == 0) {
            name_len--;
        }

        entry_name.type = FSW_STRING_TYPE_ISO88591;
        entry_name.len = name_len;
        entry_name.size = name_len;
        entry_name.data = lookup.value + 9;

        if (fsw_streq(lookup_name, &entry_name)) {
            return bcachefs_dirent_to_dnode(dno, &lookup, child_dno);
        }

        if (lookup.key.p.offset == 0xffffffffffffffffULL) return FSW_NOT_FOUND;
        offset = lookup.key.p.offset + 1;
    }
}

fsw_status_t fsw_bcachefs_dir_read(struct fsw_bcachefs_volume *vol, struct fsw_bcachefs_dnode *dno,
                                   struct fsw_shandle *shand, struct fsw_bcachefs_dnode **child_dno)
{
    fsw_status_t status;
    struct bcachefs_lookup_result lookup;

    status = bcachefs_next_dirent(vol, dno->g.dnode_id, shand->pos, &lookup);
    if (status) return status;

    if (lookup.key.p.offset == 0xffffffffffffffffULL) return FSW_NOT_FOUND;
    shand->pos = lookup.key.p.offset + 1;

    return bcachefs_dirent_to_dnode(dno, &lookup, child_dno);
}

fsw_status_t fsw_bcachefs_readlink(struct fsw_bcachefs_volume *vol, struct fsw_bcachefs_dnode *dno,
                                   struct fsw_string *link)
{
    (void)vol;
    (void)dno;
    (void)link;
    return FSW_UNSUPPORTED;
}
