// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2020-2026 Dayo Akanji
// SPDX-FileCopyrightText: 2012-2023 Roderick W. Smith
// SPDX-FileCopyrightText: 2006-2010 Christoph Pfisterer

#include "global.h"
#include "config.h"
#include "lib.h"
#include "mok.h"
#include "menu.h"
#include "scan.h"
#include "linux.h"
#include "apple.h"
#include "install.h"
#include "screenmgt.h"
#include "mystrings.h"
#include "launch_efi.h"
#include "driver_support.h"
#include "security_policy.h"

#if MERIDIAN_DEBUG > 0
static CHAR16 *Spacer = L"                          ";
#endif

extern UINTN MemTestEntryItemsCount;
extern UINTN NetBootEntryItemsCount;
extern UINTN ShellEntryItemsCount;
extern UINTN MOKEntryItemsCount;
extern UINTN GDiskEntryItemsCount;
extern UINTN GPTSyncEntryItemsCount;
extern UINTN FwUpdateEntryItemsCount;
extern UINTN CydiaEntryItemsCount;

extern LOADER_ENTRY **MemTestEntryItems;
extern LOADER_ENTRY **NetBootEntryItems;
extern LOADER_ENTRY **ShellEntryItems;
extern LOADER_ENTRY **MOKEntryItems;
extern LOADER_ENTRY **GDiskEntryItems;
extern LOADER_ENTRY **GPTSyncEntryItems;
extern LOADER_ENTRY **FwUpdateEntryItems;
extern LOADER_ENTRY **CydiaEntryItems;

#if MERIDIAN_DEBUG > 0
BOOLEAN IsToolSet(UINTN ToolTag)
{
    UINTN i;
    BOOLEAN ToolSet;

    ToolSet = FALSE;
    for (i = 0; i < NUM_TOOLS; i++) {
        if (GlobalConfig.ShowTools[i] == ToolTag) {
            ToolSet = TRUE;

            break;
        }
    }

    return ToolSet;
}
#endif

static LOADER_ENTRY *AddToolEntry(IN MERIDIAN_VOLUME *Volume, IN CHAR16 *LoaderPath,
                                  IN CHAR16 *LoaderTitle)
{
    LOADER_ENTRY *Entry;

    Entry = AllocateZeroPool(sizeof(LOADER_ENTRY));
    if (Entry == NULL) {
        return NULL;
    }

    Entry->me.Title = (LoaderTitle != NULL) ? LoaderTitle : StrDuplicate(L"Unknown Tool");

    Entry->LoaderPath = (LoaderPath != NULL) ? LoaderPath : NULL;

    Entry->me.Tag = TAG_TOOL;
    Entry->me.Row = 1;
    Entry->Volume = Volume;
    Entry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_TOOLS;

    AddMenuEntry(MainMenu, (MERIDIAN_MENU_ENTRY *)Entry);

    return Entry;
}

static BOOLEAN AddToolMenuEntry(IN CHAR16 *ToolName, IN UINTN ToolTag, IN BOOLEAN ShowSubMenuTitle)
{
    MERIDIAN_MENU_ENTRY *MenuEntry;

    if (ToolName == NULL) {
        return FALSE;
    }

    MenuEntry = AllocateZeroPool(sizeof(MERIDIAN_MENU_ENTRY));
    if (MenuEntry == NULL) {
        return FALSE;
    }

    MenuEntry->Title =
        ShowSubMenuTitle ? PoolPrint(L"Show '%s' Menu", ToolName) : StrDuplicate(ToolName);
    if (MenuEntry->Title == NULL) {
        MRD_FREE_POOL(MenuEntry);

        return FALSE;
    }

    MenuEntry->Tag = ToolTag;
    MenuEntry->Row = 1;

    AddMenuEntry(MainMenu, MenuEntry);

    return TRUE;
}

