// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2026 Dayo Akanji
// SPDX-FileCopyrightText: 2006 Christoph Pfisterer

#include "fsw_core.h"
#include "fsw_efi.h"

static void fsw_blockcache_free(struct fsw_volume *vol);

#define MAX_CACHE_LEVEL (5)

fsw_status_t fsw_mount (
    void                     *host_data,
    struct fsw_host_table    *host_table,
    struct fsw_fstype_table  *fstype_table,
    struct fsw_volume       **vol_out
) {
    fsw_status_t    status;
    struct fsw_volume *vol;

    status = fsw_alloc_zero (
        fstype_table->volume_struct_size,
        (void **) &vol
    );
    if (status) {
        FSW_MSG_L03((
            FSW_MSG_STR(
                "FSW_CORE: fsw_mount ... Leaving with Status '%d' Error (1):- Memory Allocation Failure\n"
            ), status
        ));

        return status;
    }

    vol->phys_blocksize   = 512;
    vol->log_blocksize    = 512;
    vol->label.type       = FSW_STRING_TYPE_EMPTY;
    vol->host_data        = host_data;
    vol->host_table       = host_table;
    vol->fstype_table     = fstype_table;
    vol->host_string_type = host_table->native_string_type;

    status = vol->fstype_table->volume_mount (vol);
    if (status) goto errorexit;

    *vol_out = vol;
    return FSW_SUCCESS;

errorexit:
    fsw_unmount (vol);

    FSW_MSG_L03((
        FSW_MSG_STR(
            "FSW_CORE: fsw_mount ... Leaving with Status '%d' Error (2)\n"
        ), status
    ));

    return status;
}

void fsw_unmount (
    struct fsw_volume *vol
) {
    if (vol->root) fsw_dnode_release (vol->root);

    vol->fstype_table->volume_free (vol);

    fsw_blockcache_free (vol);
    fsw_strfree (&vol->label);
    FSW_DO_FREE(vol);
}

fsw_status_t fsw_volume_stat (
    struct fsw_volume      *vol,
    struct fsw_volume_stat *sb
) {
    return vol->fstype_table->volume_stat (vol, sb);
}

void fsw_set_blocksize (
    struct fsw_volume *vol,
    fsw_u32            phys_blocksize,
    fsw_u32            log_blocksize
) {

    fsw_blockcache_free (vol);

    vol->host_table->change_blocksize (
        vol,
        vol->phys_blocksize, vol->log_blocksize,
        phys_blocksize, log_blocksize
    );

    vol->phys_blocksize = phys_blocksize;
    vol->log_blocksize  = log_blocksize;
}

