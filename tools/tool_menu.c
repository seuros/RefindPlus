// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2020-2026 Dayo Akanji
// SPDX-FileCopyrightText: 2021 Joe van Tunen
// SPDX-FileCopyrightText: 2012-2023 Roderick W. Smith
// SPDX-FileCopyrightText: 2006-2010 Christoph Pfisterer

#include "global.h"
#include "lib.h"
#include "mok.h"
#include "menu.h"
#include "scan.h"
#include "apple.h"
#include "config.h"
#include "install.h"
#include "sysinfo.h"
#include "screenmgt.h"
#include "mystrings.h"
#include "launch_efi.h"
#include "driver_support.h"
#include "security_policy.h"
#include "main.h"

extern UINTN RecoveryMacEntryItemsCount, RecoveryWinEntryItemsCount, MemTestEntryItemsCount;
extern UINTN NetBootEntryItemsCount, ShellEntryItemsCount, MOKEntryItemsCount;
extern UINTN GDiskEntryItemsCount, GPTSyncEntryItemsCount, FwUpdateEntryItemsCount;
extern UINTN CydiaEntryItemsCount;
extern LOADER_ENTRY **RecoveryMacEntryItems, **RecoveryWinEntryItems, **MemTestEntryItems;
extern LOADER_ENTRY **NetBootEntryItems, **ShellEntryItems, **MOKEntryItems;
extern LOADER_ENTRY **GDiskEntryItems, **GPTSyncEntryItems, **FwUpdateEntryItems;
extern LOADER_ENTRY **CydiaEntryItems;
extern CHAR16 *AllToolLocations, *VendorInfo;
extern UINTN EfiMajorVersion;
extern BOOLEAN WarnVersionEFI, SecureFlag, SetSysTab, ShimFound;
#include "version.h"
static CHAR16 *GetMacVersion(IN MERIDIAN_FILE *File)
{
    UINTN i;
    CHAR16 *Line;
    CHAR16 *TypeMacOS;
    BOOLEAN CheckNext;
    BOOLEAN ExitLoop;

    TypeMacOS = LABEL_UNKNOWN;
    CheckNext = FALSE;
    ExitLoop = FALSE;

    for (i = 0; i < 100; i++) {
        Line = ReadLine(File);
        if (Line == NULL) {
            break;
        }

        if (!CheckNext) {
            if (MrdStrFind(Line, L"<key>ProductVersion") ||
                MrdStrFind(Line, L"<key>ProductUserVisibleVersion")) {
                CheckNext = TRUE;
            }
        }
        else {
            CheckNext = FALSE;

            if (0)
                ;
            else if (MrdStrFind(Line, L"10.4."))
                TypeMacOS = L"10.04 Tiger";
            else if (MrdStrFind(Line, L"10.5."))
                TypeMacOS = L"10.05 Leopard";
            else if (MrdStrFind(Line, L"10.6."))
                TypeMacOS = L"10.06 Snow Leopard";
            else if (MrdStrFind(Line, L"10.7."))
                TypeMacOS = L"10.07 Lion";
            else if (MrdStrFind(Line, L"10.8."))
                TypeMacOS = L"10.08 Mountain Lion";
            else if (MrdStrFind(Line, L"10.9."))
                TypeMacOS = L"10.09 Mavericks";
            else if (MrdStrFind(Line, L"10.10."))
                TypeMacOS = L"10.10 Yosemite";
            else if (MrdStrFind(Line, L"10.11."))
                TypeMacOS = L"10.11 El Capitan";
            else if (MrdStrFind(Line, L"10.12."))
                TypeMacOS = L"10.12 Sierra";
            else if (MrdStrFind(Line, L"10.13."))
                TypeMacOS = L"10.13 High Sierra";
            else if (MrdStrFind(Line, L"10.14."))
                TypeMacOS = L"10.14 Mojave";
            else if (MrdStrFind(Line, L"<string>"))
                CheckNext = TRUE;

            if (!CheckNext) {
                ExitLoop = TRUE;
            }
        }

        MRD_FREE_POOL(Line);

        if (ExitLoop) {
            break;
        }
    }

    return TypeMacOS;
}