#if MERIDIAN_DEBUG > 0
static VOID LogToolMenuResult(IN CHAR16 *ToolName, IN BOOLEAN FoundTool,
                              IN BOOLEAN MissingMeansNotFound)
{
    CHAR16 *ToolStr;

    if (FoundTool) {
        ToolStr = PoolPrint(L"Added Tool:- '%s'", ToolName);
    }
    else if (MissingMeansNotFound) {
        ToolStr = PoolPrint(L"Could *NOT* Find Tool:- '%s'", ToolName);
    }
    else {
        ToolStr = PoolPrint(L"Could *NOT* Load Tool:- '%s'", ToolName);
    }

    if (ToolStr == NULL) {
        return;
    }

    DEBUG_LOG(1, LOG_THREE_STAR_END, L"%s", ToolStr);
    if (FoundTool) {
        INFO_LOG("%s", ToolStr);
    }
    else {
        INFO_LOG("*_ WARN _*    %s", ToolStr);
    }
    MRD_FREE_POOL(ToolStr);
}
#endif

static VOID AddOptionalToolMenuEntry(IN CHAR16 *ToolName, IN UINTN ToolTag,
                                     IN BOOLEAN ShowSubMenuTitle, IN BOOLEAN MissingMeansNotFound)
{
#if MERIDIAN_DEBUG > 0
    BOOLEAN FoundTool;

    FoundTool = AddToolMenuEntry(ToolName, ToolTag, ShowSubMenuTitle);
    LogToolMenuResult(ToolName, FoundTool, MissingMeansNotFound);
#else
    (VOID) MissingMeansNotFound;
    (VOID) AddToolMenuEntry(ToolName, ToolTag, ShowSubMenuTitle);
#endif
}

BOOLEAN HidePreboot(CHAR16 *Type)
{
    UINTN i;
    CHAR16 *OurItem;
    CHAR16 *OurSkipList;
    BOOLEAN FlagDontScan;

    if (MrdStrEqualsCI(Type, L"DontScanDirs")) {
        OurSkipList = GlobalConfig.DontScanDirs;
    }
    else if (MrdStrEqualsCI(Type, L"DontScanFiles")) {
        OurSkipList = GlobalConfig.DontScanFiles;
    }
    else {
        OurSkipList = GlobalConfig.DontScanVolumes;
    }

    i = 0;
    FlagDontScan = FALSE;
    while (1) {

        OurItem = FindCommaDelimited(OurSkipList, i);
        if (OurItem == NULL)
            break;

        if (!MrdStrEqualsCI(OurItem, L"PreBoot") && !MrdStrIncludesCI(OurItem, L"PreBoot:")) {
            i++;
        }
        else {
            FlagDontScan = TRUE;
            DeleteItemFromCsvList(OurItem, &OurSkipList);
        }

        MRD_FREE_POOL(OurItem);
    }

    return FlagDontScan;
}

