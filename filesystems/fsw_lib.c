// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2026 Dayo Akanji
// SPDX-FileCopyrightText: 2006 Christoph Pfisterer

#include "fsw_core.h"

#include "fsw_strfunc.h"

fsw_status_t fsw_alloc_zero (
    int    len,
    void **ptr_out
) {
    fsw_status_t status;

    status = FSW_DO_ALLOC(len, ptr_out);
    if (status) return status;

    FSW_DO_MEMZERO(*ptr_out, len);

    return FSW_SUCCESS;
}

fsw_status_t fsw_memdup (
    void **dest_out,
    void  *src,
    int    len
) {
    fsw_status_t status;

    status = FSW_DO_ALLOC(len, dest_out);
    if (status) return status;

    FSW_DO_MEMCPY(*dest_out, src, len);

    return FSW_SUCCESS;
}

int fsw_strlen (
    struct fsw_string *s
) {
    if (s->type == FSW_STRING_TYPE_EMPTY) {
        return 0;
    }

    return s->len;
}

int fsw_streq (
    struct fsw_string *s1,
    struct fsw_string *s2
) {
    struct fsw_string temp_s;

    if (s1->type == FSW_STRING_TYPE_EMPTY ||
        s2->type == FSW_STRING_TYPE_EMPTY
    ) {
        temp_s.type = FSW_STRING_TYPE_ISO88591;
        temp_s.size =           temp_s.len = 0;
        temp_s.data =                     NULL;

        if (s1->type == FSW_STRING_TYPE_EMPTY) {
            return fsw_streq(&temp_s, s2);
        }

        return fsw_streq(s1, &temp_s);
    }

    if (s1->len != s2->len) return 0;
    if (s1->len == 0)       return 1;

    if (s1->type == s2->type) {

        if (s1->size != s2->size) return 0;

        return FSW_DO_MEMEQ(
            s1->data,
            s2->data,
            s1->size
        );
    }

    #define STREQ_DISPATCH(type1, type2)                                              \
      if (s1->type == FSW_STRING_TYPE_##type1 && s2->type == FSW_STRING_TYPE_##type2) \
        return fsw_streq_##type1##_##type2(s1->data, s2->data, s1->len);              \
      if (s2->type == FSW_STRING_TYPE_##type1 && s1->type == FSW_STRING_TYPE_##type2) \
        return fsw_streq_##type1##_##type2(s2->data, s1->data, s1->len);
    STREQ_DISPATCH(UTF08,         UTF16);
    STREQ_DISPATCH(ISO88591,      UTF16);
    STREQ_DISPATCH(ISO88591,      UTF08);
    STREQ_DISPATCH(ISO88591, UTF16_SWAP);
    STREQ_DISPATCH(UTF16,    UTF16_SWAP);
    STREQ_DISPATCH(UTF08,    UTF16_SWAP);

    return 0;
}

int fsw_streq_cstr (
    struct fsw_string *s1,
    const char        *s2
) {
    struct fsw_string temp_s;
    int i;

    for (i = 0; s2[i]; i++);

    temp_s.type = FSW_STRING_TYPE_ISO88591;
    temp_s.size =           temp_s.len = i;
    temp_s.data =              (char *) s2;

    return fsw_streq (s1, &temp_s);
}

fsw_status_t fsw_strdup_coerce (
    struct fsw_string *dest,
    int                type,
    struct fsw_string *src
) {
    fsw_status_t    status;

    if (src->type == FSW_STRING_TYPE_EMPTY || src->len == 0) {
        dest->type =          type;
        dest->size = dest->len = 0;
        dest->data =          NULL;

        return FSW_SUCCESS;
    }

    if (src->type == type) {
        dest->type = type;
        dest->len  = src->len;
        dest->size = src->size;

        status = FSW_DO_ALLOC(dest->size, &dest->data);
        if (status) return status;

        FSW_DO_MEMCPY(dest->data, src->data, dest->size);
        return FSW_SUCCESS;
    }

    #define STRCOERCE_DISPATCH(type1, type2)                                       \
      if (src->type == FSW_STRING_TYPE_##type1 && type == FSW_STRING_TYPE_##type2) \
        return fsw_strcoerce_##type1##_##type2(src->data, src->len, dest);
    STRCOERCE_DISPATCH(ISO88591,      UTF16);
    STRCOERCE_DISPATCH(ISO88591,      UTF08);
    STRCOERCE_DISPATCH(UTF08,      ISO88591);
    STRCOERCE_DISPATCH(UTF08,         UTF16);
    STRCOERCE_DISPATCH(UTF16,         UTF08);
    STRCOERCE_DISPATCH(UTF16,      ISO88591);
    STRCOERCE_DISPATCH(UTF16_SWAP, ISO88591);
    STRCOERCE_DISPATCH(UTF16_SWAP,    UTF08);
    STRCOERCE_DISPATCH(UTF16_SWAP,    UTF16);

    return FSW_UNSUPPORTED;
}

void fsw_strsplit (
    struct fsw_string *element,
    struct fsw_string *buffer,
    char separator
) {
    int i, maxlen;

    if (buffer->type == FSW_STRING_TYPE_EMPTY || buffer->len == 0) {
        element->type = FSW_STRING_TYPE_EMPTY;

        return;
    }

    maxlen   =  buffer->len;
    *element = *buffer;

    if (buffer->type == FSW_STRING_TYPE_ISO88591) {
        fsw_u8 *p;

        p = (fsw_u8 *)element->data;
        for (i = 0; i < maxlen; i++, p++) {
            if (*p == separator) {
                buffer->data = p + 1;
                buffer->len -= i + 1;

                break;
            }
        }

        if (i == maxlen) {
            buffer->data = p;
            buffer->len -= i;
        }

        element->len  = i;
        element->size = element->len;
        buffer->size  = buffer->len;

    }
    else if (buffer->type == FSW_STRING_TYPE_UTF16) {
        fsw_u16 *p;

        p = (fsw_u16 *)element->data;
        for (i = 0; i < maxlen; i++, p++) {
            if (*p == separator) {
                buffer->data = p + 1;
                buffer->len -= i + 1;
                break;
            }
        }

        if (i == maxlen) {
            buffer->data = p;
            buffer->len -= i;
        }

        element->len  = i;
        element->size = element->len * sizeof (fsw_u16);
        buffer->size  =  buffer->len * sizeof (fsw_u16);

    }
    else {

        buffer->type = FSW_STRING_TYPE_EMPTY;
    }

}

void fsw_strfree (struct fsw_string *s) {
    if (s->type != FSW_STRING_TYPE_EMPTY && s->data) {
        FSW_DO_FREE(s->data);
    }
    s->type = FSW_STRING_TYPE_EMPTY;
}
