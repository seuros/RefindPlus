// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2021-2026 Dayo Akanji
// SPDX-FileCopyrightText: 2006 Christoph Pfisterer

#include "fsw_iso9660.h"

static fsw_status_t fsw_iso9660_volume_mount (
    struct fsw_iso9660_volume *vol
);
static void fsw_iso9660_volume_free (
    struct fsw_iso9660_volume *vol
);
static fsw_status_t fsw_iso9660_volume_stat (
    struct fsw_iso9660_volume *vol,
    struct fsw_volume_stat    *sb
);

static fsw_status_t fsw_iso9660_dnode_fill (
    struct fsw_iso9660_volume *vol,
    struct fsw_iso9660_dnode  *dno
);
static void fsw_iso9660_dnode_free (
    struct fsw_iso9660_volume *vol,
    struct fsw_iso9660_dnode  *dno
);
static fsw_status_t fsw_iso9660_dnode_stat (
    struct fsw_iso9660_volume *vol,
    struct fsw_iso9660_dnode  *dno,
    struct fsw_dnode_stat     *sb
);
static fsw_status_t fsw_iso9660_get_extent (
    struct fsw_iso9660_volume *vol,
    struct fsw_iso9660_dnode  *dno,
    struct fsw_extent         *extent
);
static fsw_status_t fsw_iso9660_dir_lookup (
    struct fsw_iso9660_volume  *vol,
    struct fsw_iso9660_dnode   *dno,
    struct fsw_string          *lookup_name,
    struct fsw_iso9660_dnode  **child_dno
);
static fsw_status_t fsw_iso9660_dir_read (
    struct fsw_iso9660_volume  *vol,
    struct fsw_iso9660_dnode   *dno,
    struct fsw_shandle         *shand,
    struct fsw_iso9660_dnode  **child_dno
);
static fsw_status_t fsw_iso9660_read_dirrec (
    struct fsw_iso9660_volume    *vol,
    struct fsw_shandle           *shand,
    struct iso9660_dirrec_buffer *dirrec_buffer
);

static fsw_status_t fsw_iso9660_readlink (
    struct fsw_iso9660_volume *vol,
    struct fsw_iso9660_dnode  *dno,
    struct fsw_string         *link
);

static fsw_status_t rr_find_sp (
    struct iso9660_dirrec          *dirrec,
    struct fsw_rock_ridge_susp_sp **psp
);
static fsw_status_t rr_find_nm (
    struct fsw_iso9660_volume *vol,
    struct iso9660_dirrec     *dirrec,
    int                        off,
    struct fsw_string         *str
);
static fsw_status_t rr_read_ce (
    struct fsw_iso9660_volume    *vol,
    union fsw_rock_ridge_susp_ce *ce,
    fsw_u8                       *begin
);

struct fsw_fstype_table FSW_FSTYPE_TABLE_NAME(iso9660) = {
    { FSW_STRING_TYPE_ISO88591, 4, 4, "iso9660" },
    sizeof (struct fsw_iso9660_volume),
    sizeof (struct fsw_iso9660_dnode),

    fsw_iso9660_volume_mount,
    fsw_iso9660_volume_free,
    fsw_iso9660_volume_stat,
    fsw_iso9660_dnode_fill,
    fsw_iso9660_dnode_free,
    fsw_iso9660_dnode_stat,
    fsw_iso9660_get_extent,
    fsw_iso9660_dir_lookup,
    fsw_iso9660_dir_read,
    fsw_iso9660_readlink,
};

struct fsw_fstype_table *fsw_active_fstype_table = &FSW_FSTYPE_TABLE_NAME(iso9660);
CONST CHAR16            *fsw_active_fstype_name  = L"iso9660";