static BOOLEAN HandleExitShowInfo(VOID)
{
#if MERIDIAN_DEBUG > 0
    const CHAR16 *AbortedMenu = L"Aborted ... ToolInfoMenu:- 'NULL'";

    DEBUG_LOG(1, LOG_LINE_NORMAL, L"%s", AbortedMenu);
    INFO_LOG("INFO: %s", AbortedMenu);
    INFO_LOG("\n\n");
#endif

    return FALSE;
}

static MERIDIAN_MENU_SCREEN *InitToolMenu(CHAR16 *ToolPurpose, CHAR16 *Title)
{
    MERIDIAN_MENU_SCREEN *Menu;

    Menu = AllocateZeroPool(sizeof(MERIDIAN_MENU_SCREEN));
    if (Menu == NULL) {
        return NULL;
    }

#if MERIDIAN_DEBUG > 0
    DEBUG_LOG(1, LOG_LINE_THIN_SEP, L"Prepare Menu Screen");
    DEBUG_LOG(1, LOG_LINE_NORMAL, L"Screen Title:- 'Prep %s Menu'", ToolPurpose);
#endif

    Menu->Title = PoolPrint(L"%s Menu", Title);
    Menu->Hint1 = StrDuplicate(SELECT_OPTION_HINT);
    Menu->Hint2 = StrDuplicate(RETURN_MAIN_SCREEN_HINT);

    return Menu;
}

static VOID HandleToolMenu(MERIDIAN_MENU_SCREEN **Menu, MERIDIAN_MENU_ENTRY **EntryItems,
                           UINTN EntryCount)
{
    UINTN i;
    BOOLEAN RetVal;

    if (EntryItems == NULL) {
        AddMenuInfoLine(*Menu, L"Could *NOT* Find Valid Instance", FALSE);
        AddMenuInfoLine(*Menu, L"  Remove from 'showtools' List", FALSE);
        AddMenuInfoLine(*Menu, L"", FALSE);
    }
    else {
        for (i = 0; i < EntryCount; i++) {
            AddMenuEntry(*Menu, (MERIDIAN_MENU_ENTRY *)EntryItems[i]);
        }
    }

    RetVal = GetMenuEntryReturn(Menu);
    if (!RetVal) {
        FreeMenuScreen(Menu);
    }
}

static BOOLEAN HandleToolSelection(MERIDIAN_MENU_SCREEN *MenuScreen,
                                   LOADER_ENTRY **ReturnEntryItem OPTIONAL,
                                   LOADER_ENTRY ***EntryItems)
{
    INTN DefaultEntry;
    UINTN MenuExit;
    MENU_STYLE_FUNC Style;
    MERIDIAN_MENU_ENTRY *ChosenOption;

    if (MenuScreen == NULL) {

        return HandleExitShowInfo();
    }

    DefaultEntry = 9999;
    Style = NULL;
    MenuExit = DrawMenuScreen(MenuScreen, Style, &DefaultEntry, &ChosenOption);

#if MERIDIAN_DEBUG > 0
    DEBUG_LOG(1, LOG_LINE_NORMAL, L"Returned '%d' (%s) from Menu Screen Option in '%a' Call ... %s",
              MenuExit, MenuExitInfo(MenuExit), __func__, ChosenOption->Title);
    INFO_LOG("\n\n");
    INFO_LOG("Received User Input:");
    INFO_LOG("%s  - %s", OffsetNext, ChosenOption->Title);
#endif

    if (MenuExit != MENU_EXIT_ENTER || ChosenOption->Tag != TAG_BASE) {
        return FALSE;
    }

    if (ReturnEntryItem != NULL) {
        *ReturnEntryItem = CopyLoaderEntry((*EntryItems)[ChosenOption->Row]);
    }

    return TRUE;
}

