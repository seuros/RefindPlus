// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2020-2026 Dayo Akanji
// SPDX-FileCopyrightText: 2021 Joe van Tunen
// SPDX-FileCopyrightText: 2012-2023 Roderick W. Smith
// SPDX-FileCopyrightText: 2006 Christoph Pfisterer

#include "global.h"
#include "lib.h"
#include "menu.h"
#include "scan.h"
#include "apple.h"
#include "config.h"
#include "screenmgt.h"
#include "mystrings.h"
#include "mok.h"

#define LAST_MINUTE (1439)

BOOLEAN KeepReading(IN OUT CHAR16 *InString, IN OUT BOOLEAN *IsQuoted)
{
    CHAR16 *Temp;
    UINTN DestSize;
    BOOLEAN MoreToRead;

    if (IsQuoted == NULL || InString == NULL || *InString == L'\0') {
        return FALSE;
    }

    MoreToRead = (*IsQuoted || (*InString != ' ' && *InString != '\t' && *InString != '=' &&
                                *InString != '#' && *InString != ','));

    if (*InString == L'"') {
        if (InString[1] != L'"') {
            *IsQuoted = !(*IsQuoted);
            MoreToRead = FALSE;
        }
        else {
            Temp = StrDuplicate(&InString[1]);
            if (Temp != NULL) {
                DestSize = StrSize(InString) / sizeof(CHAR16);
                StrCpyS(InString, DestSize, Temp);
                MRD_FREE_POOL(Temp);
            }
            MoreToRead = TRUE;
        }
    }

    return MoreToRead;
}

VOID HandleSignedInt(IN CHAR16 **TokenList, IN UINTN TokenCount, OUT INTN *Value)
{
    if (TokenCount == 2) {
        *Value = (TokenList[1][0] == '-') ? Atoi(TokenList[1] + 1) * -1 : Atoi(TokenList[1]);
    }
}

VOID HandleUnsignedInt(IN CHAR16 **TokenList, IN UINTN TokenCount, OUT UINTN *Value)
{
    if (TokenCount == 2) {
        *Value = Atoi(TokenList[1]);
    }
}

VOID HandleString(IN CHAR16 **TokenList, IN UINTN TokenCount, OUT CHAR16 **Target)
{

    if (Target == NULL || TokenCount != 2) {
        return;
    }

    MRD_FREE_POOL(*Target);
    *Target = StrDuplicate(TokenList[1]);
}

VOID HandleStrings(IN CHAR16 **TokenList, IN UINTN TokenCount, OUT CHAR16 **Target)
{
    UINTN i;
    BOOLEAN AddMode;

    if (Target == NULL) {
        return;
    }

    if (TokenCount > 2 && MrdStrEqualsCI(TokenList[1], L"+")) {
        AddMode = TRUE;
    }
    else {
        AddMode = FALSE;
    }

    if (!AddMode && *Target != NULL) {
        MRD_FREE_POOL(*Target);
    }

    for (i = 1; i < TokenCount; i++) {
        if ((i != 1) || !AddMode) {
            CleanUpPathNameSlashes(TokenList[i]);
            MergeStrings(Target, TokenList[i], L',');
        }
    }
}

VOID HandleHexes(IN CHAR16 **TokenList, IN UINTN TokenCount, IN UINTN MaxValue,
                 OUT UINT32_LIST **Target)
{
    UINTN i;
    UINTN InputIndex;
    UINT32 Value;
    UINT32_LIST *EndOfList;
    UINT32_LIST *NewEntry;

    if (TokenCount > 2 && MrdStrEqualsCI(TokenList[1], L"+")) {
        InputIndex = 2;
        EndOfList = *Target;
        while (EndOfList != NULL && EndOfList->Next != NULL) {
            EndOfList = EndOfList->Next;
        }
    }
    else {
        InputIndex = 1;
        EndOfList = NULL;
        EraseUint32List(Target);
    }

    for (i = InputIndex; i < TokenCount; i++) {
        if (!IsValidHex(TokenList[i])) {
            continue;
        }

        Value = (UINT32)StrToHex(TokenList[i], 0, 8);
        if (Value > MaxValue) {
            continue;
        }

        NewEntry = AllocatePool(sizeof(UINT32_LIST));
        if (NewEntry == NULL) {
            return;
        }

        NewEntry->Value = Value;
        NewEntry->Next = NULL;

        if (EndOfList == NULL) {
            EndOfList = NewEntry;
            *Target = NewEntry;
        }
        else {
            EndOfList->Next = NewEntry;
            EndOfList = NewEntry;
        }
    }
}

UINTN HandleTime(IN CHAR16 *TimeString)
{
    UINTN i;
    UINTN Hour;
    UINTN Minute;
    UINTN TimeLength;
    UINTN TimeMinutes;

    TimeLength = StrLen(TimeString);
    i = Hour = Minute = 0;

    while (i < TimeLength) {
        if (TimeString[i] == L':') {
            Hour = Minute;
            Minute = 0;
        }

        if (TimeString[i] >= L'0' && TimeString[i] <= L'9') {
            Minute *= 10;
            Minute += (TimeString[i] - L'0');
        }

        i += 1;
    }

    TimeMinutes = (Hour == 0) ? Minute : (Hour * 60) + Minute;

    return TimeMinutes;
}

BOOLEAN HandleBoolean(IN CHAR16 **TokenList, IN UINTN TokenCount)
{
    BOOLEAN TruthValue;

    TruthValue = TRUE;
    if (TokenCount >= 2 &&
        (MrdStrEqualsCI(TokenList[1], L"0") || MrdStrEqualsCI(TokenList[1], L"off") ||
         MrdStrEqualsCI(TokenList[1], L"false"))) {
        TruthValue = FALSE;
    }

    return TruthValue;
}
