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

#define ENCODING_ISO8859_1 (0)
#define ENCODING_UTF8 (1)
#define ENCODING_UTF16_LE (2)

#define MRD_MAX_MACROS 64
static CHAR16 *gMacroName[MRD_MAX_MACROS];
static CHAR16 *gMacroValue[MRD_MAX_MACROS];
static UINTN gMacroCount = 0;

static CHAR16 *LookupConfigMacro(IN CHAR16 *Name)
{
    UINTN i;

    if (Name == NULL) {
        return NULL;
    }
    if (MrdStrEqualsCI(Name, L"ARCH"))
        return MRD_ARCH_STR;
    if (MrdStrEqualsCI(Name, L"FW_TYPE"))
        return MRD_FWTYPE_STR;

    for (i = 0; i < gMacroCount; i++) {
        if (MrdStrEqualsCI(gMacroName[i], Name)) {
            return gMacroValue[i];
        }
    }
    return NULL;
}

static VOID SetConfigMacro(IN CHAR16 *Name, IN CHAR16 *Value)
{
    UINTN i;

    if (Name == NULL || Value == NULL) {
        return;
    }
    for (i = 0; i < gMacroCount; i++) {
        if (MrdStrEqualsCI(gMacroName[i], Name)) {
            MRD_FREE_POOL(gMacroValue[i]);
            gMacroValue[i] = StrDuplicate(Value);
            return;
        }
    }
    if (gMacroCount >= MRD_MAX_MACROS) {
        return;
    }
    gMacroName[gMacroCount] = StrDuplicate(Name);
    gMacroValue[gMacroCount] = StrDuplicate(Value);
    gMacroCount++;
}

static CHAR16 *MacroDefName(IN CHAR16 *Token)
{
    UINTN Len;
    CHAR16 *Name;

    if (Token == NULL) {
        return NULL;
    }
    Len = StrLen(Token);
    if (Len < 4 || Token[0] != L'$' || Token[1] != L'{' || Token[Len - 1] != L'}') {
        return NULL;
    }
    Name = AllocateZeroPool((Len - 2) * sizeof(CHAR16));
    if (Name == NULL) {
        return NULL;
    }
    CopyMem(Name, Token + 2, (Len - 3) * sizeof(CHAR16));
    return Name;
}

static CHAR16 *ExpandConfigMacros(IN CHAR16 *In)
{
    CHAR16 *Out;
    CHAR16 *p;
    CHAR16 Ch[2];
    BOOLEAN HasMacro;

    if (In == NULL) {
        return NULL;
    }

    HasMacro = FALSE;
    for (p = In; *p != L'\0'; p++) {
        if (p[0] == L'$' && p[1] == L'{') {
            HasMacro = TRUE;
            break;
        }
    }
    if (!HasMacro) {
        return StrDuplicate(In);
    }

    Out = NULL;
    p = In;
    while (*p != L'\0') {
        if (p[0] == L'$' && p[1] == L'{') {
            CHAR16 *End = p + 2;
            while (*End != L'\0' && *End != L'}') {
                End++;
            }
            if (*End == L'}') {
                UINTN NameLen = (UINTN)(End - (p + 2));
                CHAR16 *Name = AllocateZeroPool((NameLen + 1) * sizeof(CHAR16));
                if (Name != NULL) {
                    CHAR16 *Val;
                    CopyMem(Name, p + 2, NameLen * sizeof(CHAR16));
                    Val = LookupConfigMacro(Name);
                    if (Val != NULL) {
                        MergeStrings(&Out, Val, 0);
                    }
                    MRD_FREE_POOL(Name);
                }
                p = End + 1;
                continue;
            }
        }
        Ch[0] = *p;
        Ch[1] = L'\0';
        MergeStrings(&Out, Ch, 0);
        p++;
    }

    if (Out == NULL) {
        Out = StrDuplicate(L"");
    }
    return Out;
}

