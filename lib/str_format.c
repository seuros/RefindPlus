// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2020-2026 Dayo Akanji
// SPDX-FileCopyrightText: 2012-2020 Roderick W. Smith

#include "mystrings.h"
#include "lib.h"
#include "screenmgt.h"

#define MAX_WORD_LEN (64)

BOOLEAN NestedStrStr = FALSE;

CHAR16 *CapitalisedCase(IN CHAR16 *InputString, IN BOOLEAN SpecialCases)
{
    UINTN WordLen, i;
    UINTN IndexIn;
    UINTN IndexOut;
    CHAR16 StringChar;
    CHAR16 *ResultString;
    CHAR16 WordBuffer[MAX_WORD_LEN];
    BOOLEAN HandleCase;

    if (InputString == NULL) {
        return NULL;
    }

    if (!SpecialCases) {
        ResultString = AllocatePool(StrSize(InputString));
    }
    else {
        ResultString = AllocatePool(StrSize(InputString) * 2);
    }
    if (ResultString == NULL) {
        return NULL;
    }

    IndexIn = 0;
    IndexOut = 0;

    while (1) {
        StringChar = InputString[IndexIn];
        if (StringChar == L'\0') {
            break;
        }

        if (((StringChar < L'a') || (StringChar > L'z')) &&
            ((StringChar < L'A') || (StringChar > L'Z'))) {
            if (StringChar != L'-') {

                ResultString[IndexOut] = StringChar;
            }
            else if (InputString[IndexIn + 1] == L'\0') {
                ResultString[IndexOut] = StringChar;
            }
            else if (((InputString[IndexIn + 1] < L'a') || (InputString[IndexIn + 1] > L'z')) &&
                     ((InputString[IndexIn + 1] < L'A') || (InputString[IndexIn + 1] > L'Z'))) {

                ResultString[IndexOut] = StringChar;
            }
            else if (IndexIn == 0) {

                ResultString[IndexOut] = StringChar;
            }
            else if (((InputString[IndexIn - 1] < L'a') || (InputString[IndexIn - 1] > L'z')) &&
                     ((InputString[IndexIn - 1] < L'A') || (InputString[IndexIn - 1] > L'Z'))) {

                ResultString[IndexOut] = StringChar;
            }
            else {

                ResultString[IndexOut] = L' ';
            }

            IndexIn += 1;
            IndexOut += 1;

            continue;
        }

        WordLen = 0;
        while (((StringChar >= L'a') && (StringChar <= L'z')) ||
               ((StringChar >= L'A') && (StringChar <= L'Z'))) {
            if (WordLen < (MAX_WORD_LEN - 1)) {
                WordBuffer[WordLen] = StringChar;

                WordLen += 1;
            }
            IndexIn += 1;

            StringChar = InputString[IndexIn];
        }

        /* coverity[dead_error_line: SUPPRESS] */
        if (WordLen == 0)
            continue;

        WordBuffer[WordLen] = L'\0';

        if (!SpecialCases) {
            HandleCase = TRUE;
        }
        else {
            HandleCase = FALSE;

            if (MrdStrEqualsCI(WordBuffer, L"to") || MrdStrEqualsCI(WordBuffer, L"into")) {

                ResultString[IndexOut++] = L'i';
                ResultString[IndexOut++] = L'n';
                ResultString[IndexOut++] = L't';
                ResultString[IndexOut++] = L'o';
            }
            else if (MrdStrEqualsCI(WordBuffer, L"with")) {

                ResultString[IndexOut++] = L'w';
                ResultString[IndexOut++] = L'i';
                ResultString[IndexOut++] = L't';
                ResultString[IndexOut++] = L'h';
            }
            else if (MrdStrEqualsCI(WordBuffer, L"standard") ||
                     MrdStrEqualsCI(WordBuffer, L"normal")) {

                ResultString[IndexOut++] = L'D';
                ResultString[IndexOut++] = L'e';
                ResultString[IndexOut++] = L'f';
                ResultString[IndexOut++] = L'a';
                ResultString[IndexOut++] = L'u';
                ResultString[IndexOut++] = L'l';
                ResultString[IndexOut++] = L't';
            }
            else {
                HandleCase = TRUE;
            }
        }
        if (!HandleCase)
            continue;

        if (WordBuffer[0] >= L'a' && WordBuffer[0] <= L'z') {
            ResultString[IndexOut++] = WordBuffer[0] - L'a' + L'A';
        }
        else {
            ResultString[IndexOut++] = WordBuffer[0];
        }

        for (i = 1; i < WordLen; i++) {
            ResultString[IndexOut++] = WordBuffer[i];
        }
    }

    ResultString[IndexOut] = L'\0';

    if (SpecialCases) {
        ReplaceSubstring(&ResultString, L"Single User", L"SingleUser");
    }

    return ResultString;
}

