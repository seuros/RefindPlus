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

extern BOOLEAN ManualInclude;
extern UINTN TotalEntryCount;
extern UINTN ValidEntryCount;

static BOOLEAN GetIsDisabled(MERIDIAN_FILE *File)
{
    UINTN SubMenus;
    UINTN NumToken;
    CHAR8 *FilePtr08;
    CHAR16 *FilePtr16;
    CHAR16 **TokenList;
    BOOLEAN IsExitLoop;
    BOOLEAN IsDisabled;

    SubMenus = 0;
    IsExitLoop = FALSE;
    IsDisabled = FALSE;

    FilePtr08 = File->Current08Ptr;
    FilePtr16 = File->Current16Ptr;

    while (1) {
        NumToken = ReadTokenLine(File, &TokenList);
        if (NumToken == 0) {
            IsExitLoop = TRUE;
        }
        else if (MrdStrEqualsCI(TokenList[0], L"disabled")) {
            IsDisabled = TRUE;
        }
        else if (MrdStrEqualsCI(TokenList[0], L"submenuentry")) {
            SubMenus += 1;
        }
        else {
            if (MrdStrEqualsCI(TokenList[0], L"}")) {
                if (SubMenus < 1) {
                    IsExitLoop = TRUE;
                }
                else {
                    SubMenus -= 1;
                }
            }
        }

        FreeTokenLine(&TokenList, &NumToken);

        if (IsExitLoop)
            break;
    }

    if (!IsDisabled) {

        File->Current08Ptr = FilePtr08;
        File->Current16Ptr = FilePtr16;
    }

    return IsDisabled;
}

static CHAR16 *ResolveUriToken(IN CHAR16 *In)
{
    UINTN Len;
    CHAR16 *Open;
    CHAR16 *Scheme;
    CHAR16 *Value;
    BOOLEAN Known;

    if (In == NULL) {
        return NULL;
    }

    Len = StrLen(In);

    if (Len < 4 || In[Len - 1] != L')') {
        return StrDuplicate(In);
    }
    Open = In;
    while (*Open != L'\0' && *Open != L'(') {
        Open++;
    }
    if (*Open != L'(' || Open == In) {
        return StrDuplicate(In);
    }

    Scheme = AllocateZeroPool(((UINTN)(Open - In) + 1) * sizeof(CHAR16));
    if (Scheme == NULL) {
        return StrDuplicate(In);
    }
    CopyMem(Scheme, In, (UINTN)(Open - In) * sizeof(CHAR16));

    Known = (MrdStrEqualsCI(Scheme, L"guid") || MrdStrEqualsCI(Scheme, L"partuuid") ||
             MrdStrEqualsCI(Scheme, L"fsuuid") || MrdStrEqualsCI(Scheme, L"fslabel") ||
             MrdStrEqualsCI(Scheme, L"partlabel") || MrdStrEqualsCI(Scheme, L"label"));
    MRD_FREE_POOL(Scheme);

    if (!Known) {
        return StrDuplicate(In);
    }

    Value = AllocateZeroPool((UINTN)(&In[Len - 1] - (Open + 1) + 1) * sizeof(CHAR16));
    if (Value == NULL) {
        return StrDuplicate(In);
    }
    CopyMem(Value, Open + 1, (UINTN)(&In[Len - 1] - (Open + 1)) * sizeof(CHAR16));
    return Value;
}

