// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2020-2026 Dayo Akanji
// SPDX-FileCopyrightText: 2006 Christoph Pfisterer

#include "fsw_ext4.h"

#define MAX_EXT_ENTRIES 1024
#define EXT4_EXT_UNINIT 0x8000U
#define EXT4_EXT_LEN_MASK 0x7FFFU

struct fsw_fstype_table FSW_FSTYPE_TABLE_NAME(ext4) = {
    { FSW_STRING_TYPE_ISO88591, 4, 4, "ext4" },
    sizeof (struct fsw_ext4_volume),
    sizeof (struct fsw_ext4_dnode),

    fsw_ext4_volume_mount,
    fsw_ext4_volume_free,
    fsw_ext4_volume_stat,
    fsw_ext4_dnode_fill,
    fsw_ext4_dnode_free,
    fsw_ext4_dnode_stat,
    fsw_ext4_get_extent,
    fsw_ext4_dir_lookup,
    fsw_ext4_dir_read,
    fsw_ext4_readlink,
};

struct fsw_fstype_table *fsw_active_fstype_table = &FSW_FSTYPE_TABLE_NAME(ext4);
CONST CHAR16            *fsw_active_fstype_name  = L"ext4";

static
fsw_status_t fsw_ext4_get_by_extent (
    struct fsw_ext4_volume *vol,
    struct fsw_ext4_dnode  *dno,
    struct fsw_extent      *extent
) {
    fsw_status_t   status;
    fsw_u32        bno, buf_offset, file_bcnt, release_bno;
    int            ext_cnt;
    void          *buffer;

    struct ext4_extent_header  *ext4_header;
    struct ext4_extent_idx     *ext4_idx, *best_idx;
    struct ext4_extent         *ext4_ext;

    release_bno = 0;
    buf_offset  = 0;

    if (dno->raw->i_flags & EXT4_INLINE_DATA_FL) {
        FSW_MSG_L01((
            FSW_MSG_STR(
                "FSW_EXT4: fsw_ext4_get_by_extent ... Leaving with Status: 'FSW_UNSUPPORTED' (Uses Inline Data)\n"
            )
        ));

        return FSW_UNSUPPORTED;
    }

    bno = extent->log_start;
    file_bcnt = (fsw_u32)((dno->g.size + vol->g.log_blocksize - 1) / vol->g.log_blocksize);

    buffer = (void *)dno->raw->i_block;

    while (1) {
        ext4_header = (struct ext4_extent_header *)((char *)buffer + buf_offset);
        if (ext4_header->eh_magic != EXT4_EXT_MAGIC) {
            FSW_MSG_L01((
                FSW_MSG_STR(
                    "FSW_EXT4: fsw_ext4_get_by_extent ... Leaving with Status: 'FSW_VOLUME_CORRUPTED' (eh_magic != EXT4_EXT_MAGIC)\n"
                )
            ));

            status = FSW_VOLUME_CORRUPTED;
            goto exit;
        }

        buf_offset += sizeof (struct ext4_extent_header);

        if (ext4_header->eh_depth == 0) {

            for (ext_cnt = 0; ext_cnt < ext4_header->eh_entries; ext_cnt++) {
                ext4_ext = (struct ext4_extent *)((char *)buffer + buf_offset);
                buf_offset += sizeof (struct ext4_extent);

                if (bno >= ext4_ext->ee_block &&
                    bno <  ext4_ext->ee_block + ext4_ext->ee_len
                ) {
                    extent->phys_start  = ((fsw_u64)ext4_ext->ee_start_hi << 32) | ext4_ext->ee_start_lo;
                    extent->phys_start += (bno - ext4_ext->ee_block);
                    extent->log_count   = ext4_ext->ee_len - (bno - ext4_ext->ee_block);

                    status = FSW_SUCCESS;
                    goto exit;
                }

                if (ext4_ext->ee_block > bno) {
                    extent->log_count = ext4_ext->ee_block - bno;

                    goto signal_sparse;
                }
            }

            extent->log_count = file_bcnt - bno;

            goto signal_sparse;
        }
        else {

            best_idx = NULL;
            for (ext_cnt = 0; ext_cnt < ext4_header->eh_entries; ext_cnt++) {
                ext4_idx = (struct ext4_extent_idx *)((char *)buffer + buf_offset);
                buf_offset += sizeof (struct ext4_extent_idx);

                if (bno < ext4_idx->ei_block) break;
                best_idx = ext4_idx;
            }

            if (best_idx == NULL) {
                ext4_idx = (struct ext4_extent_idx *)((char *)buffer + sizeof (struct ext4_extent_header));
                extent->log_count = ext4_idx->ei_block - bno;

                goto signal_sparse;
            }

            if (release_bno) {
                fsw_block_release (vol, release_bno, buffer);
            }

            release_bno = ((fsw_u64)best_idx->ei_leaf_hi << 32) | best_idx->ei_leaf_lo;
            status = fsw_block_get (
                vol, release_bno, 1,
                (void **)&buffer
            );
            if (status) {
                FSW_MSG_L01((
                    FSW_MSG_STR(
                        "FSW_EXT4: fsw_ext4_get_by_extent ... Leaving with Status '%d' Error ('fsw_block_get' Failure)\n"
                    ), status
                ));

                goto exit;
            }

            buf_offset = 0;
        }
    }

signal_sparse:
    FSW_MSG_L01((
        FSW_MSG_STR(
            "FSW_EXT4: fsw_ext4_get_by_extent ... Chunk Type:- 'Sparse'\n"
        )
    ));

	extent->type = FSW_EXTENT_TYPE_INVALID;
	status = FSW_IO_ERROR;

exit:
	if (release_bno) {
		fsw_block_release (vol, release_bno, buffer);
	}

	if (!status) {
		extent->type = FSW_EXTENT_TYPE_PHYSBLOCK;

		FSW_MSG_L02((
			FSW_MSG_STR(
				"FSW_EXT4: fsw_ext4_get_by_extent ... Chunk Type:- 'Physical'\n"
			)
		));

		FSW_MSG_L03((
			FSW_MSG_STR(
				"FSW_EXT4: fsw_ext4_get_by_extent ... Leaving with Status: 'FSW_SUCCESS'\n"
			)
		));
	}

	return status;
}

