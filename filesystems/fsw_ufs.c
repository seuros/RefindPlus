// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#include "fsw_ufs.h"
#include "fsw_bytes.h"

#define UFS_SB_OFF_SBLKNO              8U
#define UFS_SB_OFF_IBLKNO             16U
#define UFS_SB_OFF_CGOFFSET           24U
#define UFS_SB_OFF_CGMASK             28U
#define UFS_SB_OFF_UFS1_SIZE          36U
#define UFS_SB_OFF_UFS1_DSIZE         40U
#define UFS_SB_OFF_NCG                44U
#define UFS_SB_OFF_BSIZE              48U
#define UFS_SB_OFF_FSIZE              52U
#define UFS_SB_OFF_FRAG               56U
#define UFS_SB_OFF_INOPB             120U
#define UFS_SB_OFF_IPG               184U
#define UFS_SB_OFF_FPG               188U
#define UFS_SB_OFF_UFS1_CSTOTAL      192U
#define UFS_SB_OFF_FSMNT             212U
#define UFS_SB_OFF_VOLNAME           680U
#define UFS_SB_OFF_UFS2_CSTOTAL     1008U
#define UFS_SB_OFF_UFS2_SIZE        1080U
#define UFS_SB_OFF_UFS2_DSIZE       1088U
#define UFS_SB_OFF_MAGIC            1372U

#define UFS_CSUM_OFF_NBFREE            4U
#define UFS_CSUM_OFF_NFFREE           12U
#define UFS_CSUM_TOTAL_OFF_NBFREE      8U
#define UFS_CSUM_TOTAL_OFF_NFFREE     24U

#define UFS1_INODE_SIZE              128U
#define UFS1_INODE_OFF_MODE           0U
#define UFS1_INODE_OFF_SIZE           8U
#define UFS1_INODE_OFF_ATIME         16U
#define UFS1_INODE_OFF_MTIME         24U
#define UFS1_INODE_OFF_CTIME         32U
#define UFS1_INODE_OFF_DB            40U
#define UFS1_INODE_OFF_IB            88U
#define UFS1_INODE_OFF_BLOCKS       104U

#define UFS2_INODE_SIZE              256U
#define UFS2_INODE_OFF_MODE           0U
#define UFS2_INODE_OFF_SIZE          16U
#define UFS2_INODE_OFF_BLOCKS        24U
#define UFS2_INODE_OFF_ATIME         32U
#define UFS2_INODE_OFF_MTIME         40U
#define UFS2_INODE_OFF_CTIME         48U
#define UFS2_INODE_OFF_DB           112U
#define UFS2_INODE_OFF_IB           208U

static int fsw_ufs_is_power_of_two (
    fsw_u32 value
) {
    return value != 0 && (value & (value - 1)) == 0;
}

static fsw_u64 fsw_ufs_div_u64_u32 (
    fsw_u64 value,
    fsw_u32 divisor
) {
    return FSW_U64_DIV(value, divisor);
}

static fsw_u32 fsw_ufs_mod_u64_u32 (
    fsw_u64 value,
    fsw_u32 divisor
) {
    fsw_u64 quotient;

    quotient = fsw_ufs_div_u64_u32(value, divisor);
    return (fsw_u32) (value - quotient * divisor);
}

static fsw_u32 fsw_ufs_inode_mode (
    struct fsw_ufs_volume *vol,
    fsw_u8                *raw
) {
    (void) vol;
    return fsw_le16_at(raw, UFS1_INODE_OFF_MODE);
}

static fsw_u64 fsw_ufs_inode_size (
    struct fsw_ufs_volume *vol,
    fsw_u8                *raw
) {
    if (vol->variant == FSW_UFS_VARIANT_UFS2)
        return fsw_le64_at(raw, UFS2_INODE_OFF_SIZE);

    return fsw_le64_at(raw, UFS1_INODE_OFF_SIZE);
}