fsw_status_t fsw_block_get (
    struct VOLSTRUCTNAME  *vol,
    fsw_u64                phys_bno,
    fsw_u32                cache_level,
    void                 **buffer_out
) {
    fsw_status_t           status;
    fsw_u32                i, discard_level, new_bcache_size;
    struct fsw_blockcache *new_bcache;

    if (cache_level > MAX_CACHE_LEVEL) {
        cache_level = MAX_CACHE_LEVEL;
    }

    for (i = 0; i < vol->bcache_size; i++) {
        if (vol->bcache[i].phys_bno == phys_bno) {

            if (vol->bcache[i].cache_level < cache_level) {
                vol->bcache[i].cache_level = cache_level;
            }

            vol->bcache[i].refcount++;
            *buffer_out = vol->bcache[i].data;

            return FSW_SUCCESS;
        }
    }

    for (i = 0; i < vol->bcache_size; i++) {
        if (vol->bcache[i].phys_bno == (fsw_u64) FSW_INVALID_BNO) {
            break;
        }
    }
    if (i >= vol->bcache_size) {
        for (
            discard_level = 0;
            discard_level <= MAX_CACHE_LEVEL;
            discard_level++
        ) {
            for (i = 0; i < vol->bcache_size; i++) {
                if (vol->bcache[i].refcount    == 0 &&
                    vol->bcache[i].cache_level <= discard_level
                ) {
                    break;
                }
            }
            if (i < vol->bcache_size) break;
        }
    }
    if (i >= vol->bcache_size) {

        new_bcache_size = (
            vol->bcache_size < 16
        ) ? 16 : vol->bcache_size << 1;

        status = FSW_DO_ALLOC(
            new_bcache_size * sizeof (struct fsw_blockcache),
            &new_bcache
        );
        if (status) {
            FSW_MSG_L03((
                FSW_MSG_STR(
                    "FSW_CORE: fsw_block_get ... Leaving with Status '%d' Error (Tag_01)\n"
                ), status
            ));

            return status;
        }

        if (vol->bcache_size > 0) {
            FSW_DO_MEMCPY(
                new_bcache, vol->bcache,
                vol->bcache_size * sizeof (struct fsw_blockcache)
            );
        }

        for (i = vol->bcache_size; i < new_bcache_size; i++) {
            new_bcache[i].refcount = 0;
            new_bcache[i].cache_level = 0;
            new_bcache[i].phys_bno = (fsw_u64) FSW_INVALID_BNO;
            new_bcache[i].data = NULL;
        }
        i = vol->bcache_size;

        if (vol->bcache != NULL) {
            FSW_DO_FREE(vol->bcache);
        }
        vol->bcache = new_bcache;
        vol->bcache_size = new_bcache_size;
    }
    vol->bcache[i].phys_bno = (fsw_u64) FSW_INVALID_BNO;

    if (vol->bcache[i].data == NULL) {
        status = FSW_DO_ALLOC(
            vol->phys_blocksize,
            &vol->bcache[i].data
        );
        if (status) {
            FSW_MSG_L03((
                FSW_MSG_STR(
                    "FSW_CORE: fsw_block_get ... Leaving with Status '%d' Error (Tag_02)\n"
                ), status
            ));

            return status;
        }
    }

    status = vol->host_table->read_block (
        vol, phys_bno,
        vol->bcache[i].data
    );
    if (status) {
        FSW_MSG_L03((
            FSW_MSG_STR(
                "FSW_CORE: fsw_block_get ... Leaving with Status '%d' Error (Tag_03)\n"
            ), status
        ));

        return status;
    }

    vol->bcache[i].phys_bno = phys_bno;
    vol->bcache[i].cache_level = cache_level;
    vol->bcache[i].refcount = 1;

    *buffer_out = vol->bcache[i].data;
    return FSW_SUCCESS;
}

void fsw_block_release (
    struct VOLSTRUCTNAME *vol,
    fsw_u64               phys_bno,
    void                 *buffer
) {
    fsw_u32 i;

    for (i = 0; i < vol->bcache_size; i++) {
        if (vol->bcache[i].refcount >  0 &&
            vol->bcache[i].phys_bno == phys_bno
        ) {
            vol->bcache[i].refcount--;
        }
    }
}

static void fsw_blockcache_free (
    struct fsw_volume *vol
) {
    fsw_u32 i;

    for (i = 0; i < vol->bcache_size; i++) {
        if (vol->bcache[i].data != NULL) {
            FSW_DO_FREE(vol->bcache[i].data);
        }
    }
    if (vol->bcache != NULL) {
        FSW_DO_FREE(vol->bcache);
        vol->bcache = NULL;
    }
    vol->bcache_size = 0;
    fsw_efi_clear_cache();
}

static void fsw_dnode_register (
    struct fsw_volume *vol,
    struct fsw_dnode  *dno
) {
    dno->next = vol->dnode_head;
    if (vol->dnode_head != NULL) {
        vol->dnode_head->prev = dno;
    }
    dno->prev = NULL;
    vol->dnode_head = dno;
}

fsw_status_t fsw_dnode_create_root_with_tree (
    struct fsw_volume *vol,
    fsw_u64            tree_id,
    fsw_u64            dnode_id,
    struct fsw_dnode **dno_out
) {
    fsw_status_t    status;
    struct fsw_dnode *dno;

    status = fsw_alloc_zero (
        vol->fstype_table->dnode_struct_size,
        (void **) &dno
    );
    if (status) {
        FSW_MSG_L03((
            FSW_MSG_STR(
                "FSW_CORE: fsw_dnode_create_root_with_tree ... Leaving with Status '%d' Error (1)\n"
            ), status
        ));

        return status;
    }

    dno->vol = vol;
    dno->parent = NULL;
    dno->tree_id = tree_id;
    dno->dnode_id = dnode_id;
    dno->refcount = 1;
    dno->type = FSW_DNODE_TYPE_DIR;
    dno->name.type = FSW_STRING_TYPE_EMPTY;

    fsw_dnode_register(vol, dno);

    *dno_out = dno;
    return FSW_SUCCESS;
}

