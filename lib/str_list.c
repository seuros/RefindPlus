// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2020-2026 Dayo Akanji
// SPDX-FileCopyrightText: 2012-2020 Roderick W. Smith

#include "mystrings.h"
#include "lib.h"
#include "screenmgt.h"

CHAR16 *FindCommaDelimited(IN CHAR16 *InString, IN UINTN Index)
{
    UINTN CurPos;
    UINTN EndPos;
    UINTN StartPos;
    UINTN InLength;
    UINTN ElemIndex;
    BOOLEAN HasContent;
    BOOLEAN InQuotes;
    BOOLEAN Found;
    CHAR16 *FoundString;

    if (InString == NULL) {
        return NULL;
    }

    CurPos = 0;
    StartPos = 0;
    EndPos = 0;
    ElemIndex = 0;
    InQuotes = FALSE;
    HasContent = FALSE;
    Found = FALSE;

    while (InString[CurPos] != L'\0') {
        if (InString[CurPos] == L'"') {

            if (!HasContent) {
                StartPos = CurPos;
            }
            InQuotes = !InQuotes;
            HasContent = TRUE;
            EndPos = CurPos + 1;
        }
        else if (InString[CurPos] == L',' && !InQuotes) {

            if (HasContent) {
                if (ElemIndex == Index) {
                    Found = TRUE;
                    break;
                }
                ElemIndex++;
                HasContent = FALSE;
            }
        }
        else {

            if (InQuotes || InString[CurPos] != L' ') {
                if (!HasContent) {
                    StartPos = CurPos;
                }
                HasContent = TRUE;
                EndPos = CurPos + 1;
            }
        }
        CurPos++;
    }

    if (!Found && HasContent && ElemIndex == Index) {
        Found = TRUE;
    }

    FoundString = NULL;
    if (Found) {
        InLength = EndPos - StartPos;

        FoundString = AllocateZeroPool((InLength + 1) * sizeof(CHAR16));
        if (FoundString != NULL) {
            gBS->CopyMem(FoundString, &InString[StartPos], InLength * sizeof(CHAR16));
            FoundString[InLength] = L'\0';
        }
    }

    return FoundString;
}

UINTN CountListItems(IN CHAR16 *InString)
{
    UINTN Count;
    UINTN Index;
    BOOLEAN InQuotes;
    BOOLEAN HasContent;

    if (InString == NULL || InString[0] == L'\0') {
        return 0;
    }

    Count = 0;
    Index = 0;
    InQuotes = FALSE;
    HasContent = FALSE;

    while (InString[Index] != L'\0') {
        if (InString[Index] == L'"') {

            InQuotes = !InQuotes;
            HasContent = TRUE;
        }
        else if (InString[Index] == L',' && !InQuotes) {

            if (HasContent) {
                Count++;
                HasContent = FALSE;
            }
        }
        else {

            if (InQuotes || InString[Index] != L' ') {
                HasContent = TRUE;
            }
        }

        Index++;
    }

    if (HasContent) {
        Count++;
    }

    return Count;
}

BOOLEAN DeleteItemFromCsvList(IN CHAR16 *ToDelete, IN CHAR16 **List)
{
    CHAR16 *TmpStr;
    CHAR16 *PartA;
    CHAR16 *PartB;
    CHAR16 *Found;
    CHAR16 *Comma;

    if (ToDelete == NULL || *List == NULL) {
        return FALSE;
    }

    Found = MrdStrFind(*List, ToDelete);
    if (Found == NULL)
        return FALSE;

    Comma = MrdStrFind(Found, L",");
    if (Comma == NULL) {

        if (Found == *List) {

            *List[0] = L'\0';
        }
        else {

            Found--;
            Found[0] = L'\0';
        }

        return TRUE;
    }

    TmpStr = PoolPrint(L",%s", ToDelete);
    PartA = GetSubStrBefore(TmpStr, *List);
    if (MrdStrEqualsCI(PartA, *List)) {

        MRD_FREE_POOL(PartA);
        PartA = GetSubStrBefore(ToDelete, *List);
        if (MrdStrEqualsCI(PartA, *List)) {
            MRD_FREE_POOL(PartA);
        }
    }
    MRD_FREE_POOL(TmpStr);

    TmpStr = PoolPrint(L"%s,", ToDelete);
    PartB = GetSubStrAfter(TmpStr, *List);
    if (MrdStrEqualsCI(PartB, *List)) {

        PartB = GetSubStrAfter(ToDelete, *List);
        if (MrdStrEqualsCI(PartB, *List)) {
            PartB = NULL;
        }
    }
    MRD_FREE_POOL(TmpStr);

    if (PartA == NULL && PartB == NULL) {

        return TRUE;
    }

    MRD_FREE_POOL(*List);
    if (PartA != NULL && PartB != NULL) {
        *List = PoolPrint(L"%s,%s", PartA, PartB);
    }
    else if (PartA != NULL) {
        *List = StrDuplicate(PartA);
    }
    else {
        *List = StrDuplicate(PartB);
    }
    MRD_FREE_POOL(PartA);

    return TRUE;
}

BOOLEAN IsIn(IN CHAR16 *SmallString, IN CHAR16 *List)
{
    if (SmallString == NULL || List == NULL) {
        return FALSE;
    }

    return IsListItem(SmallString, List);
}

BOOLEAN IsInSubstring(IN CHAR16 *BigString, IN CHAR16 *List)
{
    if (BigString == NULL || List == NULL) {
        return FALSE;
    }

    return IsListItemSubstringIn(BigString, List);
}

BOOLEAN IsListMatch(IN CHAR16 *TestString, IN CHAR16 *List)
{
    UINTN i;
    BOOLEAN Found;
    CHAR16 *OnePattern;

    if (TestString == NULL || List == NULL) {
        return FALSE;
    }

    i = 0;
    Found = FALSE;
    while (!Found) {
        OnePattern = FindCommaDelimited(List, i++);
        if (OnePattern == NULL)
            break;

        if (MeridianMetaiMatch(TestString, OnePattern)) {
            Found = TRUE;
        }
        MRD_FREE_POOL(OnePattern);
    }

    return Found;
}

BOOLEAN IsListItem(IN CHAR16 *SmallString, IN CHAR16 *List)
{
    UINTN i;
    BOOLEAN Found;
    CHAR16 *OneItem;

    if (SmallString == NULL || List == NULL) {
        return FALSE;
    }

    i = 0;
    Found = FALSE;
    while (!Found) {
        OneItem = FindCommaDelimited(List, i++);
        if (OneItem == NULL)
            break;

        if (MrdStrEqualsCI(OneItem, SmallString)) {
            Found = TRUE;
        }

        MRD_FREE_POOL(OneItem);
    }

    return Found;
}

BOOLEAN IsListItemSubstringIn(IN CHAR16 *BigString, IN CHAR16 *List)
{
    BOOLEAN Found;
    UINTN ElementLength, i;
    CHAR16 *OneElement;

    if (BigString == NULL || List == NULL) {
        return FALSE;
    }

    i = 0;
    Found = FALSE;
    while (!Found) {
        OneElement = FindCommaDelimited(List, i++);
        if (OneElement == NULL)
            break;

        ElementLength = StrLen(OneElement);
        if (ElementLength > 0 && ElementLength <= StrLen(BigString) &&
            MrdStrIncludesCI(BigString, OneElement)) {
            Found = TRUE;
        }

        if (!Found) {
            if (ElementLength <= StrLen(BigString) && MrdStrIncludesCI(BigString, OneElement)) {
                Found = TRUE;
            }
        }
        MRD_FREE_POOL(OneElement);
    }

    return Found;
}