static fsw_u64 fsw_ufs_inode_blocks (
    struct fsw_ufs_volume *vol,
    fsw_u8                *raw
) {
    if (vol->variant == FSW_UFS_VARIANT_UFS2)
        return fsw_le64_at(raw, UFS2_INODE_OFF_BLOCKS);

    return fsw_le32_at(raw, UFS1_INODE_OFF_BLOCKS);
}

static fsw_u32 fsw_ufs_inode_time (
    struct fsw_ufs_volume *vol,
    fsw_u8                *raw,
    fsw_u32                ufs1_off,
    fsw_u32                ufs2_off
) {
    if (vol->variant == FSW_UFS_VARIANT_UFS2)
        return (fsw_u32) fsw_le64_at(raw, ufs2_off);

    return fsw_le32_at(raw, ufs1_off);
}

static fsw_u32 fsw_ufs_inode_db_offset (
    struct fsw_ufs_volume *vol
) {
    return (vol->variant == FSW_UFS_VARIANT_UFS2)
        ? UFS2_INODE_OFF_DB
        : UFS1_INODE_OFF_DB;
}

static fsw_u32 fsw_ufs_inode_ib_offset (
    struct fsw_ufs_volume *vol
) {
    return (vol->variant == FSW_UFS_VARIANT_UFS2)
        ? UFS2_INODE_OFF_IB
        : UFS1_INODE_OFF_IB;
}

static fsw_u64 fsw_ufs_inode_pointer (
    struct fsw_ufs_volume *vol,
    fsw_u8                *raw,
    fsw_u32                base_off,
    fsw_u32                index
) {
    fsw_u32 off;

    off = base_off + index * vol->pointer_size;
    if (vol->pointer_size == 8)
        return fsw_le64_at(raw, off);

    return fsw_le32_at(raw, off);
}

static fsw_u64 fsw_ufs_inode_direct (
    struct fsw_ufs_volume *vol,
    fsw_u8                *raw,
    fsw_u32                index
) {
    return fsw_ufs_inode_pointer(
        vol, raw, fsw_ufs_inode_db_offset(vol), index
    );
}

static fsw_u64 fsw_ufs_inode_indirect (
    struct fsw_ufs_volume *vol,
    fsw_u8                *raw,
    fsw_u32                index
) {
    return fsw_ufs_inode_pointer(
        vol, raw, fsw_ufs_inode_ib_offset(vol), index
    );
}

static fsw_u64 fsw_ufs_sb_size (
    struct fsw_ufs_volume *vol
) {
    if (vol->variant == FSW_UFS_VARIANT_UFS2)
        return fsw_le64_at(vol->sb_raw, UFS_SB_OFF_UFS2_SIZE);

    return fsw_le32_at(vol->sb_raw, UFS_SB_OFF_UFS1_SIZE);
}

static fsw_u64 fsw_ufs_sb_dsize (
    struct fsw_ufs_volume *vol
) {
    if (vol->variant == FSW_UFS_VARIANT_UFS2)
        return fsw_le64_at(vol->sb_raw, UFS_SB_OFF_UFS2_DSIZE);

    return fsw_le32_at(vol->sb_raw, UFS_SB_OFF_UFS1_DSIZE);
}

static fsw_u64 fsw_ufs_sb_free_frags (
    struct fsw_ufs_volume *vol
) {
    fsw_u64 nbfree;
    fsw_u64 nffree;

    if (vol->variant == FSW_UFS_VARIANT_UFS2) {
        nbfree = fsw_le64_at(
            vol->sb_raw,
            UFS_SB_OFF_UFS2_CSTOTAL + UFS_CSUM_TOTAL_OFF_NBFREE
        );
        nffree = fsw_le64_at(
            vol->sb_raw,
            UFS_SB_OFF_UFS2_CSTOTAL + UFS_CSUM_TOTAL_OFF_NFFREE
        );
    } else {
        nbfree = fsw_le32_at(
            vol->sb_raw,
            UFS_SB_OFF_UFS1_CSTOTAL + UFS_CSUM_OFF_NBFREE
        );
        nffree = fsw_le32_at(
            vol->sb_raw,
            UFS_SB_OFF_UFS1_CSTOTAL + UFS_CSUM_OFF_NFFREE
        );
    }

    return nbfree * vol->frag_per_block + nffree;
}

