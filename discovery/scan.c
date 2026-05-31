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
#include "loop_timeout.h"
#include "apple.h"
#include "install.h"
#include "screenmgt.h"
#include "mystrings.h"
#include "launch_efi.h"
#include "driver_support.h"
#include "security_policy.h"

VOID BlsAppendString(IN OUT STRING_LIST **List, IN OUT STRING_LIST **Last, IN CHAR16 *Value);

EFI_GUID GlobalGuid = EFI_GLOBAL_VARIABLE;

#if MERIDIAN_DEBUG > 0

BOOLEAN LogNewLine = FALSE;
#endif

BOOLEAN HasMacOS = FALSE;
BOOLEAN HasOpenCore = FALSE;
BOOLEAN DisplayLoader = FALSE;
BOOLEAN ScanningLoaders = FALSE;

static VOID ScanEfiFiles(MERIDIAN_VOLUME *Volume)
{
    EFI_STATUS Status;
    UINTN i;
    UINTN Length;
    CHAR16 *Temp;
    CHAR16 *TmpMsg;
    CHAR16 *VolName;
    CHAR16 *FileName;
    CHAR16 *SelfPath;
    CHAR16 *Directory;

    CHAR16 *Extension = NULL;
    CHAR16 *VentoyName;
    BOOLEAN ScanFallbackLoader;
    BOOLEAN FoundBRBackup;
    BOOLEAN FoundVentoy;
    BOOLEAN CheckIter;
    EFI_FILE_INFO *EfiDirEntry;
    MERIDIAN_DIR_ITER EfiDirIter;

    static CHAR16 *MatchPatterns = NULL;

    VolName = (Volume->VolName != NULL) ? Volume->VolName : L"** No Name **";

    if (MrdStrEqualsCI(VolName, L"Whole Disk Volume")) {

        return;
    }

    i = 0;
    FoundVentoy = FALSE;
    while (GlobalConfig.HandleVentoy && !FoundVentoy) {
        VentoyName = FindCommaDelimited(VENTOY_NAMES, i++);
        if (VentoyName == NULL)
            break;

        if (MrdStrStartsWithCI(VentoyName, VolName) ||
            MrdStrStartsWithCI(VentoyName, Volume->FsName) ||
            MrdStrStartsWithCI(VentoyName, Volume->PartName)) {
            FoundVentoy = TRUE;
        }
        MRD_FREE_POOL(VentoyName);
    }

    if (!VolumeScanAllowed(Volume, TRUE, FALSE)) {
        if (!FoundVentoy) {

            return;
        }
    }

    ScanFallbackLoader = TRUE;
    if (FoundVentoy) {
        goto VentoyJump;
    }

    if (Volume->FSType == FS_TYPE_NTFS) {
        if (AppleFirmware && MrdStrFind(VolName, L"BOOTCAMP")) {

            return;
        }
    }

    if (Volume->FSType == FS_TYPE_APFS) {
        if (GlobalConfig.SyncAPFS) {
            if (SingleAPFS && (Volume->VolRole == APFS_VOLUME_ROLE_SYSTEM ||
                               Volume->VolRole == APFS_VOLUME_ROLE_PREBOOT ||
                               Volume->VolRole == APFS_VOLUME_ROLE_UNDEFINED)) {
                for (i = 0; i < SkipApfsVolumesCount; i++) {
                    if (GuidsAreEqual(&(SkipApfsVolumes[i]->PartGuid), &(Volume->PartGuid))) {

                        return;
                    }
                }
            }

            for (i = 0; i < SystemVolumesCount; i++) {
                if (GuidsAreEqual(&(SystemVolumes[i]->VolUuid), &(Volume->VolUuid))) {

                    return;
                }
            }
        }
    }

#if MERIDIAN_DEBUG > 0
    DEBUG_LOG(1, LOG_LINE_THIN_SEP, L"Handle uEFI Loaders on Volume:- '%s'", VolName);
#endif

    if (MatchPatterns == NULL) {
        MatchPatterns = StrDuplicate(LOADER_MATCH_PATTERNS);
        if (GlobalConfig.ScanAllLinux && GlobalConfig.LinuxMatchPatterns != NULL) {
            MergeUniqueStrings(&MatchPatterns, GlobalConfig.LinuxMatchPatterns, L',');
        }
        if (GlobalConfig.ScanLimine) {

            MergeUniqueStrings(&MatchPatterns, L"*.elf,*.ELF", L',');
        }
    }

    if (Volume->FSType != FS_TYPE_NTFS && ShouldScan(Volume, MACOSX_LOADER_DIR)) {

        FileName = StrDuplicate(MACOSX_LOADER_PATH);

        ScanFallbackLoader &= ScanMacOsLoader(Volume, FileName);
        MRD_FREE_POOL(FileName);

        {
            STRING_LIST *MacGuidDirs = NULL;
            STRING_LIST *MacGuidLast = NULL;
            STRING_LIST *MacGuidNode;
            LOOP_TIMER MacLt;

            DirIterOpen(Volume->RootDir, L"\\", &EfiDirIter);
            WHILE_TIMEOUT(MacLt, 2)
            {
                if (!DirIterNext(&EfiDirIter, 1, NULL, &EfiDirEntry)) {
                    break;
                }
                if (IsGuid(EfiDirEntry->FileName)) {
                    BlsAppendString(&MacGuidDirs, &MacGuidLast, EfiDirEntry->FileName);
                }
                MRD_FREE_POOL(EfiDirEntry);
            }
            LoopTimerStop(&MacLt);
            DirIterClose(&EfiDirIter);

            MacGuidNode = MacGuidDirs;
            while (MacGuidNode != NULL) {
                FileName = PoolPrint(L"%s\\%s", MacGuidNode->Value, MACOSX_LOADER_PATH);
                ScanFallbackLoader &= ScanMacOsLoader(Volume, FileName);
                MRD_FREE_POOL(FileName);

                FileName = PoolPrint(L"%s\\%s", MacGuidNode->Value, L"boot.efi");
                if (Volume->FSType != FS_TYPE_APFS) {
                    if (!MrdStrIncludesCI(GlobalConfig.MacOSRecoveryFiles, FileName)) {
                        MergeUniqueStrings(&GlobalConfig.MacOSRecoveryFiles, FileName, L',');
                    }
                }
                MRD_FREE_POOL(FileName);

                MacGuidNode = MacGuidNode->Next;
            }
            DeleteStringList(MacGuidDirs);
        }

        FileName = StrDuplicate(L"System\\Library\\CoreServices\\xom.efi");

        if (FileExists(Volume->RootDir, FileName) &&
            !FilenameIn(Volume, MACOSX_LOADER_DIR, L"xom.efi", GlobalConfig.DontScanFiles)) {
            DisplayLoader = TRUE;
            AddLoaderEntry(FileName, L"Instance: Windows XP (XoM)", Volume, TRUE, FALSE, NULL);

            if (DuplicatesFallback(Volume, FileName)) {
                ScanFallbackLoader = FALSE;
            }
        }
        MRD_FREE_POOL(FileName);
    }

    if (ShouldScan(Volume, L"EFI\\Microsoft\\Boot")) {
        FoundBRBackup = FALSE;

        FileName = StrDuplicate(L"EFI\\Microsoft\\Boot\\bkpbootmgfw.efi");

        if (FileExists(Volume->RootDir, FileName) &&
            !FilenameIn(Volume, L"EFI\\Microsoft\\Boot", L"bkpbootmgfw.efi",
                        GlobalConfig.DontScanFiles)) {
            FoundBRBackup = DisplayLoader = TRUE;

            AddLoaderEntry(FileName, L"Instance: Windows (UEFI) | BRBackup", Volume, TRUE, FALSE,
                           NULL);

            if (DuplicatesFallback(Volume, FileName)) {
                ScanFallbackLoader = FALSE;
            }
        }
        MRD_FREE_POOL(FileName);

        FileName = StrDuplicate(L"EFI\\Microsoft\\Boot\\bootmgfw.efi");
        if (FileExists(Volume->RootDir, FileName) &&
            !FilenameIn(Volume, L"EFI\\Microsoft\\Boot", L"bootmgfw.efi",
                        GlobalConfig.DontScanFiles)) {
            DisplayLoader = TRUE;
            TmpMsg = (FoundBRBackup) ? L"Instance: Windows (UEFI) | Possibly GRUB"
                                     : L"Instance: Windows (UEFI)";
            AddLoaderEntry(FileName, TmpMsg, Volume, TRUE, FALSE, NULL);

            if (DuplicatesFallback(Volume, FileName)) {
                ScanFallbackLoader = FALSE;
            }
        }
        MRD_FREE_POOL(FileName);
    }

    if (ScanLoaderDir(Volume, L"\\", MatchPatterns)) {
        ScanFallbackLoader = FALSE;
    }

    DirIterOpen(Volume->RootDir, L"EFI", &EfiDirIter);

    while (1) {
        CheckIter = DirIterNext(&EfiDirIter, 1, NULL, &EfiDirEntry);
        if (!CheckIter)
            break;

        do {
            if (EfiDirEntry->FileName[0] == '.' ||
                MrdStrEqualsCI(EfiDirEntry->FileName, L"tools")) {

                break;
            }

            if (IsDuplicateBsdEspDir(Volume, EfiDirEntry->FileName)) {
                break;
            }

            Extension = FindExtension(EfiDirEntry->FileName);
            if (Extension != NULL && !MrdStrEqualsCI(Extension, L".efi")) {

                break;
            }

            FileName = PoolPrint(L"EFI\\%s", EfiDirEntry->FileName);

            if (ScanLoaderDir(Volume, FileName, MatchPatterns)) {
                ScanFallbackLoader = FALSE;
            }
            MRD_FREE_POOL(FileName);
        } while (0);

        MRD_FREE_POOL(Extension);
        MRD_FREE_POOL(EfiDirEntry);
    }

    Status = DirIterClose(&EfiDirIter);

    if (EFI_ERROR(Status) && Status != EFI_NOT_FOUND && Status != EFI_INVALID_PARAMETER) {
        Temp = PoolPrint(L"While Scanning the EFI System Partition on '%s'", VolName);
        CheckError(Status, Temp);
        MRD_FREE_POOL(Temp);
    }

    if (ScanFallbackLoader) {
        SelfPath = DevicePathToStr(SelfLoadedImage->FilePath);

        CleanUpPathNameSlashes(SelfPath);

        if (DuplicatesFallback(Volume, SelfPath) &&
            Volume->DeviceHandle == SelfLoadedImage->DeviceHandle

        ) {
            ScanFallbackLoader = FALSE;
        }
        MRD_FREE_POOL(SelfPath);
    }

    if (ScanBsdRootLoader(Volume)) {
        ScanFallbackLoader = FALSE;
    }

    i = 0;
    VolName = NULL;
    while (ScanFallbackLoader) {
        Directory = FindCommaDelimited(GlobalConfig.AlsoScan, i++);
        if (Directory == NULL)
            break;

        if (ShouldScan(Volume, Directory)) {
            SplitVolumeAndFilename(&Directory, &VolName);

            CleanUpPathNameSlashes(Directory);

            Length = StrLen(Directory);

            if (Length > 0 && ScanLoaderDir(Volume, Directory, MatchPatterns)) {
                ScanFallbackLoader = FALSE;
            }
            MRD_FREE_POOL(VolName);
        }
        MRD_FREE_POOL(Directory);
    }

VentoyJump:
    ScanBLSEntries(Volume);

    if (ScanFallbackLoader && ShouldScan(Volume, L"EFI\\BOOT") &&
        FileExists(Volume->RootDir, FALLBACK_FULLNAME) &&
        !FilenameIn(Volume, L"EFI\\BOOT", FALLBACK_BASENAME, GlobalConfig.DontScanFiles)) {
        if (FoundVentoy) {
            TmpMsg = L"Instance: Ventoy";
        }
        else {
            TmpMsg = FALLBACK_BASENAME;
        }

        DisplayLoader = TRUE;
        Temp = StrDuplicate(FALLBACK_FULLNAME);
        AddLoaderEntry(Temp, TmpMsg, Volume, TRUE, FALSE, NULL);
        MRD_FREE_POOL(Temp);
    }
}