static fsw_status_t rr_find_sp(struct iso9660_dirrec *dirrec, struct fsw_rock_ridge_susp_sp **psp)
{
    fsw_u8 *r;
    int off = 0;
    struct fsw_rock_ridge_susp_sp *sp;
    r = (fsw_u8 *)((fsw_u8 *)dirrec + sizeof (*dirrec) + dirrec->file_identifier_length);
    off = (int)(r - (fsw_u8 *)dirrec);
    while (off < dirrec->dirrec_length)
    {
        if (*r == 'S')
        {
            sp = (struct fsw_rock_ridge_susp_sp *)r;
            if (    sp->e.sig[0] == 'S'
                && sp->e.sig[1] == 'P'
                && sp->magic[0] == 0xbe
                && sp->magic[1] == 0xef)
            {
                *psp = sp;
                return FSW_SUCCESS;
            }
        }
        r++;
        off = (int)(r - (fsw_u8 *)dirrec);
    }
    *psp = NULL;
    return FSW_NOT_FOUND;
}

static fsw_status_t rr_find_nm(struct fsw_iso9660_volume *vol, struct iso9660_dirrec *dirrec, int off, struct fsw_string *str)
{
    fsw_u8 *r, *begin;
    int fCe = 0;
    struct fsw_rock_ridge_susp_nm *nm;
    int limit = dirrec->dirrec_length;
    begin = (fsw_u8 *)dirrec;
    r = (fsw_u8 *)dirrec + off;
    str->data = NULL;
    str->len = 0;
    str->size = 0;
    str->type = 0;
    while (off < limit)
    {
        if (r[0] == 'C' && r[1] == 'E' && r[2] == 28)
        {
            int rc;
            int ce_off;
            union fsw_rock_ridge_susp_ce *ce;
            if (fCe == 0)
                fsw_alloc_zero (ISO9660_BLOCKSIZE, (void *) &begin);
            fCe = 1;

            ce = (union fsw_rock_ridge_susp_ce *)r;
            limit = ISOINT(ce->X.len);
            ce_off = ISOINT(ce->X.offset);
            rc = rr_read_ce(vol, ce, begin);
            if (rc != FSW_SUCCESS)
            {
                FSW_DO_FREE(begin);
                return rc;
            }
            begin += ce_off;
            r = begin;
        }
        if (r[0] == 'N' && r[1] == 'M')
        {
            nm = (struct fsw_rock_ridge_susp_nm *)r;
            if (    nm->e.sig[0] == 'N'
                && nm->e.sig[1] == 'M')
            {
                int len = 0;
                fsw_u8 *tmp = NULL;
                if (nm->flags & RR_NM_CURR)
                {
                     str->len = 1;
                     if (str->data == NULL) {
                         fsw_alloc_zero ((2 * str->len) + 1, (void **) &str->data);
                     }
                     fsw_memdup(str->data, ".", str->len);
                     goto done;
                }
                if (nm->flags & RR_NM_PARE)
                {
                     str->len = 2;
                     if (str->data == NULL) {
                         fsw_alloc_zero ((2 * str->len) + 1, (void **) &str->data);
                     }
                     fsw_memdup(str->data, "..", str->len);
                     goto done;
                }
                len = nm->e.len - sizeof (struct fsw_rock_ridge_susp_nm) + 1;
                fsw_alloc_zero (str->len + len, (void **) &tmp);
                if (str->data != NULL)
                {
                    FSW_DO_MEMCPY(tmp, str->data, str->len);
                    FSW_DO_FREE(str->data);
                }

                FSW_DO_MEMCPY(tmp + str->len, &nm->name[0], len);
                str->data = tmp;
                str->len += len;

                if ((nm->flags & RR_NM_CONT) == 0)
                    goto done;
            }
        }
        r++;
        off = (int)(r - (fsw_u8 *)begin);
    }
    if (fCe == 1)
        FSW_DO_FREE(begin);
    return FSW_NOT_FOUND;
done:
    str->type = FSW_STRING_TYPE_ISO88591;
    str->size = str->len;
    if (fCe == 1)
        FSW_DO_FREE(begin);
    return FSW_SUCCESS;
}

static fsw_status_t rr_read_ce(struct fsw_iso9660_volume *vol, union fsw_rock_ridge_susp_ce *ce, fsw_u8 *begin)
{
    int rc;

    rc = vol->g.host_table->read_block(&vol->g, ISOINT(ce->X.block_loc), begin);
    if (rc != FSW_SUCCESS)
        return rc;

    return FSW_SUCCESS;
}