static fsw_u64 fsw_ufs_cgstart (
    struct fsw_ufs_volume *vol,
    fsw_u64                cgx
) {
    fsw_u64 cgbase;
    fsw_s32 cgoffset;
    fsw_u32 cgmask;

    cgbase = cgx * (fsw_u64) fsw_les32_at(vol->sb_raw, UFS_SB_OFF_FPG);

    if (!vol->use_cg_offset)
        return cgbase;

    cgoffset = fsw_les32_at(vol->sb_raw, UFS_SB_OFF_CGOFFSET);
    cgmask = fsw_le32_at(vol->sb_raw, UFS_SB_OFF_CGMASK);

    return cgbase + (fsw_u64) cgoffset * (cgx & ~((fsw_u64) cgmask));
}

static fsw_status_t fsw_ufs_configure_variant (
    struct fsw_ufs_volume *vol,
    fsw_u32                superblock_offset
) {
    fsw_u32 magic;
    fsw_u32 bsize;
    fsw_u32 fsize;
    fsw_u32 frag;
    fsw_u32 sb_frag;
    fsw_s32 sb_bsize;
    fsw_s32 sb_fsize;

    magic = fsw_le32_at(vol->sb_raw, UFS_SB_OFF_MAGIC);
    if (magic != UFS1_MAGIC && magic != UFS2_MAGIC)
        return FSW_UNSUPPORTED;

    sb_bsize = fsw_les32_at(vol->sb_raw, UFS_SB_OFF_BSIZE);
    sb_fsize = fsw_les32_at(vol->sb_raw, UFS_SB_OFF_FSIZE);
    if (sb_bsize <= 0 || sb_fsize <= 0)
        return FSW_VOLUME_CORRUPTED;

    bsize = (fsw_u32) sb_bsize;
    fsize = (fsw_u32) sb_fsize;

    if (bsize < 512 || fsize < 512 || bsize < fsize)
        return FSW_VOLUME_CORRUPTED;
    if (!fsw_ufs_is_power_of_two(bsize) || !fsw_ufs_is_power_of_two(fsize))
        return FSW_VOLUME_CORRUPTED;
    if (bsize % fsize != 0)
        return FSW_VOLUME_CORRUPTED;

    frag = bsize / fsize;
    sb_frag = fsw_le32_at(vol->sb_raw, UFS_SB_OFF_FRAG);
    if (frag == 0 || sb_frag == 0 || sb_frag != frag)
        return FSW_VOLUME_CORRUPTED;

    vol->bsize = bsize;
    vol->fsize = fsize;
    vol->frag_per_block = frag;
    vol->superblock_offset = superblock_offset;

    if (magic == UFS2_MAGIC) {
        vol->variant = FSW_UFS_VARIANT_UFS2;
        vol->variant_name = "ufs2";
        vol->inode_size = UFS2_INODE_SIZE;
        vol->pointer_size = 8;
        vol->use_cg_offset = 0;
    } else {
        vol->variant = FSW_UFS_VARIANT_UFS1;
        vol->variant_name = "ufs1";
        vol->inode_size = UFS1_INODE_SIZE;
        vol->pointer_size = 4;
        vol->use_cg_offset = 1;
    }

    if (fsize < vol->inode_size || fsize % vol->inode_size != 0)
        return FSW_VOLUME_CORRUPTED;

    vol->inodes_per_block = bsize / vol->inode_size;
    vol->ind_bcnt = (fsw_u64) (bsize / vol->pointer_size);

    if (vol->inodes_per_block == 0 || vol->ind_bcnt == 0)
        return FSW_VOLUME_CORRUPTED;

    return FSW_SUCCESS;
}

