// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2024-2026 Dayo Akanji

#include "fsw_core.h"

static inline
fsw_u8 u08get (
    fsw_u8 *buf,
    int     pos
) {
    return buf[pos];
}

static inline
fsw_u16 u16get (
    fsw_u8 *buf,
    int     pos
) {
    return FSW_U16_LE_SWAP(*(fsw_u16 *)(buf+pos));
}

static inline
fsw_u32 u32get (
    fsw_u8 *buf,
    int     pos
) {
    return FSW_U32_LE_SWAP(*(fsw_u32 *)(buf+pos));
}

static inline
fsw_u64 u64get (
    fsw_u8 *buf,
    int     pos
) {
    return FSW_U64_LE_SWAP(*(fsw_u64 *)(buf+pos));
}

#define MFTMASK     ((1ULL<<48) - 1)
#define BADMFT               (~0ULL)
#define MFTNO_MFT                (0)
#define MFTNO_VOLUME             (3)
#define MFTNO_ROOT               (5)
#define MFTNO_UPCASE            (10)
#define MFTNO_META              (16)
#define BADVCN               (~0ULL)

#define AT_STANDARD_INFORMATION 0x10
#define AT_ATTRIBUTE_LIST       0x20
#define AT_FILENAME             0x30
#define AT_VOLUME_NAME          0x60
#define AT_VOLUME_INFORMATION   0x70
#define AT_DATA                 0x80
#define AT_INDEX_ROOT           0x90
#define AT_INDEX_ALLOCATION     0xa0
#define AT_BITMAP               0xb0
#define AT_REPARSE_POINT        0xc0

#define ATTRMASK	0xFFFF
#define ATTRBITS	16
#define NAME_I30	"$\0I\0003\0000\0"
#define AT_I30		0x40000

static const fsw_u16 upcase[0x80] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
    0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
    0x60, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,
};

struct extent_slot {
    fsw_u64 vcn;
    fsw_u64 lcn;
    fsw_u64 cnt;
};

struct extent_map {

    struct extent_slot *extent;
    int total;
    int used;
};

struct ntfs_mft {
    fsw_u64 mftno;
    fsw_u8 *buf;
    fsw_u8 *atlst;
    int atlen;
};

struct ntfs_attr
{
    fsw_u64 emftno;
    fsw_u8 *emft;
    fsw_u8 *ptr;
    int len;
    int type;
};

struct fsw_ntfs_volume {
    struct fsw_volume  g;
    struct extent_map  extmap;
    fsw_u64            totalbytes;
    const fsw_u16     *upcase;
    int                upcount;

    fsw_u8 sctbits;
    fsw_u8 clbits;
    fsw_u8 mftbits;
    fsw_u8 idxbits;
};

struct fsw_ntfs_dnode {
    struct fsw_dnode g;
    struct ntfs_mft mft;
    struct ntfs_attr attr;
    fsw_u8 *idxroot;
    fsw_u8 *idxbmp;
    unsigned int embeded:1;
    unsigned int has_idxtree:1;
    unsigned int compressed:1;
    unsigned int unreadable:1;
    unsigned int cpfull:1;
    unsigned int cpzero:1;
    unsigned int cperror:1;
    unsigned int islink:1;
    int idxsz;
    int rootsz;
    int bmpsz;
    struct extent_slot cext;
    fsw_u64 fsize;
    fsw_u64 finited;
    fsw_u64 cvcn;
    fsw_u64 clcn[16];
    fsw_u8 *cbuf;
};

static
fsw_status_t fixup (
    fsw_u8 *record,
    char   *magic,
    int     sectorsize,
    int     size
) {
    int off, cnt, i;
    fsw_u16 val;

    if (*(int *)record != *(int *)magic)
	return FSW_VOLUME_CORRUPTED;

    off = u16get (record, 4);
    cnt = u16get (record, 6);

    if (size && sectorsize*(cnt-1) != size)
	return FSW_VOLUME_CORRUPTED;

    val = u16get (record, off);

    for (i=1; i<cnt; i++) {
        if (u16get (record, i*sectorsize-2) != val) {
            return FSW_VOLUME_CORRUPTED;
        }

        *(fsw_u16 *)(record+i*sectorsize-2) = *(fsw_u16 *)(record+off+i*2);
    }

    return FSW_SUCCESS;
}

static
fsw_status_t find_attribute_direct (
    fsw_u8  *mft,
    int      mftsize,
    int      type,
    fsw_u8 **outptr,
    int     *outlen
) {
    int namelen, t;
    int tmp1, tmp2;
    fsw_u32 n;

    tmp1 = u32get (mft, 0x18);
    tmp2 = u16get (mft, 0x14);
    if (tmp1 < mftsize) {
        mftsize = tmp1;
    }

    mftsize -= tmp2;
    mft     += tmp2;
    namelen  = type>>ATTRBITS;
    type    &= ATTRMASK;

    for (n = 0; mftsize >= 8; mft += n, mftsize -= n) {
        t = u32get (mft, 0);
        n = u32get (mft, 4);
        if (mftsize < n ||
            n < 24      ||
            t == 0      ||
            t == 0xffff ||
            (t + 1)     == 0
        ) {
            break;
        }

        fsw_u8 ns  = u08get (mft,  9);
        fsw_u8 *nm = u08get (mft, 10) + mft;

        if (type    == t  &&
            namelen == ns && (
                ns  == 0 ||
                FSW_DO_MEMEQ(NAME_I30, nm, ns*2)
            )
        ) {
            if (outptr) *outptr = mft;
            if (outlen) *outlen = n;

            return FSW_SUCCESS;
        }
    }
    return FSW_NOT_FOUND;
}

static
fsw_status_t find_attrlist_direct (
    fsw_u8  *atlst,
    int      atlen,
    int      type,
    fsw_u64  vcn,
    fsw_u64 *out,
    int     *pos
) {
    fsw_u64 mftno = BADMFT;
    int     namelen;

    namelen = type>>ATTRBITS;
    type   &=       ATTRMASK;

    while (*pos + 0x18 <= atlen) {
        int off   = *pos;
        fsw_u32 t = u32get (atlst, off);
        fsw_u32 n = u16get (atlst, off + 4);

        *pos = off + n;
        if (t==0 || (t+1)==0 || t==0xffff || n<0x18 || *pos>atlen) break;

        fsw_u8  ns = u08get (atlst, off + 6);
        fsw_u8 *nm = atlst + off + u08get (atlst, off + 7);
        if (type == t &&
            namelen == ns && (
                ns  == 0  ||
                FSW_DO_MEMEQ(NAME_I30, nm, ns * 2)
            )
        ) {
            fsw_u64 avcn = u64get (atlst, off + 8);
            if (vcn < avcn) {
                if (mftno == BADMFT) {
                    return FSW_NOT_FOUND;
                }

                *out = mftno;
                return FSW_SUCCESS;
            }

            if (vcn == avcn) {
                *out = u64get (atlst, off + 0x10) & MFTMASK;
                return FSW_SUCCESS;
            }

            mftno = u64get (atlst, off + 0x10) & MFTMASK;
        }
    }
    if (mftno != BADMFT) {
        *out = mftno;
        return FSW_SUCCESS;
    }

    return FSW_NOT_FOUND;
}