static BOOLEAN ShowInfoRecoveryMac(CHAR16 *ToolPurpose, LOADER_ENTRY **ReturnEntryItem OPTIONAL)
{
    EFI_STATUS Status;
    UINTN i, j;
    UINTN TempSize;
    UINTN VolumeIndex;
    CHAR16 *RecoverVol;
    CHAR16 *FileName;
    MERIDIAN_FILE *TempFile;
    LOADER_ENTRY *MenuEntryItem;

    static BOOLEAN RunOnce = FALSE;
    static MERIDIAN_MENU_SCREEN *ToolInfoMenu = NULL;

    BOOLEAN SkipSystemVolume;

    do {
        if (RunOnce)
            break;

        ToolInfoMenu = InitToolMenu(ToolPurpose, LABEL_RECOVERY_MAC);
        if (ToolInfoMenu == NULL)
            break;

        i = 0;
        while (1) {
            FileName = FindCommaDelimited(GlobalConfig.MacOSRecoveryFiles, i++);
            if (FileName == NULL)
                break;

            for (VolumeIndex = 0; VolumeIndex < RecoveryVolumesHFSCount; VolumeIndex++) {
                if (RecoveryVolumesHFS[VolumeIndex]->RootDir != NULL &&
                    IsValidTool(RecoveryVolumesHFS[VolumeIndex], FileName)) {
                    Status = MeridianReadFile(RecoveryVolumesHFS[VolumeIndex]->RootDir,
                                              MACOS_RECOVERY_VERSION_FILE, TempFile, &TempSize);
                    if (EFI_ERROR(Status)) {
                        RecoverVol = L"RecoveryHD";
                    }
                    else {
                        RecoverVol = GetMacVersion(TempFile);
                        MRD_FREE_FILE(TempFile);
                    }

                    MenuEntryItem = AllocateZeroPool(sizeof(LOADER_ENTRY));
                    if (MenuEntryItem != NULL) {
                        MenuEntryItem->me.Title =
                            PoolPrint(L"%s - %s", RECOVERY_NAME_HFS, RecoverVol);
                        MenuEntryItem->me.Tag = TAG_BASE;
                        MenuEntryItem->me.Row = RecoveryMacEntryItemsCount;
                        MenuEntryItem->LoaderPath = StrDuplicate(FileName);
                        MenuEntryItem->Volume = RecoveryVolumesHFS[VolumeIndex];
                        MenuEntryItem->UseGraphicsMode = FALSE;

                        AddListElement((VOID ***)&RecoveryMacEntryItems,
                                       &RecoveryMacEntryItemsCount, MenuEntryItem);
                    }
                }
            }

            MRD_FREE_POOL(FileName);
        }

        if (SingleAPFS) {
            for (i = 0; i < RecoveryVolumesAPFSCount; i++) {
                RecoverVol = NULL;

                for (j = 0; j < SystemVolumesCount; j++) {
                    SkipSystemVolume = FALSE;
                    for (VolumeIndex = 0; VolumeIndex < SkipApfsVolumesCount; VolumeIndex++) {
                        if (GuidsAreEqual(&(SkipApfsVolumes[VolumeIndex]->VolUuid),
                                          &(SystemVolumes[j]->VolUuid))) {
                            SkipSystemVolume = TRUE;
                            break;
                        }
                    }

                    if (!SkipSystemVolume) {
                        if (GuidsAreEqual(&(RecoveryVolumesAPFS[i]->PartGuid),
                                          &(SystemVolumes[j]->PartGuid))) {
                            if (SystemVolumes[j]->VolRole == APFS_VOLUME_ROLE_SYSTEM ||
                                SystemVolumes[j]->VolRole == APFS_VOLUME_ROLE_UNDEFINED) {
                                RecoverVol = SystemVolumes[j]->VolName;

                                break;
                            }
                        }
                    }
                }

                if (RecoverVol != NULL) {
                    MenuEntryItem = AllocateZeroPool(sizeof(LOADER_ENTRY));
                    if (MenuEntryItem != NULL) {
                        MenuEntryItem->me.Title =
                            PoolPrint(L"%s - %s", RECOVERY_NAME_APFS, RecoverVol);
                        MenuEntryItem->me.Tag = TAG_BASE;
                        MenuEntryItem->me.Row = RecoveryMacEntryItemsCount;
                        MenuEntryItem->LoaderPath = StrDuplicate(L"boot.efi");
                        MenuEntryItem->Volume = RecoveryVolumesAPFS[i];
                        MenuEntryItem->UseGraphicsMode = FALSE;

                        AddListElement((VOID ***)&RecoveryMacEntryItems,
                                       &RecoveryMacEntryItemsCount, MenuEntryItem);
                    }
                }
            }
        }

        HandleToolMenu(&ToolInfoMenu, (MERIDIAN_MENU_ENTRY **)RecoveryMacEntryItems,
                       RecoveryMacEntryItemsCount);
    } while (0);

    RunOnce = TRUE;

    return HandleToolSelection(ToolInfoMenu, ReturnEntryItem,
                               (LOADER_ENTRY ***)&RecoveryMacEntryItems);
}