MERIDIAN_VOLUME *GetStanzaVolume(MERIDIAN_FILE *File, MERIDIAN_VOLUME *Volume)
{
#if MERIDIAN_DEBUG > 0
    CHAR16 *TmpStore;
#endif

    UINTN SubMenus;
    UINTN NumToken;
    CHAR8 *FilePtr08;
    CHAR16 *FilePtr16;
    CHAR16 **TokenList;
    BOOLEAN IsExitLoop;
    BOOLEAN IsDisabled;
    BOOLEAN IsContinue;
    BOOLEAN CheckedVolume;
    MERIDIAN_VOLUME *SubjectVolume;
    MERIDIAN_VOLUME *PreviousVolume;

    SubMenus = 0;
    IsExitLoop = FALSE;
    IsDisabled = FALSE;
    CheckedVolume = FALSE;
    SubjectVolume = Volume;

    FilePtr08 = File->Current08Ptr;
    FilePtr16 = File->Current16Ptr;

    while (1) {
        IsContinue = FALSE;

        NumToken = ReadTokenLine(File, &TokenList);
        if (NumToken == 0) {
            IsExitLoop = TRUE;
        }
        else if (MrdStrEqualsCI(TokenList[0], L"}")) {
            if (SubMenus < 1) {
                IsExitLoop = TRUE;
            }
            else {
                SubMenus -= 1;
                IsContinue = TRUE;
            }
        }
        else if (MrdStrEqualsCI(TokenList[0], L"submenuentry")) {
            SubMenus += 1;
            IsContinue = TRUE;
        }
        else {
            if (IsDisabled || SubMenus > 0) {
                IsContinue = TRUE;
            }
        }

        if (!IsExitLoop && !IsContinue && MrdStrEqualsCI(TokenList[0], L"disabled")) {
            IsExitLoop = TRUE;
            IsDisabled = TRUE;
        }

        if (!IsExitLoop && !IsContinue && !MrdStrEqualsCI(TokenList[0], L"volume")) {
            IsContinue = TRUE;
        }

        if (IsExitLoop || IsContinue || CheckedVolume) {
            FreeTokenLine(&TokenList, &NumToken);

            if (IsExitLoop) {
                break;
            }

            continue;
        }

        IsDisabled = GetIsDisabled(File);
        if (IsDisabled) {
            FreeTokenLine(&TokenList, &NumToken);

            break;
        }

#if MERIDIAN_DEBUG > 0
        DEBUG_LOG(1, LOG_THREE_STAR_MID, L"Handle Token:- 'volume'");
#endif

        PreviousVolume = SubjectVolume;

        CHAR16 *VolId = ResolveUriToken(TokenList[1]);
        CheckedVolume = FindVolume(&SubjectVolume, VolId);
        MRD_FREE_POOL(VolId);
        if (!CheckedVolume) {
#if MERIDIAN_DEBUG > 0
            DEBUG_LOG(1, LOG_THREE_STAR_MID, L"Could *NOT* Find Volume Object for '%s'",
                      TokenList[1]);
#endif
        }
        else {
            if (SubjectVolume == NULL || SubjectVolume->RootDir == NULL ||
                !SubjectVolume->IsReadable) {
#if MERIDIAN_DEBUG > 0
                if (SubjectVolume == NULL) {
                    TmpStore = L"Empty";
                }
                else if (SubjectVolume->RootDir == NULL) {
                    TmpStore = L"Inaccessible";
                }
                else {
                    TmpStore = L"Unreadable";
                }

                DEBUG_LOG(1, LOG_THREE_STAR_MID,
                          L"Volume Item '%s' is %s ... Adopt Current Fallback:- '%s'", TokenList[1],
                          TmpStore, PreviousVolume->VolName);
#endif

                SubjectVolume = PreviousVolume;
                CheckedVolume = FALSE;
            }
        }

        FreeTokenLine(&TokenList, &NumToken);
    }

    if (IsDisabled) {

        return NULL;
    }

    File->Current08Ptr = FilePtr08;
    File->Current16Ptr = FilePtr16;

    return SubjectVolume;
}

