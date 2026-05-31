// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2020-2026 Dayo Akanji
// SPDX-FileCopyrightText: 2012-2020 Roderick W. Smith

#include "mystrings.h"
#include "lib.h"
#include "screenmgt.h"

static BOOLEAN IsValidStrComp(IN CHAR16 *String1, IN CHAR16 *String2)
{
    UINTN Len1;
    UINTN Len2;

    if (String1 == NULL || String2 == NULL) {
        return FALSE;
    }

    Len1 = StrLen(String1);
    Len2 = StrLen(String2);

    if (Len1 > Len2) {

        return FALSE;
    }

    return TRUE;
}

BOOLEAN MrdStrEquals(IN CHAR16 *String1, IN CHAR16 *String2)
{
    if (String1 == NULL || String2 == NULL) {
        return FALSE;
    }

    return (StrCmp(String1, String2) == 0);
}

BOOLEAN MrdStrEqualsCI(IN CHAR16 *String1, IN CHAR16 *String2)
{
    CHAR16 c1;
    CHAR16 c2;

    if (String1 == NULL || String2 == NULL) {
        return FALSE;
    }

    while (*String1 && *String2) {
        c1 = *String1;
        c2 = *String2;

        if (c1 >= L'A' && c1 <= L'Z')
            c1 += 32;
        if (c2 >= L'A' && c2 <= L'Z')
            c2 += 32;
        if (c1 != c2)
            return FALSE;

        String1++;
        String2++;
    }

    return (*String1 == *String2);
}

BOOLEAN MrdStrStartsWithCI(IN CHAR16 *String1, IN CHAR16 *String2)
{
    UINTN i;
    UINTN Len1;
    CHAR16 c1, c2;
    BOOLEAN IsGood;

    IsGood = IsValidStrComp(String1, String2);
    if (!IsGood) {
        return FALSE;
    }

    Len1 = StrLen(String1);

    for (i = 0; i < Len1; i++) {
        c1 = String1[i];
        c2 = String2[i];

        if (c1 >= L'A' && c1 <= L'Z')
            c1 += 32;
        if (c2 >= L'A' && c2 <= L'Z')
            c2 += 32;
        if (c1 != c2) {

            IsGood = FALSE;

            break;
        }
    }

    return IsGood;
}

BOOLEAN MrdStrEndsWithCI(IN CHAR16 *String1, IN CHAR16 *String2)
{
    UINTN i;
    UINTN Len1;
    UINTN Len2;
    CHAR16 c1, c2;
    BOOLEAN IsGood;

    IsGood = IsValidStrComp(String1, String2);
    if (!IsGood) {
        return FALSE;
    }

    Len1 = StrLen(String1);
    Len2 = StrLen(String2);

    for (i = 0; i < Len1; i++) {
        c1 = String1[Len1 - 1 - i];
        c2 = String2[Len2 - 1 - i];

        if (c1 >= L'A' && c1 <= L'Z')
            c1 += 32;
        if (c2 >= L'A' && c2 <= L'Z')
            c2 += 32;
        if (c1 != c2) {

            IsGood = FALSE;

            break;
        }
    }

    return IsGood;
}

CHAR16 *MrdStrFind(IN CHAR16 *String, IN CHAR16 *StrCharSet)
{
    CHAR16 *Src;
    CHAR16 *Sub;

    if (!NestedStrStr)
        LOG_SEP(L"X");
    LOG_INCREMENT();
    if (String == NULL || StrCharSet == NULL) {
        LOG_DECREMENT();
        if (!NestedStrStr)
            LOG_SEP(L"X");
        return NULL;
    }

    Src = String;
    Sub = StrCharSet;

    while ((*String != L'\0') && (*StrCharSet != L'\0')) {
        if (*String++ == *StrCharSet) {
            StrCharSet++;
        }
        else {
            String = ++Src;
            StrCharSet = Sub;
        }
    }

    if (*StrCharSet == L'\0') {
        LOG_DECREMENT();
        if (!NestedStrStr)
            LOG_SEP(L"X");
        return Src;
    }

    LOG_DECREMENT();
    if (!NestedStrStr)
        LOG_SEP(L"X");

    return NULL;
}

BOOLEAN MrdStrIncludesCI(IN CHAR16 *BigStr, IN CHAR16 *SmallStr)
{
    CHAR16 c1;
    CHAR16 c2;

    UINTN BigIndex;
    UINTN SmallIndex;
    UINTN BigStart = 0;

    if (BigStr == NULL || SmallStr == NULL) {
        return FALSE;
    }

    if (*BigStr == L'\0' && *SmallStr == L'\0') {
        return TRUE;
    }

    if (*BigStr == L'\0' || *SmallStr == L'\0') {
        return FALSE;
    }

    while (BigStr[BigStart] != L'\0') {
        BigIndex = BigStart;
        SmallIndex = 0;

        while (1) {
            if (SmallStr[SmallIndex] == L'\0') {
                return TRUE;
            }

            if (BigStr[BigIndex] == L'\0') {
                return FALSE;
            }

            c1 = SmallStr[SmallIndex];
            c2 = BigStr[BigIndex];

            if (c1 >= L'A' && c1 <= L'Z')
                c1 += 32;
            if (c2 >= L'A' && c2 <= L'Z')
                c2 += 32;
            if (c1 != c2)
                break;

            SmallIndex++;
            BigIndex++;
        }

        BigStart++;
    }

    return FALSE;
}

UINTN MrdStrCommonPrefixLen(IN CHAR16 *String1, IN CHAR16 *String2)
{
    UINTN Count;

    if (String1 == NULL || String2 == NULL) {
        return 0;
    }

    Count = 0;
    while (String1[Count] != L'\0' && String2[Count] != L'\0' && String1[Count] == String2[Count]) {
        Count++;
    }

    return Count;
}