static
fsw_status_t fsw_ext4_get_by_blkaddr (
    struct fsw_ext4_volume *vol,
    struct fsw_ext4_dnode  *dno,
    struct fsw_extent      *extent
) {
    fsw_status_t     status, sparse_status;
    fsw_u32          bno, release_bno, buf_bcnt, file_bcnt;
    int              path[5], i;
    fsw_u32         *buffer;

    sparse_status = FSW_SUCCESS;
    bno = extent->log_start;

    extent->log_count = 1;
    file_bcnt = (fsw_u32)((dno->g.size + vol->g.log_blocksize - 1) / vol->g.log_blocksize);

    if (bno < EXT4_NDIR_BLOCKS) {

        path[0] = bno;
        path[1] = -1;
    }
    else {
        bno -= EXT4_NDIR_BLOCKS;

        if (bno < vol->ind_bcnt) {

            path[0] = EXT4_IND_BLOCK;
            path[1] = bno;
            path[2] = -1;
        }
        else {
            bno -= vol->ind_bcnt;

            if (bno < vol->dind_bcnt) {

                path[0] = EXT4_DIND_BLOCK;
                path[1] = bno / vol->ind_bcnt;
                path[2] = bno % vol->ind_bcnt;
                path[3] = -1;
            }
            else {
                bno -= vol->dind_bcnt;

                path[0] = EXT4_TIND_BLOCK;
                path[1] = bno / vol->dind_bcnt;
                path[2] = (bno / vol->ind_bcnt) % vol->ind_bcnt;
                path[3] = bno % vol->ind_bcnt;
                path[4] = -1;
            }
        }
    }

    buffer = dno->raw->i_block;
    buf_bcnt = EXT4_NDIR_BLOCKS;
    release_bno = 0;

    for (i = 0; ; i++) {
        bno = buffer[path[i]];
        if (bno == 0) {
            sparse_status = FSW_IO_ERROR;
            goto check_sparse;
        }

        if (path[i+1] < 0) break;

        if (release_bno) {
            fsw_block_release (
                vol,
                release_bno,
                buffer
            );
        }

        status = fsw_block_get (
            vol, bno, 1,
            (void **) &buffer
        );
        if (status) {
            FSW_MSG_L01((
                FSW_MSG_STR(
                    "FSW_EXT4: fsw_ext4_get_by_blkaddr ... Leaving with Status '%d' Error ('fsw_block_get' Failure)\n"
                ), status
            ));

            goto exit;
        }

        release_bno = bno;
        buf_bcnt = vol->ind_bcnt;
    }
    extent->phys_start = bno;

check_sparse:

    while (
        path[i]           + extent->log_count < buf_bcnt &&
        extent->log_start + extent->log_count < file_bcnt
    ) {
        if (sparse_status) {
            if (buffer[path[i] + extent->log_count] != 0) break;
        }
        else {
            if (buffer[path[i] + extent->log_count] !=
                buffer[path[i] + extent->log_count - 1] + 1
            ) {
                break;
            }
        }

        extent->log_count++;
    }

    if (!sparse_status) {
        status = FSW_SUCCESS;
        goto exit;
    }

    FSW_MSG_L01((
        FSW_MSG_STR(
            "FSW_EXT4: fsw_ext4_get_by_blkaddr ... Chunk Type:- 'Sparse'\n"
        )
    ));

    extent->type = FSW_EXTENT_TYPE_INVALID;
    status = FSW_IO_ERROR;

exit:
    if (release_bno) {
        fsw_block_release (
            vol,
            release_bno,
            buffer
        );
    }

    if (!status) {
        extent->type = FSW_EXTENT_TYPE_PHYSBLOCK;

        FSW_MSG_L02((
            FSW_MSG_STR(
                "FSW_EXT4: fsw_ext4_get_by_blkaddr ... Chunk Type:- 'Physical'\n"
            )
        ));

        FSW_MSG_L03((
            FSW_MSG_STR(
                "FSW_EXT4: fsw_ext4_get_by_blkaddr ... Leaving with Status: 'FSW_SUCCESS'\n"
            )
        ));
    }

    return status;
}