static LOADER_ENTRY *InitializeStanza(MERIDIAN_FILE *File, MERIDIAN_VOLUME *Volume, CHAR16 *Title)
{
#if MERIDIAN_DEBUG > 0
    static BOOLEAN OtherCall = FALSE;
#endif

    UINTN TokenCount;
    CHAR16 *GraphicsTag;
    CHAR16 *LoadOptions;
    CHAR16 *BootNumber;
    CHAR16 **TokenList;
    BOOLEAN RetVal;
    BOOLEAN HasPath;
    BOOLEAN DefaultsSet;
    BOOLEAN SeekSubmenu;
    BOOLEAN AddedSubmenu;
    BOOLEAN GotFirmwareTag;
    BOOLEAN StanzaSkip;
    MERIDIAN_VOLUME *CurrentVolume;
    LOADER_ENTRY *StanzaEntry;

#if MERIDIAN_DEBUG > 0
    DEBUG_LOG(1, LOG_LINE_THIN_SEP, L"%s", (!OtherCall) ? L"FIRST STANZA" : L"NEXT STANZA");
    OtherCall = TRUE;
#endif

    CurrentVolume = GetStanzaVolume(File, Volume);
    if (CurrentVolume == NULL) {

#if MERIDIAN_DEBUG > 0
        DEBUG_LOG(1, LOG_STAR_HEAD_SEP, L"Stanza is Disabled");
#endif

        return NULL;
    }

    StanzaEntry = InitializeLoaderEntry(NULL);
    if (StanzaEntry == NULL) {
#if MERIDIAN_DEBUG > 0
        DEBUG_LOG(1, LOG_STAR_SEPARATOR, L"Could *NOT* Initialise Stanza");
#endif

        return NULL;
    }

    StanzaEntry->Title = (Title != NULL) ? PoolPrint(L"Manual Stanza: %s", Title)
                                         : StrDuplicate(L"Manual Stanza: Title *NOT* Found");
    StanzaEntry->me.Row = 0;
    StanzaEntry->Enabled = TRUE;
    StanzaEntry->Volume = CurrentVolume;
    StanzaEntry->DiscoveryType = DISCOVERY_TYPE_MANUAL;

    LoadOptions = NULL;
    GraphicsTag = BootNumber = NULL;
    AddedSubmenu = FALSE;
    GotFirmwareTag = DefaultsSet = FALSE;
    StanzaSkip = FALSE;

    while (1) {
        TokenCount = ReadTokenLine(File, &TokenList);
        if (TokenCount == 0 || MrdStrEqualsCI(TokenList[0], L"}")) {
            FreeTokenLine(&TokenList, &TokenCount);

            break;
        }

        if (GraphicsTag == NULL && MrdStrEqualsCI(TokenList[0], L"graphics")) {
#if MERIDIAN_DEBUG > 0
            DEBUG_LOG(1, LOG_THREE_STAR_MID, L"Handle Token:- 'graphics'");
#endif

            GraphicsTag = StrDuplicate((TokenCount > 1) ? TokenList[1] : L"on");
        }
        else if (MrdStrEqualsCI(TokenList[0], L"ostype")) {
#if MERIDIAN_DEBUG > 0
            DEBUG_LOG(1, LOG_THREE_STAR_MID, L"Handle Token:- 'ostype'");
#endif

            StanzaEntry->OSType = TokenList[1][0];
        }
        else if (MrdStrEqualsCI(TokenList[0], L"if_arch") ||
                 MrdStrEqualsCI(TokenList[0], L"if_fw_type")) {

            CHAR16 *Want = MrdStrEqualsCI(TokenList[0], L"if_arch") ? MRD_ARCH_STR : MRD_FWTYPE_STR;
            BOOLEAN Match = FALSE;
            UINTN ci;

#if MERIDIAN_DEBUG > 0
            DEBUG_LOG(1, LOG_THREE_STAR_MID, L"Handle Token:- '%s'", TokenList[0]);
#endif

            for (ci = 1; ci < TokenCount; ci++) {
                if (MrdStrEqualsCI(TokenList[ci], Want)) {
                    Match = TRUE;
                    break;
                }
            }
            if (!Match) {
                StanzaSkip = TRUE;
            }
        }
        else if (MrdStrEqualsCI(TokenList[0], L"loader")) {
#if MERIDIAN_DEBUG > 0
            DEBUG_LOG(1, LOG_THREE_STAR_MID, L"Handle Token:- 'loader'");
#endif

            HasPath = (TokenList[1] && StrLen(TokenList[1]) > 0);
            if (HasPath) {

                MRD_FREE_POOL(StanzaEntry->LoaderPath);
                if (!GotFirmwareTag) {
                    StanzaEntry->LoaderPath = StrDuplicate(TokenList[1]);

#if MERIDIAN_DEBUG > 0
                    DEBUG_LOG(1, LOG_LINE_NORMAL, L"Add Loader Path:- '%s'",
                              StanzaEntry->LoaderPath);
#endif

                    SetLoaderDefaults(StanzaEntry, TokenList[1], CurrentVolume);
                }

                DefaultsSet = TRUE;
            }
        }
        else if (MrdStrEqualsCI(TokenList[0], L"initrd")) {
#if MERIDIAN_DEBUG > 0
            DEBUG_LOG(1, LOG_THREE_STAR_MID, L"Handle Token:- 'initrd'");
#endif

            MRD_FREE_POOL(StanzaEntry->InitrdPath);
            if (!GotFirmwareTag) {
                StanzaEntry->InitrdPath = StrDuplicate(TokenList[1]);
            }
        }
        else if (MrdStrEqualsCI(TokenList[0], L"options")) {
#if MERIDIAN_DEBUG > 0
            DEBUG_LOG(1, LOG_THREE_STAR_MID, L"Handle Token:- 'options'");
#endif

            MRD_FREE_POOL(LoadOptions);
            LoadOptions = StrDuplicate(TokenList[1]);
        }
        else if (MrdStrEqualsCI(TokenList[0], L"firmware_bootnum")) {
#if MERIDIAN_DEBUG > 0
            DEBUG_LOG(1, LOG_THREE_STAR_MID, L"Handle Token:- 'firmware_bootnum'");
#endif

            StanzaEntry->me.Tag = TAG_FIRMWARE_LOADER;

            DefaultsSet = TRUE;
            GotFirmwareTag = TRUE;

            MRD_FREE_POOL(BootNumber);
            BootNumber = StrDuplicate(TokenList[1]);
        }
        else {
            if (MrdStrEqualsCI(TokenList[0], L"submenuentry")) {
#if MERIDIAN_DEBUG > 0
                DEBUG_LOG(1, LOG_BLANK_LINE_SEP, L"X");
                DEBUG_LOG(1, LOG_LINE_SPECIAL, L"***[ Add SubMenu Entry ]***");
#endif

                SeekSubmenu = AddSubmenu(StanzaEntry, File, CurrentVolume, TokenList[1]);
                if (!AddedSubmenu) {
                    AddedSubmenu = SeekSubmenu;
                }
            }
        }

        FreeTokenLine(&TokenList, &TokenCount);
    }

    if (!GotFirmwareTag && CurrentVolume->VolName != NULL) {
        StanzaEntry->me.Title =
            BuildLoaderTitle(StanzaEntry->Title, Volume->VolName, Volume->FSType);
    }
    else {
        if (!GotFirmwareTag) {
            StanzaEntry->me.Title = PoolPrint(L"Load %s", StanzaEntry->Title);
        }
        else {

            MRD_FREE_POOL(StanzaEntry->InitrdPath);
            MRD_FREE_POOL(StanzaEntry->LoaderPath);
            MRD_FREE_POOL(StanzaEntry->EfiLoaderPath);

            StanzaEntry->me.Title =
                PoolPrint(L"Load %s ... [Firmware Boot Number]", StanzaEntry->Title);

            StanzaEntry->EfiBootNum = StrToHex(BootNumber, 0, 16);
        }
    }

    MRD_FREE_POOL(StanzaEntry->LoadOptions);
    if (LoadOptions != NULL && StrLen(LoadOptions) > 0) {
        StanzaEntry->LoadOptions = StrDuplicate(LoadOptions);
    }

    if (AddedSubmenu) {
        RetVal = GetMenuEntryReturn(&StanzaEntry->me.SubScreen);
        if (!RetVal) {
            FreeMenuScreen(&StanzaEntry->me.SubScreen);
        }
    }

    if (StanzaEntry->InitrdPath != NULL && StrLen(StanzaEntry->InitrdPath) > 0) {
        if (StanzaEntry->LoadOptions != NULL && StrLen(StanzaEntry->LoadOptions) > 0) {
            MergeStrings(&StanzaEntry->LoadOptions, L"initrd=", L' ');
            MergeStrings(&StanzaEntry->LoadOptions, StanzaEntry->InitrdPath, 0);
        }
        else {
            if (StanzaEntry->LoadOptions != NULL && StrLen(StanzaEntry->LoadOptions) == 0) {
                MRD_FREE_POOL(StanzaEntry->LoadOptions);
            }

            StanzaEntry->LoadOptions = PoolPrint(L"initrd=%s", StanzaEntry->InitrdPath);
        }

        MRD_FREE_POOL(StanzaEntry->InitrdPath);
    }

    if (!DefaultsSet) {

        SetLoaderDefaults(StanzaEntry, L"\\EFI\\BOOT\\bogusnemo.efi", CurrentVolume);
    }

    if (GraphicsTag != NULL) {
        if (!MrdStrEqualsCI(GraphicsTag, L"false") && !MrdStrEqualsCI(GraphicsTag, L"off") &&
            !MrdStrEqualsCI(GraphicsTag, L"0")) {
            StanzaEntry->UseGraphicsMode = TRUE;
        }
        MRD_FREE_POOL(GraphicsTag);
    }

    MRD_FREE_POOL(LoadOptions);
    MRD_FREE_POOL(BootNumber);

    if (StanzaSkip) {

#if MERIDIAN_DEBUG > 0
        DEBUG_LOG(1, LOG_STAR_HEAD_SEP, L"Stanza Skipped by Conditional (arch:'%s' fw:'%s')",
                  MRD_ARCH_STR, MRD_FWTYPE_STR);
#endif
        FreeMenuEntry((MERIDIAN_MENU_ENTRY **)&StanzaEntry);

        return NULL;
    }

    return StanzaEntry;
}