static VOID ScanVolumesByKind(IN UINTN DiskKind, IN CHAR16 *KindLabel)
{
    UINTN VolumeIndex;

#if MERIDIAN_DEBUG > 0
    DEBUG_LOG(1, LOG_THREE_STAR_SEP, L"Scan for %s with Mode:- 'UEFI'", KindLabel);
#else
    (VOID) KindLabel;
#endif

    LOG_SEP(L"X");
    LOG_INCREMENT();

    DisplayLoader = FALSE;
    for (VolumeIndex = 0; VolumeIndex < VolumesCount; VolumeIndex++) {
        if (Volumes[VolumeIndex]->DiskKind == DiskKind) {
            ScanEfiFiles(Volumes[VolumeIndex]);
        }
    }

#if MERIDIAN_DEBUG > 0
    if (!DisplayLoader) {
        DEBUG_LOG(1, LOG_STAR_HEAD_SEP, L"None Found");
    }
#endif

    LOG_DECREMENT();
    LOG_SEP(L"X");
}

BOOLEAN MeridianLaunchedByOpenCore(VOID)
{
    EFI_STATUS Status;
    EFI_LOADED_IMAGE_PROTOCOL *ParentImage;
    CHAR16 *ParentPath;
    BOOLEAN Result;

    if (AppleFirmware) {
        return FALSE;
    }

    if (SelfLoadedImage == NULL || SelfLoadedImage->ParentHandle == NULL) {
        return FALSE;
    }

    ParentImage = NULL;
    Status = gBS->HandleProtocol(SelfLoadedImage->ParentHandle, &gEfiLoadedImageProtocolGuid,
                                 (VOID **)&ParentImage);
    if (EFI_ERROR(Status) || ParentImage == NULL || ParentImage->FilePath == NULL) {
        return FALSE;
    }

    ParentPath = DevicePathToStr(ParentImage->FilePath);
    Result = (ParentPath != NULL && MrdStrIncludesCI(ParentPath, L"OpenCore"));
    MRD_FREE_POOL(ParentPath);

    return Result;
}

