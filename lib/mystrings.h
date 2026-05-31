// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2020-2025 Dayo Akanji
// SPDX-FileCopyrightText: 2012-2020 Roderick W. Smith

#ifndef __MYSTRINGS_H_
#define __MYSTRINGS_H_

#include "tiano_includes.h"
#include "GenericBdsLib.h"

typedef struct _string_list
{
    CHAR16 *Value;
    struct _string_list *Next;
} STRING_LIST;

extern BOOLEAN NestedStrStr;

BOOLEAN IsValidHex(IN CHAR16 *Input);
BOOLEAN IsGuid(IN CHAR16 *UnknownString);
BOOLEAN IsIn(IN CHAR16 *SmallString, IN CHAR16 *List);
BOOLEAN IsListItem(IN CHAR16 *SmallString, IN CHAR16 *List);
BOOLEAN IsListMatch(IN CHAR16 *TestString, IN CHAR16 *List);
BOOLEAN IsInSubstring(IN CHAR16 *BigString, IN CHAR16 *List);
BOOLEAN TruncateString(IN CHAR16 *TheString, IN UINTN Limit);
BOOLEAN LimitStringLength(IN CHAR16 *TheString, IN UINTN Limit);
BOOLEAN DeleteItemFromCsvList(IN CHAR16 *ToDelete, IN CHAR16 **List);
BOOLEAN IsListItemSubstringIn(IN CHAR16 *BigString, IN CHAR16 *List);
BOOLEAN ReplaceSubstring(IN OUT CHAR16 **MainString, IN CHAR16 *SearchString,
                         IN CHAR16 *ReplString);
BOOLEAN MrdStrEquals(IN CHAR16 *String1, IN CHAR16 *String2);
BOOLEAN MrdStrEqualsCI(IN CHAR16 *String1, IN CHAR16 *String2);
BOOLEAN MrdStrEndsWithCI(IN CHAR16 *String1, IN CHAR16 *String2);
BOOLEAN MrdStrStartsWithCI(IN CHAR16 *String1, IN CHAR16 *String2);
BOOLEAN MrdStrIncludesCI(IN CHAR16 *BigStr, IN CHAR16 *SmallStr);

CHAR16 *FindNumbers(IN CHAR16 *InString);
CHAR16 *GuidAsString(EFI_GUID *GuidData);
CHAR16 *SanitiseString(CHAR16 *InString);
CHAR16 *MrdStrFind(IN CHAR16 *String, IN CHAR16 *StrCharSet);
CHAR16 *FindCommaDelimited(IN CHAR16 *InString, IN UINTN Index);
CHAR16 *MrdAsciiToUnicode(IN CHAR8 *AsciiString, IN UINTN Length);
CHAR16 *GetSubStrAfter(IN CHAR16 *InputDelimiter, IN CHAR16 *String);
CHAR16 *GetSubStrBefore(IN CHAR16 *InputDelimiter, IN CHAR16 *String);
CHAR16 *CapitalisedCase(IN CHAR16 *InputString, IN BOOLEAN SpecialCases);

VOID ToLower(IN OUT CHAR16 *MyString);
VOID DeleteStringList(STRING_LIST *StringList);
VOID MergeWords(IN OUT CHAR16 **MergeTo, IN CHAR16 *InString, IN CHAR16 AddChar);
VOID MergeUniqueWords(IN OUT CHAR16 **MergeTo, IN CHAR16 *InString, IN CHAR16 AddChar);
VOID MergeUniqueItems(IN OUT CHAR16 **MergeTo, IN CHAR16 *InString, IN CHAR16 AddChar);
VOID MergeStrings(IN OUT CHAR16 **First, IN CHAR16 *Second, IN CHAR16 AddChar);
VOID MergeUniqueStrings(IN OUT CHAR16 **First, IN CHAR16 *Second, IN CHAR16 AddChar);
VOID MrdStrFilterAscii(IN OUT CHAR16 *String, IN BOOLEAN SingleLine);

UINTN CountListItems(IN CHAR16 *InString);
UINTN MrdStrCommonPrefixLen(IN CHAR16 *String1, IN CHAR16 *String2);

UINT64 StrToHex(IN CHAR16 *OurStr, IN UINTN Pos, IN UINTN NumChars);

EFI_GUID StringAsGuid(CHAR16 *InString);

EFI_STATUS SafeStrCat(OUT CHAR16 *Dest, IN UINTN DestSize, IN CONST CHAR16 *Src);
#endif