static fsw_status_t fsw_ufs_read_indirect (
    struct fsw_ufs_volume *vol,
    fsw_u64                indir_bno,
    fsw_u64                index,
    fsw_u64               *phys_bno_out
) {
    fsw_status_t status;
    fsw_u8      *buf;
    fsw_u64      byte_offset;
    fsw_u64      frag_offset;
    fsw_u32      in_frag_offset;
    fsw_u64      phys_bno;

    byte_offset = index * vol->pointer_size;
    frag_offset = fsw_ufs_div_u64_u32(byte_offset, vol->fsize);
    in_frag_offset = fsw_ufs_mod_u64_u32(byte_offset, vol->fsize);

    status = fsw_block_get(
        vol, indir_bno + frag_offset, 1, (void **) &buf
    );
    if (status)
        return status;

    if (vol->pointer_size == 8)
        phys_bno = fsw_le64_at(buf, in_frag_offset);
    else
        phys_bno = fsw_le32_at(buf, in_frag_offset);

    fsw_block_release(vol, indir_bno + frag_offset, buf);

    *phys_bno_out = phys_bno;
    return FSW_SUCCESS;
}

static fsw_status_t fsw_ufs_validate_root (
    struct fsw_ufs_volume *vol
) {
    fsw_status_t          status;
    struct fsw_ufs_dnode  probe_dno;

    FSW_DO_MEMZERO(&probe_dno, sizeof(probe_dno));
    probe_dno.g.dnode_id = UFS_ROOT_INO;

    status = fsw_ufs_dnode_fill(vol, &probe_dno);
    if (!status && probe_dno.g.type != FSW_DNODE_TYPE_DIR)
        status = FSW_VOLUME_CORRUPTED;

    fsw_ufs_dnode_free(vol, &probe_dno);
    return status;
}

struct fsw_fstype_table FSW_FSTYPE_TABLE_NAME(ufs) = {
    { FSW_STRING_TYPE_ISO88591, 3, 3, "ufs" },
    sizeof (struct fsw_ufs_volume),
    sizeof (struct fsw_ufs_dnode),

    fsw_ufs_volume_mount,
    fsw_ufs_volume_free,
    fsw_ufs_volume_stat,
    fsw_ufs_dnode_fill,
    fsw_ufs_dnode_free,
    fsw_ufs_dnode_stat,
    fsw_ufs_get_extent,
    fsw_ufs_dir_lookup,
    fsw_ufs_dir_read,
    fsw_ufs_readlink,
};

struct fsw_fstype_table *fsw_active_fstype_table = &FSW_FSTYPE_TABLE_NAME(ufs);
CONST CHAR16            *fsw_active_fstype_name  = L"ufs";