static __inline
int test_root (
    fsw_u32 a,
    int     b
) {
    fsw_u32 num = b;

    while (a > num) num *= b;

    return num == a;
}

static
int fsw_ext4_group_sparse (
    fsw_u32 group
) {
    if (group <= 1)   return 1;
    if (!(group & 1)) return 0;

    return (
        test_root(group, 7) ||
        test_root(group, 5) ||
        test_root(group, 3)
    );
}

static __inline
fsw_u64 fsw_ext4_group_first_block_no (
    struct ext4_super_block *sb,
    fsw_u32                  group_no
) {
    return (
        group_no                           *
        (fsw_u64)EXT4_BLOCKS_PER_GROUP(sb) +
        sb->s_first_data_block
    );
}

static
void fsw_ext4_log_vol_mount (
    fsw_status_t status,
    int          log_flag
) {
    #if FSW_DEBUG_LEVEL > 0
    if (status == 12) {
        FSW_MSG_L03((
            FSW_MSG_STR(
                "FSW_EXT4: fsw_ext4_log_vol_mount ... Leaving with Status: 'No Media' (Tag_%02u)\n"
            ), (unsigned) log_flag
        ));
    }
    else {
        FSW_MSG_L01((
            FSW_MSG_STR(
                "FSW_EXT4: fsw_ext4_log_vol_mount ... Leaving with Status '%d' Error ('fsw_block_get' failure) ... Tag_01\n"
            ), status
        ));
    }
    #endif

    return;
}