static
fsw_status_t get_extent (
    fsw_u8  **rlep,
    int      *rlenp,
    fsw_u64  *lcnp,
    fsw_u64  *lenp,
    fsw_u64  *pos
) {
    fsw_u8 *rle = *rlep;
    fsw_u8 *rle_end = rle + *rlenp;
    int f = *rle++;
    fsw_u64 m = 1;
    fsw_u64 c = 0;
    fsw_u64 v = 0;
    int n = f & 0xf;

    if (n == 0) {
        return FSW_NOT_FOUND;
    }

    if (rle + n > rle_end) {
        return FSW_VOLUME_CORRUPTED;
    }

    while (--n >= 0) {
        c += m * (*rle++);
        m <<= 8;
    }

    n = f >> 4;
    if (n == 0) {

        *lcnp = 0;
        *lenp = c;
    }
    else {
        if (rle + n > rle_end) {
            return FSW_VOLUME_CORRUPTED;
        }

        m = 1;
        while (--n >= 0) {
            v += m * (*rle++);
            m <<= 8;
        }

        *pos += v;
        if (v & (m>>1))
        *pos -= m;

        *lcnp = *pos;
        *lenp = c;
    }
    *rlenp -= rle - *rlep;
    *rlep   = rle;

    return FSW_SUCCESS;
}

static inline
int attribute_ondisk (
    fsw_u8 *ptr,
    int len
) {
    return u08get (ptr, 8);
}

static inline
int attribute_compressed (
    fsw_u8 *ptr,
    int     len
) {
    return (u16get (ptr, 12) & 0xFF) == 1;
}

static inline
int attribute_compressed_future (
    fsw_u8 *ptr,
    int     len
) {
    return (u16get (ptr, 12) & 0xFF) > 1;
}

static inline
int attribute_encrypted (
    fsw_u8 *ptr,
    int     len
) {
    return u16get (ptr, 12) & 0x4000;
}

static
void attribute_get_embeded (
    fsw_u8  *ptr,
    int      len,
    fsw_u8 **outp,
    int     *outlenp
) {
    int off  = u16get (ptr, 0x14);
    int olen = u16get (ptr, 0x10);
    if (olen + off > len) {
        olen = len - off;
    }

    *outp = ptr + off;
    *outlenp = olen;
}

static inline
int attribute_rle_offset (
    fsw_u8 *ptr,
    int     len
) {
    return u16get (ptr, 0x20);
}

static
void attribute_get_rle (
    fsw_u8  *ptr,
    int      len,
    fsw_u8 **outp,
    int     *outlenp
) {
    int off = attribute_rle_offset (ptr, len);
    int olen = len - off;
    *outp = ptr + off;
    *outlenp = olen;
}

static inline
fsw_u64 attribute_size (
    fsw_u8 *ptr,
    int len
) {
    return attribute_ondisk (
        ptr, len
    ) ? u64get (ptr, 0x30) : u16get (ptr, 0x10);
}

static inline fsw_u64 attribute_inited_size (
    fsw_u8 *ptr,
    int     len
) {
    return attribute_ondisk (
        ptr, len
    ) ? u64get (ptr, 0x38) : u16get (ptr, 0x10);
}

static inline
int attribute_has_vcn (
    fsw_u8  *ptr,
    int      len,
    fsw_u64  vcn
) {
    if (attribute_ondisk (ptr, len)==0) return 1;

    return (
        vcn >= u64get (ptr, 0x10) &&
        vcn <= u64get (ptr, 0x18)
    );
}

static inline
fsw_u64 attribute_first_vcn (
    fsw_u8 *ptr,
    int     len
) {
    return attribute_ondisk (
        ptr, len
    ) ? u64get (ptr, 0x10) : 0;
}

static inline
fsw_u64 attribute_last_vcn (
    fsw_u8 *ptr,
    int     len
) {
    return attribute_ondisk (
        ptr, len
    ) ? u64get (ptr, 0x18) : 0;
}

static
fsw_status_t read_attribute_direct (
    struct fsw_ntfs_volume  *vol,
    fsw_u8                  *ptr,
    int                      len,
    fsw_u8                 **optrp,
    int                     *olenp
) {
    fsw_status_t err;
    int olen;

    if (attribute_ondisk (ptr, len) == 0) {

        attribute_get_embeded (
            ptr, len,
            &ptr, &len
        );
        *olenp = len;

        if (optrp) {
            if ((
                err = FSW_DO_ALLOC(
                    len, (void **)optrp
                )) != FSW_SUCCESS
            ) {
                return err;
            }
            FSW_DO_MEMCPY(*optrp, ptr, len);
        }

        return FSW_SUCCESS;
    }

    olen = attribute_size (ptr, len);
    *olenp = olen;
    if (!optrp) return FSW_SUCCESS;

    if ((
        err = fsw_alloc_zero (
            olen, (void **)optrp
        )) != FSW_SUCCESS
    ) {
        return err;
    }
    fsw_u8 *buf = *optrp;

    attribute_get_rle (ptr, len, &ptr, &len);

    fsw_u64 pos = 0;
    int clustersize = 1<<vol->clbits;
    fsw_u64 lcn, cnt;

    while (
        olen > 0 &&
        len  > 0 &&
        get_extent (
            &ptr, &len,
            &lcn, &cnt, &pos
        ) == FSW_SUCCESS
    ) {
        if (!lcn) {
            buf  += cnt << vol->clbits;
            olen -= cnt << vol->clbits;
        }
        else {
            for (; cnt>0; lcn++, cnt--) {
                fsw_u8 *block;
                if (
                    fsw_block_get (
                        &vol->g, lcn, 0,
                        (void **) &block
                    ) != FSW_SUCCESS
                ) {
                    FSW_DO_FREE(*optrp);
                    *optrp = NULL;
                    *olenp = 0;

                    return FSW_VOLUME_CORRUPTED;
                }

                FSW_DO_MEMCPY(
                    buf, block,
                    (
                        clustersize <= olen
                    ) ? clustersize :  olen
                );
                fsw_block_release (&vol->g, lcn, block);

                buf += clustersize;
                olen -= clustersize;
            }
        }
    }

    return FSW_SUCCESS;
}

static void init_mft (
    struct fsw_ntfs_volume *vol,
    struct ntfs_mft *mft,
    fsw_u64 mftno
) {
    mft->mftno = mftno;
    mft->atlst = NULL;
    mft->atlen = FSW_DO_ALLOC(1<<vol->mftbits, &mft->buf);
    mft->atlen = 0;
}

static void free_mft (
    struct ntfs_mft *mft
) {
    if (mft->buf)   FSW_DO_FREE(mft->buf);
    if (mft->atlst) FSW_DO_FREE(mft->atlst);
}

static fsw_status_t load_atlist (
    struct fsw_ntfs_volume *vol,
    struct ntfs_mft        *mft
) {
    fsw_u8 *ptr;
    int len;

    fsw_status_t err = find_attribute_direct (
        mft->buf, 1<<vol->mftbits,
        AT_ATTRIBUTE_LIST, &ptr, &len
    );
    if (err != FSW_SUCCESS) return err;

    return read_attribute_direct (vol, ptr, len, &mft->atlst, &mft->atlen);
}