fsw_status_t fsw_ufs_volume_mount (
    struct fsw_ufs_volume *vol
) {
    static const fsw_u32 sblock_offsets[] = {
        UFS_SBLOCK_UFS2,
        UFS_SBLOCK_UFS1,
        UFS_SBLOCK_FLOPPY,
        UFS_SBLOCK_PIGGY
    };

    fsw_status_t      status;
    fsw_status_t      last_io_status;
    void             *block_buf;
    struct fsw_string s;
    char             *volname;
    int               original_use_cg_offset;
    int               found;
    int               read_any;
    int               i;
    int               j;

    status = FSW_DO_ALLOC(UFS_SBLOCK_SIZE, &vol->sb_raw);
    if (status)
        return status;

    fsw_set_blocksize(vol, UFS_SBLOCK_SIZE, UFS_SBLOCK_SIZE);

    found = 0;
    read_any = 0;
    last_io_status = FSW_SUCCESS;
    for (i = 0; i < (int) (sizeof(sblock_offsets) / sizeof(sblock_offsets[0])); i++) {
        status = fsw_block_get(
            vol, sblock_offsets[i] / UFS_SBLOCK_SIZE, 0, &block_buf
        );
        if (status) {
            last_io_status = status;
            continue;
        }

        read_any = 1;
        FSW_DO_MEMCPY(vol->sb_raw, block_buf, UFS_SBLOCK_SIZE);
        fsw_block_release(
            vol, sblock_offsets[i] / UFS_SBLOCK_SIZE, block_buf
        );

        status = fsw_ufs_configure_variant(vol, sblock_offsets[i]);
        if (status == FSW_UNSUPPORTED)
            continue;
        if (status) {
            FSW_DO_FREE(vol->sb_raw);
            vol->sb_raw = NULL;
            return status;
        }

        found = 1;
        break;
    }

    if (!found) {
        FSW_DO_FREE(vol->sb_raw);
        vol->sb_raw = NULL;
        return read_any
            ? FSW_UNSUPPORTED
            : (last_io_status ? last_io_status : FSW_UNSUPPORTED);
    }

    fsw_set_blocksize(vol, vol->fsize, vol->bsize);

    original_use_cg_offset = vol->use_cg_offset;
    status = fsw_ufs_validate_root(vol);
    if (status == FSW_VOLUME_CORRUPTED &&
        vol->variant == FSW_UFS_VARIANT_UFS2 &&
        !original_use_cg_offset
    ) {
        vol->use_cg_offset = 1;
        status = fsw_ufs_validate_root(vol);
        if (status)
            vol->use_cg_offset = original_use_cg_offset;
    }
    if (status)
        return status;

    volname = (char *) (vol->sb_raw + UFS_SB_OFF_VOLNAME);
    for (j = 0; j < 32; j++) {
        if (volname[j] == '\0')
            break;
    }
    if (j == 0) {
        volname = (char *) (vol->sb_raw + UFS_SB_OFF_FSMNT);
        for (j = 0; j < 468; j++) {
            if (volname[j] == '\0')
                break;
        }
    }

    s.type = FSW_STRING_TYPE_ISO88591;
    s.len = s.size = j;
    s.data = volname;
    status = fsw_strdup_coerce(&vol->g.label, vol->g.host_string_type, &s);
    if (status)
        return status;

    status = fsw_dnode_create_root(vol, UFS_ROOT_INO, &vol->g.root);
    if (status)
        return status;

    FSW_MSG_L03((
        FSW_MSG_STR(
            "FSW_UFS: mounted bsize=%d fsize=%d ncg=%d sboff=%d\n"
        ),
        vol->bsize,
        vol->fsize,
        (int) fsw_le32_at(vol->sb_raw, UFS_SB_OFF_NCG),
        vol->superblock_offset
    ));

    return FSW_SUCCESS;
}

void fsw_ufs_volume_free (
    struct fsw_ufs_volume *vol
) {
    if (vol->sb_raw) {
        FSW_DO_FREE(vol->sb_raw);
        vol->sb_raw = NULL;
    }
}

fsw_status_t fsw_ufs_volume_stat (
    struct fsw_ufs_volume *vol,
    struct fsw_volume_stat *sb
) {
    sb->total_bytes = fsw_ufs_sb_size(vol) * vol->fsize;
    sb->free_bytes = fsw_ufs_sb_free_frags(vol) * vol->fsize;

    (void) fsw_ufs_sb_dsize(vol);
    return FSW_SUCCESS;
}

