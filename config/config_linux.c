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

#define LINUX_OPTIONS_FILENAMES L"meridian_linux.conf,meridian-linux.conf"

#define ENCODING_ISO8859_1 (0)
#define ENCODING_UTF8 (1)
#define ENCODING_UTF16_LE (2)

static CHAR16 *SubvolFromTokens(IN CHAR16 **TokenList, IN UINTN TokenCount)
{
    CHAR16 *Value;
    CHAR16 *Subvol;
    UINTN Index, Len, i;

    if (TokenList == NULL) {
        return NULL;
    }
    for (Index = 0; (Index + 1) < TokenCount; Index++) {
        if (!MrdStrEqualsCI(TokenList[Index], L"subvol")) {
            continue;
        }
        Value = TokenList[Index + 1];
        if (Value == NULL) {
            return NULL;
        }
        Len = StrLen(Value);
        if (Len == 0) {
            return NULL;
        }
        Subvol = AllocateZeroPool((Len + 1) * sizeof(CHAR16));
        if (Subvol == NULL) {
            return NULL;
        }
        for (i = 0; i < Len; i++) {
            Subvol[i] = (Value[i] == L'\\') ? L'/' : Value[i];
        }
        Subvol[Len] = L'\0';

        return Subvol;
    }

    return NULL;
}

static MERIDIAN_FILE *GenerateOptionsFromEtcFstab(MERIDIAN_VOLUME *Volume)
{
    EFI_STATUS Status;
    UINTN i;
    UINTN TokenCount;
    CHAR16 **TokenList;
    CHAR16 *Line;
    CHAR16 *Root;
    CHAR16 *Subvol;
    CHAR16 *FstabPath;
    MERIDIAN_FILE *Fstab;
    MERIDIAN_FILE *Options;

    LOG_SEP(L"X");
    LOG_INCREMENT();

    FstabPath = NULL;
    if (FileExists(Volume->RootDir, L"\\etc\\fstab")) {
        FstabPath = L"\\etc\\fstab";
    }
    else if (FileExists(Volume->RootDir, L"\\@\\etc\\fstab")) {

        FstabPath = L"\\@\\etc\\fstab";
    }
    if (FstabPath == NULL) {
        LOG_DECREMENT();
        LOG_SEP(L"X");

        return NULL;
    }

    Options = AllocateZeroPool(sizeof(MERIDIAN_FILE));
    if (Options == NULL) {
        LOG_DECREMENT();
        LOG_SEP(L"X");

        return NULL;
    }

    Fstab = AllocateZeroPool(sizeof(MERIDIAN_FILE));
    if (Fstab == NULL) {
        LOG_DECREMENT();
        LOG_SEP(L"X");

        MRD_FREE_FILE(Options);

        return NULL;
    }

    Status = MeridianReadFile(Volume->RootDir, FstabPath, Fstab, &i);

    if (EFI_ERROR(Status)) {
        CheckError(Status, L"while reading /etc/fstab");
        LOG_DECREMENT();
        LOG_SEP(L"X");

        MRD_FREE_FILE(Options);
        MRD_FREE_FILE(Fstab);

        return NULL;
    }

    Options->Encoding = ENCODING_UTF16_LE;

    while (1) {
        TokenCount = ReadTokenLine(Fstab, &TokenList);
        if (TokenCount == 0) {
            FreeTokenLine(&TokenList, &TokenCount);

            break;
        }

#if MERIDIAN_DEBUG > 0
        DEBUG_LOG(1, LOG_THREE_STAR_MID, L"Read Line Holding %d Token%s from '/etc/fstab'",
                  TokenCount, (TokenCount == 1) ? L"" : L"s");
#endif

        LOG_SEP(L"X");
        Subvol = NULL;
        if (TokenCount > 2) {
            if (MrdStrEquals(TokenList[1], L"\\")) {
                Root = PoolPrint(L"%s", TokenList[0]);
            }
            else if (MrdStrEquals(TokenList[2], L"\\")) {
                Root = PoolPrint(L"%s=%s", TokenList[0], TokenList[1]);
            }
            else {
                Root = NULL;
            }

            if (Root != NULL) {
                Subvol = SubvolFromTokens(TokenList, TokenCount);
            }

            if (Root != NULL && Root[0] != L'\0') {
                for (i = 0; i < StrLen(Root); i++) {
                    LOG_SEP(L"X");
                    if (Root[i] == '\\') {
                        Root[i] = '/';
                    }
                    LOG_SEP(L"X");
                }

                Line = (Subvol != NULL)
                           ? PoolPrint(L"\"Boot with Default Options\"    \"ro root=%s "
                                       L"rootflags=subvol=%s\"\n",
                                       Root, Subvol)
                           : PoolPrint(L"\"Boot with Default Options\"    \"ro root=%s\"\n", Root);

                MergeStrings((CHAR16 **)&(Options->BufferData), Line, 0);

                MRD_FREE_POOL(Line);

                Line = (Subvol != NULL)
                           ? PoolPrint(L"\"Boot into SingleUser Mode\"    \"ro root=%s "
                                       L"rootflags=subvol=%s single\"\n",
                                       Root, Subvol)
                           : PoolPrint(L"\"Boot into SingleUser Mode\"    \"ro root=%s single\"\n",
                                       Root);

                MergeStrings((CHAR16 **)&(Options->BufferData), Line, 0);

                MRD_FREE_POOL(Line);

                Options->BufferSize = StrSize((CHAR16 *)Options->BufferData);
            }

            MRD_FREE_POOL(Root);
            MRD_FREE_POOL(Subvol);
        }

        FreeTokenLine(&TokenList, &TokenCount);

        LOG_SEP(L"X");
    }

    if (Options->BufferData == NULL) {
        MRD_FREE_POOL(Options);
    }
    else {
        Options->Current08Ptr = (CHAR8 *)Options->BufferData;
        Options->Current16Ptr = (CHAR16 *)Options->BufferData;
        Options->End08Ptr = Options->Current08Ptr + Options->BufferSize;
        Options->End16Ptr = Options->Current16Ptr + (Options->BufferSize >> 1);
    }

    MRD_FREE_FILE(Fstab);

    LOG_DECREMENT();
    LOG_SEP(L"X");

    return Options;
}