VOID ScanUserConfigured(CHAR16 *FileName)
{
#if MERIDIAN_DEBUG > 0
    CHAR16 *TmpName;
    CHAR16 *CountStr;
    UINTN LogLineType;
#endif

    EFI_STATUS Status;
    MERIDIAN_FILE *File;
    CHAR16 **TokenList;
    UINTN size;
    UINTN TokenCount;
    LOADER_ENTRY *Entry;

    if (!ManualInclude) {
        LOG_SEP(L"X");
        LOG_INCREMENT();

        TotalEntryCount = ValidEntryCount = 0;
    }

    File = AllocateZeroPool(sizeof(MERIDIAN_FILE));
    if (File == NULL) {
        LOG_DECREMENT();
        LOG_SEP(L"X");

        return;
    }

    if (FileExists(SelfDir, FileName)) {
        Status = MeridianReadFile(SelfDir, FileName, File, &size);
        if (!EFI_ERROR(Status)) {
            while (1) {
                TokenCount = ReadTokenLine(File, &TokenList);
                if (TokenCount == 0) {
                    FreeTokenLine(&TokenList, &TokenCount);

                    break;
                }

                if (TokenCount > 1 && MrdStrEqualsCI(TokenList[0], L"menuentry")) {
                    TotalEntryCount = TotalEntryCount + 1;

                    Entry = InitializeStanza(File, SelfVolume, TokenList[1]);
                    if (Entry == NULL) {
                        FreeTokenLine(&TokenList, &TokenCount);
                        continue;
                    }

                    ValidEntryCount = ValidEntryCount + 1;
#if MERIDIAN_DEBUG > 0
                    TmpName =
                        (SelfVolume->VolName != NULL) ? SelfVolume->VolName : Entry->LoaderPath;
                    INFO_LOG("%s  - Found %s%s%s%s%s", OffsetNext, Entry->Title,
                             SetVolJoin(Entry->Title, FALSE), SetVolKind(Entry->Title, TmpName, 0),
                             SetVolFlag(Entry->Title, TmpName),
                             SetVolType(Entry->Title, TmpName, 0));
#endif

                    if (Entry->me.SubScreen == NULL) {
                        GenerateSubScreen(Entry, SelfVolume, TRUE);
                    }

                    AddMenuEntry(MainMenu, (MERIDIAN_MENU_ENTRY *)Entry);
                }
                else {
                    if (!ManualInclude && TokenCount == 2 &&
                        !MrdStrEqualsCI(TokenList[1], FileName) &&
                        MrdStrEqualsCI(TokenList[0], L"include") &&
                        MrdStrEqualsCI(FileName, GlobalConfig.ConfigFilename)

                    ) {

#if MERIDIAN_DEBUG > 0
#if MERIDIAN_DEBUG < 2
                        DEBUG_LOG(1, LOG_BLANK_LINE_SEP, L"X");
                        DEBUG_LOG(1, LOG_THREE_STAR_MID,
                                  L"Process Include File for Manual Stanzas");
#else
                        LOG_SEP(L"X");
#endif
#endif

                        ManualInclude = TRUE;
                        ScanUserConfigured(TokenList[1]);
                        ManualInclude = FALSE;

#if MERIDIAN_DEBUG > 0
#if MERIDIAN_DEBUG < 2
                        DEBUG_LOG(1, LOG_THREE_STAR_MID,
                                  L"Scanned Include File for Manual Stanzas");
#else
                        LOG_SEP(L"X");
#endif
#endif
                    }
                }

                FreeTokenLine(&TokenList, &TokenCount);
            }
        }
    }

    MRD_FREE_FILE(File);

#if MERIDIAN_DEBUG > 0
    CountStr = (ValidEntryCount > 0) ? PoolPrint(L"%d", ValidEntryCount) : NULL;

    if (ManualInclude) {
        LogLineType = LOG_THREE_STAR_MID;
    }
    else {
        LogLineType = LOG_STAR_HEAD_SEP;
        DEBUG_LOG(1, LOG_BLANK_LINE_SEP, L"X");
    }

    DEBUG_LOG(1, LogLineType, L"Processed %d Manual Stanza%s in '%s'%s%s%s%s", TotalEntryCount,
              (TotalEntryCount == 1) ? L"" : L"s", FileName,
              (TotalEntryCount == 0) ? L"" : L" ... Found ",
              (ValidEntryCount > 0)    ? CountStr
              : (TotalEntryCount == 0) ? L""
                                       : L"0",
              (TotalEntryCount == 0) ? L"" : L" Valid/Active Stanza",
              (ValidEntryCount == 1)   ? L""
              : (TotalEntryCount == 0) ? L""
                                       : L"s");
    MRD_FREE_POOL(CountStr);
#endif

    if (ManualInclude) {
        ManualInclude = FALSE;
    }
    else {
        LOG_DECREMENT();
        LOG_SEP(L"X");
    }
}