fsw_status_t fsw_ufs_dnode_fill (
    struct fsw_ufs_volume *vol,
    struct fsw_ufs_dnode  *dno
) {
    fsw_status_t status;
    fsw_u8      *buf;
    fsw_u64      cgx;
    fsw_u64      ino_in_cg;
    fsw_u64      inode_block_frag;
    fsw_u64      inode_byte;
    fsw_u64      read_frag;
    fsw_u32      ipg;
    fsw_u32      fpg;
    fsw_u32      iblkno;
    fsw_u32      in_frag_offset;
    fsw_u32      mode;
    fsw_s32      sb_fpg;
    fsw_s32      sb_iblkno;

    if (dno->raw)
        return FSW_SUCCESS;

    if (dno->g.dnode_id == 0)
        return FSW_VOLUME_CORRUPTED;

    ipg = fsw_le32_at(vol->sb_raw, UFS_SB_OFF_IPG);
    sb_fpg = fsw_les32_at(vol->sb_raw, UFS_SB_OFF_FPG);
    sb_iblkno = fsw_les32_at(vol->sb_raw, UFS_SB_OFF_IBLKNO);
    if (sb_fpg <= 0 || sb_iblkno < 0)
        return FSW_VOLUME_CORRUPTED;

    fpg = (fsw_u32) sb_fpg;
    iblkno = (fsw_u32) sb_iblkno;

    if (ipg == 0 || fpg == 0)
        return FSW_VOLUME_CORRUPTED;

    cgx = fsw_ufs_div_u64_u32(dno->g.dnode_id, ipg);
    ino_in_cg = fsw_ufs_mod_u64_u32(dno->g.dnode_id, ipg);

    inode_block_frag = fsw_ufs_cgstart(vol, cgx) + iblkno +
        fsw_ufs_div_u64_u32(ino_in_cg, vol->inodes_per_block) *
        vol->frag_per_block;

    inode_byte = fsw_ufs_mod_u64_u32(
        ino_in_cg, vol->inodes_per_block
    ) * vol->inode_size;

    read_frag = inode_block_frag +
        fsw_ufs_div_u64_u32(inode_byte, vol->fsize);
    in_frag_offset = fsw_ufs_mod_u64_u32(inode_byte, vol->fsize);

    status = fsw_block_get(vol, read_frag, 2, (void **) &buf);
    if (status)
        return status;

    status = fsw_memdup(
        (void **) &dno->raw,
        buf + in_frag_offset,
        vol->inode_size
    );
    fsw_block_release(vol, read_frag, buf);
    if (status)
        return status;

    dno->g.size = fsw_ufs_inode_size(vol, dno->raw);
    mode = fsw_ufs_inode_mode(vol, dno->raw);

    if      (S_ISREG(mode)) dno->g.type = FSW_DNODE_TYPE_FILE;
    else if (S_ISDIR(mode)) dno->g.type = FSW_DNODE_TYPE_DIR;
    else if (S_ISLNK(mode)) dno->g.type = FSW_DNODE_TYPE_SYMLINK;
    else                    dno->g.type = FSW_DNODE_TYPE_SPECIAL;

    return FSW_SUCCESS;
}

void fsw_ufs_dnode_free (
    struct fsw_ufs_volume *vol,
    struct fsw_ufs_dnode  *dno
) {
    (void) vol;

    if (dno->raw) {
        FSW_DO_FREE(dno->raw);
        dno->raw = NULL;
    }
}

fsw_status_t fsw_ufs_dnode_stat (
    struct fsw_ufs_volume *vol,
    struct fsw_ufs_dnode  *dno,
    struct fsw_dnode_stat *sb
) {
    sb->used_bytes = fsw_ufs_inode_blocks(vol, dno->raw) * 512ULL;

    fsw_store_time_posix(
        sb, FSW_DNODE_STAT_CTIME,
        fsw_ufs_inode_time(vol, dno->raw, UFS1_INODE_OFF_CTIME, UFS2_INODE_OFF_CTIME)
    );
    fsw_store_time_posix(
        sb, FSW_DNODE_STAT_MTIME,
        fsw_ufs_inode_time(vol, dno->raw, UFS1_INODE_OFF_MTIME, UFS2_INODE_OFF_MTIME)
    );
    fsw_store_time_posix(
        sb, FSW_DNODE_STAT_ATIME,
        fsw_ufs_inode_time(vol, dno->raw, UFS1_INODE_OFF_ATIME, UFS2_INODE_OFF_ATIME)
    );
    fsw_store_attr_posix(
        sb, (fsw_u16) fsw_ufs_inode_mode(vol, dno->raw)
    );

    return FSW_SUCCESS;
}