fsw_status_t fsw_ext4_volume_mount (
    struct fsw_ext4_volume *vol
) {
    int                     i;
    int                     logtag;
    fsw_status_t            status;
    void                   *buffer;
    fsw_u32                 blocksize, groupcnt, gdesc_per_block;
    fsw_u32                 groupno, gdesc_index, metabg_of_gdesc;
    fsw_u64                 gdesc_bno;
    struct ext4_group_desc *gdesc;
    struct fsw_string       s;

    status = FSW_DO_ALLOC(
        sizeof (
            struct ext4_super_block
        ),
        &vol->sb
    );
    if (status) {
        FSW_MSG_L01((
            FSW_MSG_STR(
                "FSW_EXT4: fsw_ext4_volume_mount ... Leaving with Status '%d' Error ('fsw_alloc' failure)\n"
            ), status
        ));

        return status;
    }

    fsw_set_blocksize (
        vol,
        EXT4_SUPERBLOCK_BLOCKSIZE,
        EXT4_SUPERBLOCK_BLOCKSIZE
    );

    status = fsw_block_get (
        vol,
        EXT4_SUPERBLOCK_BLOCKNO, 0,
        &buffer
    );
    if (status) {
        logtag = 1;
        fsw_ext4_log_vol_mount (
            status, logtag
        );

        return status;
    }

    FSW_DO_MEMCPY(
        vol->sb, buffer,
        sizeof (struct ext4_super_block)
    );
    fsw_block_release (
        vol,
        EXT4_SUPERBLOCK_BLOCKNO,
        buffer
    );

    if (vol->sb->s_magic != EXT4_SUPER_MAGIC) {
        return FSW_UNSUPPORTED;
    }

    if (vol->sb->s_rev_level != EXT4_DYNAMIC_REV &&
        vol->sb->s_rev_level != EXT4_GOOD_OLD_REV

    ) {
        return FSW_UNSUPPORTED;
    }

    FSW_MSG_L03((
        FSW_MSG_STR(
            "FSW_EXT4: fsw_ext4_volume_mount ... Incompat flag %x\n"
        ), vol->sb->s_feature_incompat
    ));

    if ((vol->sb->s_rev_level == EXT4_DYNAMIC_REV) &&
        (vol->sb->s_feature_incompat & ~(
            EXT4_FEATURE_INCOMPAT_64BIT    |
            EXT4_FEATURE_INCOMPAT_RECOVER  |
            EXT4_FEATURE_INCOMPAT_EXTENTS  |
            EXT4_FEATURE_INCOMPAT_FLEX_BG  |
            EXT4_FEATURE_INCOMPAT_META_BG  |
            EXT4_FEATURE_INCOMPAT_ENCRYPT  |
            EXT4_FEATURE_INCOMPAT_FILETYPE |
            EXT4_FEATURE_INCOMPAT_BG_USE_META_CSUM
        ))
    ) {
        return FSW_UNSUPPORTED;
    }

    if (vol->sb->s_rev_level == EXT4_DYNAMIC_REV &&
        vol->sb->s_feature_incompat & EXT4_FEATURE_INCOMPAT_RECOVER
    ) {
        FSW_MSG_L03((
            FSW_MSG_STR(
                "FSW_EXT4: fsw_ext4_volume_mount ... This file system needs recovery, trying to use it anyway\n"
            )
        ));
    }

    blocksize = EXT4_BLOCK_SIZE(vol->sb);
    if (blocksize < EXT4_MIN_BLOCK_SIZE ||
        blocksize > EXT4_MAX_BLOCK_SIZE
    ) {
        return FSW_UNSUPPORTED;
    }

    fsw_set_blocksize (vol, blocksize, blocksize);

    vol->ind_bcnt   = EXT4_ADDR_PER_BLOCK(vol->sb);
    vol->dind_bcnt  = vol->ind_bcnt * vol->ind_bcnt;
    vol->inode_size = vol->sb->s_inode_size;

    for (i = 0; i < 16; i++) {
        if (vol->sb->s_volume_name[i] == 0) break;
    }

    s.size = s.len = i;
    s.data = vol->sb->s_volume_name;
    s.type = FSW_STRING_TYPE_ISO88591;

    status = fsw_strdup_coerce (
        &vol->g.label, vol->g.host_string_type, &s
    );
    if (status) {
        return status;
    }

    if (!(vol->sb->s_feature_incompat & EXT4_FEATURE_INCOMPAT_64BIT)) {

        vol->sb->s_desc_size = EXT4_MIN_DESC_SIZE;
    }

    groupcnt = (
        vol->sb->s_blocks_count_lo  -
        vol->sb->s_first_data_block +
        vol->sb->s_blocks_per_group - 1
    ) / vol->sb->s_blocks_per_group;

    gdesc_per_block = EXT4_DESC_PER_BLOCK(vol->sb);

    status = FSW_DO_ALLOC(sizeof (fsw_u64) * groupcnt, &vol->inotab_bno);
    if (status) {
        return status;
    }

    for (groupno = 0; groupno < groupcnt; groupno++) {

        if (groupno >= vol->sb->s_first_meta_bg &&
            vol->sb->s_feature_incompat & EXT4_FEATURE_INCOMPAT_META_BG
        ) {

            metabg_of_gdesc = (fsw_u32)(groupno / gdesc_per_block) * gdesc_per_block;
            gdesc_bno = fsw_ext4_group_first_block_no(vol->sb, metabg_of_gdesc);

            if (fsw_ext4_group_sparse (metabg_of_gdesc) ||
                !(vol->sb->s_feature_ro_compat & EXT4_FEATURE_RO_COMPAT_SPARSE_SUPER)
            ) {
                gdesc_bno += 1;
            }
        }
        else {

            gdesc_bno = (
                vol->sb->s_first_data_block + 1
            ) + groupno / gdesc_per_block;
        }
        gdesc_index = groupno % gdesc_per_block;

        status = fsw_block_get (
            vol, gdesc_bno, 1,
            (void **) &buffer
        );
        if (status) {
            logtag = 2;
            fsw_ext4_log_vol_mount (
                status, logtag
            );

            return status;
        }

        gdesc = (struct ext4_group_desc *)(
            (char *)buffer +
            gdesc_index * vol->sb->s_desc_size
        );
        vol->inotab_bno[groupno] = gdesc->bg_inode_table_lo;
        if (vol->sb->s_desc_size >= EXT4_MIN_DESC_SIZE_64BIT) {
            vol->inotab_bno[groupno] |= (fsw_u64)gdesc->bg_inode_table_hi << 32;
        }

        fsw_block_release (vol, gdesc_bno, buffer);
    }

    status = fsw_dnode_create_root (
        vol, EXT4_ROOT_INO,
        &vol->g.root
    );
    if (status) {
        return status;
    }

    FSW_MSG_L03((
        FSW_MSG_STR(
            "FSW_EXT4: fsw_ext4_volume_mount ... success, blocksize %d\n"
        ), blocksize
    ));

    return FSW_SUCCESS;
}

