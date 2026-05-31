// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2026 Dayo Akanji
// SPDX-FileCopyrightText: 2006 Christoph Pfisterer

#ifndef _FSW_CORE_H_
#define _FSW_CORE_H_

#include "fsw_base.h"

#define FSW_PATH_MAX (4096)

#define FSW_CONCAT3(a,b,c) a##b##c

#define FSW_FSTYPE_TABLE_NAME(t) FSW_CONCAT3(fsw_,t,_table)

#define FSW_INVALID_BNO 0xFFFFFFFFFFFFFFFF

typedef fsw_u16             fsw_u16_le;
typedef fsw_u16             fsw_u16_be;
typedef fsw_u32             fsw_u32_le;
typedef fsw_u32             fsw_u32_be;
typedef fsw_u64             fsw_u64_le;
typedef fsw_u64             fsw_u64_be;

#define FSW_SWAPVALUE_U16(v) ((((fsw_u16)(v) & 0xff00) >> 8) | \
                              (((fsw_u16)(v) & 0x00ff) << 8))
#define FSW_SWAPVALUE_U32(v) ((((fsw_u32)(v) & 0xff000000UL) >> 24) | \
                              (((fsw_u32)(v) & 0x00ff0000UL) >> 8)  | \
                              (((fsw_u32)(v) & 0x0000ff00UL) << 8)  | \
                              (((fsw_u32)(v) & 0x000000ffUL) << 24))
#define FSW_SWAPVALUE_U64(v) ((((fsw_u64)(v) & 0xff00000000000000ULL) >> 56) | \
                              (((fsw_u64)(v) & 0x00ff000000000000ULL) >> 40) | \
                              (((fsw_u64)(v) & 0x0000ff0000000000ULL) >> 24) | \
                              (((fsw_u64)(v) & 0x000000ff00000000ULL) >> 8)  | \
                              (((fsw_u64)(v) & 0x00000000ff000000ULL) << 8)  | \
                              (((fsw_u64)(v) & 0x0000000000ff0000ULL) << 24) | \
                              (((fsw_u64)(v) & 0x000000000000ff00ULL) << 40) | \
                              (((fsw_u64)(v) & 0x00000000000000ffULL) << 56))

#ifdef FSW_LITTLE_ENDIAN

#define FSW_U16_LE_SWAP(v) (v)
#define FSW_U16_BE_SWAP(v) FSW_SWAPVALUE_U16(v)
#define FSW_U32_LE_SWAP(v) (v)
#define FSW_U32_BE_SWAP(v) FSW_SWAPVALUE_U32(v)
#define FSW_U64_LE_SWAP(v) (v)
#define FSW_U64_BE_SWAP(v) FSW_SWAPVALUE_U64(v)

#define FSW_U16_LE_SIP(var)
#define FSW_U16_BE_SIP(var) (var = FSW_SWAPVALUE_U16(var))
#define FSW_U32_LE_SIP(var)
#define FSW_U32_BE_SIP(var) (var = FSW_SWAPVALUE_U32(var))
#define FSW_U64_LE_SIP(var)
#define FSW_U64_BE_SIP(var) (var = FSW_SWAPVALUE_U64(var))

#else
#ifdef FSW_BIG_ENDIAN

#define FSW_U16_LE_SWAP(v) FSW_SWAPVALUE_U16(v)
#define FSW_U16_BE_SWAP(v) (v)
#define FSW_U32_LE_SWAP(v) FSW_SWAPVALUE_U32(v)
#define FSW_U32_BE_SWAP(v) (v)
#define FSW_U64_LE_SWAP(v) FSW_SWAPVALUE_U64(v)
#define FSW_U64_BE_SWAP(v) (v)

#define FSW_U16_LE_SIP(var) (var = FSW_SWAPVALUE_U16(var))
#define FSW_U16_BE_SIP(var)
#define FSW_U32_LE_SIP(var) (var = FSW_SWAPVALUE_U32(var))
#define FSW_U32_BE_SIP(var)
#define FSW_U64_LE_SIP(var) (var = FSW_SWAPVALUE_U64(var))
#define FSW_U64_BE_SIP(var)