static CHAR16 *GetDelimiter(IN CHAR16 *InputDelimiter, IN CHAR16 *String)
{

    if (!MrdStrEqualsCI(InputDelimiter, DEFAULT_STRING_DELIM)) {
        return InputDelimiter;
    }

    if (MrdStrFind(String, INITIAL_STRING_DELIM)) {
        return INITIAL_STRING_DELIM;
    }

    return DEFAULT_STRING_DELIM;
}

CHAR16 *GetSubStrAfter(IN CHAR16 *InputDelimiter, IN CHAR16 *String)
{
    CHAR16 *Substring;
    CHAR16 *Delimiter;

    if (String == NULL) {
        return NULL;
    }

    Delimiter = GetDelimiter(InputDelimiter, String);

    Substring = MrdStrFind(String, Delimiter);
    if (Substring == NULL) {

        return String;
    }

    Substring += StrLen(Delimiter);
    if (*Substring == L'\0') {

        return String;
    }

    return Substring;
}

CHAR16 *GetSubStrBefore(IN CHAR16 *InputDelimiter, IN CHAR16 *String)
{
    UINTN Length;
    CHAR16 *Result;
    CHAR16 *Substring;
    CHAR16 *Delimiter;

    if (String == NULL) {
        return NULL;
    }

    Delimiter = GetDelimiter(InputDelimiter, String);

    Substring = MrdStrFind(String, Delimiter);
    if (Substring == NULL) {

        return StrDuplicate(String);
    }

    if (MrdStrEqualsCI(Substring, String)) {

        return StrDuplicate(String);
    }

    Length = StrLen(String) - StrLen(Substring);
    Result = AllocateZeroPool((Length + 1) * sizeof(CHAR16));
    if (Result == NULL) {

        return NULL;
    }

    gBS->CopyMem(Result, String, sizeof(CHAR16) * Length);
    Result[Length] = L'\0';

    return Result;
}

EFI_STATUS SafeStrCat(OUT CHAR16 *Dest, IN UINTN DestSize, IN CONST CHAR16 *Src)
{
    EFI_STATUS Status;
    UINTN i;
    BOOLEAN FoundNull;

    if (Dest == NULL || Src == NULL || DestSize == 0) {
        return EFI_INVALID_PARAMETER;
    }

    FoundNull = FALSE;
    for (i = 0; i < DestSize; i++) {
        if (Dest[i] == L'\0') {
            FoundNull = TRUE;

            break;
        }
    }

    if (!FoundNull) {

        Dest[DestSize - 1] = L'\0';
    }

    Status = StrnCatS(Dest, DestSize, Src, StrLen(Src));
    return Status;
}

VOID ToLower(IN OUT CHAR16 *MyString)
{
    UINTN i;

    if (MyString == NULL) {
        return;
    }

    i = 0;
    while (MyString[i] != L'\0') {
        if ((MyString[i] >= L'A') && (MyString[i] <= L'Z')) {
            MyString[i] = MyString[i] - L'A' + L'a';
        }
        i++;
    }
}