static fsw_status_t read_mft (
    struct fsw_ntfs_volume *vol,
    fsw_u8                 *mft,
    fsw_u64                 mftno
) {
    int l = 0;
    int r = vol->extmap.used - 1;
    int m;
    fsw_u64 vcn = (mftno << vol->mftbits) >> vol->clbits;
    struct extent_slot *e = vol->extmap.extent;
    fsw_status_t err;

    while (l <= r) {
        m = (l+r)/2;
        if (vcn < e[m].vcn) {
            r = m - 1;
        }
        else if (vcn >= e[m].vcn + e[m].cnt) {
            l = m + 1;
        }
        else {
            if (vol->mftbits <= vol->clbits) {
                fsw_u64 lcn = e[m].lcn + (vcn - e[m].vcn);
                int offset = (mftno << vol->mftbits) & ((1<<vol->clbits)-1);
                fsw_u8 *buffer;

                if (e[m].lcn + 1 == 0) {
                    return FSW_VOLUME_CORRUPTED;
                }

                err = fsw_block_get (&vol->g, lcn, 0, (void **) &buffer);
                if (err != FSW_SUCCESS) {
                    return FSW_VOLUME_CORRUPTED;
                }

                FSW_DO_MEMCPY(mft, buffer+offset, 1<<vol->mftbits);
                fsw_block_release (&vol->g, lcn, buffer);

                return fixup (mft, "FILE", 1<<vol->sctbits, 1<<vol->mftbits);
            }

            fsw_u8 *p = mft;
            fsw_u64 lcn = e[m].lcn + (vcn - e[m].vcn);
            fsw_u64 ecnt = e[m].cnt - (vcn - e[m].vcn);
            int count = 1 << (vol->mftbits - vol->clbits);

            if (e[m].lcn + 1 == 0) {
                return FSW_VOLUME_CORRUPTED;
            }

            while (count-- > 0) {
                fsw_u8 *buffer;
                err = fsw_block_get (&vol->g, lcn, 0, (void **) &buffer);
                if (err != FSW_SUCCESS) return FSW_VOLUME_CORRUPTED;

                FSW_DO_MEMCPY(p, buffer, 1<<vol->clbits);
                fsw_block_release (&vol->g, lcn, buffer);

                p += 1<<vol->clbits;
                ecnt--;
                vcn++;
                if (count==0) break;
                if (ecnt > 0) {
                    lcn++;
                }
                else {
                    if (++m >= vol->extmap.used || e[m].vcn != vcn) {
                        return FSW_VOLUME_CORRUPTED;
                    }

                    lcn = e[m].lcn;
                    ecnt = e[m].cnt;
                }
            }

            return fixup (
                mft, "FILE",
                1<<vol->sctbits,
                1<<vol->mftbits
            );
        }
    }

    return FSW_NOT_FOUND;
}

static void init_attr (
    struct fsw_ntfs_volume *vol,
    struct ntfs_attr       *attr,
    int                     type
) {
    FSW_DO_MEMZERO(attr, sizeof (*attr));
    attr->type = type;
    attr->emftno = BADMFT;
}

static void free_attr (
    struct ntfs_attr *attr
) {
    if (attr->emft) FSW_DO_FREE(attr->emft);
}

static
fsw_status_t find_attribute (
    struct fsw_ntfs_volume *vol,
    struct ntfs_mft        *mft,
    struct ntfs_attr       *attr,
    fsw_u64                 vcn
) {
    fsw_u8 *buf = mft->buf;
    if (mft->atlst && mft->atlen) {
        fsw_status_t err;
        fsw_u64 mftno;
        int pos = 0;

        err = find_attrlist_direct (
            mft->atlst, mft->atlen,
            attr->type, vcn,
            &mftno, &pos
        );
        if (err != FSW_SUCCESS) return err;

        if (mftno == mft->mftno) {
            buf = mft->buf;
        }
        else if (mftno == attr->emftno && attr->emft) {
            buf = attr->emft;
        }
        else {
            attr->emftno = BADMFT;
            if (attr->emft==NULL) {
                err = FSW_DO_ALLOC(1<<vol->mftbits, &attr->emft);
                if (err != FSW_SUCCESS) return err;
            }
            err = read_mft (vol, attr->emft, mftno);
            if (err != FSW_SUCCESS) return err;

            attr->emftno = mftno;
            buf = attr->emft;
        }
    }

    return find_attribute_direct (
        buf, 1<<vol->mftbits,
        attr->type, &attr->ptr,
        &attr->len
    );
}

static
fsw_status_t read_small_attribute (
    struct fsw_ntfs_volume  *vol,
    struct ntfs_mft         *mft,
    int                      type,
    fsw_u8                 **optrp,
    int                     *olenp
) {
    fsw_status_t err;
    struct ntfs_attr attr;

    init_attr (vol, &attr, type);
    err = find_attribute (vol, mft, &attr, 0);
    if (err == FSW_SUCCESS) {
        err = read_attribute_direct (
            vol, attr.ptr, attr.len,
            optrp, olenp
        );
    }

    free_attr (&attr);
    return err;
}

static
void add_single_mft_map (
    struct fsw_ntfs_volume *vol,
    fsw_u8                 *mft
) {
    fsw_u8 *ptr;
    int len;

    if (find_attribute_direct (mft, 1<<vol->mftbits, AT_DATA, &ptr, &len) != FSW_SUCCESS) {
        return;
    }

    if (attribute_ondisk (ptr, len) == 0) {
        return;
    }

    fsw_u64 vcn = u64get (ptr, 0x10);
    int     off = u16get (ptr, 0x20);
    ptr += off;
    len -= off;

    fsw_u64 pos = 0;
    fsw_u64 lcn, cnt;

    while (len > 0 && get_extent (&ptr, &len, &lcn, &cnt, &pos)==FSW_SUCCESS) {
	if (lcn) {
	    int u = vol->extmap.used;
	    if (u >= vol->extmap.total) {
		vol->extmap.total = vol->extmap.extent ? u*2 : 16;
		struct extent_slot *e;
		if (FSW_DO_ALLOC(vol->extmap.total * sizeof (struct extent_slot), &e) != FSW_SUCCESS)
		    break;
		if (vol->extmap.extent) {
		    FSW_DO_MEMCPY(e, vol->extmap.extent, u*sizeof (struct extent_slot));
		    FSW_DO_FREE(vol->extmap.extent);
		}
		vol->extmap.extent = e;
	    }
	    vol->extmap.extent[u].vcn = vcn;
	    vol->extmap.extent[u].lcn = lcn;
	    vol->extmap.extent[u].cnt = cnt;
	    vol->extmap.used++;
	}
	vcn += cnt;
    }
}

static void add_mft_map (
    struct fsw_ntfs_volume *vol,
    struct ntfs_mft        *mft
) {
    load_atlist (vol, mft);
    add_single_mft_map (vol, mft->buf);
    if (mft->atlst == NULL) return;

    fsw_u64 mftno;
    int pos = 0;

    fsw_u8 *emft;
    if (FSW_DO_ALLOC(1<<vol->mftbits, &emft) != FSW_SUCCESS) return;

    while (
        find_attrlist_direct (
            mft->atlst, mft->atlen,
            AT_DATA, 0, &mftno, &pos
        ) == FSW_SUCCESS
    ) {
        if (mftno == 0) continue;

        if (read_mft (vol, emft, mftno) == FSW_SUCCESS) {
            add_single_mft_map (vol, emft);
        }
    }
    FSW_DO_FREE(emft);
}

static
int tobits (fsw_u32 val) {
    return 31 - __builtin_clz (val);
}