static VOID ReconcileHackintoshMacOS(VOID)
{
    UINTN i;
    UINTN j;
    UINTN Removed;
    BOOLEAN UnderOpenCore;
    LOADER_ENTRY *LoaderEntry;
    MERIDIAN_MENU_ENTRY *MenuEntry;
    MERIDIAN_MENU_ENTRY *RemovedEntry;

    if (AppleFirmware || !HasMacOS || !HasOpenCore) {

        return;
    }

    if (MainMenu == NULL || MainMenu->Entries == NULL) {
        return;
    }

    UnderOpenCore = MeridianLaunchedByOpenCore();

    if (!UnderOpenCore) {
        for (i = 0; i < MainMenu->EntryCount; i++) {
            MenuEntry = MainMenu->Entries[i];
            if (MenuEntry == NULL || MenuEntry->Tag != TAG_LOADER) {
                continue;
            }

            LoaderEntry = (LOADER_ENTRY *)MenuEntry;
            if (LoaderEntry->OSType != 'O') {
                continue;
            }

            MRD_FREE_POOL(MenuEntry->Title);
            MenuEntry->Title = StrDuplicate(L"macOS (via OpenCore)");
            break;
        }
    }

    Removed = 0;
    i = 0;
    while (i < MainMenu->EntryCount) {
        MenuEntry = MainMenu->Entries[i];
        if (MenuEntry == NULL || MenuEntry->Tag != TAG_LOADER) {
            i++;
            continue;
        }

        LoaderEntry = (LOADER_ENTRY *)MenuEntry;
        if (!(

                (LoaderEntry->OSType == 'M' && LoaderEntry->Volume != NULL &&
                 LoaderEntry->Volume->FSType == FS_TYPE_APFS) ||

                (UnderOpenCore && LoaderEntry->OSType == 'O'))) {
            i++;
            continue;
        }

        RemovedEntry = MenuEntry;
        for (j = i + 1; j < MainMenu->EntryCount; j++) {
            MainMenu->Entries[j - 1] = MainMenu->Entries[j];
        }
        MainMenu->EntryCount--;
        MainMenu->Entries[MainMenu->EntryCount] = NULL;
        FreeMenuEntry(&RemovedEntry);
        Removed++;

    }

#if MERIDIAN_DEBUG > 0
    DEBUG_LOG(1, LOG_STAR_SEPARATOR,
              L"Hackintosh Fingerprint (Non-Apple FW + macOS + OpenCore) ... %s ... Dropped %d "
              L"Unbootable macOS Entr%s",
              (UnderOpenCore) ? L"Downstream of OpenCore: No In-Meridian macOS Path (Boot macOS "
                                L"From OpenCore, or Make Meridian Primary)"
                              : L"Primary: macOS Routed via OpenCore Launch",
              Removed, (Removed == 1) ? L"y" : L"ies");
    INFO_LOG("\n");
    INFO_LOG("INFO: Hackintosh Auto-Route ... %s (Dropped %d Unbootable macOS Entr%s)",
             (UnderOpenCore) ? L"Downstream of OpenCore (macOS Entry Removed)"
                             : L"macOS via OpenCore",
             Removed, (Removed == 1) ? L"y" : L"ies");
    INFO_LOG("\n");
#endif
    (VOID) Removed;
}