static BOOLEAN ShowInfoRecoveryWin(CHAR16 *ToolPurpose, LOADER_ENTRY **ReturnEntryItem OPTIONAL)
{
    UINTN i, j;
    UINTN VolumeIndex;

    CHAR16 *RecoverVol = NULL;
    CHAR16 *FileName;
    LOADER_ENTRY *MenuEntryItem;

    static BOOLEAN RunOnce = FALSE;
    static MERIDIAN_MENU_SCREEN *ToolInfoMenu = NULL;

    do {
        if (RunOnce)
            break;

        ToolInfoMenu = InitToolMenu(ToolPurpose, LABEL_RECOVERY_WIN);
        if (ToolInfoMenu == NULL)
            break;

        for (i = 0; i < RecoveryWinEntryItemsCount; i++) {
            j = 0;
            while (1) {
                FileName = FindCommaDelimited(GlobalConfig.WindowsRecoveryFiles, j++);
                if (FileName == NULL)
                    break;

                SplitVolumeAndFilename(&FileName, &RecoverVol);
                for (VolumeIndex = 0; VolumeIndex < VolumesCount; VolumeIndex++) {
                    if (Volumes[VolumeIndex]->RootDir != NULL &&
                        IsValidTool(Volumes[VolumeIndex], FileName) &&
                        (RecoverVol == NULL ||
                         MrdStrEqualsCI(RecoverVol, Volumes[VolumeIndex]->VolName))) {
                        MenuEntryItem = AllocateZeroPool(sizeof(LOADER_ENTRY));
                        if (MenuEntryItem != NULL) {
                            MenuEntryItem->me.Title =
                                PoolPrint(L"Windows Recovery from %s via %s",
                                          SetVolType(NULL, Volumes[VolumeIndex]->VolName,
                                                     Volumes[VolumeIndex]->FSType),
                                          FileName);
                            MenuEntryItem->me.Tag = TAG_BASE;
                            MenuEntryItem->me.Row = RecoveryWinEntryItemsCount;
                            MenuEntryItem->LoaderPath = StrDuplicate(FileName);
                            MenuEntryItem->Volume = Volumes[VolumeIndex];
                            MenuEntryItem->UseGraphicsMode = FALSE;

                            AddListElement((VOID ***)&RecoveryWinEntryItems,
                                           &RecoveryWinEntryItemsCount, MenuEntryItem);
                        }
                    }
                }

                MRD_FREE_POOL(RecoverVol);
                MRD_FREE_POOL(FileName);
            }
        }

        HandleToolMenu(&ToolInfoMenu, (MERIDIAN_MENU_ENTRY **)RecoveryWinEntryItems,
                       RecoveryWinEntryItemsCount);
    } while (0);

    RunOnce = TRUE;

    return HandleToolSelection(ToolInfoMenu, ReturnEntryItem,
                               (LOADER_ENTRY ***)&RecoveryWinEntryItems);
}