fsw_status_t fsw_ufs_get_extent (
    struct fsw_ufs_volume *vol,
    struct fsw_ufs_dnode  *dno,
    struct fsw_extent     *extent
) {
    fsw_status_t status;
    fsw_u64      lbn;
    fsw_u64      phys_bno;
    fsw_u64      next_phys;
    fsw_u64      file_bcnt;
    fsw_u64      indir_bno;
    fsw_u64      indir_idx;

    lbn = extent->log_start;
    file_bcnt = fsw_ufs_div_u64_u32(dno->g.size + vol->bsize - 1, vol->bsize);

    extent->type = FSW_EXTENT_TYPE_PHYSBLOCK;
    extent->log_count = 1;

    if (lbn < UFS_NDADDR) {
        phys_bno = fsw_ufs_inode_direct(vol, dno->raw, (fsw_u32) lbn);
        if (phys_bno == 0) {
            extent->type = FSW_EXTENT_TYPE_SPARSE;
            return FSW_SUCCESS;
        }

        extent->phys_start = phys_bno;
        while (
            extent->log_start + extent->log_count < (fsw_u64) UFS_NDADDR &&
            extent->log_start + extent->log_count < file_bcnt
        ) {
            next_phys = fsw_ufs_inode_direct(
                vol, dno->raw,
                (fsw_u32) (extent->log_start + extent->log_count)
            );
            if (next_phys != phys_bno +
                (fsw_u64) extent->log_count * vol->frag_per_block)
                break;
            extent->log_count++;
        }

        return FSW_SUCCESS;
    }

    lbn -= UFS_NDADDR;
    if (lbn < vol->ind_bcnt) {
        indir_bno = fsw_ufs_inode_indirect(vol, dno->raw, 0);
        if (indir_bno == 0) {
            extent->type = FSW_EXTENT_TYPE_SPARSE;
            return FSW_SUCCESS;
        }

        indir_idx = lbn;
        status = fsw_ufs_read_indirect(vol, indir_bno, indir_idx, &phys_bno);
        if (status)
            return status;

        if (phys_bno == 0) {
            extent->type = FSW_EXTENT_TYPE_SPARSE;
            return FSW_SUCCESS;
        }

        extent->phys_start = phys_bno;
        while (
            indir_idx + extent->log_count < vol->ind_bcnt &&
            extent->log_start + extent->log_count < file_bcnt
        ) {
            status = fsw_ufs_read_indirect(
                vol,
                indir_bno,
                indir_idx + extent->log_count,
                &next_phys
            );
            if (status)
                return status;
            if (next_phys != phys_bno +
                (fsw_u64) extent->log_count * vol->frag_per_block)
                break;
            extent->log_count++;
        }

        return FSW_SUCCESS;
    }

    FSW_MSG_L01((
        FSW_MSG_STR(
            "FSW_UFS: double/triple indirect not supported (inode %lld, lbn %lld)\n"
        ),
        (long long) dno->g.dnode_id,
        (long long) extent->log_start
    ));

    return FSW_UNSUPPORTED;
}

fsw_status_t fsw_ufs_dir_lookup (
    struct fsw_ufs_volume  *vol,
    struct fsw_ufs_dnode   *dno,
    struct fsw_string      *lookup_name,
    struct fsw_ufs_dnode  **child_dno_out
) {
    fsw_status_t       status;
    struct fsw_shandle shand;
    struct ufs_direct  entry;
    struct fsw_string  entry_name;
    fsw_u32            child_ino;

    (void) vol;

    entry_name.type = FSW_STRING_TYPE_ISO88591;

    status = fsw_shandle_open(dno, &shand);
    if (status)
        return status;

    child_ino = 0;
    while (child_ino == 0) {
        status = fsw_ufs_read_dentry(&shand, &entry);
        if (status)
            goto errorexit;

        if (entry.d_ino == 0) {
            status = FSW_NOT_FOUND;
            goto errorexit;
        }

        entry_name.len = entry_name.size = entry.d_namlen;
        entry_name.data = entry.d_name;

        if (fsw_streq(lookup_name, &entry_name)) {
            child_ino = entry.d_ino;
            break;
        }
    }

    status = fsw_dnode_create(
        dno, child_ino, FSW_DNODE_TYPE_UNKNOWN, &entry_name, child_dno_out
    );

errorexit:
    fsw_shandle_close(&shand);
    return status;
}