VOID ScanForBootloaders(VOID)
{
#if MERIDIAN_DEBUG > 0
    CHAR16 *MsgStr;
    BOOLEAN LogNewLine;
#endif

    UINTN i;
    UINTN SetOptions;
    CHAR16 *OrigDontScanDirs;
    BOOLEAN AmendedDontScan;
    BOOLEAN HandledPreboot;
    ScanningLoaders = TRUE;
    ClearBlsClaimedLoaders();

    ClearDirCache();

#if MERIDIAN_DEBUG > 0
    DEBUG_LOG(1, LOG_BLANK_LINE_SEP, L"X");
    MsgStr = StrDuplicate(L"S E E K   I N S T A N C E   L O A D E R S");
    DEBUG_LOG(1, LOG_LINE_SEPARATOR, L"%s", MsgStr);
    INFO_LOG("%s", MsgStr);
    INFO_LOG("\n");
    MRD_FREE_POOL(MsgStr);
#endif

    LOG_SEP(L"X");
    LOG_INCREMENT();

    OrigDontScanDirs = NULL;
    AmendedDontScan = FALSE;
    if (GlobalConfig.SyncAPFS) {
        OrigDontScanDirs = StrDuplicate(GlobalConfig.DontScanDirs);

        if (GlobalConfig.DontScanDirs != NULL) {
            HandledPreboot = HidePreboot(L"DontScanDirs");
            if (HandledPreboot && !AmendedDontScan) {
                AmendedDontScan = TRUE;
            }
        }

        if (GlobalConfig.DontScanFiles != NULL) {
            HandledPreboot = HidePreboot(L"DontScanFiles");
            if (HandledPreboot && !AmendedDontScan) {
                AmendedDontScan = TRUE;
            }
        }

        if (GlobalConfig.DontScanVolumes != NULL) {
            HandledPreboot = HidePreboot(L"DontScanVolumes");
            if (HandledPreboot && !AmendedDontScan) {
                AmendedDontScan = TRUE;
            }
        }

#if MERIDIAN_DEBUG > 0
        if (AmendedDontScan) {
            DEBUG_LOG(1, LOG_STAR_SEPARATOR,
                      L"Sync PreBoot Volumes in 'Dont Scan' Lists ... Config Setting *IS NOT* "
                      L"Active:- 'disable_apfs_sync'");
        }
#endif
    }

    SetOptions = 0;
    for (i = 0; i < NUM_SCAN_OPTIONS; i++) {
        switch (GlobalConfig.ScanFor[i]) {
        case 'm':
        case 'M':
        case 'i':
        case 'I':
        case 'e':
        case 'E':
        case 'o':
        case 'O':
        case 'n':
        case 'N':
        case 'f':
        case 'F':
            SetOptions = SetOptions + 1;
        }
    }

#if MERIDIAN_DEBUG > 0
    LogNewLine = FALSE;
#endif

    for (i = 0; i <= SetOptions; i++) {
        switch (GlobalConfig.ScanFor[i]) {
        case 'm':
        case 'M':
#if MERIDIAN_DEBUG > 0
            if (LogNewLine) {
                INFO_LOG("\n");
                DEBUG_LOG(1, LOG_BLANK_LINE_TWO, L"X");
            }
            LogNewLine = TRUE;

            INFO_LOG("Scan Manual:");
            DEBUG_LOG(1, LOG_THREE_STAR_SEP, L"Scan for User Defined Stanzas");
#endif

            ScanUserConfigured(GlobalConfig.ConfigFilename);
            break;

        case 'i':
        case 'I':
#if MERIDIAN_DEBUG > 0
            if (LogNewLine) {
                INFO_LOG("\n");
                DEBUG_LOG(1, LOG_BLANK_LINE_TWO, L"X");
            }
            LogNewLine = TRUE;

            INFO_LOG("Scan Internal:");
#endif

            ScanVolumesByKind(DISK_KIND_INTERNAL, L"Internal Disk Volumes");
            break;

        case 'e':
        case 'E':
#if MERIDIAN_DEBUG > 0
            if (LogNewLine) {
                INFO_LOG("\n");
                DEBUG_LOG(1, LOG_BLANK_LINE_TWO, L"X");
            }
            LogNewLine = TRUE;

            INFO_LOG("Scan External:");
#endif

            ScanVolumesByKind(DISK_KIND_EXTERNAL, L"External Disk Volumes");
            break;

        case 'o':
        case 'O':
#if MERIDIAN_DEBUG > 0
            if (LogNewLine) {
                INFO_LOG("\n");
                DEBUG_LOG(1, LOG_BLANK_LINE_TWO, L"X");
            }
            LogNewLine = TRUE;

            INFO_LOG("Scan Optical:");
#endif

            ScanVolumesByKind(DISK_KIND_OPTICAL, L"Optical Discs");
            break;

        case 'n':
        case 'N':
#if MERIDIAN_DEBUG > 0
            if (LogNewLine) {
                INFO_LOG("\n");
                DEBUG_LOG(1, LOG_BLANK_LINE_TWO, L"X");
            }
            LogNewLine = TRUE;

            INFO_LOG("Scan Net Boot:");
#endif

            ScanNetboot();
            break;

        case 'f':
        case 'F':
#if MERIDIAN_DEBUG > 0
            if (LogNewLine) {
                INFO_LOG("\n");
                DEBUG_LOG(1, LOG_BLANK_LINE_TWO, L"X");
            }
            LogNewLine = TRUE;

            INFO_LOG("Scan Firmware:");
#endif

            ScanFirmwareDefined(0, NULL, TAG_BASE);
            break;
        }
    }

#if MERIDIAN_DEBUG > 0

    LogNewLine = FALSE;
#endif

    if (!AmendedDontScan) {
        MRD_FREE_POOL(OrigDontScanDirs);
    }

    if (OrigDontScanDirs != NULL) {

        MRD_FREE_POOL(GlobalConfig.DontScanDirs);
        GlobalConfig.DontScanDirs = OrigDontScanDirs;
    }

    ReconcileHackintoshMacOS();

    do {
        if (MainMenu->EntryCount == 0) {
#if MERIDIAN_DEBUG > 0
            MsgStr = StrDuplicate(L"Could *NOT* Locate Valid Instance Loaders");
            DEBUG_LOG(1, LOG_STAR_SEPARATOR, L"%s", MsgStr);
            MRD_FREE_POOL(MsgStr);
#endif

            break;
        }

#if MERIDIAN_DEBUG > 0
        DEBUG_LOG(1, LOG_STAR_HEAD_SEP, L"Located %d Instance Loader%s", MainMenu->EntryCount,
                  (MainMenu->EntryCount == 1) ? L"" : L"s");
#endif
    } while (0);

    FinishTextScreen(FALSE);

    ScanningLoaders = FALSE;

    LOG_DECREMENT();
    LOG_SEP(L"X");
}