static MERIDIAN_FILE *GenerateOptionsFromPartTypes(VOID)
{
    CHAR16 *WriteStatus;
    CHAR16 *GuidString;
    CHAR16 *Line;
    MERIDIAN_FILE *Options;

    LOG_SEP(L"X");
    LOG_INCREMENT();
    if (GlobalConfig.DiscoveredRoot == NULL) {
        LOG_DECREMENT();
        LOG_SEP(L"X");

        return NULL;
    }

    WriteStatus = (GlobalConfig.DiscoveredRoot->IsMarkedReadOnly) ? L"ro" : L"rw";

    Options = AllocateZeroPool(sizeof(MERIDIAN_FILE));
    if (Options == NULL) {
        LOG_DECREMENT();
        LOG_SEP(L"X");

        return NULL;
    }

    GuidString = GuidAsString(&(GlobalConfig.DiscoveredRoot->PartGuid));
    if (GuidString != NULL) {
        ToLower(GuidString);

        Line = PoolPrint(L"\"Boot with Default Options\"    \"%s root=/dev/disk/by-partuuid/%s\"\n",
                         WriteStatus, GuidString);
        MergeStrings((CHAR16 **)&(Options->BufferData), Line, 0);
        MRD_FREE_POOL(Line);

        Line = PoolPrint(
            L"\"Boot into SingleUser Mode\"    \"%s root=/dev/disk/by-partuuid/%s single\"\n",
            WriteStatus, GuidString);

        MergeStrings((CHAR16 **)&(Options->BufferData), Line, 0);

        MRD_FREE_POOL(Line);
        MRD_FREE_POOL(GuidString);
    }

    Options->Encoding = ENCODING_UTF16_LE;
    Options->Current08Ptr = (CHAR8 *)Options->BufferData;
    Options->Current16Ptr = (CHAR16 *)Options->BufferData;
    Options->BufferSize = StrSize((CHAR16 *)Options->BufferData);
    Options->End08Ptr = Options->Current08Ptr + Options->BufferSize;
    Options->End16Ptr = Options->Current16Ptr + (Options->BufferSize >> 1);

    LOG_DECREMENT();
    LOG_SEP(L"X");

    return Options;
}

MERIDIAN_FILE *ReadLinuxOptionsFile(IN CHAR16 *LoaderPath, IN MERIDIAN_VOLUME *Volume)
{
    EFI_STATUS Status;
    CHAR16 *OptionsFilename;
    CHAR16 *FullFilename;
    CHAR16 *BaseFilename;
    UINTN size, i;
    BOOLEAN FileFound;
    MERIDIAN_FILE *File;

    LOG_SEP(L"X");
    LOG_INCREMENT();

    File = NULL;
    FileFound = FALSE;
    BaseFilename = NULL;
    FullFilename = FindPath(LoaderPath);

    i = 0;
    while (!FileFound && FullFilename != NULL) {
        OptionsFilename = FindCommaDelimited(LINUX_OPTIONS_FILENAMES, i++);
        if (OptionsFilename == NULL)
            break;

        LOG_SEP(L"X");
        BaseFilename = StrDuplicate(FullFilename);

        MergeStrings(&BaseFilename, OptionsFilename, '\\');

        if (!FileExists(Volume->RootDir, BaseFilename)) {
        }
        else {
            MRD_FREE_FILE(File);
            File = AllocateZeroPool(sizeof(MERIDIAN_FILE));
            if (File == NULL) {
                MRD_FREE_POOL(OptionsFilename);
                MRD_FREE_POOL(FullFilename);
                MRD_FREE_POOL(BaseFilename);

                LOG_DECREMENT();
                LOG_SEP(L"X");

                return NULL;
            }

            Status = MeridianReadFile(Volume->RootDir, BaseFilename, File, &size);
            if (!EFI_ERROR(Status)) {
                FileFound = TRUE;
            }
            else {
                CheckError(Status, L"While Loading the Linux Options File");
            }
        }

        MRD_FREE_POOL(OptionsFilename);
        MRD_FREE_POOL(BaseFilename);

        LOG_SEP(L"X");
    }
    MRD_FREE_POOL(FullFilename);

    if (!FileFound) {

        MRD_FREE_FILE(File);
        File = GenerateOptionsFromEtcFstab(Volume);

        if (File == NULL) {
            UINTN VolIndex;

            for (VolIndex = 0; VolIndex < VolumesCount && File == NULL; VolIndex++) {
                if (Volumes[VolIndex] == NULL || Volumes[VolIndex] == Volume ||
                    Volumes[VolIndex]->RootDir == NULL) {
                    continue;
                }
                File = GenerateOptionsFromEtcFstab(Volumes[VolIndex]);
            }
        }

        if (File == NULL) {
            File = GenerateOptionsFromPartTypes();
        }
    }

    LOG_DECREMENT();
    LOG_SEP(L"X");
    return File;
}