static fsw_status_t fsw_iso9660_volume_mount(struct fsw_iso9660_volume *vol)
{
    fsw_status_t    status;
    void            *buffer;
    fsw_u32         blockno;
    struct iso9660_volume_descriptor *voldesc;
    struct iso9660_primary_volume_descriptor *pvoldesc;
    fsw_u32         voldesc_type;
    int             i;
    struct fsw_string s;
    struct iso9660_dirrec rootdir;
    int sua_pos;
    char *sig;
    struct fsw_rock_ridge_susp_entry *entry;

    fsw_set_blocksize (vol, ISO9660_BLOCKSIZE, ISO9660_BLOCKSIZE);
    blockno = ISO9660_SUPERBLOCK_BLOCKNO;

    do {
        status = fsw_block_get (vol, blockno, 0, &buffer);
        if (status)
            return status;

        voldesc = (struct iso9660_volume_descriptor *)buffer;
        voldesc_type = voldesc->volume_descriptor_type;
        if (FSW_DO_MEMEQ(voldesc->standard_identifier, "CD001", 5)) {

            if (voldesc_type == 1 && voldesc->volume_descriptor_version == 1) {

                if (vol->primary_voldesc) {
                    FSW_DO_FREE(vol->primary_voldesc);
                    vol->primary_voldesc = NULL;
                }
                status = fsw_memdup((void **) &vol->primary_voldesc, voldesc, ISO9660_BLOCKSIZE);
            }
        } else if (!FSW_DO_MEMEQ(voldesc->standard_identifier, "CD", 2)) {

            voldesc_type = 255;
        }

        fsw_block_release (vol, blockno, buffer);
        blockno++;
    } while (!status && voldesc_type != 255);
    if (status)
        return status;

    if (vol->primary_voldesc == NULL)
        return FSW_UNSUPPORTED;
    pvoldesc = vol->primary_voldesc;

    for (i = 32; i > 0; i--)
        if (pvoldesc->volume_identifier[i-1] != ' ')
            break;
    s.type = FSW_STRING_TYPE_ISO88591;
    s.size = s.len = i;
    s.data = pvoldesc->volume_identifier;
    status = fsw_strdup_coerce (&vol->g.label, vol->g.host_string_type, &s);
    if (status)
        return status;

    status = fsw_dnode_create_root(vol, ISO9660_SUPERBLOCK_BLOCKNO << ISO9660_BLOCKSIZE_BITS, &vol->g.root);
    if (status)
        return status;
    FSW_DO_MEMCPY(&vol->g.root->dirrec, &pvoldesc->root_directory, sizeof (struct iso9660_dirrec));

    if (   pvoldesc->escape[0] == 0x25
        && pvoldesc->escape[1] == 0x2f
        && (   pvoldesc->escape[2] == 0x40
            || pvoldesc->escape[2] == 0x43
            || pvoldesc->escape[2] == 0x45))
    {
        FSW_MSG_L03((
            FSW_MSG_STR(
                "FSW_ISO9660: fsw_iso9660_volume_mount ... Success (joliet)\n"
            )
        ));
        vol->fJoliet = 1;
    }

    rootdir = pvoldesc->root_directory;
    sua_pos = (sizeof (struct iso9660_dirrec)) +
            rootdir.file_identifier_length +
            (rootdir.file_identifier_length % 2) - 2;
    FSW_MSG_L03((
        FSW_MSG_STR(
            "FSW_ISO9660: fsw_iso9660_volume_mount ... Success (SUA Pos:%x)\n"
        ), sua_pos
    ));

#if 1
    status = fsw_block_get (vol, ISOINT(rootdir.extent_location), 0, &buffer);
    if (status)
        return status;

    sig   = (char *) buffer + sua_pos;
    entry = (struct fsw_rock_ridge_susp_entry *) sig;

    if (entry->sig[0] == 'S' &&
        entry->sig[1] == 'P'
    ) {
        struct fsw_rock_ridge_susp_sp *sp = (struct fsw_rock_ridge_susp_sp *) entry;
        if (sp->magic[0] == 0xbe && sp->magic[1] == 0xef) {
            vol->fRockRidge = 1;
        } else {
            FSW_MSG_L03((
                FSW_MSG_STR(
                    "FSW_ISO9660: fsw_iso9660_volume_mount: SP magic is not valid\n"
                )
            ));
        }
    }
#endif

    FSW_DO_FREE(vol->primary_voldesc);
    vol->primary_voldesc = NULL;

    FSW_MSG_L03((
        FSW_MSG_STR(
            "FSW_ISO9660: fsw_iso9660_volume_mount: success\n"
        )
    ));

    return FSW_SUCCESS;
}