static BOOLEAN ShowInfoMemTest(CHAR16 *ToolPurpose, LOADER_ENTRY **ReturnEntryItem OPTIONAL)
{
    CHAR16 *ToolLoc;

    static BOOLEAN RunOnce = FALSE;
    static MERIDIAN_MENU_SCREEN *ToolInfoMenu = NULL;

    do {
        if (RunOnce)
            break;

        ToolInfoMenu = InitToolMenu(ToolPurpose, LABEL_MEMTEST);
        if (ToolInfoMenu == NULL)
            break;

        ToolLoc = StrDuplicate(SelfDirPath);
        MergeStrings(&ToolLoc, SelfToolPath, L',');
        MergeUniqueItems(&ToolLoc, GlobalConfig.ToolLocations, L',');
        MergeUniqueItems(&ToolLoc, MEMTEST_LOCATIONS, L',');

        FindTool(ToolLoc, MEMTEST_FILES, ToolPurpose, TRUE, FALSE, TAG_MEMTEST);
        MRD_FREE_POOL(ToolLoc);

        HandleToolMenu(&ToolInfoMenu, (MERIDIAN_MENU_ENTRY **)MemTestEntryItems,
                       MemTestEntryItemsCount);
    } while (0);

    RunOnce = TRUE;

    return HandleToolSelection(ToolInfoMenu, ReturnEntryItem, (LOADER_ENTRY ***)&MemTestEntryItems);
}

static BOOLEAN ShowInfoShell(CHAR16 *ToolPurpose, LOADER_ENTRY **ReturnEntryItem OPTIONAL)
{
#if MERIDIAN_DEBUG > 0
    BOOLEAN CheckMute = FALSE;
#endif

    static BOOLEAN RunOnce = FALSE;
    static MERIDIAN_MENU_SCREEN *ToolInfoMenu = NULL;

    do {
        if (RunOnce)
            break;

        ToolInfoMenu = InitToolMenu(ToolPurpose, LABEL_SHELL);
        if (ToolInfoMenu == NULL)
            break;

        FindTool(AllToolLocations, SHELL_FILES, ToolPurpose, TRUE, TRUE, TAG_SHELL);

#if MERIDIAN_DEBUG > 0
        MRD_MUTELOGGER_SET;
#endif
        ScanFirmwareDefined(1, L"Shell", TAG_SHELL);
#if MERIDIAN_DEBUG > 0
        MRD_MUTELOGGER_OFF;
#endif

        HandleToolMenu(&ToolInfoMenu, (MERIDIAN_MENU_ENTRY **)ShellEntryItems,
                       ShellEntryItemsCount);

        RunOnce = TRUE;
    } while (0);

    return HandleToolSelection(ToolInfoMenu, ReturnEntryItem, (LOADER_ENTRY ***)&ShellEntryItems);
}

static BOOLEAN ShowInfoToolMenu(CHAR16 *ToolPurpose, LOADER_ENTRY **ReturnEntryItem OPTIONAL,
                                BOOLEAN *RunOnce, MERIDIAN_MENU_SCREEN **ToolInfoMenu,
                                CHAR16 *Title, CHAR16 *Locations, CHAR16 *Files,
                                BOOLEAN SelfVolOnly, BOOLEAN ScanMultiple, UINTN TypeTag,
                                LOADER_ENTRY ***EntryItems, UINTN *EntryItemsCount)
{
    do {
        if (*RunOnce)
            break;

        *ToolInfoMenu = InitToolMenu(ToolPurpose, Title);
        if (*ToolInfoMenu == NULL)
            break;

        FindTool(Locations, Files, ToolPurpose, SelfVolOnly, ScanMultiple, TypeTag);

        HandleToolMenu(ToolInfoMenu, (MERIDIAN_MENU_ENTRY **)*EntryItems, *EntryItemsCount);
    } while (0);

    *RunOnce = TRUE;

    return HandleToolSelection(*ToolInfoMenu, ReturnEntryItem, EntryItems);
}