static
fsw_status_t fsw_ntfs_volume_mount (
    struct fsw_volume *volg
) {
    struct fsw_ntfs_volume *vol = (struct fsw_ntfs_volume *)volg;
    fsw_status_t err;
    fsw_u8 *buffer;
    int sector_size;
    int cluster_size;
    signed char tmp;
    fsw_u64 mft_start[2];
    struct ntfs_mft mft0;

    fsw_set_blocksize (volg, 512, 512);
    if ((
        err = fsw_block_get (
            volg, 0, 0,
            (void **) &buffer
        )) != FSW_SUCCESS
    ) {
        return FSW_UNSUPPORTED;
    }

    if (!FSW_DO_MEMEQ(buffer+3, "NTFS    ", 8))
    return FSW_UNSUPPORTED;

    sector_size = u16get (buffer, 0xB);
    if (sector_size == 0      ||
        sector_size  < 0x100  ||
        sector_size  > 0x1000 ||
        (sector_size & (sector_size-1))
    ) {
        return FSW_UNSUPPORTED;
    }

    vol->sctbits    = tobits (sector_size);
    vol->totalbytes = u64get (buffer, 0x28) << vol->sctbits;

    FSW_MSG_L03((
        FSW_MSG_STR(
            "FSW_NTFS: fsw_ntfs_volume_mount ... NTFS size=%ld M\n"
        ), vol->totalbytes>>20
    ));

    cluster_size = u08get (buffer, 0xD) * sector_size;
    if (cluster_size == 0       ||
        cluster_size  > 0x10000 ||
        (cluster_size & (cluster_size-1))
    ) {
        return FSW_UNSUPPORTED;
    }

    vol->clbits = tobits (cluster_size);

    tmp = u08get (buffer, 0x40);
    vol->mftbits = (
        tmp > 0
    ) ? vol->clbits + tobits (tmp) : -tmp;

    if (vol->mftbits > 16 ||
        vol->mftbits < vol->sctbits
    ) {
        return FSW_UNSUPPORTED;
    }

    tmp = u08get (buffer, 0x44);
    vol->idxbits = (
        tmp > 0
    ) ? vol->clbits + tobits (tmp) : -tmp;

    if (vol->idxbits > 16 ||
        vol->idxbits < vol->sctbits
    ) {
        return FSW_UNSUPPORTED;
    }

    mft_start[0] = u64get (buffer, 0x30);
    mft_start[1] = u64get (buffer, 0x38);

    fsw_block_release (volg, 0, (void *)buffer);
    fsw_set_blocksize (volg, cluster_size, cluster_size);

    init_mft (vol, &mft0, MFTNO_MFT);
    for (tmp=0; tmp<2; tmp++) {
        fsw_u8 *ptr = mft0.buf;
        int len = 1 << vol->mftbits;
        fsw_u64 lcn = mft_start[tmp];
        while (len > 0) {
            if ((err = fsw_block_get (volg, lcn, 0, (void **) &buffer)) != FSW_SUCCESS) {
                free_mft (&mft0);
                return FSW_UNSUPPORTED;
            }

            int n = (
                vol->mftbits < vol->clbits
            ) ? (1<<vol->mftbits) : cluster_size;
            FSW_DO_MEMCPY(ptr, buffer, n);
            fsw_block_release (
                volg, lcn,
                (void *)buffer
            );

            ptr += n;
            len -= n;
            lcn++;
        }

        err = fixup (
            mft0.buf, "FILE",
            sector_size,
            1<<vol->mftbits
        );
        if (err != FSW_SUCCESS)
        return err;
    }

    add_mft_map (vol, &mft0);
    free_mft (&mft0);

    init_mft (vol, &mft0, MFTNO_VOLUME);
    fsw_u8 *ptr;
    int len;

    if (read_mft (
            vol, mft0.buf, MFTNO_VOLUME
        ) == FSW_SUCCESS &&
        find_attribute_direct (
            mft0.buf,
            1<<vol->mftbits,
            AT_VOLUME_NAME,
            &ptr, &len
        ) == FSW_SUCCESS &&
        attribute_ondisk (ptr, len) == 0
    ) {
        struct fsw_string s;
        s.type = FSW_STRING_TYPE_UTF16_LE;
        s.size = u16get (ptr, 0x10);
        s.data = u16get (ptr, 0x14) + ptr;
        s.len  = s.size / 2;

        fsw_strdup_coerce (&volg->label, volg->host_string_type, &s);
    }
    free_mft (&mft0);

    err = fsw_dnode_create_root (
        volg,
        MFTNO_ROOT,
        &volg->root
    );
    if (err) return err;

    return FSW_SUCCESS;
}

static void fsw_ntfs_volume_free (
    struct fsw_volume *volg
) {
    struct fsw_ntfs_volume *vol = (struct fsw_ntfs_volume *)volg;

    if (vol->extmap.extent) {
        FSW_DO_FREE(vol->extmap.extent);
    }

    if (vol->upcase && vol->upcase != upcase) {
        FSW_DO_FREE((void *)vol->upcase);
    }
}

static fsw_status_t fsw_ntfs_volume_stat (
    struct fsw_volume      *volg,
    struct fsw_volume_stat *sb
) {
    struct fsw_ntfs_volume *vol = (struct fsw_ntfs_volume *)volg;
    sb->total_bytes = vol->totalbytes;

    sb->free_bytes  = 0;
    return FSW_SUCCESS;
}

static void fsw_ntfs_dnode_free (
    struct fsw_volume *vol,
    struct fsw_dnode  *dnog
) {
    struct fsw_ntfs_dnode *dno = (struct fsw_ntfs_dnode *)dnog;

    free_mft (&dno->mft);
    free_attr (&dno->attr);

    if (dno->idxroot) FSW_DO_FREE(dno->idxroot);
    if (dno->idxbmp)  FSW_DO_FREE(dno->idxbmp);
    if (dno->cbuf)    FSW_DO_FREE(dno->cbuf);
}

static fsw_status_t fsw_ntfs_dnode_fill (
    struct fsw_volume *volg,
    struct fsw_dnode  *dnog
) {
    struct fsw_ntfs_volume *vol = (struct fsw_ntfs_volume *)volg;
    struct fsw_ntfs_dnode *dno = (struct fsw_ntfs_dnode *)dnog;
    fsw_status_t err;
    int len;

    if (dno->mft.buf != NULL){
        return FSW_SUCCESS;
    }

    init_mft (vol, &dno->mft, dno->g.dnode_id);
    err = read_mft (vol, dno->mft.buf, dno->g.dnode_id);
    if (err != FSW_SUCCESS) goto error_out;

    if (dno->mft.buf == NULL) {
        err = FSW_NOT_FOUND;
        goto error_out;
    }

    len = u08get (dno->mft.buf, 22);
    if ((len & 1) == 0 ) {
        err = FSW_NOT_FOUND;
        goto error_out;
    }

    load_atlist (vol, &dno->mft);

    if (read_small_attribute (
            vol, &dno->mft, AT_REPARSE_POINT,
            &dno->cbuf, &len
        )==FSW_SUCCESS
    ) {
        switch (u32get (dno->cbuf, 0)) {
            case 0xa0000003:
            case 0xa000000c: {
                dno->g.size = len;
                dno->g.type = FSW_DNODE_TYPE_SYMLINK;
                dno->islink = 1;
                dno->fsize  = len;
                return FSW_SUCCESS;
            }

            default: {
                FSW_DO_FREE(dno->cbuf);
                dno->cbuf = NULL;
            }
        }
    }

    if ((len & 2) ) {
        dno->g.type = FSW_DNODE_TYPE_DIR;

        err = read_small_attribute (
            vol, &dno->mft,
            AT_INDEX_ROOT|AT_I30,
            &dno->idxroot,
            &dno->rootsz
        );
        if (err != FSW_SUCCESS) {
            FSW_MSG_L03((
                FSW_MSG_STR(
                    "FSW_NTFS: fsw_ntfs_dnode_fill ... dno_fill INDEX_ROOT:$I30 error==%d\n"
                ), err
            ));

            goto error_out;
        }

        dno->idxsz = u32get (dno->idxroot, 8);
        if (dno->idxsz == 0) dno->idxsz = 1<<vol->idxbits;

        err = read_small_attribute (
            vol, &dno->mft,
            AT_BITMAP|AT_I30,
            &dno->idxbmp,
            &dno->bmpsz
        );
        if (err != FSW_SUCCESS && err != FSW_NOT_FOUND) {
            FSW_MSG_L03((
                FSW_MSG_STR(
                    "FSW_NTFS: fsw_ntfs_dnode_fill ... dno_fill $Bitmap:$I30 error==%d\n"
                ), err
            ));

            goto error_out;
        }

        init_attr (
            vol, &dno->attr,
            AT_INDEX_ALLOCATION|AT_I30
        );

        err = find_attribute (
            vol, &dno->mft,
            &dno->attr, 0
        );
        if (err == FSW_SUCCESS) {
            dno->has_idxtree = 1;
            dno->fsize = attribute_size (
                dno->attr.ptr,
                dno->attr.len
            );
            dno->finited = dno->fsize;
        }
        else if (err != FSW_NOT_FOUND) {
            FSW_MSG_L03((
                FSW_MSG_STR(
                    "FSW_NTFS: fsw_ntfs_dnode_fill ... dno_fill $INDEX_ALLOCATION:$I30 error==%d\n"
                ), err
            ));

            goto error_out;
        }
    }
    else {
        dno->g.type = FSW_DNODE_TYPE_FILE;
        init_attr (
            vol, &dno->attr,
            AT_DATA
        );
        err = find_attribute (
            vol, &dno->mft,
            &dno->attr, 0
        );
        if (err != FSW_SUCCESS) {
            FSW_MSG_L03((
                FSW_MSG_STR(
                    "FSW_NTFS: fsw_ntfs_dnode_fill ... dno_fill AT_DATA error==%d\n"
                ), err
            ));

            goto error_out;
        }
        dno->embeded = !attribute_ondisk (
            dno->attr.ptr,
            dno->attr.len
        );
        dno->fsize = attribute_size (
            dno->attr.ptr,
            dno->attr.len
        );
        dno->finited = attribute_inited_size (
            dno->attr.ptr,
            dno->attr.len
        );

        if (0);
        else if (attribute_encrypted (dno->attr.ptr, dno->attr.len        )) dno->unreadable = 1;
        else if (attribute_compressed_future (dno->attr.ptr, dno->attr.len)) dno->unreadable = 1;
        else if (attribute_compressed (dno->attr.ptr, dno->attr.len       )) dno->compressed = 1;
        dno->cvcn = BADVCN;
        dno->g.size = dno->fsize;
    }

    return FSW_SUCCESS;
error_out:
    fsw_ntfs_dnode_free (volg, dnog);

    dno->mft.buf = NULL;

    return err;
}