static BOOLEAN FindToolEx(CHAR16 *Description, CHAR16 *FileName, CHAR16 *PathName,
                          BOOLEAN FoundTool, MERIDIAN_VOLUME *Volume, UINTN TypeTag)
{
#if MERIDIAN_DEBUG > 0
    CHAR16 *ToolStr;
#endif

    CHAR16 *TypeString;
    LOADER_ENTRY *MenuEntry;

#if MERIDIAN_DEBUG > 0
    if (TypeTag == TAG_BASE) {
        DEBUG_LOG(1, LOG_LINE_NORMAL, L"Adding Tag for '%s' on '%s'", FileName, Volume->VolName);
    }
#endif

    TypeString = PoolPrint(L"%s from %s%s via %s", Description, Volume->VolName,
                           SetVolType(NULL, Volume->VolName, Volume->FSType), PathName);

    if (TypeTag != TAG_BASE) {
        MenuEntry = AllocateZeroPool(sizeof(LOADER_ENTRY));
        if (MenuEntry == NULL) {
            return FALSE;
        }

        MenuEntry->me.Title = TypeString;
        MenuEntry->me.Tag = TAG_BASE;
        MenuEntry->LoaderPath = StrDuplicate(PathName);
        MenuEntry->Volume = Volume;
        MenuEntry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_TOOLS;
    }

    switch (TypeTag) {
    case TAG_SHELL:
        MenuEntry->me.Row = ShellEntryItemsCount;
        AddListElement((VOID ***)&ShellEntryItems, &ShellEntryItemsCount, MenuEntry);
        break;
    case TAG_MEMTEST:
        MenuEntry->me.Row = MemTestEntryItemsCount;
        AddListElement((VOID ***)&MemTestEntryItems, &MemTestEntryItemsCount, MenuEntry);
        break;
    case TAG_CYDIA:
        MenuEntry->me.Row = CydiaEntryItemsCount;
        AddListElement((VOID ***)&CydiaEntryItems, &CydiaEntryItemsCount, MenuEntry);
        break;
    case TAG_GDISK:
        MenuEntry->me.Row = GDiskEntryItemsCount;
        AddListElement((VOID ***)&GDiskEntryItems, &GDiskEntryItemsCount, MenuEntry);
        break;
    case TAG_GPTSYNC:
        MenuEntry->me.Row = GPTSyncEntryItemsCount;
        AddListElement((VOID ***)&GPTSyncEntryItems, &GPTSyncEntryItemsCount, MenuEntry);
        break;
    case TAG_FWUPDATE:
        MenuEntry->me.Row = FwUpdateEntryItemsCount;
        AddListElement((VOID ***)&FwUpdateEntryItems, &FwUpdateEntryItemsCount, MenuEntry);
        break;
    case TAG_MOK:
        MenuEntry->me.Row = MOKEntryItemsCount;
        AddListElement((VOID ***)&MOKEntryItems, &MOKEntryItemsCount, MenuEntry);
        break;
    default:
        AddToolEntry(Volume, StrDuplicate(PathName), TypeString);
    }

#if MERIDIAN_DEBUG > 0
    if (TypeTag == TAG_BASE) {
        ToolStr = PoolPrint(L"Added Tool:- '%-18s     :::     %s'", Description, PathName);

        DEBUG_LOG(1, LOG_THREE_STAR_END, L"%s", ToolStr);

        if (FoundTool) {
            INFO_LOG("%s%s", OffsetNext, Spacer);
        }
        INFO_LOG("%s", ToolStr);
        MRD_FREE_POOL(ToolStr);
    }
#endif

    return TRUE;
}

BOOLEAN FindTool(CHAR16 *Locations, CHAR16 *Names, CHAR16 *Description, BOOLEAN SelfVolOnly,
                 BOOLEAN ScanMultiple, UINTN TypeTag)
{
    UINTN i, j;
    UINTN Index;
    CHAR16 *VolName;
    CHAR16 *DirName;
    CHAR16 *FileName;
    CHAR16 *PathName;
    CHAR16 *PrevFind;
    BOOLEAN VolMatch;
    BOOLEAN FoundTool;
    BOOLEAN BreakLoop;
    BOOLEAN MemTestRun;

    if (Names == NULL) {
        return FALSE;
    }

    VolName = NULL;
    DirName = NULL;
    PrevFind = NULL;
    FoundTool = FALSE;
    BreakLoop = FALSE;

    MemTestRun = MrdStrIncludesCI(Locations, L"\\memtest");

    i = 0;
    while (!BreakLoop) {
        DirName = FindCommaDelimited(Locations, i++);
        if (DirName == NULL)
            break;

        if (MemTestRun) {
            if (MrdStrEqualsCI(Locations, MEMTEST_LOCATIONS)) {
                if (MrdStrStartsWithCI(SelfDirPath, DirName)) {
                    MRD_FREE_POOL(DirName);

                    continue;
                }
            }
        }

        SplitVolumeAndFilename(&DirName, &VolName);
        if (SelfVolOnly) {
            VolMatch = (VolName == NULL || MrdStrEqualsCI(VolName, SelfVolume->FsName) ||
                        MrdStrEqualsCI(VolName, SelfVolume->PartName) ||
                        MrdStrEqualsCI(VolName, SelfVolume->VolName));

            if (!VolMatch) {
                MRD_FREE_POOL(DirName);
                MRD_FREE_POOL(VolName);

                continue;
            }
        }

        j = 0;
        while (!BreakLoop) {
            FileName = FindCommaDelimited(Names, j++);
            if (FileName == NULL)
                break;

            if (MrdStrEqualsCI(FileName, FALLBACK_BASENAME)) {
                if (!MemTestRun || !MrdStrIncludesCI(DirName, L"\\memtest")) {
                    MRD_FREE_POOL(FileName);

                    continue;
                }
            }

            PathName = StrDuplicate(DirName);
            MergeStrings(&PathName, FileName, MrdStrEqualsCI(PathName, L"\\") ? 0 : L'\\');

            if (SelfVolOnly) {
                if (!FileExists(SelfVolume->RootDir, DirName) ||
                    !IsValidTool(SelfVolume, PathName)) {
                    MRD_FREE_POOL(PathName);
                    MRD_FREE_POOL(FileName);

                    continue;
                }

                FindToolEx(Description, FileName, PathName, FoundTool, SelfVolume, TypeTag);

                FoundTool = TRUE;
            }
            else {
                for (Index = 0; Index < VolumesCount; Index++) {
                    VolMatch =
                        (VolName == NULL || MrdStrEqualsCI(VolName, Volumes[Index]->VolName) ||
                         MrdStrEqualsCI(VolName, Volumes[Index]->PartName) ||
                         MrdStrEqualsCI(VolName, Volumes[Index]->FsName));

                    if (!VolMatch || Volumes[Index]->RootDir == NULL ||
                        !FileExists(Volumes[Index]->RootDir, DirName) ||
                        !IsValidTool(Volumes[Index], PathName)) {

                        continue;
                    }

                    if (PrevFind == NULL) {
                        PrevFind = StrDuplicate(PathName);
                    }
                    else {
                        if (IsListItem(PathName, PrevFind)) {

                            continue;
                        }

                        MergeStrings(&PrevFind, PathName, L',');
                    }

                    FindToolEx(Description, FileName, PathName, FoundTool, Volumes[Index], TypeTag);

                    FoundTool = TRUE;
                }

                if (!ScanMultiple && FoundTool) {
                    BreakLoop = TRUE;
                }
            }

            MRD_FREE_POOL(PathName);
            MRD_FREE_POOL(FileName);
        }

        MRD_FREE_POOL(PrevFind);
        MRD_FREE_POOL(DirName);
        MRD_FREE_POOL(VolName);
    }

    return FoundTool;
}

