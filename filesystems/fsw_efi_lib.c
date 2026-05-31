// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2026 Dayo Akanji
// SPDX-FileCopyrightText: 2006 Christoph Pfisterer

#include "fsw_efi.h"

#define SECSPERMIN      60
#define MINSPERHOUR     60
#define HOURSPERDAY     24
#define DAYSPERWEEK     7
#define DAYSPERNYEAR    365
#define DAYSPERLYEAR    366
#define SECSPERHOUR     (SECSPERMIN * MINSPERHOUR)
#define SECSPERDAY      ((long) SECSPERHOUR * HOURSPERDAY)
#define MONSPERYEAR     12

#define EPOCH_YEAR      1970
#define EPOCH_WDAY      TM_THURSDAY

#define isleap(y) (((y) % 4) == 0 && (((y) % 100) != 0 || ((y) % 400) == 0))
#define LEAPS_THRU_END_OF(y)    ((y) / 4 - (y) / 100 + (y) / 400)

static const int mon_lengths[2][MONSPERYEAR] = {
    { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
    { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }
};
static const int year_lengths[2] = {
    DAYSPERNYEAR, DAYSPERLYEAR
};

VOID fsw_efi_decode_time (
    OUT EFI_TIME *EfiTime,
    IN  UINT32    UnixTime
) {
    long        days, rem;
    int         y, newy, yleap;
    const int   *ip;

    ZeroMem(EfiTime, sizeof (EFI_TIME));

    days = UnixTime / SECSPERDAY;
    rem  = UnixTime % SECSPERDAY;

    EfiTime->Hour = (UINT8) (rem / SECSPERHOUR);
    rem = rem % SECSPERHOUR;
    EfiTime->Minute = (UINT8) (rem / SECSPERMIN);
    EfiTime->Second = (UINT8) (rem % SECSPERMIN);

    y = EPOCH_YEAR;
    while (days < 0 || days >= (long) year_lengths[yleap = isleap(y)]) {
        newy = y + days / DAYSPERNYEAR;
        if (days < 0) --newy;

        days -= (newy - y) * DAYSPERNYEAR +
            LEAPS_THRU_END_OF(newy - 1) -
            LEAPS_THRU_END_OF(y - 1);

        y = newy;
    }
    EfiTime->Year = (UINT16)y;
    ip = mon_lengths[yleap];
    for (
        EfiTime->Month = 0;
        days >= (long) ip[EfiTime->Month];
        ++(EfiTime->Month)
    ) {
        days = days - (long) ip[EfiTime->Month];
    }

    EfiTime->Month++;
    EfiTime->Day = (UINT8) (days + 1);
}

UINTN fsw_efi_strsize (
    struct fsw_string *s
) {
    if (s->type == FSW_STRING_TYPE_EMPTY) {
        return sizeof (CHAR16);
    }

    return (s->len + 1) * sizeof (CHAR16);
}

VOID fsw_efi_strcpy (
    CHAR16            *Dest,
    struct fsw_string *src
) {
    if (src->size == 0 ||
        src->type == FSW_STRING_TYPE_EMPTY
    ) {
        Dest[0] = 0;
    }
    else if (src->type == FSW_STRING_TYPE_UTF16) {
        CopyMem(Dest, src->data, src->size);
        Dest[src->len] = 0;
    }
    else {

        Dest[0] = 0;
    }
}