fsw_status_t fsw_dnode_create_root (
    struct fsw_volume *vol,
    fsw_u64            dnode_id,
    struct fsw_dnode **dno_out
) {
	return fsw_dnode_create_root_with_tree (
        vol, 0, dnode_id, dno_out
    );
}

fsw_status_t fsw_dnode_create_with_tree (
    struct fsw_dnode   *parent_dno,
    fsw_u64             tree_id,
    fsw_u64             dnode_id,
    int                 type,
    struct fsw_string  *name,
    struct fsw_dnode  **dno_out
) {
    fsw_status_t       status;
    struct fsw_volume *vol = parent_dno->vol;
    struct fsw_dnode  *dno;

    for (dno = vol->dnode_head; dno; dno = dno->next) {
        if (dno->dnode_id == dnode_id && dno->tree_id == tree_id) {
            fsw_dnode_retain (dno);

            *dno_out = dno;
            return FSW_SUCCESS;
        }
    }

    status = fsw_alloc_zero (
        vol->fstype_table->dnode_struct_size,
        (void **) &dno
    );
    if (status) {
        FSW_MSG_L03((
            FSW_MSG_STR(
                "FSW_CORE: fsw_dnode_create_with_tree ... Leaving with Status '%d' Error (1)\n"
            ), status
        ));

        return status;
    }

    dno->vol = vol;
    dno->parent = parent_dno;
    fsw_dnode_retain (dno->parent);
    dno->tree_id = tree_id;
    dno->dnode_id = dnode_id;
    dno->type = type;
    dno->refcount = 1;
    status = fsw_strdup_coerce (
        &dno->name,
        vol->host_table->native_string_type,
        name
    );
    if (status) {
        FSW_DO_FREE(dno);

        FSW_MSG_L03((
            FSW_MSG_STR(
                "FSW_CORE: fsw_dnode_create_with_tree ... Leaving with Status '%d' Error (2)\n"
            ), status
        ));

        return status;
    }

    fsw_dnode_register (vol, dno);

    *dno_out = dno;
    return FSW_SUCCESS;
}

fsw_status_t fsw_dnode_create (
    struct fsw_dnode   *parent_dno,
    fsw_u64             dnode_id,
    int                 type,
    struct fsw_string  *name,
    struct fsw_dnode  **dno_out
) {
	return fsw_dnode_create_with_tree (
        parent_dno, 0, dnode_id,
        type, name, dno_out
    );
}

void fsw_dnode_retain (
    struct fsw_dnode *dno
) {
    dno->refcount++;
}

void fsw_dnode_release (
    struct fsw_dnode *dno
) {
    struct fsw_volume *vol = dno->vol;
    struct fsw_dnode *parent_dno;

    dno->refcount--;

    if (dno->refcount == 0) {
        parent_dno = dno->parent;

        if (dno->next)              dno->next->prev = dno->prev;
        if (dno->prev)              dno->prev->next = dno->next;
        if (vol->dnode_head == dno) vol->dnode_head = dno->next;

        vol->fstype_table->dnode_free (vol, dno);

        fsw_strfree (&dno->name);
        FSW_DO_FREE(dno);

        if (parent_dno) fsw_dnode_release (parent_dno);
    }
}

fsw_status_t fsw_dnode_fill (
    struct fsw_dnode *dno
) {

    return dno->vol->fstype_table->dnode_fill (dno->vol, dno);
}