VOID ScanFirmwareDefined(IN UINTN Row, IN CHAR16 *MatchThis OPTIONAL, IN UINTN TypeTag)
{
#if MERIDIAN_DEBUG > 0
    EFI_STATUS Status;
#endif

    UINTN index;
    CHAR16 *SkipThese;
    CHAR16 *OneItem;
    BOOLEAN ScanIt;
    BOOT_ENTRY_LIST *ThisEntry;
    BOOT_ENTRY_LIST *BootEntries;

#if MERIDIAN_DEBUG > 0
    LOG_SEP(L"X");
    LOG_INCREMENT();

    if (Row == 0) {
        DEBUG_LOG(1, LOG_THREE_STAR_SEP, L"Firmware Defined Boot Options");
    }
#endif

    SkipThese = (GlobalConfig.DontScanFirmware != NULL)
                    ? StrDuplicate(GlobalConfig.DontScanFirmware)
                    : NULL;

    if (Row == 0) {
        if (SkipThese == NULL) {
            SkipThese = StrDuplicate(L"shell");
        }
        else {
            MergeUniqueStrings(&SkipThese, L"shell", L',');
        }
    }

#if MERIDIAN_DEBUG > 0
    Status = EFI_NOT_FOUND;

    DEBUG_LOG(1, LOG_LINE_NORMAL, L"Firmware Defined Option Scan Match List:- '%s'",
              (MatchThis != NULL) ? MatchThis : L"NULL");

    DEBUG_LOG(1, LOG_LINE_NORMAL, L"Firmware Defined Option Scan Exclusions:- '%s'",
              (SkipThese != NULL) ? SkipThese : L"NULL");
#endif

    BootEntries = FindBootOrderEntries();
    ThisEntry = BootEntries;

    while (ThisEntry != NULL) {
        ScanIt = FALSE;

        if (MatchThis == NULL) {
            if (SkipThese == NULL ||
                !IsListItemSubstringIn(ThisEntry->BootEntry.Label, SkipThese)) {
                ScanIt = TRUE;
            }
        }
        else {
            index = 0;
            while (!ScanIt) {
                OneItem = FindCommaDelimited(MatchThis, index++);
                if (OneItem == NULL)
                    break;

                if (MrdStrIncludesCI(ThisEntry->BootEntry.Label, OneItem) &&
                    !IsListItemSubstringIn(ThisEntry->BootEntry.Label, SkipThese)) {
                    ScanIt = TRUE;
                }

                MRD_FREE_POOL(OneItem);
            }
        }

        if (ScanIt) {
            AddEfiLoaderEntry(ThisEntry->BootEntry.DevPath, ThisEntry->BootEntry.Label,
                              ThisEntry->BootEntry.BootNum, Row, TypeTag);

#if MERIDIAN_DEBUG > 0
            if (EFI_ERROR(Status)) {
                Status = EFI_SUCCESS;
            }
#endif
        }

        ThisEntry = ThisEntry->NextBootEntry;
    }

    MRD_FREE_POOL(SkipThese);
    DeleteBootOrderEntries(BootEntries);

#if MERIDIAN_DEBUG > 0
    DEBUG_LOG(1, LOG_THREE_STAR_END, L"Evaluate and Add Firmware Defined Options:- '%r'", Status);
#endif

    LOG_DECREMENT();
    LOG_SEP(L"X");
}