CHAR16 *ReadLine(IN MERIDIAN_FILE *File)
{
    CHAR16 *Line;
    CHAR16 *qChar16;
    CHAR16 *pChar16;
    CHAR16 *LineEndChar16;
    CHAR16 *LineStartChar16;
    CHAR8 *pChar08;
    CHAR8 *LineEndChar08;
    CHAR8 *LineStartChar08;
    UINTN LineLength;

    if (File->BufferData == NULL) {

        return NULL;
    }

    if (File->Encoding != ENCODING_UTF8 && File->Encoding != ENCODING_UTF16_LE &&
        File->Encoding != ENCODING_ISO8859_1) {

        return NULL;
    }

    if (File->Encoding == ENCODING_UTF8 || File->Encoding == ENCODING_ISO8859_1) {
        pChar08 = File->Current08Ptr;
        if (pChar08 >= File->End08Ptr) {

            return NULL;
        }

        LineStartChar08 = pChar08;
        for (; pChar08 < File->End08Ptr; pChar08++) {
            if (*pChar08 == 13 || *pChar08 == 10) {
                break;
            }
        }
        LineEndChar08 = pChar08;
        for (; pChar08 < File->End08Ptr; pChar08++) {
            if (*pChar08 != 13 && *pChar08 != 10) {
                break;
            }
        }
        File->Current08Ptr = pChar08;

        LineLength = (UINTN)(LineEndChar08 - LineStartChar08) + 1;
        Line = AllocatePool(sizeof(CHAR16) * LineLength);
        if (Line == NULL) {

            return NULL;
        }

        qChar16 = Line;
        if (File->Encoding == ENCODING_ISO8859_1) {
            for (pChar08 = LineStartChar08; pChar08 < LineEndChar08;) {
                *qChar16++ = *pChar08++;
            }
        }
        else {
            if (File->Encoding == ENCODING_UTF8) {

                for (pChar08 = LineStartChar08; pChar08 < LineEndChar08;) {
                    *qChar16++ = *pChar08++;
                }
            }
        }
        *qChar16 = 0;

        return Line;
    }

    pChar16 = File->Current16Ptr;
    if (pChar16 >= File->End16Ptr) {

        return NULL;
    }

    LineStartChar16 = pChar16;
    for (; pChar16 < File->End16Ptr; pChar16++) {
        if (*pChar16 == 13 || *pChar16 == 10) {
            break;
        }
    }
    LineEndChar16 = pChar16;
    for (; pChar16 < File->End16Ptr; pChar16++) {
        if (*pChar16 != 13 && *pChar16 != 10) {
            break;
        }
    }
    File->Current16Ptr = pChar16;

    LineLength = (UINTN)(LineEndChar16 - LineStartChar16) + 1;
    Line = AllocatePool(sizeof(CHAR16) * LineLength);
    if (Line == NULL) {

        return NULL;
    }

    for (pChar16 = LineStartChar16, qChar16 = Line; pChar16 < LineEndChar16;) {
        *qChar16++ = *pChar16++;
    }
    *qChar16 = 0;

    return Line;
}

EFI_STATUS MeridianReadFile(IN EFI_FILE_HANDLE BaseDir, IN CHAR16 *FileName,
                            IN OUT MERIDIAN_FILE *File, OUT UINTN *size)
{
    EFI_STATUS Status;
    EFI_FILE_HANDLE FileHandle;
    EFI_FILE_INFO *FileInfo;
    CHAR16 *Message;
    UINT64 ReadSize;

    File->BufferData = NULL;
    File->BufferSize = 0;
    *size = 0;

    Status = BaseDir->Open(BaseDir, &FileHandle, FileName, MeridianReadOnly, 0);
    if (EFI_ERROR(Status)) {
        Message = PoolPrint(L"While Loading File:- '%s'", FileName);
        CheckError(Status, Message);
        MRD_FREE_POOL(Message);

        return Status;
    }

    FileInfo = LibFileInfo(FileHandle);
    if (FileInfo == NULL) {

        FileHandle->Close(FileHandle);

        return EFI_LOAD_ERROR;
    }
    ReadSize = FileInfo->FileSize;
    MRD_FREE_POOL(FileInfo);

    File->BufferSize = (UINTN)ReadSize;

    File->BufferData = AllocatePool(File->BufferSize);
    if (File->BufferData == NULL) {

        FileHandle->Close(FileHandle);

        return EFI_OUT_OF_RESOURCES;
    }

    Status = FileHandle->Read(FileHandle, &File->BufferSize, File->BufferData);
    if (EFI_ERROR(Status)) {
        Message = PoolPrint(L"While Reading File:- '%s'", FileName);
        CheckError(Status, Message);
        MRD_FREE_POOL(Message);
        MRD_FREE_POOL(File->BufferData);

        return Status;
    }

    FileHandle->Close(FileHandle);

    File->Encoding = ENCODING_UTF8;
    File->End08Ptr = (CHAR8 *)(File->BufferData + File->BufferSize);
    File->Current08Ptr = (CHAR8 *)(File->BufferData);
    File->Current16Ptr = (CHAR16 *)(File->BufferData);
    File->End16Ptr = (CHAR16 *)(File->BufferData + (File->BufferSize / 2));

    if (File->BufferSize >= 2) {
        if ((UINT8)(File->BufferData[0]) == 0xFF && (UINT8)(File->BufferData[1]) == 0xFE) {
            File->Encoding = ENCODING_UTF16_LE;
            File->Current16Ptr = (CHAR16 *)(File->BufferData + 2);
            File->End16Ptr = (CHAR16 *)(File->BufferData + File->BufferSize);
        }
    }

    *size = File->BufferSize;

    return Status;
}