static fsw_u32 get_ntfs_time (
    fsw_u8 *buf,
    int     pos
) {
    fsw_u64 t = u64get (buf, pos);
    t  = FSW_U64_DIV(t, 10000000);
    t -= 134774ULL * 86400;
    return t;
}

static fsw_status_t fsw_ntfs_dnode_stat (
    struct fsw_volume     *volg,
    struct fsw_dnode      *dnog,
    struct fsw_dnode_stat *sb
) {
    struct fsw_ntfs_volume *vol = (struct fsw_ntfs_volume *)volg;
    struct fsw_ntfs_dnode  *dno = (struct fsw_ntfs_dnode  *)dnog;
    fsw_status_t err;
    fsw_u16 attr;
    fsw_u8 *ptr;
    int len;

    err = find_attribute_direct (
        dno->mft.buf, 1<<vol->mftbits,
        AT_STANDARD_INFORMATION, &ptr, &len
    );
    if (err != FSW_SUCCESS || attribute_ondisk (ptr, len)) {
        return err;
    }

    ptr += u16get (ptr, 0x14);
    attr = u08get (ptr, 0x20);

#ifndef EFI_FILE_READ_ONLY
#define EFI_FILE_READ_ONLY    1
#define EFI_FILE_HIDDEN       2
#define EFI_FILE_SYSTEM       4
#define EFI_FILE_DIRECTORY 0x10
#define EFI_FILE_ARCHIVE   0x20
#endif

    attr &= EFI_FILE_READ_ONLY | EFI_FILE_HIDDEN | EFI_FILE_SYSTEM | EFI_FILE_ARCHIVE;

    if (u08get (dno->mft.buf, 22) & 2)
	attr |= EFI_FILE_DIRECTORY;

    fsw_store_attr_efi (sb, attr);
    sb->used_bytes = dno->fsize;
    fsw_store_time_posix (sb, FSW_DNODE_STAT_ATIME, get_ntfs_time (ptr, 24));
    fsw_store_time_posix (sb, FSW_DNODE_STAT_CTIME, get_ntfs_time (ptr,  0));
    fsw_store_time_posix (sb, FSW_DNODE_STAT_MTIME, get_ntfs_time (ptr,  8));

    return FSW_SUCCESS;
}

static fsw_status_t fsw_ntfs_dnode_get_lcn (
    struct fsw_ntfs_volume *vol,
    struct fsw_ntfs_dnode  *dno,
    fsw_u64                 vcn,
    fsw_u64                *lcnp
) {
    fsw_status_t err;
    if (vcn >= dno->cext.vcn && vcn < dno->cext.vcn+dno->cext.cnt) {
        *lcnp = dno->cext.lcn + vcn - dno->cext.vcn;
        return FSW_SUCCESS;
    }

    if (!attribute_has_vcn (dno->attr.ptr, dno->attr.len, vcn)) {

        err = find_attribute (vol, &dno->mft, &dno->attr, vcn);
        if (err != FSW_SUCCESS) return err;

        if (!attribute_has_vcn (dno->attr.ptr, dno->attr.len, vcn))
        return FSW_VOLUME_CORRUPTED;
    }

    fsw_u8 *ptr = dno->attr.ptr;
    int len = dno->attr.len;
    fsw_u64 pos = 0;
    fsw_u64 lcn, cnt;
    fsw_u64 svcn = attribute_first_vcn (ptr, len);
    fsw_u64 evcn = attribute_last_vcn (ptr, len) + 1;
    int off = u16get (ptr, 0x20);

    ptr += off;
    len -= off;

    while (
        len > 0 &&
        get_extent (
            &ptr, &len,
            &lcn, &cnt, &pos
        ) == FSW_SUCCESS
    ) {
        if (vcn >= svcn && vcn < svcn+cnt) {
            dno->cext.vcn = svcn;
            dno->cext.lcn = lcn;
            dno->cext.cnt = cnt;
            if (lcn == 0) {
                return FSW_NOT_FOUND;
            }

            *lcnp = lcn + vcn - svcn;
            return FSW_SUCCESS;
        }
        svcn += cnt;
        if (svcn >= evcn) break;
    }

    return FSW_NOT_FOUND;
}