BOOLEAN IsValidTool(MERIDIAN_VOLUME *BaseVolume, CHAR16 *PathName)
{
#if MERIDIAN_DEBUG > 0
    BOOLEAN CheckMute = FALSE;
#endif

    UINTN i;
    CHAR16 *DontVolName;
    CHAR16 *TestVolName;
    CHAR16 *TestPathName;
    CHAR16 *TestFileName;
    CHAR16 *DontPathName;
    CHAR16 *DontFileName;
    CHAR16 *DontScanThis;
    CHAR16 *DontScanTools;
    BOOLEAN retval;

    if (!FileExists(BaseVolume->RootDir, PathName)) {

        return FALSE;
    }

#if MERIDIAN_DEBUG > 0
    MRD_MUTELOGGER_SET;
#endif
    retval = IsValidLoader(BaseVolume->RootDir, PathName);
#if MERIDIAN_DEBUG > 0
    MRD_MUTELOGGER_OFF;
#endif

    if (!retval) {

        return FALSE;
    }

    DontScanTools =
        (GlobalConfig.DontScanTools != NULL) ? StrDuplicate(GlobalConfig.DontScanTools) : NULL;

    if (DontScanTools == NULL) {

        return TRUE;
    }

#if MERIDIAN_DEBUG > 0
    DEBUG_LOG(1, LOG_LINE_NORMAL, L"Check File is Valid:- '%s'", PathName);
#endif

    retval = TRUE;
    TestVolName = TestPathName = TestFileName = NULL;
    DontPathName = DontFileName = DontVolName = NULL;
    SplitPathName(PathName, &TestVolName, &TestPathName, &TestFileName);

    i = 0;
    while (retval) {
        DontScanThis = FindCommaDelimited(DontScanTools, i++);
        if (DontScanThis == NULL)
            break;

        SplitPathName(DontScanThis, &DontVolName, &DontPathName, &DontFileName);

        if (MrdStrEqualsCI(TestFileName, DontFileName) &&
            (!DontPathName || MrdStrEqualsCI(TestPathName, DontPathName)) &&
            (!DontVolName || VolumeMatchesDescription(BaseVolume, DontVolName))) {
            retval = FALSE;
        }

        MRD_FREE_POOL(DontVolName);
        MRD_FREE_POOL(DontPathName);
        MRD_FREE_POOL(DontFileName);
        MRD_FREE_POOL(DontScanThis);
    }

    MRD_FREE_POOL(TestVolName);
    MRD_FREE_POOL(TestPathName);
    MRD_FREE_POOL(TestFileName);
    MRD_FREE_POOL(DontScanTools);

    return retval;
}