UINTN ReadTokenLine(IN MERIDIAN_FILE *File, OUT CHAR16 ***TokenList)
{
    BOOLEAN LineFinished;
    BOOLEAN IsQuoted;
    CHAR16 *Token;
    CHAR16 *Line;
    CHAR16 *p;
    UINTN TokenCount;

    *TokenList = NULL;

    IsQuoted = FALSE;
    TokenCount = 0;
    while (TokenCount == 0) {
        Line = ReadLine(File);
        if (Line == NULL) {
            return 0;
        }

        p = Line;
        LineFinished = FALSE;
        while (!LineFinished) {

            while (!IsQuoted && (*p == ' ' || *p == '\t' || *p == '=' || *p == ',')) {
                p++;
            }

            if (*p == 0 || *p == '#') {
                break;
            }

            if (*p == '"') {
                IsQuoted = !IsQuoted;
                p++;
            }

            Token = p;

            while (KeepReading(p, &IsQuoted)) {
                if ((*p == L'/') && !IsQuoted) {

                    *p = L'\\';
                }
                p++;
            }

            if (*p == L'\0' || *p == L'#') {
                LineFinished = TRUE;
            }
            *p++ = 0;

            AddListElement((VOID ***)TokenList, &TokenCount, (VOID *)StrDuplicate(Token));
        }

        if (TokenCount >= 2) {
            CHAR16 *DefName = MacroDefName((*TokenList)[0]);
            if (DefName != NULL) {
                CHAR16 *Val = ExpandConfigMacros((*TokenList)[1]);
                SetConfigMacro(DefName, Val);
                MRD_FREE_POOL(Val);
                MRD_FREE_POOL(DefName);

                FreeTokenLine(TokenList, &TokenCount);
                TokenCount = 0;
            }
        }

        if (TokenCount > 0) {
            UINTN ti;
            for (ti = 0; ti < TokenCount; ti++) {
                CHAR16 *Exp = ExpandConfigMacros((*TokenList)[ti]);
                MRD_FREE_POOL((*TokenList)[ti]);
                (*TokenList)[ti] = Exp;
            }
        }

        MRD_FREE_POOL(Line);
    }

    return TokenCount;
}

VOID FreeTokenLine(IN OUT CHAR16 ***TokenList, IN OUT UINTN *TokenCount)
{

    FreeList((VOID ***)TokenList, TokenCount);
}

CHAR16 *GetFirstOptionsFromFile(IN CHAR16 *LoaderPath, IN MERIDIAN_VOLUME *Volume)
{
    UINTN TokenCount;
    CHAR16 **TokenList;
    CHAR16 *Options;
    MERIDIAN_FILE *File;

    LOG_SEP(L"X");
    LOG_INCREMENT();
    File = ReadLinuxOptionsFile(LoaderPath, Volume);

    Options = NULL;
    if (File != NULL) {
        TokenCount = ReadTokenLine(File, &TokenList);

        if (TokenCount > 1) {
            Options = StrDuplicate(TokenList[1]);
        }

        FreeTokenLine(&TokenList, &TokenCount);

        MRD_FREE_FILE(File);
    }

    LOG_DECREMENT();
    LOG_SEP(L"X");

    return Options;
}