fsw_status_t fsw_dnode_stat(
    struct fsw_dnode      *dno,
    struct fsw_dnode_stat *sb
) {
    fsw_status_t    status;

    status = fsw_dnode_fill (dno);
    if (status) {
        FSW_MSG_L03((
            FSW_MSG_STR(
                "FSW_CORE: fsw_dnode_stat ... Exit on Failure to Load Node Info\n"
            )
        ));

        return status;
    }

    sb->used_bytes = 0;
    status = dno->vol->fstype_table->dnode_stat (
        dno->vol, dno, sb
    );
    if (!status && !sb->used_bytes) {
        sb->used_bytes = FSW_U64_DIV(
            dno->size + dno->vol->log_blocksize - 1,
            dno->vol->log_blocksize
        );
    }

    return status;
}

fsw_status_t fsw_dnode_lookup (
    struct fsw_dnode   *dno,
    struct fsw_string  *lookup_name,
    struct fsw_dnode  **child_dno_out
) {
    fsw_status_t    status;

    status = fsw_dnode_fill (dno);
    if (status) {
        FSW_MSG_L03((
            FSW_MSG_STR(
                "FSW_CORE: fsw_dnode_lookup ... Exit on Failure to Load Node Info\n"
            ), status
        ));

        return status;
    }

    if (dno->type != FSW_DNODE_TYPE_DIR) {
        FSW_MSG_L03((
            FSW_MSG_STR(
                "FSW_CORE: fsw_dnode_lookup ... Leaving with Status: FSW_UNSUPPORTED\n"
            )
        ));

        return FSW_UNSUPPORTED;
    }

    return dno->vol->fstype_table->dir_lookup (
        dno->vol, dno,
        lookup_name,
        child_dno_out
    );
}

fsw_status_t fsw_dnode_lookup_path (
    struct fsw_dnode *dno,
    struct fsw_string *lookup_path,
    char separator,
    struct fsw_dnode **child_dno_out
) {
    fsw_status_t    status;
    struct fsw_volume *vol = dno->vol;
    struct fsw_dnode *child_dno = NULL;
    struct fsw_string lookup_name;
    struct fsw_string remaining_path;
    int             root_if_empty;

    remaining_path = *lookup_path;
    fsw_dnode_retain (dno);

    root_if_empty = 1;
    while (1) {

        fsw_strsplit (
            &lookup_name, &remaining_path, separator
        );

        FSW_MSG_L03((
            FSW_MSG_STR(
                "FSW_CORE: fsw_dnode_lookup_path ... Split Path into '%s' and '%s'\n"
            ), lookup_name.data, remaining_path.data
        ));

        if (fsw_strlen (&lookup_name) == 0) {

            child_dno = (
                root_if_empty
            ) ? vol->root : dno;
            fsw_dnode_retain (child_dno);
        }
        else {

            status = fsw_dnode_fill (dno);
            if (status) {
                FSW_MSG_L03((
                    FSW_MSG_STR(
                        "FSW_CORE: fsw_dnode_lookup_path ... Exit on Failure to Load Node Info (1)\n"
                    )
                ));

                goto errorexit;
            }

            if (dno->type == FSW_DNODE_TYPE_SYMLINK) {
                status = fsw_dnode_resolve (dno, &child_dno);
                if (status) {
                    FSW_MSG_L03((
                        FSW_MSG_STR(
                            "FSW_CORE: fsw_dnode_lookup_path ... Exit on Failure to Resolve Symlink\n"
                        )
                    ));

                    goto errorexit;
                }

                fsw_dnode_release (dno);
                dno = child_dno;
                child_dno = NULL;

                status = fsw_dnode_fill (dno);
                if (status) {
                    FSW_MSG_L03((
                        FSW_MSG_STR(
                            "FSW_CORE: fsw_dnode_lookup ... Exit on Failure to Load Node Info (2)\n"
                        )
                    ));

                    goto errorexit;
                }
            }

            if (dno->type != FSW_DNODE_TYPE_DIR) {
                status = FSW_UNSUPPORTED;

                FSW_MSG_L03((
                    FSW_MSG_STR(
                        "FSW_CORE: fsw_dnode_lookup_path ... Exit with Directory Error 'FSW_UNSUPPORTED'\n"
                    )
                ));

                goto errorexit;
            }

            if (fsw_streq_cstr (&lookup_name, ".")) {

                child_dno = dno;
                fsw_dnode_retain (child_dno);

                FSW_MSG_L03((
                    FSW_MSG_STR(
                        "FSW_CORE: fsw_dnode_lookup_path ... Handling Special Case ( . )\n"
                    )
                ));
            }
            else if (
                fsw_streq_cstr (&lookup_name, "..")
            ) {

                if (dno->parent == NULL) {

                    status = FSW_NOT_FOUND;

                    FSW_MSG_L03((
                        FSW_MSG_STR(
                            "FSW_CORE: fsw_dnode_lookup_path ... Handling Special Case ( .. )\n"
                        )
                    ));

                    goto errorexit;
                }

                child_dno = dno->parent;
                fsw_dnode_retain (child_dno);
            }
            else {

                status = vol->fstype_table->dir_lookup (
                    vol, dno,
                    &lookup_name, &child_dno
                );
                if (status) {
                    FSW_MSG_L03((
                        FSW_MSG_STR(
                            "FSW_CORE: fsw_dnode_lookup_path ... Exit on Failed Actual Lookup with Error '%d'\n"
                        ), status
                    ));

                    goto errorexit;
                }
            }
        }
        if (root_if_empty) root_if_empty = 0;

        fsw_dnode_release (dno);
        dno = child_dno;
        child_dno = NULL;

        FSW_MSG_L03((
            FSW_MSG_STR(
                "FSW_CORE: fsw_dnode_lookup_path ... Current Inode ID is %d\n"
            ), dno->dnode_id
        ));

        if (remaining_path.len < 1) break;
    }

    FSW_MSG_L03((
        FSW_MSG_STR(
            "FSW_CORE: fsw_dnode_lookup_path ... Leaving with Status: FSW_SUCCESS\n"
        )
    ));

    *child_dno_out = dno;
    return FSW_SUCCESS;

errorexit:
    if (status == FSW_NOT_FOUND) {
        FSW_MSG_L03((
            FSW_MSG_STR(
                "FSW_CORE: fsw_dnode_lookup_path ... Leaving with Status: FSW_NOT_FOUND\n"
            )
        ));
    }
    else {
        FSW_MSG_L03((
            FSW_MSG_STR(
                "FSW_CORE: fsw_dnode_lookup_path ... Leaving with Status '%d' Error\n"
            ), status
        ));
    }

    fsw_dnode_release (dno);
    if (child_dno != NULL) {
        fsw_dnode_release (child_dno);
    }

    return status;
}