VOID ScanForTools(VOID)
{
#if MERIDIAN_DEBUG > 0
    BOOLEAN CheckMute = FALSE;
    CHAR16 *ToolStr;
    CHAR16 *LogSection = L"H A N D L E   T O O L   O P T I O N S";
#endif

    EFI_STATUS Status;
    VOID *ItemBuffer;
    UINTN ToolTotal;
    UINTN i;
    UINT64 osind;
    UINT32 CsrValue;
    CHAR16 *ToolName;

#if MERIDIAN_DEBUG > 0
    DEBUG_LOG(1, LOG_BLANK_LINE_SEP, L"X");
    DEBUG_LOG(1, LOG_LINE_SEPARATOR, L"%s", LogSection);
    INFO_LOG("%s", LogSection);

    LOG_SEP(L"X");
    LOG_INCREMENT();
#endif

    if (GlobalConfig.DirectBoot) {
#if MERIDIAN_DEBUG > 0
        LogSection = L"INFO: Tool Options:- Skip ... 'DirectBoot' is Active";
        DEBUG_LOG(1, LOG_THREE_STAR_MID, L"%s", LogSection);
        DEBUG_LOG(1, LOG_BLANK_LINE_SEP, L"X");
        INFO_LOG("\n");
        INFO_LOG("%s", LogSection);
        INFO_LOG("\n\n");
#endif

        LOG_DECREMENT();
        LOG_SEP(L"X");

        return;
    }

    ToolTotal = 0;
    for (i = 0; i < NUM_TOOLS; i++) {
        switch (GlobalConfig.ShowTools[i]) {
        case TAG_SHELL:
        case TAG_BOOTORDER:
#if defined(EFIX64)
        case TAG_CLEAN_NVRAM:
#endif
        case TAG_CSR_ROTATE:
        case TAG_FIRMWARE:
        case TAG_FWUPDATE:
        case TAG_INSTALL:
        case TAG_EXIT:
        case TAG_MOK:
        case TAG_GDISK:
        case TAG_GPTSYNC:
        case TAG_MEMTEST:
        case TAG_CYDIA:
        case TAG_NETBOOT:
        case TAG_REBOOT:
        case TAG_SHUTDOWN:
        case TAG_RECOVERY_MAC:
        case TAG_RECOVERY_WIN:
            ToolTotal++;
            break;
        default:
            break;
        }
    }
    if (ToolTotal == 0) {
#if MERIDIAN_DEBUG > 0
        LogSection = L"INFO: Tool Options:- Skip ... Empty 'showtools' List";
        DEBUG_LOG(1, LOG_LINE_NORMAL, L"%s", LogSection);
        DEBUG_LOG(1, LOG_BLANK_LINE_SEP, L"X");
        INFO_LOG("\n");
        INFO_LOG("%s", LogSection);
        INFO_LOG("\n\n");
#endif

        LOG_DECREMENT();
        LOG_SEP(L"X");

        return;
    }

#if MERIDIAN_DEBUG > 0
    LogSection = L"Check and Set Items";
    DEBUG_LOG(1, LOG_LINE_NORMAL, L"%s", LogSection);
    INFO_LOG("\n");
    INFO_LOG("%s:", LogSection);
#endif

    ToolTotal = 0;
    for (i = 0; i < NUM_TOOLS; i++) {
        switch (GlobalConfig.ShowTools[i]) {
        case TAG_SHELL:
            ToolName = LABEL_SHELL;
            break;
        case TAG_BOOTORDER:
            ToolName = LABEL_BOOTORDER;
            break;
#if defined(EFIX64)
        case TAG_CLEAN_NVRAM:
            ToolName = LABEL_CLEAN_NVRAM;
            break;
#endif
        case TAG_CSR_ROTATE:
            ToolName = LABEL_CSR_ROTATE;
            break;
        case TAG_FIRMWARE:
            ToolName = LABEL_FIRMWARE;
            break;
        case TAG_FWUPDATE:
            ToolName = LABEL_FWUPDATE;
            break;
        case TAG_INSTALL:
            ToolName = LABEL_INSTALL;
            break;
        case TAG_EXIT:
            ToolName = LABEL_EXIT;
            break;
        case TAG_MOK:
            ToolName = LABEL_MOK;
            break;
        case TAG_GDISK:
            ToolName = LABEL_GDISK;
            break;
        case TAG_GPTSYNC:
            ToolName = LABEL_GPTSYNC;
            break;
        case TAG_MEMTEST:
            ToolName = LABEL_MEMTEST;
            break;
        case TAG_CYDIA:
            ToolName = LABEL_CYDIA;
            break;
        case TAG_NETBOOT:
            ToolName = LABEL_NETBOOT;
            break;
        case TAG_REBOOT:
            ToolName = LABEL_REBOOT;
            break;
        case TAG_SHUTDOWN:
            ToolName = LABEL_SHUTDOWN;
            break;
        case TAG_RECOVERY_MAC:
            ToolName = LABEL_RECOVERY_MAC;
            break;
        case TAG_RECOVERY_WIN:
            ToolName = LABEL_RECOVERY_WIN;
            break;
        default:
            continue;
        }
        ToolTotal++;

#if MERIDIAN_DEBUG > 0
        INFO_LOG("%s  - Tool List Item %02d ... ", OffsetNext, ToolTotal);
#endif

        switch (GlobalConfig.ShowTools[i]) {
#if defined(EFIX64)
        case TAG_CLEAN_NVRAM:
            AddOptionalToolMenuEntry(ToolName, TAG_CLEAN_NVRAM, TRUE, FALSE);
            break;
#endif
        case TAG_SHUTDOWN:
            AddOptionalToolMenuEntry(ToolName, TAG_SHUTDOWN, FALSE, FALSE);
            break;
        case TAG_REBOOT:
            AddOptionalToolMenuEntry(ToolName, TAG_REBOOT, FALSE, FALSE);
            break;
        case TAG_EXIT:
            AddOptionalToolMenuEntry(ToolName, TAG_EXIT, FALSE, FALSE);
            break;
        case TAG_FIRMWARE:
            ItemBuffer = NULL;

            if (EfivarGetRaw(&GlobalGuid, L"OsIndicationsSupported", &ItemBuffer, NULL) !=
                EFI_SUCCESS) {
#if MERIDIAN_DEBUG > 0
                ToolStr = PoolPrint(L"Did *NOT* Enable Tool:- '%s' ... 'OsIndicationsSupported' "
                                    L"Flag Was *NOT* Found",
                                    ToolName);
                DEBUG_LOG(1, LOG_THREE_STAR_END, L"%s", ToolStr);
                INFO_LOG(" * NOTE *     %s", ToolStr);
                MRD_FREE_POOL(ToolStr);
#endif
            }
            else {
                osind = *(UINT64 *)ItemBuffer;
                if (osind & EFI_OS_INDICATIONS_BOOT_TO_FW_UI) {
                    AddOptionalToolMenuEntry(ToolName, TAG_FIRMWARE, FALSE, FALSE);
                }
                else {
#if MERIDIAN_DEBUG > 0
                    ToolStr = PoolPrint(L"Could *NOT* Find Tool:- '%s'", ToolName);
                    DEBUG_LOG(1, LOG_THREE_STAR_END, L"%s", ToolStr);
                    INFO_LOG("*_ WARN _*    %s", ToolStr);
                    MRD_FREE_POOL(ToolStr);
#endif
                }
                MRD_FREE_POOL(ItemBuffer);
            }

            break;
        case TAG_SHELL:
            AddOptionalToolMenuEntry(ToolName, TAG_SHELL, TRUE, FALSE);
            break;
        case TAG_GPTSYNC:
            AddOptionalToolMenuEntry(ToolName, TAG_GPTSYNC, TRUE, FALSE);
            break;
        case TAG_GDISK:
            AddOptionalToolMenuEntry(ToolName, TAG_GDISK, TRUE, FALSE);
            break;
        case TAG_MOK:
            AddOptionalToolMenuEntry(ToolName, TAG_MOK, TRUE, FALSE);
            break;
        case TAG_FWUPDATE:
            AddOptionalToolMenuEntry(ToolName, TAG_FWUPDATE, TRUE, FALSE);
            break;
        case TAG_NETBOOT:
            AddOptionalToolMenuEntry(ToolName, TAG_NETBOOT, TRUE, TRUE);
            break;
        case TAG_RECOVERY_MAC:
            AddOptionalToolMenuEntry(ToolName, TAG_RECOVERY_MAC, TRUE, FALSE);
            break;
        case TAG_RECOVERY_WIN:
            AddOptionalToolMenuEntry(ToolName, TAG_RECOVERY_WIN, TRUE, FALSE);
            break;
        case TAG_CSR_ROTATE:
            if (!AppleFirmware && !HasMacOS) {
                VetCSR();

                MRD_FREE_POOL(gCsrStatus);
                gCsrStatus = StrDuplicate(L"Incompatible Setup");
                Status = EFI_UNSUPPORTED;
            }
            else if (GlobalConfig.CsrValues != NULL) {
#if MERIDIAN_DEBUG > 0
                MRD_MUTELOGGER_SET;
#endif

                Status = GetCsrStatus(&CsrValue);
                if (!EFI_ERROR(Status)) {
                    if (GlobalConfig.DynamicCSR == -1) {
                        MRD_FREE_POOL(gCsrStatus);
                        gCsrStatus = StrDuplicate(L"Dynamic SIP/SSV Disable");
                    }
                    else {
                        if (GlobalConfig.DynamicCSR == 1) {
                            MRD_FREE_POOL(gCsrStatus);
                            gCsrStatus = StrDuplicate(L"Dynamic SIP/SSV Enable");
                        }
                    }
                }
#if MERIDIAN_DEBUG > 0
                MRD_MUTELOGGER_OFF;
#endif
            }
            else {

                Status = FlagNoCSR();
            }

            if (EFI_ERROR(Status)) {
#if MERIDIAN_DEBUG > 0
                ToolStr = PoolPrint(L"Did *NOT* Enable Tool:- '%s' ... %r (%s)", ToolName, Status,
                                    gCsrStatus);
                DEBUG_LOG(1, LOG_THREE_STAR_END, L"%s", ToolStr);
                INFO_LOG(" * NOTE *     %s", ToolStr);
                MRD_FREE_POOL(ToolStr);
#endif

                break;
            }

            if (!AddToolMenuEntry(ToolName, TAG_CSR_ROTATE, TRUE)) {
                LOG_DECREMENT();
                LOG_SEP(L"X");
                return;
            }

#if MERIDIAN_DEBUG > 0
            LogToolMenuResult(ToolName, TRUE, FALSE);
#endif

            break;
        case TAG_INSTALL:
            if (!AddToolMenuEntry(ToolName, TAG_INSTALL, FALSE)) {
                LOG_DECREMENT();
                LOG_SEP(L"X");
                return;
            }

#if MERIDIAN_DEBUG > 0
            LogToolMenuResult(ToolName, TRUE, FALSE);
#endif

            break;
        case TAG_BOOTORDER:
            if (!AddToolMenuEntry(ToolName, TAG_BOOTORDER, TRUE)) {
                LOG_DECREMENT();
                LOG_SEP(L"X");
                return;
            }

#if MERIDIAN_DEBUG > 0
            LogToolMenuResult(ToolName, TRUE, FALSE);
#endif

            break;
        case TAG_MEMTEST:
            AddOptionalToolMenuEntry(ToolName, TAG_MEMTEST, TRUE, FALSE);
            break;
        case TAG_CYDIA:
            AddOptionalToolMenuEntry(ToolName, TAG_CYDIA, TRUE, FALSE);
            break;
        }
    }

#if MERIDIAN_DEBUG > 0
    ToolStr = PoolPrint(L"Processed %d Tool List Item%s", ToolTotal, (ToolTotal == 1) ? L"" : L"s");
    DEBUG_LOG(1, LOG_STAR_HEAD_SEPX, L"%s", ToolStr);
    DEBUG_LOG(1, LOG_BLANK_LINE_SEP, L"%s", ToolStr);
    INFO_LOG("\n\n");
    INFO_LOG("INFO: %s", ToolStr);
    INFO_LOG("\n\n");
#endif

    {
        MERIDIAN_MENU_ENTRY *MenuEntryScanAll = AllocateZeroPool(sizeof(MERIDIAN_MENU_ENTRY));
        if (MenuEntryScanAll) {
            MenuEntryScanAll->Title = StrDuplicate(L"Scan for Bootloaders");
            MenuEntryScanAll->Tag = TAG_SCAN_ALL;
            MenuEntryScanAll->Row = 1;
            AddMenuEntry(MainMenu, MenuEntryScanAll);
        }
    }

    LOG_DECREMENT();
    LOG_SEP(L"X");
}