fsw_status_t fsw_ufs_dir_read (
    struct fsw_ufs_volume  *vol,
    struct fsw_ufs_dnode   *dno,
    struct fsw_shandle     *shand,
    struct fsw_ufs_dnode  **child_dno_out
) {
    fsw_status_t      status;
    struct ufs_direct entry;
    struct fsw_string entry_name;

    (void) vol;
    while (1) {
        status = fsw_ufs_read_dentry(shand, &entry);
        if (status)
            return status;

        if (entry.d_ino == 0)
            return FSW_NOT_FOUND;

        if ((entry.d_namlen == 1 &&
             entry.d_name[0] == '.') ||
            (entry.d_namlen == 2 &&
             entry.d_name[0] == '.' && entry.d_name[1] == '.'))
            continue;

        break;
    }

    entry_name.type = FSW_STRING_TYPE_ISO88591;
    entry_name.len = entry_name.size = entry.d_namlen;
    entry_name.data = entry.d_name;

    return fsw_dnode_create(
        dno,
        entry.d_ino,
        FSW_DNODE_TYPE_UNKNOWN,
        &entry_name,
        child_dno_out
    );
}

fsw_status_t fsw_ufs_read_dentry (
    struct fsw_shandle *shand,
    struct ufs_direct  *entry
) {
    fsw_status_t status;
    fsw_u32      buf_size;

    while (1) {
        buf_size = UFS_DIRENT_HEADER_SIZE;
        status = fsw_shandle_read(shand, &buf_size, entry);
        if (status)
            return status;

        if (buf_size < UFS_DIRENT_HEADER_SIZE || entry->d_reclen == 0) {
            entry->d_ino = 0;
            return FSW_SUCCESS;
        }

        if (entry->d_reclen < UFS_DIRENT_HEADER_SIZE)
            return FSW_VOLUME_CORRUPTED;

        if (entry->d_ino != 0) {
            if (entry->d_reclen <
                (fsw_u16) (UFS_DIRENT_HEADER_SIZE + entry->d_namlen))
                return FSW_VOLUME_CORRUPTED;
            break;
        }

        shand->pos += entry->d_reclen - UFS_DIRENT_HEADER_SIZE;
    }

    buf_size = entry->d_namlen;
    status = fsw_shandle_read(shand, &buf_size, entry->d_name);
    if (status)
        return status;
    if (buf_size < entry->d_namlen)
        return FSW_VOLUME_CORRUPTED;

    shand->pos += entry->d_reclen -
        (UFS_DIRENT_HEADER_SIZE + entry->d_namlen);

    return FSW_SUCCESS;
}

fsw_status_t fsw_ufs_readlink (
    struct fsw_ufs_volume *vol,
    struct fsw_ufs_dnode  *dno,
    struct fsw_string     *link_target
) {
    fsw_status_t      status;
    struct fsw_string s;
    fsw_u32           max_inline_len;

    if (dno->g.size > FSW_PATH_MAX)
        return FSW_VOLUME_CORRUPTED;

    max_inline_len = (UFS_NDADDR + UFS_NIADDR) * vol->pointer_size;

    if (fsw_ufs_inode_blocks(vol, dno->raw) == 0) {
        if (dno->g.size > max_inline_len)
            return FSW_VOLUME_CORRUPTED;

        s.type = FSW_STRING_TYPE_ISO88591;
        s.len = s.size = (int) dno->g.size;
        s.data = dno->raw + fsw_ufs_inode_db_offset(vol);
        status = fsw_strdup_coerce(link_target, vol->g.host_string_type, &s);
    } else {
        status = fsw_dnode_readlink_data(dno, link_target);
    }

    return status;
}