void fsw_ext4_volume_free (
    struct fsw_ext4_volume *vol
) {
    if (vol->sb)         FSW_DO_FREE(vol->sb);
    if (vol->inotab_bno) FSW_DO_FREE(vol->inotab_bno);
}

fsw_status_t fsw_ext4_volume_stat (
    struct fsw_ext4_volume *vol,
    struct fsw_volume_stat *sb
) {
    fsw_u64         count;

    count = vol->sb->s_blocks_count_lo;
    if (vol->sb->s_desc_size >= EXT4_MIN_DESC_SIZE_64BIT) {
        count |= (fsw_u64)vol->sb->s_blocks_count_hi << 32;
    }
    sb->total_bytes = count * vol->g.log_blocksize;

    count = vol->sb->s_free_blocks_count_lo;
    if (vol->sb->s_desc_size >= EXT4_MIN_DESC_SIZE_64BIT) {
        count |= (fsw_u64)vol->sb->s_free_blocks_count_hi << 32;
    }
    sb->free_bytes  = count * vol->g.log_blocksize;

    return FSW_SUCCESS;
}

fsw_status_t fsw_ext4_dnode_fill (
    struct fsw_ext4_volume *vol,
    struct fsw_ext4_dnode  *dno
) {
    fsw_status_t    status;
    fsw_u32         groupno, ino_in_group, ino_index;
    fsw_u64         ino_bno;
    fsw_u8          *buffer;

    if (dno->raw) {
        return FSW_SUCCESS;
    }

    groupno = (fsw_u32) (dno->g.dnode_id - 1) / vol->sb->s_inodes_per_group;
    ino_in_group = (fsw_u32) (dno->g.dnode_id - 1) % vol->sb->s_inodes_per_group;
    ino_bno = vol->inotab_bno[groupno] +
        ino_in_group / (vol->g.phys_blocksize / vol->inode_size);
    ino_index = ino_in_group % (vol->g.phys_blocksize / vol->inode_size);

    status = fsw_block_get (
        vol, ino_bno, 2,
        (void **) &buffer
    );
    if (status) {
        return status;
    }

    status = fsw_memdup(
        (void **) &dno->raw,
        buffer + ino_index * vol->inode_size,
        vol->inode_size
    );
    fsw_block_release (vol, ino_bno, buffer);
    if (status) {
        return status;
    }

    dno->g.size = dno->raw->i_size_lo;

    if (0);
    else if (S_ISDIR(dno->raw->i_mode)) dno->g.type = FSW_DNODE_TYPE_DIR;
    else if (S_ISREG(dno->raw->i_mode)) dno->g.type = FSW_DNODE_TYPE_FILE;
    else if (S_ISLNK(dno->raw->i_mode)) dno->g.type = FSW_DNODE_TYPE_SYMLINK;
    else                                dno->g.type = FSW_DNODE_TYPE_SPECIAL;

    FSW_MSG_L03((
        FSW_MSG_STR(
            "FSW_EXT4: fsw_ext4_dnode_fill ... inode flags %x\n"
        ), dno->raw->i_flags
    ));
    FSW_MSG_L03((
        FSW_MSG_STR(
            "FSW_EXT4: fsw_ext4_dnode_fill ... i_mode %x\n"
        ), dno->raw->i_mode
    ));
    return FSW_SUCCESS;
}

