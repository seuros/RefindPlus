// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2020-2026 Dayo Akanji
// SPDX-FileCopyrightText: 2012-2020 Roderick W. Smith

#include "mystrings.h"
#include "lib.h"
#include "screenmgt.h"

static VOID MergeStringsHelper(IN OUT CHAR16 **First, IN CHAR16 *Second, IN CHAR16 AddChar,
                               IN BOOLEAN UniqueOnly)
{
    UINTN i;
    UINTN Length1;
    UINTN Length2;
    UINTN BufSize;
    CHAR16 *TestStr;
    CHAR16 *NewString;
    BOOLEAN SkipMerge;

    if (*First == NULL) {
        *First = StrDuplicate(Second);

        return;
    }

    Length1 = StrLen(*First);
    Length2 = (Second != NULL) ? StrLen(Second) : 0;

    BufSize = Length1 + Length2 + 2;
    NewString = AllocatePool(BufSize * sizeof(CHAR16));
    if (NewString == NULL) {
        return;
    }

    if (*First != NULL && Length1 == 0) {
        MRD_FREE_POOL(*First);
    }

    NewString[0] = L'\0';
    if (*First != NULL) {
        SafeStrCat(NewString, BufSize, *First);

        if (AddChar) {
            StrnCatS(NewString, BufSize, &AddChar, 1);
        }
    }

    if (Second != NULL) {
        SkipMerge = FALSE;

        if (UniqueOnly && AddChar) {
            i = 0;
            while (!SkipMerge) {
                TestStr = FindCommaDelimited(NewString, i++);
                if (TestStr == NULL)
                    break;

                NestedStrStr = TRUE;
                if (MrdStrEqualsCI(TestStr, Second)) {
                    SkipMerge = TRUE;
                }
                NestedStrStr = FALSE;

                MRD_FREE_POOL(TestStr);
            }
        }

        if (!SkipMerge) {
            SafeStrCat(NewString, BufSize, Second);
        }
        else {
            if (AddChar) {

                NewString[Length1] = L'\0';
            }
        }
    }

    MRD_FREE_POOL(*First);
    *First = NewString;
}

VOID MergeStrings(IN OUT CHAR16 **First, IN CHAR16 *Second, IN CHAR16 AddChar)
{
    LOG_SEP(L"X");
    LOG_INCREMENT();

    MergeStringsHelper(First, Second, AddChar, FALSE);

    LOG_DECREMENT();
    LOG_SEP(L"X");
}

VOID MergeUniqueStrings(IN OUT CHAR16 **First, IN CHAR16 *Second, IN CHAR16 AddChar)
{
    LOG_SEP(L"X");
    LOG_INCREMENT();

    MergeStringsHelper(First, Second, AddChar, TRUE);

    LOG_DECREMENT();
    LOG_SEP(L"X");
}

static VOID MergeWordsHelper(CHAR16 **MergeTo, CHAR16 *InString, CHAR16 AddChar, BOOLEAN UniqueOnly)
{
    CHAR16 *Temp, *Word, *p;
    BOOLEAN LineFinished;

    if (InString == NULL) {
        return;
    }

    Temp = Word = p = StrDuplicate(InString);
    if (Temp) {
        LineFinished = FALSE;

        while (!LineFinished) {
            if ((*p == L' ') || (*p == L':') || (*p == L'_') || (*p == L'-') || (*p == L'/') ||
                (*p == L'\\') || (*p == L'\0')) {
                if (*p == L'\0') {
                    LineFinished = TRUE;
                }

                *p = L'\0';

                if (*Word != L'\0') {
                    if (UniqueOnly) {
                        MergeUniqueStrings(MergeTo, Word, AddChar);
                    }
                    else {
                        MergeStrings(MergeTo, Word, AddChar);
                    }
                }

                Word = p + 1;
            }

            p++;
        }

        MRD_FREE_POOL(Temp);
    }
}

VOID MergeWords(CHAR16 **MergeTo, CHAR16 *InString, CHAR16 AddChar)
{
    MergeWordsHelper(MergeTo, InString, AddChar, FALSE);
}

VOID MergeUniqueWords(CHAR16 **MergeTo, CHAR16 *InString, CHAR16 AddChar)
{
    MergeWordsHelper(MergeTo, InString, AddChar, TRUE);
}

VOID MergeUniqueItems(CHAR16 **MergeTo, CHAR16 *InString, CHAR16 AddChar)
{
    UINTN i;
    CHAR16 *Item;

    if (InString == NULL) {
        return;
    }

    i = 0;
    while (1) {
        Item = FindCommaDelimited(InString, i++);
        if (Item == NULL)
            break;

        MergeUniqueStrings(MergeTo, Item, AddChar);
        MRD_FREE_POOL(Item);
    }
}