static void fsw_iso9660_volume_free(struct fsw_iso9660_volume *vol)
{
    if (vol->primary_voldesc)
        FSW_DO_FREE(vol->primary_voldesc);
}

static fsw_status_t fsw_iso9660_volume_stat(struct fsw_iso9660_volume *vol, struct fsw_volume_stat *sb)
{
    sb->total_bytes = 0;
    sb->free_bytes  = 0;
    return FSW_SUCCESS;
}

static fsw_status_t fsw_iso9660_dnode_fill(struct fsw_iso9660_volume *vol, struct fsw_iso9660_dnode *dno)
{

    dno->g.size = ISOINT(dno->dirrec.data_length);
    if (dno->dirrec.file_flags & 0x02)
        dno->g.type = FSW_DNODE_TYPE_DIR;
    else
        dno->g.type = FSW_DNODE_TYPE_FILE;

    return FSW_SUCCESS;
}

static void fsw_iso9660_dnode_free(struct fsw_iso9660_volume *vol, struct fsw_iso9660_dnode *dno)
{
}

static fsw_status_t fsw_iso9660_dnode_stat(struct fsw_iso9660_volume *vol, struct fsw_iso9660_dnode *dno,
                                           struct fsw_dnode_stat *sb)
{
    sb->used_bytes = (dno->g.size + (ISO9660_BLOCKSIZE-1)) & ~(ISO9660_BLOCKSIZE-1);

    return FSW_SUCCESS;
}

static fsw_status_t fsw_iso9660_get_extent(struct fsw_iso9660_volume *vol, struct fsw_iso9660_dnode *dno,
                                           struct fsw_extent *extent)
{

    extent->type = FSW_EXTENT_TYPE_PHYSBLOCK;
    extent->phys_start = ISOINT(dno->dirrec.extent_location);
    extent->log_start = 0;
    extent->log_count = (ISOINT(dno->dirrec.data_length) + (ISO9660_BLOCKSIZE-1)) >> ISO9660_BLOCKSIZE_BITS;
    return FSW_SUCCESS;
}

static fsw_status_t fsw_iso9660_dir_lookup(struct fsw_iso9660_volume *vol, struct fsw_iso9660_dnode *dno,
                                           struct fsw_string *lookup_name, struct fsw_iso9660_dnode **child_dno_out)
{
    fsw_status_t    status;
    struct fsw_shandle shand;
    struct iso9660_dirrec_buffer dirrec_buffer;
    struct iso9660_dirrec *dirrec = &dirrec_buffer.dirrec;

    status = fsw_shandle_open(dno, &shand);
    if (status)
        return status;

    dirrec_buffer.ino = 0;

    while (1) {

        status = fsw_iso9660_read_dirrec(vol, &shand, &dirrec_buffer);
        if (status)
            goto errorexit;
        if (dirrec->dirrec_length == 0) {

            status = FSW_NOT_FOUND;
            goto errorexit;
        }

        if (dirrec->file_identifier_length == 1 &&
            (dirrec->file_identifier[0] == 0 || dirrec->file_identifier[0] == 1))
            continue;

        if (fsw_streq(lookup_name, &dirrec_buffer.name))
            break;
    }

    status = fsw_dnode_create(dno, dirrec_buffer.ino, FSW_DNODE_TYPE_UNKNOWN, &dirrec_buffer.name, child_dno_out);
    if (status == FSW_SUCCESS)
        FSW_DO_MEMCPY(&(*child_dno_out)->dirrec, dirrec, sizeof (struct iso9660_dirrec));

errorexit:
    fsw_shandle_close(&shand);
    return status;
}

static fsw_status_t fsw_iso9660_dir_read(struct fsw_iso9660_volume *vol, struct fsw_iso9660_dnode *dno,
                                         struct fsw_shandle *shand, struct fsw_iso9660_dnode **child_dno_out)
{
    fsw_status_t    status;
    struct iso9660_dirrec_buffer dirrec_buffer;
    struct iso9660_dirrec *dirrec = &dirrec_buffer.dirrec;