void fsw_ext4_dnode_free (
    struct fsw_ext4_volume *vol,
    struct fsw_ext4_dnode  *dno
) {
    if (dno->raw) FSW_DO_FREE(dno->raw);
}

fsw_status_t fsw_ext4_dnode_stat (
    struct fsw_ext4_volume *vol,
    struct fsw_ext4_dnode  *dno,
    struct fsw_dnode_stat  *sb
) {
    sb->used_bytes = EXT4_BLOCK_SIZE(
        vol->sb
    ) * ((fsw_u64)dno->raw->i_blocks_lo);
    fsw_store_time_posix(sb, FSW_DNODE_STAT_CTIME, dno->raw->i_ctime);
    fsw_store_time_posix(sb, FSW_DNODE_STAT_ATIME, dno->raw->i_atime);
    fsw_store_time_posix(sb, FSW_DNODE_STAT_MTIME, dno->raw->i_mtime);
    fsw_store_attr_posix(sb, dno->raw->i_mode);

    return FSW_SUCCESS;
}

fsw_status_t fsw_ext4_get_extent (
    struct fsw_ext4_volume *vol,
    struct fsw_ext4_dnode  *dno,
    struct fsw_extent      *extent
) {
    fsw_status_t   status;

    FSW_MSG_L03((
		FSW_MSG_STR(
			"FSW_EXT4: fsw_ext4_get_extent ... inode %d, block %d\n"
		), dno->g.dnode_id, extent->log_start
	));

	extent->log_count = 1;

    if (dno->raw->i_flags & 1 << EXT4_INODE_EXTENTS) {
      FSW_MSG_L03((
		  FSW_MSG_STR(
			  "FSW_EXT4: fsw_ext4_get_extent ... Inode '%d' Uses Extents\n"
		  ), dno->g.dnode_id
	  ));

	  status = fsw_ext4_get_by_extent (
		  vol, dno, extent
	  );
    }
	else {
		FSW_MSG_L03((
			FSW_MSG_STR(
				"FSW_EXT4: fsw_ext4_get_extent ... Inode '%d' Uses Block Addressing\n"
			), dno->g.dnode_id
		));

		status = fsw_ext4_get_by_blkaddr (
			vol, dno, extent
		);
	}

	if (!status) {
		FSW_MSG_L03((
			FSW_MSG_STR(
				"FSW_EXT4: fsw_ext4_get_extent ... Leaving with Status: 'FSW_SUCCESS'\n"
			)
		));
	}
	else {
		if (status == FSW_IO_ERROR) {
			FSW_MSG_L02((
				FSW_MSG_STR(
					"FSW_EXT4: fsw_ext4_get_extent ... Leaving with Status: 'FSW_IO_ERROR' (Found Sparse Hole)\n"
				)
			));
		}
		else {
			FSW_MSG_L02((
				FSW_MSG_STR(
					"FSW_EXT4: fsw_ext4_get_extent ... Leaving with Status '%d' Error (Failed to Get Extent)\n"
				), status
			));
		}
	}

	return status;
}