static BOOLEAN ShowInfoGPTSync(CHAR16 *ToolPurpose, LOADER_ENTRY **ReturnEntryItem OPTIONAL)
{
    static BOOLEAN RunOnce = FALSE;
    static MERIDIAN_MENU_SCREEN *ToolInfoMenu = NULL;

    return ShowInfoToolMenu(ToolPurpose, ReturnEntryItem, &RunOnce, &ToolInfoMenu, LABEL_GPTSYNC,
                            AllToolLocations, GPTSYNC_FILES, TRUE, FALSE, TAG_GPTSYNC,
                            &GPTSyncEntryItems, &GPTSyncEntryItemsCount);
}

static BOOLEAN ShowInfoGDisk(CHAR16 *ToolPurpose, LOADER_ENTRY **ReturnEntryItem OPTIONAL)
{
    static BOOLEAN RunOnce = FALSE;
    static MERIDIAN_MENU_SCREEN *ToolInfoMenu = NULL;

    return ShowInfoToolMenu(ToolPurpose, ReturnEntryItem, &RunOnce, &ToolInfoMenu, LABEL_GDISK,
                            AllToolLocations, GDISK_FILES, TRUE, FALSE, TAG_GDISK, &GDiskEntryItems,
                            &GDiskEntryItemsCount);
}

static BOOLEAN ShowInfoMOK(CHAR16 *ToolPurpose, LOADER_ENTRY **ReturnEntryItem OPTIONAL)
{
    static BOOLEAN RunOnce = FALSE;
    static MERIDIAN_MENU_SCREEN *ToolInfoMenu = NULL;

    return ShowInfoToolMenu(ToolPurpose, ReturnEntryItem, &RunOnce, &ToolInfoMenu, LABEL_MOK,
                            AllToolLocations, MOK_FILES, FALSE, TRUE, TAG_MOK, &MOKEntryItems,
                            &MOKEntryItemsCount);
}

static BOOLEAN ShowInfoFwUpdate(CHAR16 *ToolPurpose, LOADER_ENTRY **ReturnEntryItem OPTIONAL)
{
    static BOOLEAN RunOnce = FALSE;
    static MERIDIAN_MENU_SCREEN *ToolInfoMenu = NULL;

    return ShowInfoToolMenu(ToolPurpose, ReturnEntryItem, &RunOnce, &ToolInfoMenu, LABEL_FWUPDATE,
                            AllToolLocations, FWUPDATE_FILES, FALSE, TRUE, TAG_FWUPDATE,
                            &FwUpdateEntryItems, &FwUpdateEntryItemsCount);
}

static BOOLEAN ShowInfoNetBoot(CHAR16 *ToolPurpose, LOADER_ENTRY **ReturnEntryItem OPTIONAL)
{
    static BOOLEAN RunOnce = FALSE;
    static MERIDIAN_MENU_SCREEN *ToolInfoMenu = NULL;

    return ShowInfoToolMenu(ToolPurpose, ReturnEntryItem, &RunOnce, &ToolInfoMenu, LABEL_NETBOOT,
                            REMOTE_LOCATIONS, NETBOOT_FILES, TRUE, TRUE, TAG_NETBOOT,
                            &NetBootEntryItems, &NetBootEntryItemsCount);
}

static BOOLEAN ShowInfoCydia(CHAR16 *ToolPurpose, LOADER_ENTRY **ReturnEntryItem OPTIONAL)
{
    static BOOLEAN RunOnce = FALSE;
    static MERIDIAN_MENU_SCREEN *ToolInfoMenu = NULL;

    return ShowInfoToolMenu(ToolPurpose, ReturnEntryItem, &RunOnce, &ToolInfoMenu, LABEL_CYDIA,
                            AllToolLocations, CYDIA_FILES, TRUE, FALSE, TAG_CYDIA, &CydiaEntryItems,
                            &CydiaEntryItemsCount);
}