fsw_status_t fsw_dnode_dir_read (
    struct fsw_shandle  *shand,
    struct fsw_dnode   **child_dno_out
) {
    fsw_status_t      status;
    fsw_u64           saved_pos;
    struct fsw_dnode *dno = shand->dnode;

    if (dno->type != FSW_DNODE_TYPE_DIR) {
        FSW_MSG_L03((
            FSW_MSG_STR(
                "FSW_CORE: fsw_dnode_dir_read ... Leaving with Status: FSW_UNSUPPORTED\n"
            )
        ));

        return FSW_UNSUPPORTED;
    }

    saved_pos = shand->pos;
    status = dno->vol->fstype_table->dir_read (
        dno->vol, dno, shand, child_dno_out
    );
    if (status == FSW_SUCCESS) {
        FSW_MSG_L03((
            FSW_MSG_STR(
                "FSW_CORE: fsw_dnode_dir_read ... Leaving with Status: FSW_SUCCESS\n"
            )
        ));
    }
    else {
        shand->pos = saved_pos;

        if (status == FSW_NOT_FOUND) {
            FSW_MSG_L03((
                FSW_MSG_STR(
                    "FSW_CORE: fsw_dnode_dir_read ... Leaving with Status: FSW_NOT_FOUND\n"
                )
            ));
        }
        else {
            FSW_MSG_L03((
                FSW_MSG_STR(
                    "FSW_CORE: fsw_dnode_dir_read ... Leaving with Status '%d' Error\n"
                ), status
            ));
        }
    }

    return status;
}