    dirrec_buffer.ino = 0;

    while (1) {

        if (shand->pos >= dno->g.size)
            return FSW_NOT_FOUND;
        status = fsw_iso9660_read_dirrec(vol, shand, &dirrec_buffer);
        if (status)
            return status;
        if (dirrec->dirrec_length == 0)
        {

            shand->pos =(shand->pos & ~(vol->g.log_blocksize - 1)) + vol->g.log_blocksize;
            continue;
        }

        if (dirrec->file_identifier_length == 1 &&
            (dirrec->file_identifier[0] == 0 || dirrec->file_identifier[0] == 1))
            continue;
        break;
    }

    status = fsw_dnode_create(dno, dirrec_buffer.ino, FSW_DNODE_TYPE_UNKNOWN, &dirrec_buffer.name, child_dno_out);
    if (status == FSW_SUCCESS)
        FSW_DO_MEMCPY(&(*child_dno_out)->dirrec, dirrec, sizeof (struct iso9660_dirrec));

    return status;
}

static fsw_status_t fsw_iso9660_read_dirrec(struct fsw_iso9660_volume *vol, struct fsw_shandle *shand, struct iso9660_dirrec_buffer *dirrec_buffer)
{
    fsw_status_t    status;
    fsw_u32         i, buffer_size, remaining_size, name_len;
    struct fsw_rock_ridge_susp_sp *sp = NULL;
    struct iso9660_dirrec *dirrec = &dirrec_buffer->dirrec;
    int sp_off;
    int rc;

    dirrec_buffer->ino = (ISOINT(((struct fsw_iso9660_dnode *)shand->dnode)->dirrec.extent_location)
                          << ISO9660_BLOCKSIZE_BITS)
        + (fsw_u32)shand->pos;

    buffer_size = 33;
    status = fsw_shandle_read(shand, &buffer_size, dirrec);
    if (status)
    {

        return status;
    }

    if (buffer_size < 33 || dirrec->dirrec_length == 0) {
        dirrec->dirrec_length = 0;
        return FSW_SUCCESS;
    }
    if (dirrec->dirrec_length < 33 ||
        dirrec->dirrec_length < 33 + dirrec->file_identifier_length)
        return FSW_VOLUME_CORRUPTED;

    buffer_size = remaining_size = dirrec->dirrec_length - 33;
    status = fsw_shandle_read(shand, &buffer_size, dirrec->file_identifier);
    if (status)
        return status;
    if (buffer_size < remaining_size)
        return FSW_VOLUME_CORRUPTED;

     if (vol->fRockRidge)
     {
         sp_off = sizeof (*dirrec) + dirrec->file_identifier_length;
         rc = rr_find_sp(dirrec, &sp);
         if (   rc == FSW_SUCCESS
             && sp != NULL)
         {
            sp_off = (fsw_u8 *) &sp[1] - (fsw_u8*)dirrec + sp->skip;
         }
         rc = rr_find_nm(vol, dirrec, sp_off,  &dirrec_buffer->name);
         if (rc == FSW_SUCCESS)
            return FSW_SUCCESS;
    }

    name_len = dirrec->file_identifier_length;
    for (i = name_len - 1; i > 0; i--) {
        if (dirrec->file_identifier[i] == ';') {
            name_len = i;
            break;
        }
    }
    if (name_len > 0 && dirrec->file_identifier[name_len-1] == '.')
        name_len--;
    dirrec_buffer->name.type = FSW_STRING_TYPE_ISO88591;
    dirrec_buffer->name.len = dirrec_buffer->name.size = name_len;
    dirrec_buffer->name.data = dirrec->file_identifier;

    return FSW_SUCCESS;
}

static fsw_status_t fsw_iso9660_readlink(struct fsw_iso9660_volume *vol, struct fsw_iso9660_dnode *dno,
                                         struct fsw_string *link_target)
{
    fsw_status_t    status;

    if (dno->g.size > FSW_PATH_MAX)
        return FSW_VOLUME_CORRUPTED;

    status = fsw_dnode_readlink_data(dno, link_target);

    return status;
}