CHAR16 *SanitiseString(CHAR16 *InString)
{
    CHAR16 *Temp, *Word, *p;
    CHAR16 *OutString;
    BOOLEAN LineFinished;

    if (InString == NULL) {
        return NULL;
    }

    OutString = NULL;
    Temp = Word = p = StrDuplicate(InString);
    if (Temp) {
        LineFinished = FALSE;

        while (!LineFinished) {
            if ((*p != L' ') && (*p != L'_') && (*p != L'-') && !('a' <= *p && 'z' >= *p) &&
                !('A' <= *p && 'Z' >= *p) && !('0' <= *p && '9' >= *p)) {
                if (*p == L'\0') {
                    LineFinished = TRUE;
                }

                *p = L'\0';

                if (*Word != L'\0') {
                    MergeStrings(&OutString, Word, L' ');
                }

                Word = p + 1;
            }

            p++;
        }

        MRD_FREE_POOL(Temp);
    }

    if (OutString == NULL) {
        OutString = StrDuplicate(InString);
    }

    return OutString;
}

BOOLEAN LimitStringLength(IN CHAR16 *TheString, IN UINTN Limit)
{
    UINTN i;
    UINTN DestSize;
    CHAR16 *SubString;
    CHAR16 *TempString;
    BOOLEAN HasChanged;
    BOOLEAN WasTruncated;

    if (TheString == NULL) {
        return FALSE;
    }

    LOG_SEP(L"X");
    LOG_INCREMENT();

    if (StrLen(TheString) < Limit) {
        LOG_DECREMENT();
        LOG_SEP(L"X");

        return FALSE;
    }

    HasChanged = FALSE;

    SubString = MrdStrFind(TheString, L"  ");

    while (SubString != NULL) {
        i = 0;
        while (SubString[i] == L' ') {
            i++;
        }

        if (i >= StrLen(SubString)) {
            SubString[0] = '\0';
        }
        else {
            TempString = StrDuplicate(&SubString[i]);
            if (TempString == NULL) {

                break;
            }

            DestSize = StrSize(&SubString[1]) / sizeof(CHAR16);
            StrCpyS(&SubString[1], DestSize, TempString);
            MRD_FREE_POOL(TempString);
        }

        HasChanged = TRUE;
        SubString = MrdStrFind(TheString, L"  ");
    }

    WasTruncated = TruncateString(TheString, Limit);

    if (!HasChanged) {
        HasChanged = WasTruncated;
    }

    LOG_DECREMENT();
    LOG_SEP(L"X");

    return HasChanged;
}

BOOLEAN TruncateString(IN CHAR16 *TheString, IN UINTN Limit)
{
    BOOLEAN WasTruncated;

    if (StrLen(TheString) <= Limit) {
        WasTruncated = FALSE;
    }
    else {
        TheString[Limit] = '\0';
        WasTruncated = TRUE;
    }

    return WasTruncated;
}

CHAR16 *FindNumbers(IN CHAR16 *InString)
{
    UINTN i, EndOfElement, StartOfElement, CopyLength;
    CHAR16 *Found, *ExtraFound, *LookFor;

    if (InString == NULL) {
        return NULL;
    }

    StartOfElement = StrLen(InString);

    EndOfElement = i = 0;
    ExtraFound = NULL;
    while (ExtraFound == NULL) {
        LookFor = FindCommaDelimited(GlobalConfig.ExtraKernelVersionStrings, i++);
        if (LookFor == NULL)
            break;

        ExtraFound = MrdStrFind(InString, LookFor);
        if (ExtraFound != NULL) {
            StartOfElement = ExtraFound - InString;
            EndOfElement = (StrLen(LookFor) + StartOfElement) - 1;
        }

        MRD_FREE_POOL(LookFor);
    }

    for (i = 0; InString[i] != L'\0'; i++) {
        if ((InString[i] >= L'0') && (InString[i] <= L'9')) {
            if (StartOfElement > i) {
                StartOfElement = i;
            }

            if (EndOfElement < i) {
                EndOfElement = i;
            }
        }
    }

    Found = NULL;
    if (EndOfElement > 0) {
        if (EndOfElement >= StartOfElement) {
            CopyLength = EndOfElement - StartOfElement + 1;

            Found = StrDuplicate(&InString[StartOfElement]);
            if (Found != NULL) {
                Found[CopyLength] = 0;
            }
        }
    }

    return (Found);
}