static int fsw_ntfs_read_buffer (
    struct fsw_ntfs_volume *vol,
    struct fsw_ntfs_dnode  *dno,
    fsw_u8                 *buf,
    fsw_u64                 offset,
    int                     size
) {
    if (dno->embeded) {
        fsw_u8 *ptr;
        int len;
        attribute_get_embeded (
            dno->attr.ptr,
            dno->attr.len,
            &ptr, &len
        );
        if (offset >= len) {
            return 0;
        }

        ptr += offset;
        len -= offset;

        if (size > len) {
            size = len;
        }

        FSW_DO_MEMCPY(buf, ptr, size);
        return size;
    }

    if (!dno->attr.ptr || !dno->attr.len) {
        FSW_MSG_L03((
            FSW_MSG_STR(
                "FSW_NTFS: fsw_ntfs_read_buffer ... BAD--------: attr.ptr==%p attr.len==%x cleared\n"
            ), dno->attr.ptr, dno->attr.len
        ));

        if (find_attribute (
                vol, &dno->mft,
                &dno->attr, 0
            ) != FSW_SUCCESS
        ) {
            return 0;
        }
    }

    if (offset >= dno->fsize)
    return 0;
    if (offset + size >= dno->fsize)
    size = dno->fsize - offset;

    if (offset >= dno->finited) {
        FSW_DO_MEMZERO(buf, size);
        return size;
    }

    int ret = 0;
    int zsize = 0;
    if (offset + size >= dno->finited) {
        zsize = offset + size - dno->finited;
        size = dno->finited - offset;
    }

    fsw_u64 vcn = offset >> vol->clbits;
    int boff = offset & ((1<<vol->clbits)-1);
    fsw_u64 lcn = 0;

    while (size > 0) {
        fsw_u8 *block;
        fsw_status_t err;
        int bsz;

        err = fsw_ntfs_dnode_get_lcn (
            vol, dno,
            vcn, &lcn
        );
        if (err != FSW_SUCCESS) {
            break;
        }

        err = fsw_block_get (
            &vol->g, lcn, 0,
            (void **) &block
        );
        if (err != FSW_SUCCESS) {
            break;
        }

        bsz = (1<<vol->clbits) - boff;
        if (bsz > size) {
            bsz = size;
        }

        FSW_DO_MEMCPY(buf, block+boff, bsz);
        fsw_block_release (&vol->g, lcn, block);

        ret += bsz;
        buf += bsz;
        size -= bsz;
        boff = 0;
        vcn++;
    }

    if (size==0 && zsize > 0) {
        FSW_DO_MEMZERO(buf, zsize);
        ret += zsize;
    }
    return ret;
}

static fsw_status_t fsw_ntfs_get_extent_embeded (
    struct fsw_ntfs_volume *vol,
    struct fsw_ntfs_dnode  *dno,
    struct fsw_extent      *extent
) {
    fsw_status_t err;
    fsw_u8 *ptr;
    int len;

    if (extent->log_start > 0) {
        return FSW_NOT_FOUND;
    }

    extent->log_count = 1;
    err = FSW_DO_ALLOC(1<<vol->clbits, &extent->buffer);
    if (err != FSW_SUCCESS) return err;

    attribute_get_embeded (
        dno->attr.ptr,
        dno->attr.len,
        &ptr, &len
    );

    if (len > (1<<vol->clbits)) {
        len = (1<<vol->clbits);
    }

    FSW_DO_MEMCPY(extent->buffer, ptr, len);
    extent->type = FSW_EXTENT_TYPE_BUFFER;

    return FSW_SUCCESS;
}

static
int ntfs_decomp_1page (
    fsw_u8 *src,
    int     slen,
    fsw_u8 *dst
) {
    int soff = 0;
    int doff = 0;

    while (soff < slen) {
        int j;
        int tag = src[soff++];
        for (j = 0; j < 8 && soff < slen; j++) {
            if (tag & (1<<j)){
                int len;
                int back;
                int bits;

                if (!doff || soff + 2 > slen) return -1;
                len  = u16get (src, soff); soff += 2;
                bits = __builtin_clz ((doff-1)>>3)-19;
                back = (len >> bits) + 1;
                len  = (len & ((1<<bits)-1)) + 3;
                if (doff < back || doff + len > 0x1000) return -1;

                while (len-- > 0) {
                    dst[doff] = dst[doff-back];
                    doff++;
                }
            }
            else {
                if (doff >= 0x1000) {
                    return -1;
                }
                dst[doff++] = src[soff++];
            }
        }
    }
    return doff;
}

static int ntfs_decomp (
    fsw_u8 *src,
    int     slen,
    fsw_u8 *dst,
    int     npage
) {
    fsw_u8 *se = src + slen;
    fsw_u8 *de = dst + (npage<<12);
    int i;

    for (i=0; i<npage; i++) {
        fsw_u16 slen = u16get (src, 0);
        int comp = slen & 0x8000;
        slen = (slen&0xfff)+1;
        src += 2;

        if (src + slen > se || dst + 0x1000 > de) {
            return -1;
        }

        if (!comp) {
            FSW_DO_MEMCPY(dst, src, slen);
            if (slen < 0x1000) {
                FSW_DO_MEMZERO(dst+slen, 0x1000-slen);
            }
        }
        else if (slen == 1) {
            FSW_DO_MEMZERO(dst, 0x1000);
        }
        else {
            int dlen = ntfs_decomp_1page (src, slen, dst);
            if (dlen < 0) {
                return -1;
            }
            if (dlen < 0x1000) {
                FSW_DO_MEMZERO(dst+dlen, 0x1000-dlen);
            }
        }
        src += slen;
        dst += 0x1000;
    }
    return 0;
}

static fsw_status_t fsw_ntfs_get_extent_compressed (
    struct fsw_ntfs_volume *vol,
    struct fsw_ntfs_dnode  *dno,
    struct fsw_extent      *extent
) {
    if (vol->clbits > 16) {
        return FSW_VOLUME_CORRUPTED;
    }

    if ((extent->log_start << vol->clbits) > dno->fsize) {
        return FSW_NOT_FOUND;
    }

    int i;
    fsw_u64 vcn = extent->log_start & ~15;

    if (vcn == dno->cvcn) goto hit;
    dno->cvcn = vcn;
    dno->cperror = 0;
    dno->cpfull = 0;
    dno->cpzero = 0;

    for (i=0; i<16; i++) {
        fsw_status_t err;
        err = fsw_ntfs_dnode_get_lcn (
            vol, dno,
            vcn+i, &dno->clcn[i]
        );
        if (err == FSW_NOT_FOUND) {
            break;
        }
        else if (err != FSW_SUCCESS) {
            FSW_MSG_L03((
                FSW_MSG_STR(
                    "FSW_NTFS: fsw_ntfs_get_extent_compressed ... BAD LCN\n"
                )
            ));

            dno->cperror = 1;

            return FSW_VOLUME_CORRUPTED;
        }
    }

    if (i == 0) {
        dno->cpzero = 1;
    }
    else if (i==16) {
        dno->cpfull = 1;
    }
    else {
        fsw_status_t err;
        if (dno->cbuf == NULL) {
            err = FSW_DO_ALLOC(16<<vol->clbits, &dno->cbuf);
            if (err != FSW_SUCCESS) {
                dno->cvcn = BADVCN;
                return err;
            }
        }

        fsw_u8 *src;
        err = FSW_DO_ALLOC(i << vol->clbits, &src);
        if (err != FSW_SUCCESS) {
            dno->cvcn = BADVCN;
            return err;
        }

        int b;
        for (b=0; b<i; b++) {
            char *block;
            if (fsw_block_get (
                    &vol->g,
                    dno->clcn[b], 0,
                    (void **) &block
                ) != FSW_SUCCESS
            ) {
                dno->cperror = 1;
                FSW_MSG_L03((
                    FSW_MSG_STR(
                        "FSW_NTFS: fsw_ntfs_get_extent_compressed ... Read ERROR at block==%d\n"
                    ), i
                ));

                break;
            }
            FSW_DO_MEMCPY(src+(b<<vol->clbits), block, 1<<vol->clbits);
            fsw_block_release (&vol->g, dno->clcn[b], block);
        }

        if (dno->fsize >= ((vcn+16)<<vol->clbits)) {
            b = 16<<vol->clbits>>12;
        }
        else {
            b = (dno->fsize - (vcn << vol->clbits) + 0xfff)>>12;
        }

        if (!dno->cperror && ntfs_decomp (src, i<<vol->clbits, dno->cbuf, b) < 0) {
            dno->cperror = 1;
        }

        FSW_DO_FREE(src);
    }
hit:
    if (dno->cperror)
    return FSW_VOLUME_CORRUPTED;
    i = extent->log_start - vcn;
    if (dno->cpfull) {
        fsw_u64 lcn = dno->clcn[i];
        extent->phys_start = lcn;
        extent->log_count = 1;
        extent->type = FSW_EXTENT_TYPE_PHYSBLOCK;

        for (i++, lcn++; i<16 && lcn==dno->clcn[i]; i++, lcn++) {
            extent->log_count++;
        }
    }
    else if (dno->cpzero) {
        extent->log_count = 16 - i;
        extent->buffer = NULL;
        extent->type = FSW_EXTENT_TYPE_SPARSE;
    }
    else {
        extent->log_count = 1;
        fsw_status_t err = FSW_DO_ALLOC(1<<vol->clbits, &extent->buffer);
        if (err != FSW_SUCCESS) return err;

        FSW_DO_MEMCPY(extent->buffer, dno->cbuf + (i<<vol->clbits), 1<<vol->clbits);
        extent->type = FSW_EXTENT_TYPE_BUFFER;
    }
    return FSW_SUCCESS;
}