#if defined(EFIX64)
BOOLEAN ShowInfoCleanNvram(CHAR16 *ToolPurpose)
{
    INTN DefaultEntry;
    UINTN i, j;
    UINTN MenuExit;
    CHAR16 *FilePath;
    CHAR16 *FileName;
    BOOLEAN RetVal;
    MENU_STYLE_FUNC Style;
    MERIDIAN_MENU_ENTRY *ChosenOption;
    MERIDIAN_MENU_ENTRY *MenuEntryCleanNvram;

    static BOOLEAN RunOnce = FALSE;
    static MERIDIAN_MENU_SCREEN *ToolInfoMenu = NULL;

    do {
        if (RunOnce)
            break;

        ToolInfoMenu = AllocateZeroPool(sizeof(MERIDIAN_MENU_SCREEN));
        if (ToolInfoMenu == NULL) {
            break;
        }

#if MERIDIAN_DEBUG > 0
        DEBUG_LOG(1, LOG_LINE_THIN_SEP, L"Prepare Menu Screen");
        DEBUG_LOG(1, LOG_LINE_NORMAL, L"Screen Title:- 'Run %s Info'", ToolPurpose);
#endif

        ToolInfoMenu->Title = StrDuplicate(LABEL_CLEAN_NVRAM);
        ToolInfoMenu->Hint1 = StrDuplicate(RETURN_MAIN_SCREEN_HINT);
        ToolInfoMenu->Hint2 = StrDuplicate(L"");

        AddMenuInfoLine(ToolInfoMenu, L"The tool binary must be placed in one of the paths below",
                        FALSE);
        AddMenuInfoLine(ToolInfoMenu, L" - The first file found in the order listed will be used",
                        FALSE);
        AddMenuInfoLine(ToolInfoMenu, L" - You will be returned to the main menu if not found",
                        FALSE);
        AddMenuInfoLine(ToolInfoMenu, L"", FALSE);

        i = 0;
        while (1) {
            FilePath = FindCommaDelimited(GlobalConfig.ToolLocations, i++);
            if (FilePath == NULL)
                break;

            j = 0;
            while (1) {
                FileName = FindCommaDelimited(NVRAMCLEAN_FILES, j++);
                if (FileName == NULL)
                    break;

                AddMenuInfoLine(ToolInfoMenu, PoolPrint(L"%s\\%s", FilePath, FileName), TRUE);

                MRD_FREE_POOL(FileName);
            }

            MRD_FREE_POOL(FilePath);
        }

        AddMenuInfoLine(ToolInfoMenu, L"", FALSE);

        MenuEntryCleanNvram = AllocateZeroPool(sizeof(MERIDIAN_MENU_ENTRY));
        if (MenuEntryCleanNvram == NULL) {
            FreeMenuScreen(&ToolInfoMenu);
            break;
        }

        MenuEntryCleanNvram->Title = StrDuplicate(ToolPurpose);
        MenuEntryCleanNvram->Tag = TAG_BASE;
        AddMenuEntry(ToolInfoMenu, MenuEntryCleanNvram);

        RetVal = GetMenuEntryReturn(&ToolInfoMenu);
        if (!RetVal)
            FreeMenuScreen(&ToolInfoMenu);
    } while (0);

    if (ToolInfoMenu == NULL) {
        RetVal = HandleExitShowInfo();
    }
    else {
        DefaultEntry = 9999;
        Style = NULL;
        ChosenOption = NULL;
        MenuExit = DrawMenuScreen(ToolInfoMenu, Style, &DefaultEntry, &ChosenOption);

#if MERIDIAN_DEBUG > 0
        DEBUG_LOG(1, LOG_LINE_NORMAL,
                  L"Returned '%d' (%s) from Menu Screen Option in '%a' Call ... %s", MenuExit,
                  MenuExitInfo(MenuExit), __func__, ChosenOption->Title);
        INFO_LOG("Received User Input:");
        INFO_LOG("%s  - %s", OffsetNext, ChosenOption->Title);
        INFO_LOG("\n\n");
#endif

        if (MenuExit != MENU_EXIT_ENTER || ChosenOption->Tag != TAG_BASE) {
            RetVal = FALSE;
        }
        else {
            RetVal = TRUE;
        }
    }

    RunOnce = TRUE;

    return RetVal;
}
#endif