fsw_status_t fsw_dnode_readlink (
    struct fsw_dnode  *dno,
    struct fsw_string *target_name
) {
    fsw_status_t    status;

    status = fsw_dnode_fill (dno);
    if (status) {
        FSW_MSG_L03((
            FSW_MSG_STR(
                "FSW_CORE: fsw_dnode_readlink ... Leaving with Status '%d' Error\n"
            ), status
        ));

        return status;
    }

    if (dno->type != FSW_DNODE_TYPE_SYMLINK) {
        FSW_MSG_L03((
            FSW_MSG_STR(
                "FSW_CORE: fsw_dnode_readlink ... Leaving with Status: FSW_UNSUPPORTED (Not Symlink)\n"
            )
        ));

        return FSW_UNSUPPORTED;
    }

    return dno->vol->fstype_table->readlink (
        dno->vol, dno, target_name
    );
}

fsw_status_t fsw_dnode_readlink_data (
    struct fsw_dnode  *dno,
    struct fsw_string *link_target
) {
    fsw_status_t       status;
    fsw_u32            buffer_size;
    char               buffer[FSW_PATH_MAX];

    if (dno->size > FSW_PATH_MAX) {
        return FSW_VOLUME_CORRUPTED;
    }

    struct fsw_string s;
    s.type = FSW_STRING_TYPE_ISO88591;
    s.size = s.len = (int)dno->size;
    s.data = buffer;

    struct fsw_shandle shand;
    shand.extent.type       =    0;
    shand.extent.log_start  =    0;
    shand.extent.log_count  =    0;
    shand.extent.phys_start =    0;
    shand.extent.buffer     = NULL;

    status = fsw_shandle_open (dno, &shand);
    if (status) return status;

    buffer_size = (fsw_u32)s.size;
    status = fsw_shandle_read (
        &shand, &buffer_size, buffer
    );

    fsw_shandle_close(&shand);
    if (status) return status;

    if ((int)buffer_size < s.size) {
        return FSW_VOLUME_CORRUPTED;
    }

    status = fsw_strdup_coerce (
        link_target,
        dno->vol->host_string_type, &s
    );

    return status;
}

fsw_status_t fsw_dnode_resolve (
    struct fsw_dnode  *dno,
    struct fsw_dnode **target_dno_out
) {
    fsw_status_t    status = FSW_NOT_FOUND;
    struct fsw_string target_name;
    struct fsw_dnode *target_dno;

    int link_count = 40;

    fsw_dnode_retain (dno);

    while (--link_count > 0) {

        status = fsw_dnode_fill(dno);
        if (status) {
            FSW_MSG_L03((
                FSW_MSG_STR(
                    "FSW_CORE: fsw_dnode_resolve ... Leaving with Status: 'FSW_SUCCESS'\n"
                )
            ));

            goto errorexit;
        }

        if (dno->type != FSW_DNODE_TYPE_SYMLINK) {

            *target_dno_out = dno;
            return FSW_SUCCESS;
        }

        if (dno->parent == NULL) {

            FSW_MSG_L01((
                FSW_MSG_STR(
                    "FSW_CORE: fsw_dnode_resolve ... Leaving with Status: 'FSW_NOT_FOUND' (dno->parent==NULL)\n"
                )
            ));

            status = FSW_NOT_FOUND;
            goto errorexit;
        }

        FSW_MSG_L02((
            FSW_MSG_STR(
                "FSW_CORE: fsw_dnode_resolve ... Resolve Symlink:- 'Begin'\n"
            )
        ));

        status = fsw_dnode_readlink (
            dno, &target_name
        );
        if (!status) {

            status = fsw_dnode_lookup_path (
                dno->parent,
                &target_name, '/',
                &target_dno
            );
        }

        fsw_strfree (&target_name);

        if (status) {
            FSW_MSG_L01((
                FSW_MSG_STR(
                    "FSW_CORE: fsw_dnode_resolve ... Leaving with Status '%d' Error\n"
                ), status
            ));

            goto errorexit;
        }

        FSW_MSG_L02((
            FSW_MSG_STR(
                "FSW_CORE: fsw_dnode_resolve ... Resolve Symlink:- 'Ended'\n"
            )
        ));

        fsw_dnode_release (dno);
        dno = target_dno;
    }

    if (link_count == 0) {
        FSW_MSG_L01((
            FSW_MSG_STR(
                "FSW_CORE: fsw_dnode_resolve ... Leaving with Status: 'FSW_NOT_FOUND' (link_count==0)\n"
            )
        ));

        status = FSW_NOT_FOUND;
    }

errorexit:
    fsw_dnode_release (dno);

    #if FSW_DEBUG_LEVEL >= 1
    if (status) {
        FSW_MSG_L01((
            FSW_MSG_STR(
                "FSW_CORE: fsw_dnode_resolve ... Leaving with Status '%d' Error\n"
            ), status
        ));
    }
    else {
        FSW_MSG_L03((
            FSW_MSG_STR(
                "FSW_CORE: fsw_dnode_resolve ... Leaving with Status: FSW_SUCCESS\n"
            )
        ));
    }
    #endif

    return status;
}