static fsw_status_t fsw_ntfs_get_extent_sparse (
    struct fsw_ntfs_volume *vol,
    struct fsw_ntfs_dnode  *dno,
    struct fsw_extent      *extent
) {
    fsw_status_t err;
    fsw_u64 lcn;

    if ((extent->log_start << vol->clbits) > dno->fsize) {
        return FSW_NOT_FOUND;
    }

    if ((extent->log_start << vol->clbits) > dno->finited) {
        extent->log_count = 1;
        extent->buffer = NULL;
        extent->type = FSW_EXTENT_TYPE_SPARSE;
        return FSW_SUCCESS;
    }

    err = fsw_ntfs_dnode_get_lcn (
        vol, dno,
        extent->log_start, &lcn
    );
    if (err == FSW_NOT_FOUND) {
        extent->log_count = 1;
        extent->buffer = NULL;
        extent->type = FSW_EXTENT_TYPE_SPARSE;
        return FSW_SUCCESS;
    }
    if (err != FSW_SUCCESS) return err;

    extent->phys_start = lcn;
    extent->log_count = 1;

    if (extent->log_start >= dno->cext.vcn &&
        extent->log_start < dno->cext.vcn+dno->cext.cnt
    ) {
        extent->log_count = dno->cext.cnt + extent->log_start - dno->cext.vcn;
    }

    extent->type = FSW_EXTENT_TYPE_PHYSBLOCK;
    return FSW_SUCCESS;
}

static
fsw_status_t fsw_ntfs_get_extent (
    struct fsw_volume *volg,
    struct fsw_dnode  *dnog,
    struct fsw_extent *extent
) {
    struct fsw_ntfs_volume *vol = (struct fsw_ntfs_volume *)volg;
    struct fsw_ntfs_dnode  *dno = (struct fsw_ntfs_dnode  *)dnog;

    if (dno->unreadable) return FSW_UNSUPPORTED;
    if (dno->embeded)    return fsw_ntfs_get_extent_embeded (vol, dno, extent);
    if (dno->compressed) return fsw_ntfs_get_extent_compressed (vol, dno, extent);

    return fsw_ntfs_get_extent_sparse (vol, dno, extent);
}

static fsw_status_t load_upcase (
    struct fsw_ntfs_volume *vol
) {
    fsw_status_t err;
    struct ntfs_mft mft;
    init_mft (vol, &mft, MFTNO_UPCASE);
    err = read_mft (vol, mft.buf, MFTNO_UPCASE);
    if (err == FSW_SUCCESS) {
        if ((err = read_small_attribute (
                vol, &mft, AT_DATA,
                (fsw_u8 **) &vol->upcase,
                &vol->upcount
            )) == FSW_SUCCESS
        ) {
            vol->upcount /= 2;

            #ifndef FSW_LITTLE_ENDIAN
            int i;
            for ( i=0; i<vol->upcount; i++) {
                vol->upcase[i] = FSW_U16_LE_SWAP(vol->upcase[i]);
            }
            #endif
        }
    }

    free_mft (&mft);
    return err;
}

static int ntfs_filename_cmp (
    struct fsw_ntfs_volume *vol,
    fsw_u8                 *p1,
    int                     s1,
    fsw_u8                 *p2,
    int                     s2
) {
    while (s1 > 0 && s2 > 0) {
        fsw_u16 c1 = u16get (p1, 0);
        fsw_u16 c2 = u16get (p2, 0);

        if (c1 < 0x80 || c2 < 0x80) {
            if (c1 < 0x80) c1 = upcase[c1];
            if (c2 < 0x80) c2 = upcase[c2];
        }
        else {

            if (!vol->upcase) {
                load_upcase (vol);
                if (!vol->upcase) {

                    vol->upcase = upcase;
                    vol->upcount = 0;
                }
            }
            if (c1 < vol->upcount) c1 = vol->upcase[c1];
            if (c2 < vol->upcount) c2 = vol->upcase[c2];
        }
        if (c1 < c2) return -1;
        if (c1 > c2) return 1;

        p1+=2;
        p2+=2;

        s1--;
        s2--;
    }

    if (s1 < s2) return -1;
    if (s1 > s2) return 1;

    return 0;
}

static fsw_status_t fsw_ntfs_create_subnode (
    struct fsw_ntfs_dnode  *dno,
    fsw_u8                 *buf,
    struct fsw_dnode      **child_dno
) {
    fsw_u64 mftno = u64get (buf, 0) & MFTMASK;
    if (mftno < MFTNO_META) return FSW_NOT_FOUND;

    int type = (
        u32get (buf, 0x48) & 0x10000000
    ) ? FSW_DNODE_TYPE_DIR: FSW_DNODE_TYPE_FILE;

    struct fsw_string s;
    s.type = FSW_STRING_TYPE_UTF16_LE;
    s.len  = u08get (buf, 0x50);
    s.data = buf + 0x52;
    s.size = s.len * 2;

    return fsw_dnode_create (
        &dno->g, mftno,
        type, &s, child_dno
    );
}

static fsw_u8 *fsw_ntfs_read_index_block (
    struct fsw_ntfs_volume *vol,
    struct fsw_ntfs_dnode  *dno,
    fsw_u64 block
) {
    if (dno->cbuf==NULL) {
	if (FSW_DO_ALLOC(dno->idxsz, &dno->cbuf) != FSW_SUCCESS)
	    return NULL;
    }

    if (block == dno->cvcn) {
        return dno->cbuf;
    }

    dno->cvcn = BADVCN;
    if (fsw_ntfs_read_buffer (
            vol, dno, dno->cbuf,
            (block-1)*dno->idxsz, dno->idxsz
        ) != dno->idxsz
    ) {
        return NULL;
    }

    if (fixup (
            dno->cbuf, "INDX",
            1<<vol->sctbits,
            dno->idxsz
        ) != FSW_SUCCESS
    ) {
        return NULL;
    }

    dno->cvcn = block;
    return dno->cbuf;
}