VOID HandleToolRun(CHAR16 *TypeStr, BOOLEAN ToolFlag, LOADER_ENTRY *OurLoaderEntry OPTIONAL)
{
    if (!ToolFlag || OurLoaderEntry == NULL) {
#if MERIDIAN_DEBUG > 0
        INFO_LOG("\n\n");
#endif

        return;
    }

#if MERIDIAN_DEBUG > 0
    INFO_LOG("%s  - Load Tool to Run %s", OffsetNext, TypeStr);
#endif

    StartTool(OurLoaderEntry);
}

VOID PrepToolMenu(UINTN LabelTag)
{
    CHAR16 *TypeStr;
    BOOLEAN ToolFlag;
    LOADER_ENTRY *OurEntry;

    switch (LabelTag) {
    case TAG_MOK:
        TypeStr = LABEL_MOK;
        break;
    case TAG_SHELL:
        TypeStr = LABEL_SHELL;
        break;
    case TAG_GDISK:
        TypeStr = LABEL_GDISK;
        break;
    case TAG_GPTSYNC:
        TypeStr = LABEL_GPTSYNC;
        break;
    case TAG_MEMTEST:
        TypeStr = LABEL_MEMTEST;
        break;
    case TAG_CYDIA:
        TypeStr = LABEL_CYDIA;
        break;
    case TAG_NETBOOT:
        TypeStr = LABEL_NETBOOT;
        break;
    case TAG_FWUPDATE:
        TypeStr = LABEL_FWUPDATE;
        break;
    case TAG_RECOVERY_MAC:
        TypeStr = LABEL_RECOVERY_MAC;
        break;
    case TAG_RECOVERY_WIN:
        TypeStr = LABEL_RECOVERY_WIN;
        break;
    default:
        TypeStr = LABEL_UNKNOWN;
        break;
    }

#if MERIDIAN_DEBUG > 0
    INFO_LOG("Received User Input:");
    INFO_LOG("%s  - Show 'Run %s Tool' Menu", OffsetNext, TypeStr);
#endif

    OurEntry = NULL;

    switch (LabelTag) {
    case TAG_MOK:
        ToolFlag = ShowInfoMOK(TypeStr, &OurEntry);
        break;
    case TAG_SHELL:
        ToolFlag = ShowInfoShell(TypeStr, &OurEntry);
        break;
    case TAG_GDISK:
        ToolFlag = ShowInfoGDisk(TypeStr, &OurEntry);
        break;
    case TAG_GPTSYNC:
        ToolFlag = ShowInfoGPTSync(TypeStr, &OurEntry);
        break;
    case TAG_MEMTEST:
        ToolFlag = ShowInfoMemTest(TypeStr, &OurEntry);
        break;
    case TAG_CYDIA:
        ToolFlag = ShowInfoCydia(TypeStr, &OurEntry);
        break;
    case TAG_NETBOOT:
        ToolFlag = ShowInfoNetBoot(TypeStr, &OurEntry);
        break;
    case TAG_FWUPDATE:
        ToolFlag = ShowInfoFwUpdate(TypeStr, &OurEntry);
        break;
    case TAG_RECOVERY_MAC:
        ToolFlag = ShowInfoRecoveryMac(TypeStr, &OurEntry);
        break;
    case TAG_RECOVERY_WIN:
        ToolFlag = ShowInfoRecoveryWin(TypeStr, &OurEntry);
        break;
    default:
        ToolFlag = FALSE;
        break;
    }

    if (!MrdStrEqualsCI(TypeStr, LABEL_UNKNOWN)) {
        HandleToolRun(TypeStr, ToolFlag, OurEntry);
    }
}