fsw_status_t fsw_ext4_dir_lookup (
    struct fsw_ext4_volume  *vol,
    struct fsw_ext4_dnode   *dno,
    struct fsw_string       *lookup_name,
    struct fsw_ext4_dnode  **child_dno_out
) {
    fsw_status_t    status;
    struct fsw_shandle shand;
    fsw_u32         child_ino;
    struct ext4_dir_entry entry;
    struct fsw_string entry_name;

    entry_name.type = FSW_STRING_TYPE_ISO88591;

    status = fsw_shandle_open (dno, &shand);
    if (status) {
        return status;
    }

    child_ino = 0;
    while (child_ino == 0) {

        status = fsw_ext4_read_dentry (&shand, &entry);
        if (status) goto errorexit;

        if (entry.inode == 0) {

            status = FSW_NOT_FOUND;
            goto errorexit;
        }

        entry_name.len = entry_name.size = entry.name_len;
        entry_name.data = entry.name;
        if (fsw_streq(lookup_name, &entry_name)) {
            child_ino = entry.inode;
            break;
        }
    }

    status = fsw_dnode_create (
        dno, child_ino,
        FSW_DNODE_TYPE_UNKNOWN,
        &entry_name, child_dno_out
    );

errorexit:
    fsw_shandle_close(&shand);
    return status;
}

fsw_status_t fsw_ext4_dir_read (
    struct fsw_ext4_volume  *vol,
    struct fsw_ext4_dnode   *dno,
    struct fsw_shandle      *shand,
    struct fsw_ext4_dnode **child_dno_out
) {
    fsw_status_t    status;
    struct ext4_dir_entry entry;
    struct fsw_string entry_name;

    FSW_MSG_L03((
        FSW_MSG_STR(
            "FSW_EXT4: fsw_ext4_dir_read ... Started Reading Dir\n"
        )
    ));

    entry.name_len = 0;
    entry.inode    = 0;

    while (1) {

        status = fsw_ext4_read_dentry(shand, &entry);
        if (status) {
            return status;
        }

        if (entry.inode == 0) {

            return FSW_NOT_FOUND;
        }

        if ((entry.name_len == 1 && entry.name[0] == '.') ||
            (entry.name_len == 2 && entry.name[0] == '.'  && entry.name[1] == '.')
        ) {
            continue;
        }
        break;
    }

    entry_name.data = entry.name;
    entry_name.type = FSW_STRING_TYPE_ISO88591;
    entry_name.len  = entry_name.size = entry.name_len;

    status = fsw_dnode_create (
        dno, entry.inode,
        FSW_DNODE_TYPE_UNKNOWN,
        &entry_name, child_dno_out
    );

    return status;
}