static fsw_status_t fsw_ntfs_dir_lookup (
    struct fsw_volume  *volg,
    struct fsw_dnode   *dnog,
    struct fsw_string  *lookup_name,
    struct fsw_dnode  **child_dno
) {
    struct fsw_ntfs_volume *vol = (struct fsw_ntfs_volume *)volg;
    struct fsw_ntfs_dnode  *dno = (struct fsw_ntfs_dnode  *)dnog;

    int depth = 0;
    struct fsw_string  s;
    fsw_u8            *buf;
    int                len;
    int                off;
    fsw_status_t       err;
    fsw_u64            block;
    fsw_u8             cpb;

    *child_dno = NULL;
    err = fsw_strdup_coerce (
        &s, FSW_STRING_TYPE_UTF16_LE,
        lookup_name
    );
    if (err) return err;

    buf = dno->idxroot + 16;
    len = dno->rootsz  - 16;
    if (len < 0x18) goto notfound;

    cpb = u08get (dno->idxroot, 12);
    if (cpb == 0) cpb = 1;

    while (depth < 10) {

        if (u32get (buf, 4) < len) len = u32get (buf, 4);

        off = u32get (buf, 0);
        if (off >= len) goto notfound;

        block = 0;
        while (off + 0x18 <= len) {
            int flag = u08get (buf, off + 12);
            int next = u16get (buf, off + 8) + off;
            int cmp;

            if (flag & 2) {

                cmp = -1;
                FSW_MSG_L03((
                    FSW_MSG_STR(
                        "FSW_NTFS: fsw_ntfs_dir_lookup ... depth==%d len==%x off==%x flag==%x next==%x cmp==%d\n"
                    ), depth, len, off, flag, next, cmp
                ));
            }
            else {
                int nlen = u08get (buf, off + 0x50);
                fsw_u8 *name = buf + off + 0x52;
                cmp = ntfs_filename_cmp (
                    vol, s.data, s.len,
                    name, nlen
                );
                FSW_MSG_L03((
                    FSW_MSG_STR(
                        "FSW_NTFS: fsw_ntfs_dir_lookup ... depth==%d len==%x off==%x flag==%x next==%x cmp==%d\n"
                    ), depth, len, off, flag, next, cmp
                ));
            }

            if (cmp == 0) {
                fsw_strfree (&s);
                return fsw_ntfs_create_subnode (
                    dno, buf+off, child_dno
                );
            }
            else if (cmp < 0) {
                if (!(flag & 1) || !dno->has_idxtree) goto notfound;

                block = FSW_U64_DIV(u64get (buf, next - 8), cpb) + 1;
                break;
            }
            else {

                off = next;
            }
        }
        if (!block) break;

        buf = fsw_ntfs_read_index_block (
            vol, dno, block
        );
        if (buf == NULL) break;

        buf += 24;
        len  = dno->idxsz - 24;
        depth++;
    }

notfound:
    fsw_strfree (&s);
    return FSW_NOT_FOUND;
}

static inline
void set_shand_pos (
    struct fsw_shandle *shand,
    int                 block,
    int                 off
) {
    shand->pos = (((fsw_u64)block) << 32) + off;
}

static inline
int test_idxbmp (
    struct fsw_ntfs_dnode *dno,
    int block
) {
    int mask;

    if (dno->idxbmp==NULL) return 1;

    block--;
    mask = 1 << (block & 7);

    block >>= 3;
    if (block > dno->bmpsz) return 0;
    return dno->idxbmp[block] & mask;
}

static fsw_status_t fsw_ntfs_dir_read (
    struct fsw_volume   *volg,
    struct fsw_dnode    *dnog,
    struct fsw_shandle  *shand,
    struct fsw_dnode   **child_dno
) {
    struct fsw_ntfs_volume *vol = (struct fsw_ntfs_volume *)volg;
    struct fsw_ntfs_dnode  *dno = (struct fsw_ntfs_dnode  *)dnog;

    int off   = shand->pos &  0xFFFFFFFF;
    int block = shand->pos >> 32;
    int mblocks;

    mblocks = FSW_U64_DIV(dno->fsize, dno->idxsz);
    while (block <= mblocks) {
        fsw_u8 *buf;
        int len;
        if (block == 0) {

            buf = dno->idxroot + 16;
            len = dno->rootsz - 16;
            if (len < 0x18)
            goto miss;
        }
        else if (
            !test_idxbmp (dno, block) ||
            !(buf = fsw_ntfs_read_index_block (vol, dno, block))
        ) {

            goto miss;
        }
        else {

            buf += 24;
            len = dno->idxsz - 24;
        }

        if (u32get (buf, 4) < len) len = u32get (buf, 4);
        if (off == 0) off = u32get (buf, 0);

        FSW_MSG_L03((
            FSW_MSG_STR(
                "FSW_NTFS: fsw_ntfs_dir_lookup ... block==%d len==%x off==%x\n"
            ), block, len, off
        ));

        while (off + 0x18 <= len) {
            int flag = u08get (buf, off + 12);
            if (flag & 2) break;

            int next = u16get (buf, off + 8) + off;

            FSW_MSG_L03((
                FSW_MSG_STR(
                    "FSW_NTFS: fsw_ntfs_dir_lookup ... flag==%x next==%x nt==%x\n"
                ), flag, next, u08get (buf, off + 0x51)
            ));

            if ((u08get (buf, off + 0x51) != 2)) {

                fsw_status_t err = fsw_ntfs_create_subnode (
                    dno, buf + off, child_dno
                );
                if (err != FSW_NOT_FOUND) {
                    set_shand_pos (shand, block, next);
                    return err;
                }
            }
            off = next;
        }
miss:
        if (!dno->has_idxtree) break;
        block++;
        off = 0;
    }

    set_shand_pos (shand, mblocks+1, 0);

    return FSW_NOT_FOUND;
}

static fsw_status_t fsw_ntfs_readlink (
    struct fsw_volume *volg,
    struct fsw_dnode  *dnog,
    struct fsw_string *link
) {
    struct fsw_ntfs_dnode *dno = (struct fsw_ntfs_dnode *)dnog;
    fsw_u8 *name;
    int     len;
    int     i;

    if (!dno->islink) {
        return FSW_UNSUPPORTED;
    }

    len  = u16get (dno->cbuf, 10);
    name = dno->cbuf + 0x10 + u16get (dno->cbuf, 8);

    if (u32get (dno->cbuf, 0) == 0xa000000c) {
        name += 4;
    }

    for (i=0; i<len; i+=2) {
        if (u16get (name, i) == '\\') {
            *(fsw_u16 *)(name+i) = FSW_U16_LE_SWAP('/');
        }
    }

    if (        (len > 6)         &&
        (u16get (name, 0)       ) == '/' && (u16get (name, 2) == '?') &&
        (u16get (name, 4)       ) == '?' && (u16get (name, 6) == '/') &&
        (u16get (name,10)       ) == ':' && (u16get (name,12) == '/') &&
	    (u16get (name, 8) | 0x20) >= 'a' && (u16get (name, 8) | 0x20) <= 'z'
    ) {
        len  -= 12;
        name += 12;
    }

    struct fsw_string s;
    s.type = FSW_STRING_TYPE_UTF16_LE;
    s.len  = len / 2;
    s.size = len;
    s.data = name;

    return fsw_strdup_coerce (
        link, volg->host_string_type, &s
    );
}

struct fsw_fstype_table   FSW_FSTYPE_TABLE_NAME(ntfs) = {
    { FSW_STRING_TYPE_UTF08, 4, 4, "ntfs" },
    sizeof (struct fsw_ntfs_volume),
    sizeof (struct fsw_ntfs_dnode),

    fsw_ntfs_volume_mount,
    fsw_ntfs_volume_free,
    fsw_ntfs_volume_stat,
    fsw_ntfs_dnode_fill,
    fsw_ntfs_dnode_free,
    fsw_ntfs_dnode_stat,
    fsw_ntfs_get_extent,
    fsw_ntfs_dir_lookup,
    fsw_ntfs_dir_read,
    fsw_ntfs_readlink,
};

struct fsw_fstype_table *fsw_active_fstype_table = &FSW_FSTYPE_TABLE_NAME(ntfs);
CONST CHAR16            *fsw_active_fstype_name  = L"ntfs";