#else
#fail Neither FSW_BIG_ENDIAN nor FSW_LITTLE_ENDIAN are defined
#endif
#endif

#ifndef VOLSTRUCTNAME
#define VOLSTRUCTNAME fsw_volume
#else
struct VOLSTRUCTNAME;
#endif
#ifndef DNODESTRUCTNAME
#define DNODESTRUCTNAME fsw_dnode
#else
struct DNODESTRUCTNAME;
#endif

typedef int fsw_status_t;

enum {
    FSW_SUCCESS,
    FSW_OUT_OF_MEMORY,
    FSW_IO_ERROR,
    FSW_UNSUPPORTED,
    FSW_NOT_FOUND,
    FSW_VOLUME_CORRUPTED,
    FSW_UNKNOWN_ERROR
};

struct fsw_string {
    int         type;
    int         len;
    int         size;
    void        *data;
};

enum {
    FSW_STRING_TYPE_EMPTY,
    FSW_STRING_TYPE_ISO88591,
    FSW_STRING_TYPE_UTF08,
    FSW_STRING_TYPE_UTF16,
    FSW_STRING_TYPE_UTF16_SWAP
};

#ifdef FSW_LITTLE_ENDIAN
#define FSW_STRING_TYPE_UTF16_LE FSW_STRING_TYPE_UTF16
#define FSW_STRING_TYPE_UTF16_BE FSW_STRING_TYPE_UTF16_SWAP
#else
#define FSW_STRING_TYPE_UTF16_LE FSW_STRING_TYPE_UTF16_SWAP
#define FSW_STRING_TYPE_UTF16_BE FSW_STRING_TYPE_UTF16
#endif

#define FSW_STRING_INIT { FSW_STRING_TYPE_EMPTY, 0, 0, NULL }

struct fsw_dnode;
struct fsw_host_table;
struct fsw_fstype_table;

struct fsw_blockcache {
    fsw_u32     refcount;
    fsw_u32     cache_level;
    fsw_u64     phys_bno;
    void        *data;
};

struct fsw_volume {
    fsw_u32                   phys_blocksize;
    fsw_u32                   log_blocksize;
    struct DNODESTRUCTNAME   *root;
    struct fsw_string         label;
    struct fsw_dnode         *dnode_head;
    struct fsw_blockcache    *bcache;
    fsw_u32                   bcache_size;
    void                     *host_data;
    struct fsw_host_table    *host_table;
    struct fsw_fstype_table  *fstype_table;
    int                       host_string_type;
};

struct fsw_dnode {
    fsw_u32                 refcount;
    struct VOLSTRUCTNAME   *vol;
    struct DNODESTRUCTNAME *parent;
    struct fsw_string       name;
    fsw_u64                 tree_id;
    fsw_u64                 dnode_id;
    int                     type;
    fsw_u64                 size;
    struct fsw_dnode       *next;
    struct fsw_dnode       *prev;
};

enum {
    FSW_DNODE_TYPE_UNKNOWN,
    FSW_DNODE_TYPE_FILE,
    FSW_DNODE_TYPE_DIR,
    FSW_DNODE_TYPE_SYMLINK,
    FSW_DNODE_TYPE_SPECIAL
};

struct fsw_extent {
    fsw_u32     type;
    fsw_u64     log_start;
    fsw_u32     log_count;
    fsw_u64     phys_start;
    void        *buffer;
};

enum {
    FSW_EXTENT_TYPE_INVALID,
    FSW_EXTENT_TYPE_SPARSE,
    FSW_EXTENT_TYPE_PHYSBLOCK,
    FSW_EXTENT_TYPE_BUFFER
};

struct fsw_shandle {
    struct fsw_dnode *dnode;

    fsw_u64     pos;
    struct fsw_extent extent;
};

struct fsw_volume_stat {
    fsw_u64     total_bytes;
    fsw_u64     free_bytes;
};

struct fsw_dnode_stat {
    fsw_u64     used_bytes;
    void        *host_data;
};