fsw_status_t fsw_shandle_open (
    struct fsw_dnode   *dno,
    struct fsw_shandle *shand
) {
    fsw_status_t    status;
    struct fsw_volume *vol = dno->vol;

    status = vol->fstype_table->dnode_fill(vol, dno);
    if (status) {
        FSW_MSG_L01((
            FSW_MSG_STR(
                "FSW_CORE: fsw_shandle_open ... Leaving with Status '%d' Error\n"
            ), status
        ));

        return status;
    }

    fsw_dnode_retain (dno);

    shand->dnode = dno;
    shand->pos = 0;
    shand->extent.buffer = NULL;
    shand->extent.type = FSW_EXTENT_TYPE_INVALID;

    FSW_MSG_L03((
        FSW_MSG_STR(
            "FSW_CORE: fsw_shandle_open ... Leaving with Status: 'FSW_SUCCESS'\n"
        )
    ));

    return FSW_SUCCESS;
}

void fsw_shandle_close(
    struct fsw_shandle *shand
) {
    if (shand->extent.buffer &&
        shand->extent.type   == FSW_EXTENT_TYPE_BUFFER
    ) {
        FSW_DO_FREE(shand->extent.buffer);
    }
    fsw_dnode_release (shand->dnode);
}

fsw_status_t fsw_shandle_read (
    struct fsw_shandle *shand,
    fsw_u32 *buffer_size_inout,
    void *buffer_in
) {
    fsw_status_t       status;
    struct fsw_dnode  *dno = shand->dnode;
    struct fsw_volume *vol = dno->vol;
    fsw_u8            *block_buffer;
    fsw_u8            *buffer;
    fsw_u64            pos_in_extent;
    fsw_u64            pos_in_physblock;
    fsw_u64            buflen, copylen, pos;
    fsw_u64            remaining_file_data;
    fsw_u64            log_bno, phys_bno;
    fsw_u32            cache_level;
    BOOLEAN            void_hole;

    if (shand->pos >= dno->size) {

        *buffer_size_inout = 0;

        return FSW_SUCCESS;
    }

    pos = shand->pos;
    buffer = buffer_in;
    remaining_file_data = dno->size - pos;

    cache_level = (
        dno->type != FSW_DNODE_TYPE_FILE
    ) ? 1 : 0;

    buflen = *buffer_size_inout;
    if (buflen > remaining_file_data) {
        buflen = remaining_file_data;
    }
    while (buflen > 0) {
        void_hole = 0;

        FSW_MSG_L02((
            FSW_MSG_STR(
                "FSW_CORE: fsw_shandle_read ... buflen==%llu this_pos==%llu\n"
            ),
            (unsigned long long) buflen,
            (unsigned long long) pos
        ));

        log_bno = FSW_U64_DIV(pos, vol->log_blocksize);
        FSW_MSG_L03((
            FSW_MSG_STR(
                "FSW_CORE: fsw_shandle_read ... log_bno==%llu log_start==%llu\n"
            ),
            (unsigned long long) log_bno,
            (unsigned long long) shand->extent.log_start
        ));

        if (shand->extent.type == FSW_EXTENT_TYPE_INVALID ||
            log_bno <  shand->extent.log_start            ||
            log_bno >= shand->extent.log_start + shand->extent.log_count
        ) {

            if (shand->extent.buffer &&
                shand->extent.type   == FSW_EXTENT_TYPE_BUFFER
            ) {
                FSW_DO_FREE(shand->extent.buffer);
            }

            shand->extent.log_start = log_bno;
            status = vol->fstype_table->get_extent (
                vol, dno, &shand->extent
            );
            if (status) {
                void_hole = (
                    status == FSW_IO_ERROR &&
                    shand->extent.type == FSW_EXTENT_TYPE_INVALID
                );
                if (!void_hole) {
                    shand->extent.type = FSW_EXTENT_TYPE_INVALID;

                    FSW_MSG_L01((
                        FSW_MSG_STR(
                            "FSW_CORE: fsw_shandle_read ... Leaving with Status '%d' Error (Invalid Storage Handle Extents)\n"
                        ), status
                    ));

                    return status;
                }

                FSW_MSG_L01((
                    FSW_MSG_STR(
                        "FSW_CORE: fsw_shandle_read ... Located Sparse Hole at log_start==%llu\n"
                    ), (unsigned long long) shand->extent.log_start
                ));
            }
        }

        pos_in_extent = pos - (
            shand->extent.log_start * vol->log_blocksize
        );

        if (shand->extent.type == FSW_EXTENT_TYPE_PHYSBLOCK) {

            phys_bno = FSW_U64_DIV(
                pos_in_extent,
                vol->phys_blocksize
            ) + shand->extent.phys_start;
            pos_in_physblock = pos_in_extent & (vol->phys_blocksize - 1);

            copylen = vol->phys_blocksize - pos_in_physblock;
            if (copylen > buflen) copylen = buflen;

            FSW_MSG_L03((
                FSW_MSG_STR(
                    "FSW_CORE: fsw_shandle_read ... phys_blocksize==%llu this_copylen==%llu pos_in_physblock==%llu\n"
                ),
                (unsigned long long) vol->phys_blocksize,
                (unsigned long long) copylen,
                (unsigned long long) pos_in_physblock
            ));

            status = fsw_block_get (
                vol, phys_bno,
                cache_level,
                (void **) &block_buffer
            );
            if (status) {
                FSW_MSG_L01((
                    FSW_MSG_STR(
                        "FSW_CORE: fsw_shandle_read ... Leaving with Status '%d' Error ('fsw_block_get' failure)\n"
                    ), status
                ));

                return status;
            }

            FSW_DO_MEMCPY(
                buffer,
                block_buffer + pos_in_physblock,
                copylen
            );
            fsw_block_release (
                vol, phys_bno,
                block_buffer
            );
        }
        else {

            copylen = (
                shand->extent.log_count * vol->log_blocksize
            ) - pos_in_extent;
            if (copylen > buflen) copylen = buflen;

            if (shand->extent.type == FSW_EXTENT_TYPE_BUFFER) {
                FSW_DO_MEMCPY(
                    buffer,
                    ((fsw_u8 *)shand->extent.buffer) + pos_in_extent,
                    copylen
                );
            }
            else {

                FSW_DO_MEMZERO(buffer, copylen);

                #if FSW_DEBUG_LEVEL >= 1
                if (void_hole) {
                    FSW_MSG_L01((
                        FSW_MSG_STR(
                            "FSW_CORE: fsw_shandle_read ... Plugged Sparse Hole at log_start==%llu (log_count==%llu)\n"
                        ),
                        (unsigned long long) shand->extent.log_start,
                        (unsigned long long) shand->extent.log_count
                    ));
                }

                FSW_MSG_L03((
                    FSW_MSG_STR(
                        "FSW_CORE: fsw_shandle_read ... this_copylen==%llu pos_in_extent==%llu\n"
                    ),
                    (unsigned long long) copylen,
                    (unsigned long long) pos_in_extent
                ));
                #endif
            }
        }

        buffer += copylen;
        pos    += copylen;

        buflen = (
            buflen > copylen
        ) ? buflen - copylen : 0;

        #if FSW_DEBUG_LEVEL >= 2
        if (buflen > 0) {
            FSW_MSG_L02((
                FSW_MSG_STR("****  *  *  *  ****\n\n")
            ));
        }
        else {
            FSW_MSG_L02((
                FSW_MSG_STR("=======  *  =======\n\n")
            ));
        }
        #endif
    }

    *buffer_size_inout = (fsw_u32)(pos - shand->pos);
    shand->pos = pos;

    return FSW_SUCCESS;
}
