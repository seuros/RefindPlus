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

BOOLEAN AddSubmenu(LOADER_ENTRY *Entry, MERIDIAN_FILE *File, MERIDIAN_VOLUME *Volume, CHAR16 *Title)
{
    MERIDIAN_MENU_SCREEN *SubScreen;
    LOADER_ENTRY *SubEntry;
    UINTN TokenCount;
    CHAR16 *GraphicsTag;
    CHAR16 *TmpName;
    CHAR16 **TokenList;
    MERIDIAN_VOLUME *TargetVolume;

    LOG_SEP(L"X");
    LOG_INCREMENT();

    TargetVolume = GetStanzaVolume(File, Volume);
    if (TargetVolume == NULL) {

#if MERIDIAN_DEBUG > 0
        DEBUG_LOG(1, LOG_STAR_HEAD_SEP, L"SubMenu Entry is Disabled");
#endif

        LOG_DECREMENT();
        LOG_SEP(L"X");

        return FALSE;
    }

    SubScreen = InitializeSubScreen(Entry);
    if (SubScreen == NULL) {
        LOG_DECREMENT();
        LOG_SEP(L"X");

        return FALSE;
    }

    SubEntry = CopyLoaderEntry(Entry);

    if (SubEntry == NULL) {
        FreeMenuScreen(&SubScreen);

        LOG_DECREMENT();
        LOG_SEP(L"X");

        return FALSE;
    }

    SubEntry->Enabled = TRUE;

    SubEntry->Volume = TargetVolume;

    GraphicsTag = NULL;
    while (1) {
        TokenCount = ReadTokenLine(File, &TokenList);
        if (TokenCount == 0 || MrdStrEqualsCI(TokenList[0], L"}")) {
            FreeTokenLine(&TokenList, &TokenCount);

            break;
        }

        LOG_SEP(L"X");
        if (MrdStrEqualsCI(TokenList[0], L"loader")) {

            MRD_FREE_POOL(SubEntry->LoaderPath);
            SubEntry->LoaderPath = StrDuplicate(TokenList[1]);
            SubEntry->Volume = Volume;
        }
        else if (MrdStrEqualsCI(TokenList[0], L"initrd")) {
            MRD_FREE_POOL(SubEntry->InitrdPath);
            SubEntry->InitrdPath = StrDuplicate(TokenList[1]);
        }
        else if (MrdStrEqualsCI(TokenList[0], L"options")) {
            MRD_FREE_POOL(SubEntry->LoadOptions);
            SubEntry->LoadOptions = StrDuplicate(TokenList[1]);
        }
        else if (MrdStrEqualsCI(TokenList[0], L"add_options")) {

            MergeStrings(&SubEntry->LoadOptions, TokenList[1], L' ');
        }
        else if (GraphicsTag == NULL && MrdStrEqualsCI(TokenList[0], L"graphics")) {

            GraphicsTag = StrDuplicate((TokenCount > 1) ? TokenList[1] : L"on");
        }
        else {
        }

        FreeTokenLine(&TokenList, &TokenCount);

        LOG_SEP(L"X");
    }

    MRD_FREE_POOL(SubEntry->me.Title);
    TmpName = (Title != NULL) ? Title : L"Instance: Unknown";

    SubEntry->me.Title = BuildLoaderTitle(TmpName, Volume->VolName, Volume->FSType);

    if (SubEntry->InitrdPath != NULL) {
        MergeStrings(&SubEntry->LoadOptions, L"initrd=", L' ');
        MergeStrings(&SubEntry->LoadOptions, SubEntry->InitrdPath, 0);
        MRD_FREE_POOL(SubEntry->InitrdPath);
    }

    if (GraphicsTag != NULL) {
        if (!MrdStrEqualsCI(GraphicsTag, L"false") && !MrdStrEqualsCI(GraphicsTag, L"off") &&
            !MrdStrEqualsCI(GraphicsTag, L"0")) {
            SubEntry->UseGraphicsMode = TRUE;
        }
        MRD_FREE_POOL(GraphicsTag);
    }

    AddSubMenuEntry(SubScreen, (MERIDIAN_MENU_ENTRY *)SubEntry);

    Entry->me.SubScreen = SubScreen;

    LOG_DECREMENT();
    LOG_SEP(L"X");

    return TRUE;
}