enum {
    FSW_DNODE_STAT_CTIME,
    FSW_DNODE_STAT_MTIME,
    FSW_DNODE_STAT_ATIME
};

struct fsw_host_table {
    int         native_string_type;

    void         EFIAPI (*change_blocksize)(struct fsw_volume *vol,
                                     fsw_u32 old_phys_blocksize, fsw_u32 old_log_blocksize,
                                     fsw_u32 new_phys_blocksize, fsw_u32 new_log_blocksize);
    fsw_status_t EFIAPI (*read_block)(struct fsw_volume *vol, fsw_u64 phys_bno, void *buffer);
};

struct fsw_fstype_table {
    struct fsw_string name;
    fsw_u32     volume_struct_size;
    fsw_u32     dnode_struct_size;

    fsw_status_t (*volume_mount)(struct VOLSTRUCTNAME *vol);
    void         (*volume_free)(struct VOLSTRUCTNAME *vol);
    fsw_status_t (*volume_stat)(struct VOLSTRUCTNAME *vol, struct fsw_volume_stat *sb);

    fsw_status_t (*dnode_fill)(struct VOLSTRUCTNAME *vol, struct DNODESTRUCTNAME *dno);
    void         (*dnode_free)(struct VOLSTRUCTNAME *vol, struct DNODESTRUCTNAME *dno);
    fsw_status_t (*dnode_stat)(struct VOLSTRUCTNAME *vol, struct DNODESTRUCTNAME *dno,
                               struct fsw_dnode_stat *sb);
    fsw_status_t (*get_extent)(struct VOLSTRUCTNAME *vol, struct DNODESTRUCTNAME *dno,
                               struct fsw_extent *extent);

    fsw_status_t (*dir_lookup)(struct VOLSTRUCTNAME *vol, struct DNODESTRUCTNAME *dno,
                               struct fsw_string *lookup_name, struct DNODESTRUCTNAME **child_dno);
    fsw_status_t (*dir_read)(struct VOLSTRUCTNAME *vol, struct DNODESTRUCTNAME *dno,
                             struct fsw_shandle *shand, struct DNODESTRUCTNAME **child_dno);
    fsw_status_t (*readlink)(struct VOLSTRUCTNAME *vol, struct DNODESTRUCTNAME *dno,
                             struct fsw_string *link_target);
};

fsw_status_t fsw_mount (
    void                     *host_data,
    struct fsw_host_table    *host_table,
    struct fsw_fstype_table  *fstype_table,
    struct fsw_volume       **vol_out
);
fsw_status_t fsw_volume_stat (
    struct fsw_volume      *vol,
    struct fsw_volume_stat *sb
);
fsw_status_t fsw_block_get (
    struct VOLSTRUCTNAME  *vol,
    fsw_u64                phys_bno,
    fsw_u32                cache_level,
    void                 **buffer_out
);

void fsw_unmount (struct fsw_volume *vol);
void fsw_set_blocksize (
    struct VOLSTRUCTNAME *vol,
    fsw_u32               phys_blocksize,
    fsw_u32               log_blocksize
);
void fsw_block_release (
    struct VOLSTRUCTNAME *vol,
    fsw_u64               phys_bno,
    void                 *buffer
);

fsw_status_t fsw_dnode_create_root(struct VOLSTRUCTNAME *vol, fsw_u64 dnode_id, struct DNODESTRUCTNAME **dno_out);
fsw_status_t fsw_dnode_create(struct DNODESTRUCTNAME *parent_dno, fsw_u64 dnode_id, int type,
                              struct fsw_string *name, struct DNODESTRUCTNAME **dno_out);
fsw_status_t fsw_dnode_create_root_with_tree(struct VOLSTRUCTNAME *vol, fsw_u64 tree_id, fsw_u64 dnode_id, struct DNODESTRUCTNAME **dno_out);
fsw_status_t fsw_dnode_create_with_tree(struct DNODESTRUCTNAME *parent_dno, fsw_u64 tree_id, fsw_u64 dnode_id, int type,
                              struct fsw_string *name, struct DNODESTRUCTNAME **dno_out);
