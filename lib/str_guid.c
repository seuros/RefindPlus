// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2020-2026 Dayo Akanji
// SPDX-FileCopyrightText: 2012-2020 Roderick W. Smith

#include "mystrings.h"
#include "lib.h"
#include "screenmgt.h"

BOOLEAN IsValidHex(IN CHAR16 *Input)
{
    UINTN i;
    BOOLEAN GoodHex;

    if (Input == NULL || Input[0] == L'\0') {
        return FALSE;
    }

    if (MrdStrEqualsCI(Input, L"0x")) {
        return FALSE;
    }

    i = 0;
    GoodHex = TRUE;
    while (GoodHex && Input[i] != L'\0') {
        if (i == 0 && Input[i] == L'0') {
            i++;
            continue;
        }
        if (i == 1 && (Input[i] == L'x' || Input[i] == L'X')) {
            if (Input[0] != L'0') {
                GoodHex = FALSE;
                break;
            }

            i++;
            continue;
        }

        if ((Input[i] < L'0' || Input[i] > L'9') && (Input[i] < L'A' || Input[i] > L'F') &&
            (Input[i] < L'a' || Input[i] > L'f')) {
            GoodHex = FALSE;
            break;
        }

        i++;
    }

    return GoodHex;
}

UINT64 StrToHex(IN CHAR16 *OurStr, IN UINTN Pos, IN UINTN NumChars)
{
    UINTN InputLength;
    UINTN NumDone;
    UINT64 retval;
    CHAR16 *Input;
    CHAR16 a;

    if (OurStr == NULL) {
        return 0;
    }

    Input = GetSubStrAfter(L"0x", OurStr);
    if (NumChars == 0 || NumChars > 16) {
        return 0;
    }

    NumDone = 0;
    retval = 0x00;
    InputLength = StrLen(Input);
    while (Pos <= InputLength && NumDone < NumChars) {
        a = Input[Pos];

        if ((a >= '0') && (a <= '9')) {
            retval *= 0x10;
            retval += (a - '0');
            NumDone++;
        }

        if ((a >= 'a') && (a <= 'f')) {
            retval *= 0x10;
            retval += (a - 'a' + 0x0a);
            NumDone++;
        }

        if ((a >= 'A') && (a <= 'F')) {
            retval *= 0x10;
            retval += (a - 'A' + 0x0a);
            NumDone++;
        }

        Pos++;
    }

    return retval;
}

BOOLEAN IsGuid(IN CHAR16 *UnknownString)
{
    UINTN Length, i;
    CHAR16 a;
    BOOLEAN retval;

    if (UnknownString == NULL) {
        return FALSE;
    }

    Length = StrLen(UnknownString);
    if (Length != 36) {
        return FALSE;
    }

    retval = TRUE;
    for (i = 0; i < Length; i++) {
        a = UnknownString[i];
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (a != L'-') {
                retval = FALSE;
                break;
            }
        }
    }

    return retval;
}

CHAR16 *GuidAsString(EFI_GUID *GuidData)
{
    CHAR16 *TheString;

    if (GuidData == NULL) {

        return NULL;
    }

    TheString = AllocatePool(sizeof(CHAR16) * 37);
    if (TheString == NULL) {

        return NULL;
    }

    SPrint(TheString, sizeof(CHAR16) * 37, L"%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
           (UINTN)GuidData->Data1, (UINTN)GuidData->Data2, (UINTN)GuidData->Data3,
           (UINTN)GuidData->Data4[0], (UINTN)GuidData->Data4[1], (UINTN)GuidData->Data4[2],
           (UINTN)GuidData->Data4[3], (UINTN)GuidData->Data4[4], (UINTN)GuidData->Data4[5],
           (UINTN)GuidData->Data4[6], (UINTN)GuidData->Data4[7]);

    return TheString;
}

EFI_GUID StringAsGuid(CHAR16 *InString)
{
    EFI_GUID Guid = NULL_GUID_VALUE;

    if (!IsGuid(InString)) {
        return Guid;
    }

    Guid.Data1 = (UINT32)StrToHex(InString, 0, 8);
    Guid.Data2 = (UINT16)StrToHex(InString, 9, 4);
    Guid.Data3 = (UINT16)StrToHex(InString, 14, 4);
    Guid.Data4[0] = (UINT8)StrToHex(InString, 19, 2);
    Guid.Data4[1] = (UINT8)StrToHex(InString, 21, 2);
    Guid.Data4[2] = (UINT8)StrToHex(InString, 23, 2);
    Guid.Data4[3] = (UINT8)StrToHex(InString, 26, 2);
    Guid.Data4[4] = (UINT8)StrToHex(InString, 28, 2);
    Guid.Data4[5] = (UINT8)StrToHex(InString, 30, 2);
    Guid.Data4[6] = (UINT8)StrToHex(InString, 32, 2);
    Guid.Data4[7] = (UINT8)StrToHex(InString, 34, 2);

    return Guid;
}