fsw_status_t fsw_ext4_read_dentry (
    struct fsw_shandle    *shand,
    struct ext4_dir_entry *entry
) {
    fsw_status_t    status;
    fsw_u32         buffer_size;

    while (1) {

        buffer_size = 8;
        status = fsw_shandle_read(shand, &buffer_size, entry);
        if (status)
            return status;

        if (buffer_size < 8 || entry->rec_len == 0) {

            entry->inode = 0;
            return FSW_SUCCESS;
        }
        if (entry->rec_len < 8) {
            FSW_MSG_L01((
                FSW_MSG_STR(
                    "FSW_EXT4: fsw_ext4_read_dentry ... Leaving with Status: 'FSW_VOLUME_CORRUPTED' (rec_len < 8)\n"
                )
            ));

            return FSW_VOLUME_CORRUPTED;

        }

        if (entry->inode != 0) {

            if (entry->rec_len < entry->name_len + 8) {
                FSW_MSG_L01((
                    FSW_MSG_STR(
                        "FSW_EXT4: fsw_ext4_read_dentry ... Leaving with Status: 'FSW_VOLUME_CORRUPTED' (rec_len < name_len + 8)\n"
                    )
                ));

                return FSW_VOLUME_CORRUPTED;
            }

            break;
        }

        shand->pos += entry->rec_len - 8;
    }

    buffer_size = entry->name_len;
    status = fsw_shandle_read (
        shand, &buffer_size, entry->name
    );
    if (status) {
        FSW_MSG_L01((
            FSW_MSG_STR(
                "FSW_EXT4: fsw_ext4_read_dentry ... Leaving with Status '%d' Error ('fsw_shandle_read' failure)\n"
            ), status
        ));

        return status;
    }

    if (buffer_size < entry->name_len) {
        FSW_MSG_L01((
            FSW_MSG_STR(
                "FSW_EXT4: fsw_ext4_read_dentry ... Leaving with Status: 'FSW_VOLUME_CORRUPTED' (buffer_size < name_len)\n"
            )
        ));

        return FSW_VOLUME_CORRUPTED;
    }

    shand->pos += entry->rec_len - (8 + entry->name_len);

    return FSW_SUCCESS;
}

fsw_status_t fsw_ext4_readlink (
    struct fsw_ext4_volume *vol,
    struct fsw_ext4_dnode  *dno,
    struct fsw_string      *link_target
) {
    fsw_status_t    status;
    int             ea_blocks;
    struct fsw_string s;

    if (dno->g.size > FSW_PATH_MAX)  {
        FSW_MSG_L01((
            FSW_MSG_STR(
                "FSW_EXT4: fsw_ext4_readlink ... Leaving with Status: 'FSW_VOLUME_CORRUPTED' (g.size > FSW_PATH_MAX)\n"
            )
        ));

        return FSW_VOLUME_CORRUPTED;
    }

    ea_blocks = (
        dno->raw->i_file_acl_lo
    ) ? (vol->g.log_blocksize >> 9) : 0;

    if (dno->raw->i_blocks_lo - ea_blocks != 0) {

        status = fsw_dnode_readlink_data(dno, link_target);
    }
    else {
        if (dno->g.size > sizeof (dno->raw->i_block)) {
            FSW_MSG_L01((
                FSW_MSG_STR(
                    "FSW_EXT4: fsw_ext4_readlink ... Leaving with Status: 'FSW_VOLUME_CORRUPTED' (g.size > sizeof i_block)\n"
                )
            ));

            return FSW_VOLUME_CORRUPTED;
        }

        s.type = FSW_STRING_TYPE_ISO88591;
        s.size = s.len = (int)dno->g.size;
        s.data = dno->raw->i_block;
        status = fsw_strdup_coerce (
            link_target,
            vol->g.host_string_type, &s
        );
    }

    return status;
}