BOOLEAN ReplaceSubstring(IN OUT CHAR16 **MainString, IN CHAR16 *SearchString, IN CHAR16 *ReplString)
{
    UINTN DestSize;
    CHAR16 *EndString;
    CHAR16 *NewString;
    CHAR16 *FoundSearchString;

    LOG_SEP(L"X");
    LOG_INCREMENT();
    if (*MainString == NULL || SearchString == NULL || ReplString == NULL) {
        LOG_DECREMENT();
        LOG_SEP(L"X");

        return FALSE;
    }

    FoundSearchString = MrdStrFind(*MainString, SearchString);
    NestedStrStr = FALSE;

    if (FoundSearchString == NULL) {
        LOG_DECREMENT();
        LOG_SEP(L"X");
        return FALSE;
    }

    DestSize = StrLen(*MainString) + 1;
    NewString = AllocateZeroPool(DestSize * sizeof(CHAR16));
    if (NewString == NULL) {
        LOG_DECREMENT();
        LOG_SEP(L"X");
        return FALSE;
    }

    EndString = &(FoundSearchString[StrLen(SearchString)]);
    FoundSearchString[0] = L'\0';

    if ((FoundSearchString > *MainString) && (FoundSearchString[-1] == L'%')) {
        FoundSearchString[-1] = L'\0';
        ReplString = SearchString;
    }

    StrCpyS(NewString, DestSize, *MainString);

    MergeStrings(&NewString, ReplString, L'\0');

    MergeStrings(&NewString, EndString, L'\0');

    MRD_FREE_POOL(*MainString);
    *MainString = NewString;

    LOG_DECREMENT();
    LOG_SEP(L"X");

    return TRUE;
}

VOID DeleteStringList(STRING_LIST *StringList)
{
    STRING_LIST *Current, *Previous;

    if (StringList == NULL) {
        return;
    }

    Current = StringList;
    while (Current != NULL) {
        MRD_FREE_POOL(Current->Value);
        Previous = Current;
        Current = Current->Next;
        MRD_FREE_POOL(Previous);
    }
}

CHAR16 *MrdAsciiToUnicode(IN CHAR8 *AsciiString, IN UINTN Length)
{
    CHAR16 *UnicodeString;
    CHAR16 *UnicodeStringWalker;
    UINTN UnicodeStringSize;

    if (AsciiString == NULL) {
        return NULL;
    }

    if (Length == 0) {
        Length = AsciiStrLen(AsciiString);
    }

    UnicodeStringSize = (Length + 1) * sizeof(CHAR16);
    UnicodeString = AllocatePool(UnicodeStringSize);

    if (UnicodeString != NULL) {
        UnicodeStringWalker = UnicodeString;
        while (*AsciiString != '\0' && Length--) {
            *(UnicodeStringWalker++) = *(AsciiString++);
        }
        *UnicodeStringWalker = L'\0';
    }

    return UnicodeString;
}

VOID MrdStrFilterAscii(IN OUT CHAR16 *String, IN BOOLEAN SingleLine)
{
    while (*String != L'\0') {
        if ((*String & 0x7FU) != *String) {

            *String = L'_';
        }
        else if (SingleLine && (*String == L'\r' || *String == L'\n')) {

            *String = L'\0';

            break;
        }
        else {
            if (*String < 0x20 || *String == 0x7F) {

                *String = L'_';
            }
        }

        ++String;
    }
}