void         fsw_dnode_retain (struct fsw_dnode *dno);
void         fsw_dnode_release (struct fsw_dnode *dno);

fsw_status_t fsw_dnode_fill(struct fsw_dnode *dno);
fsw_status_t fsw_dnode_stat(struct fsw_dnode *dno, struct fsw_dnode_stat *sb);

fsw_status_t fsw_dnode_lookup(struct fsw_dnode *dno,
                              struct fsw_string *lookup_name, struct fsw_dnode **child_dno_out);
fsw_status_t fsw_dnode_lookup_path(struct fsw_dnode *dno,
                                   struct fsw_string *lookup_path, char separator,
                                   struct fsw_dnode **child_dno_out);
fsw_status_t fsw_dnode_dir_read(struct fsw_shandle *shand, struct fsw_dnode **child_dno_out);
fsw_status_t fsw_dnode_readlink(struct fsw_dnode *dno, struct fsw_string *link_target);
fsw_status_t fsw_dnode_readlink_data(struct DNODESTRUCTNAME *dno, struct fsw_string *link_target);
fsw_status_t fsw_dnode_resolve(struct fsw_dnode *dno, struct fsw_dnode **target_dno_out);
void fsw_store_time_posix(struct fsw_dnode_stat *sb, int which, fsw_u32 posix_time);
void fsw_store_attr_posix(struct fsw_dnode_stat *sb, fsw_u16 posix_mode);
void fsw_store_attr_efi(struct fsw_dnode_stat *sb, fsw_u16 attr);

fsw_status_t fsw_shandle_open(struct DNODESTRUCTNAME *dno, struct fsw_shandle *shand);
void         fsw_shandle_close(struct fsw_shandle *shand);
fsw_status_t fsw_shandle_read(struct fsw_shandle *shand, fsw_u32 *buffer_size_inout, void *buffer);

fsw_status_t fsw_alloc_zero (int len, void **ptr_out);
fsw_status_t fsw_memdup (void **dest_out, void *src, int len);

int          fsw_strlen (struct fsw_string *s);
int          fsw_streq (struct fsw_string *s1, struct fsw_string *s2);
int          fsw_streq_cstr (struct fsw_string *s1, const char *s2);
fsw_status_t fsw_strdup_coerce (struct fsw_string *dest, int type, struct fsw_string *src);
void         fsw_strsplit (struct fsw_string *lookup_name, struct fsw_string *buffer, char separator);

void         fsw_strfree (struct fsw_string *s);

#ifndef S_IRWXU

#define	S_ISUID	0004000
#define	S_ISGID	0002000
#define	S_ISTXT	0001000

#define	S_IRWXU	0000700
#define	S_IRUSR	0000400
#define	S_IWUSR	0000200
#define	S_IXUSR	0000100

#define	S_IRWXG	0000070
#define	S_IRGRP	0000040
#define	S_IWGRP	0000020
#define	S_IXGRP	0000010

#define	S_IRWXO	0000007
#define	S_IROTH	0000004
#define	S_IWOTH	0000002
#define	S_IXOTH	0000001

#define	S_IFMT	 0170000
#define	S_IFIFO	 0010000
#define	S_IFCHR	 0020000
#define	S_IFDIR	 0040000
#define	S_IFBLK	 0060000
#define	S_IFREG	 0100000
#define	S_IFLNK	 0120000
#define	S_IFSOCK 0140000
#define	S_ISVTX	 0001000
#define	S_IFWHT  0160000

#define	S_ISDIR(m)	(((m) & 0170000) == 0040000)
#define	S_ISCHR(m)	(((m) & 0170000) == 0020000)
#define	S_ISBLK(m)	(((m) & 0170000) == 0060000)
#define	S_ISREG(m)	(((m) & 0170000) == 0100000)
#define	S_ISFIFO(m)	(((m) & 0170000) == 0010000)
#define	S_ISLNK(m)	(((m) & 0170000) == 0120000)
#define	S_ISSOCK(m)	(((m) & 0170000) == 0140000)
#define	S_ISWHT(m)	(((m) & 0170000) == 0160000)

#define S_BLKSIZE	512

#endif

#endif
