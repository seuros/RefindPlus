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
#include "limine_boot.h"
#include "btrfs_snapshots.h"

extern LOADER_ENTRY **ShellEntryItems;
extern UINTN ShellEntryItemsCount;

static VOID GetBaseEntry(MERIDIAN_MENU_SCREEN *Screen, CHAR16 **InMainName,
                         CHAR16 *InTokenName OPTIONAL, BOOLEAN LogStartLine)
{
    CHAR16 *TmpName;
    CHAR16 *KernTag;
    CHAR16 *StrKern;

    KernTag = (GlobalConfig.FoldLinuxKernels) ? StrDuplicate(L" : Current Default Kernel") : NULL;

    StrKern = (KernTag != NULL) ? KernTag : L"";

    TmpName = (InTokenName != NULL) ? PoolPrint(L"%s%s", InTokenName, StrKern) : NULL;

    MRD_FREE_POOL(*InMainName);
    *InMainName = (InTokenName != NULL) ? CapitalisedCase(TmpName, TRUE)
                                        : StrDuplicate(L"Load Instance: Linux");

    if (LogStartLine) {
#if MERIDIAN_DEBUG > 0
        DEBUG_LOG(1, LOG_LINE_NORMAL, L"Append Menu Entry to %s  -  Boot with Default Options%s",
                  Screen->Title, StrKern);
#endif
    }

    MRD_FREE_POOL(TmpName);
    MRD_FREE_POOL(KernTag);
}

VOID VetCSR(VOID)
{
#if MERIDIAN_DEBUG > 0
    BOOLEAN CheckMute = FALSE;
#endif

    EFI_STATUS Status;
    UINTN CsrLength;
    UINT32 *ReturnValue;

#if MERIDIAN_DEBUG > 0
    MRD_MUTELOGGER_SET;
#endif

    Status = EfivarGetRaw(&AppleBootGuid, L"csr-active-config", (VOID **)&ReturnValue, &CsrLength);
    if (!EFI_ERROR(Status)) {

        EfivarSetRaw(&AppleBootGuid, L"csr-active-config", NULL, 0, TRUE);
    }
#if MERIDIAN_DEBUG > 0
    MRD_MUTELOGGER_OFF;
#endif
}

CHAR16 *GetShowName(IN CHAR16 *LinuxName)
{
    CHAR16 *ShowName;

    if (0)
        ;
    else if (MrdStrEqualsCI(LinuxName, L"LinuxMint"))
        ShowName = L"Mint";
    else if (MrdStrIncludesCI(LinuxName, L"Opensuse"))
        ShowName = L"OpenSUSE";
    else
        ShowName = LinuxName;

    return ShowName;
}

MERIDIAN_MENU_SCREEN *CopyMenuScreen(MERIDIAN_MENU_SCREEN *Entry)
{
    UINTN i;
    MERIDIAN_MENU_SCREEN *NewEntry;

    if (Entry == NULL) {
        return NULL;
    }

    NewEntry = AllocateZeroPool(sizeof(MERIDIAN_MENU_SCREEN));
    if (NewEntry == NULL) {
        return NULL;
    }

    NewEntry->Title = (Entry->Title == NULL) ? Entry->Title : StrDuplicate(Entry->Title);

    NewEntry->InfoLineCount = Entry->InfoLineCount;
    if (NewEntry->InfoLineCount > 0) {
        NewEntry->InfoLines = (CHAR16 **)AllocateZeroPool(Entry->InfoLineCount * sizeof(CHAR16 *));
        for (i = 0; i < Entry->InfoLineCount && NewEntry->InfoLines; i++) {
            NewEntry->InfoLines[i] = (Entry->InfoLines[i] == NULL)
                                         ? Entry->InfoLines[i]
                                         : StrDuplicate(Entry->InfoLines[i]);
        }
    }

    NewEntry->EntryCount = Entry->EntryCount;
    if (NewEntry->EntryCount > 0) {
        NewEntry->Entries = (MERIDIAN_MENU_ENTRY **)AllocateZeroPool(
            Entry->EntryCount * (sizeof(MERIDIAN_MENU_ENTRY *)));
        for (i = 0; i < Entry->EntryCount && NewEntry->Entries; i++) {
            AddMenuEntryCopy(NewEntry, Entry->Entries[i]);
        }
    }

    NewEntry->TimeoutSeconds = Entry->TimeoutSeconds;
    NewEntry->TimeoutText =
        (Entry->TimeoutText == NULL) ? Entry->TimeoutText : StrDuplicate(Entry->TimeoutText);

    NewEntry->Hint1 = (Entry->Hint1 == NULL) ? Entry->Hint1 : StrDuplicate(Entry->Hint1);
    NewEntry->Hint2 = (Entry->Hint2 == NULL) ? Entry->Hint2 : StrDuplicate(Entry->Hint2);

    return NewEntry;
}

MERIDIAN_MENU_ENTRY *CopyMenuEntry(MERIDIAN_MENU_ENTRY *Entry)
{
    MERIDIAN_MENU_ENTRY *NewEntry;

    if (Entry == NULL) {

        return NULL;
    }

    NewEntry = AllocateZeroPool(sizeof(MERIDIAN_MENU_ENTRY));
    if (NewEntry == NULL) {

        return NULL;
    }

    NewEntry->Tag = Entry->Tag;
    NewEntry->Row = Entry->Row;
    NewEntry->Title = (Entry->Title != NULL) ? StrDuplicate(Entry->Title) : NULL;
    NewEntry->SubScreen = (Entry->SubScreen != NULL) ? CopyMenuScreen(Entry->SubScreen) : NULL;

    return NewEntry;
}

LOADER_ENTRY *CopyLoaderEntry(IN LOADER_ENTRY *Entry) { return InitializeLoaderEntry(Entry); }

LOADER_ENTRY *InitializeLoaderEntry(IN LOADER_ENTRY *Entry)
{
    LOADER_ENTRY *NewEntry;

    NewEntry = AllocateZeroPool(sizeof(LOADER_ENTRY));
    if (NewEntry == NULL) {

        return NULL;
    }

    NewEntry->OSType = 0;
    NewEntry->Enabled = TRUE;
    NewEntry->EfiLoaderPath = NULL;
    NewEntry->me.Title = NULL;
    NewEntry->me.Tag = TAG_LOADER;

    if (Entry != NULL) {
        NewEntry->Volume = Entry->Volume;
    }

    if (Entry == NULL) {
        NewEntry->EfiBootNum = 0;
        NewEntry->UseGraphicsMode = FALSE;
        NewEntry->LoaderPath = NULL;
        NewEntry->InitrdPath = NULL;
        NewEntry->LoadOptions = NULL;
        NewEntry->EfiLoaderPath = NULL;
    }
    else {
        NewEntry->EfiBootNum = Entry->EfiBootNum;
        NewEntry->UseGraphicsMode = Entry->UseGraphicsMode;
        NewEntry->LoaderPath = (Entry->LoaderPath != NULL) ? StrDuplicate(Entry->LoaderPath) : NULL;
        NewEntry->InitrdPath = (Entry->InitrdPath != NULL) ? StrDuplicate(Entry->InitrdPath) : NULL;
        NewEntry->LoadOptions =
            (Entry->LoadOptions != NULL) ? StrDuplicate(Entry->LoadOptions) : NULL;
        NewEntry->EfiLoaderPath =
            (Entry->EfiLoaderPath != NULL) ? DuplicateDevicePath(Entry->EfiLoaderPath) : NULL;
    }

    return NewEntry;
}

MERIDIAN_MENU_SCREEN *InitializeSubScreen(IN LOADER_ENTRY *Entry)
{
#if MERIDIAN_DEBUG > 0
    BOOLEAN CheckMute = FALSE;
#endif

    UINTN i;
    CHAR16 *NameOS;
    CHAR16 *TmpStr;
    CHAR16 *TmpName;
    CHAR16 *FileName;
    CHAR16 *ShowName;
    CHAR16 *LinuxName;
    CHAR16 *SearchName;
    CHAR16 *DisplayName;
    CHAR16 *RawOptions;
    BOOLEAN CheckFlag;
    BOOLEAN Found;
    LOADER_ENTRY *SubEntry;
    MERIDIAN_MENU_SCREEN *SubScreen;

    if (Entry->me.SubScreen != NULL) {

        return CopyMenuScreen(Entry->me.SubScreen);
    }

    SubScreen = AllocateZeroPool(sizeof(MERIDIAN_MENU_SCREEN));
    if (SubScreen == NULL) {

        return NULL;
    }

    if (GlobalConfig.SyncAPFS && Entry->Volume->FSType == FS_TYPE_APFS &&
        Entry->Volume->VolRole == APFS_VOLUME_ROLE_PREBOOT) {
        DisplayName = GetVolumeGroupName(Entry->LoaderPath, Entry->Volume);
    }
    else {
        DisplayName = NULL;
    }

    FileName = (Entry->LoaderPath != NULL) ? Basename(Entry->LoaderPath) : NULL;
    TmpStr = (Entry->Title != NULL) ? Entry->Title : FileName;
    TmpName = (DisplayName != NULL) ? DisplayName : Entry->Volume->VolName;
    SubScreen->Title =
        PoolPrint(L"Boot Options for %s%s%s%s%s", TmpStr, SetVolJoin(TmpStr, TRUE),
                  SetVolKind(TmpStr, TmpName, Entry->Volume->FSType), SetVolFlag(TmpStr, TmpName),
                  SetVolType(TmpStr, TmpName, Entry->Volume->FSType));

    do {
#if MERIDIAN_DEBUG > 0
        DEBUG_LOG(1, LOG_THREE_STAR_MID, L"Build SubScreen:- '%s'", SubScreen->Title);
#endif

        SubEntry = CopyLoaderEntry(Entry);
        if (SubEntry == NULL) {
            break;
        }

        NameOS = NULL;

        if (0)
            ;
        else if (Entry->OSType == 'M') {
            NameOS = (IsInstallerMac(Entry->Volume)) ? L"Instance: Mac OS Installer"
                                                     : L"Instance: Mac OS";
        }
        else if (Entry->OSType == 'W') {
            if (MrdStrFind(SubScreen->Title, L"Legacy")) {
                NameOS = (MrdStrFind(SubScreen->Title, L"Legacy -"))
                             ? L"Instance: Windows (Legacy - NT/XP)"
                             : L"Instance: Windows (Legacy)";
            }
            else if (MrdStrFind(SubScreen->Title, L"UEFI")) {
                NameOS = L"Instance: Windows (UEFI)";
            }
            else {
                NameOS = L"Instance: Windows";
            }
        }
        else if (Entry->OSType == 'L' || Entry->OSType == 'E' || Entry->OSType == 'G' ||
                 Entry->OSType == 'S') {
            Found = FALSE;

            CheckFlag = (!MrdStrIncludesCI(SubScreen->Title, L"vmlinuz") &&
                         !MrdStrIncludesCI(SubScreen->Title, L"bzImage") &&
                         !MrdStrIncludesCI(SubScreen->Title, L"Manual Stanza:"));

            if (CheckFlag) {
                i = 0;
                while (!Found) {
                    LinuxName = FindCommaDelimited(MAIN_LINUX_DISTROS, i++);
                    if (LinuxName == NULL)
                        break;

                    ShowName = GetShowName(LinuxName);

                    SearchName = PoolPrint(L"- %s", ShowName);
                    if (MrdStrIncludesCI(SubScreen->Title, SearchName)) {
                        MRD_FREE_POOL(DisplayName);

                        if (Entry->OSType == 'L') {
                            DisplayName = PoolPrint(L"Instance: Linux - %s", ShowName);
                        }
                        else if (Entry->OSType == 'G') {
                            DisplayName = PoolPrint(L"Instance: Linux via Grub - %s", ShowName);
                        }
                        else if (Entry->OSType == 'S') {
                            DisplayName = PoolPrint(L"Instance: Linux via SDBoot - %s", ShowName);
                        }
                        else {
                            DisplayName = PoolPrint(L"Instance: Linux via Elilo - %s", ShowName);
                        }

                        NameOS = DisplayName;
                        Found = TRUE;
                    }

                    MRD_FREE_POOL(LinuxName);
                    MRD_FREE_POOL(SearchName);
                }
            }

            if (!Found) {
                if (0)
                    ;
                else if (Entry->OSType == 'L')
                    NameOS = L"Instance: Linux";
                else if (Entry->OSType == 'E')
                    NameOS = L"Instance: Elilo";
                else if (Entry->OSType == 'G')
                    NameOS = L"Instance: Grub";
                else if (Entry->OSType == 'S')
                    NameOS = L"Instance: SDBoot";
            }
        }
        else if (Entry->OSType == 'O')
            NameOS = L"Instance: OpenCore";
        else if (Entry->OSType == 'C')
            NameOS = L"Instance: Clover";
        else if (MrdStrFind(SubScreen->Title, L"Clover"))
            NameOS = L"Instance: Clover";
        else if (MrdStrFind(SubScreen->Title, L"OpenCore"))
            NameOS = L"Instance: OpenCore";

        if (NameOS == NULL) {
            NameOS = Entry->Title;
        }

        SubEntry->me.Title = PoolPrint(L"Load %s with Default Options", NameOS);

        if (SubEntry->InitrdPath != NULL) {
            RawOptions = SubEntry->LoadOptions;
            SubEntry->LoadOptions = AddInitrdToOptions(RawOptions, SubEntry->InitrdPath);
            MRD_FREE_POOL(RawOptions);
        }

#if MERIDIAN_DEBUG > 0
        if (Entry->OSType == 'L') {
            MRD_MUTELOGGER_SET;
        }
#endif
        AddMenuEntry(SubScreen, (MERIDIAN_MENU_ENTRY *)SubEntry);
#if MERIDIAN_DEBUG > 0
        if (Entry->OSType == 'L') {
            MRD_MUTELOGGER_OFF;
        }
#endif
    } while (0);

    SubScreen->Hint1 = StrDuplicate(SUBSCREEN_HINT1);
    SubScreen->Hint2 =
        StrDuplicate((GlobalConfig.HideUIFlags & HIDEUI_FLAG_EDITOR) ? SUBSCREEN_HINT2_NO_EDITOR
                                                                     : SUBSCREEN_HINT2);

    MRD_FREE_POOL(FileName);
    MRD_FREE_POOL(DisplayName);

    return SubScreen;
}

static VOID AddMacSubEntry(LOADER_ENTRY *Entry, MERIDIAN_MENU_SCREEN *SubScreen, CHAR16 *Title,
                           CHAR16 *Options, BOOLEAN UseGfx)
{
    LOADER_ENTRY *SubEntry = CopyLoaderEntry(Entry);
    if (SubEntry == NULL)
        return;
    SubEntry->me.Title = StrDuplicate(Title);
    SubEntry->LoadOptions = StrDuplicate(Options);
    SubEntry->UseGraphicsMode = UseGfx;
    AddMenuEntry(SubScreen, (MERIDIAN_MENU_ENTRY *)SubEntry);
}

VOID GenerateSubScreen(IN OUT LOADER_ENTRY *Entry, IN MERIDIAN_VOLUME *Volume,
                       IN BOOLEAN GenerateReturn)
{
    UINTN i;
    UINTN TokenCount;
    CHAR16 *InitrdName;
    CHAR16 *KernelVersion;
    CHAR16 **TokenList;
    BOOLEAN UseSysAPFS;
    MERIDIAN_FILE *File;
    LOADER_ENTRY *SubEntry;
    MERIDIAN_VOLUME *DiagVolume;
    MERIDIAN_MENU_SCREEN *SubScreen;

    LOG_SEP(L"X");
    LOG_INCREMENT();

    if (StrLen(Entry->Title) == 0) {
        MRD_FREE_POOL(Entry->Title);
    }

    SubScreen = InitializeSubScreen(Entry);

    if (SubScreen != NULL) {

        if (Entry->OSType == 'M') {
            LOG_SEP(L"X");

#if defined(EFIX64)
            AddMacSubEntry(Entry, SubScreen, L"Load Instance: Mac OS with a 64-bit Kernel",
                           L"arch=x86_64", GlobalConfig.GraphicsFor & GRAPHICS_FOR_OSX);
#endif

#if defined(EFIX64)
            AddMacSubEntry(Entry, SubScreen, L"Load Instance: Mac OS with a 32-bit Kernel",
                           L"arch=i386", GlobalConfig.GraphicsFor & GRAPHICS_FOR_OSX);
#endif

            AddMacSubEntry(Entry, SubScreen, L"Load Instance: Mac OS in Verbose Mode", L"-v",
                           FALSE);

#if defined(EFIX64)
            AddMacSubEntry(Entry, SubScreen, L"Load Instance: Mac OS in Verbose Mode (64-bit)",
                           L"-v arch=x86_64", FALSE);
#endif

#if defined(EFIX64)
            AddMacSubEntry(Entry, SubScreen, L"Load Instance: Mac OS in Verbose Mode (32-bit)",
                           L"-v arch=i386", FALSE);
#endif

            if (!(GlobalConfig.HideUIFlags & HIDEUI_FLAG_SAFEMODE)) {
                AddMacSubEntry(Entry, SubScreen, L"Load Instance: Mac OS in Safe Mode (Laconic)",
                               L"-x", FALSE);
                AddMacSubEntry(Entry, SubScreen, L"Load Instance: Mac OS in Safe Mode (Verbose)",
                               L"-v -x", FALSE);
            }

            if (!(GlobalConfig.HideUIFlags & HIDEUI_FLAG_SINGLEUSER)) {
                AddMacSubEntry(Entry, SubScreen,
                               L"Load Instance: Mac OS in SingleUser Mode (Laconic)", L"-s", FALSE);
                AddMacSubEntry(Entry, SubScreen,
                               L"Load Instance: Mac OS in SingleUser Mode (Verbose)", L"-v -s",
                               FALSE);
            }

            if (!(GlobalConfig.HideUIFlags & HIDEUI_FLAG_HWTEST)) {
                UseSysAPFS = FALSE;

                if (SingleAPFS) {
                    if (GlobalConfig.SyncAPFS && Volume->FSType == FS_TYPE_APFS &&
                        Volume->VolRole == APFS_VOLUME_ROLE_PREBOOT) {
                        for (i = 0; i < SystemVolumesCount; i++) {
                            if (GuidsAreEqual(&(SystemVolumes[i]->PartGuid), &(Volume->PartGuid))) {
                                UseSysAPFS = TRUE;
                                break;
                            }
                        }
                    }
                }

                DiagVolume = (UseSysAPFS) ? SystemVolumes[i] : Volume;
                if (FileExists(DiagVolume->RootDir, MACOSX_DIAGNOSTICS)) {
                    SubEntry = CopyLoaderEntry(Entry);
                    if (SubEntry != NULL) {
                        MRD_FREE_POOL(SubEntry->LoaderPath);
                        SubEntry->Volume = DiagVolume;
                        SubEntry->me.Title = StrDuplicate(L"Run Apple Hardware Test");
                        SubEntry->LoaderPath = StrDuplicate(MACOSX_DIAGNOSTICS);
                        SubEntry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_OSX;
                        AddMenuEntry(SubScreen, (MERIDIAN_MENU_ENTRY *)SubEntry);
                    }
                }
            }
        }
        else if (Entry->OSType == 'L') {
            LOG_SEP(L"X");
            File = ReadLinuxOptionsFile(Entry->LoaderPath, Volume);

            if (File != NULL) {
                KernelVersion = FindNumbers(Entry->LoaderPath);

                TokenCount = ReadTokenLine(File, &TokenList);

                if (TokenCount > 1) {

                    ReplaceSubstring(&(TokenList[1]), KERNEL_VERSION, KernelVersion);

                    if (SubScreen->Entries != NULL && SubScreen->Entries[0] != NULL) {
                        GetBaseEntry(SubScreen, &SubScreen->Entries[0]->Title, TokenList[0], FALSE);
                    }
                }

                FreeTokenLine(&TokenList, &TokenCount);

                InitrdName = FindInitrd(Entry->LoaderPath, Volume);

                i = 0;
                while (1) {
                    i += 1;

                    LOG_SEP(L"X");

                    TokenCount = ReadTokenLine(File, &TokenList);
                    if (TokenCount < 1) {
                        FreeTokenLine(&TokenList, &TokenCount);

                        break;
                    }

                    ReplaceSubstring(&(TokenList[1]), KERNEL_VERSION, KernelVersion);

                    SubEntry = CopyLoaderEntry(Entry);

                    if (SubEntry != NULL) {
                        GetBaseEntry(SubScreen, &SubEntry->me.Title, TokenList[0],
                                     (i == 1) ? TRUE : FALSE);

                        MRD_FREE_POOL(SubEntry->LoadOptions);

                        SubEntry->LoadOptions = AddInitrdToOptions(TokenList[1], InitrdName);

                        SubEntry->UseGraphicsMode = (GlobalConfig.GraphicsFor & GRAPHICS_FOR_LINUX);

                        AddMenuEntry(SubScreen, (MERIDIAN_MENU_ENTRY *)SubEntry);
                    }

                    FreeTokenLine(&TokenList, &TokenCount);

                    LOG_SEP(L"X");
                }

                MRD_FREE_POOL(KernelVersion);
                MRD_FREE_POOL(InitrdName);
                MRD_FREE_FILE(File);
            }

            if (GlobalConfig.ScanBtrfsSnapshots) {
                CHAR16 *BaseOptions = NULL;

                if (SubScreen->Entries != NULL && SubScreen->Entries[0] != NULL) {
                    BaseOptions = ((LOADER_ENTRY *)SubScreen->Entries[0])->LoadOptions;
                }

                AddBtrfsSnapshotSubEntries(Entry, SubScreen, BaseOptions);
            }
        }
        else if (Entry->OSType == 'E') {
            LOG_SEP(L"X");
            SubEntry = CopyLoaderEntry(Entry);
            if (SubEntry != NULL) {
                SubEntry->me.Title = StrDuplicate(L"Load Instance: ELILO in Interactive Mode");
                SubEntry->LoadOptions = StrDuplicate(L"-p");
                SubEntry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_ELILO;
                AddMenuEntry(SubScreen, (MERIDIAN_MENU_ENTRY *)SubEntry);
            }

            SubEntry = CopyLoaderEntry(Entry);
            if (SubEntry != NULL) {
                SubEntry->me.Title =
                    StrDuplicate(L"Load Instance: Linux for a 17\" iMac or a 15\" MacBook Pro (*)");
                SubEntry->LoadOptions = StrDuplicate(L"-d 0 i17");
                SubEntry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_ELILO;
                AddMenuEntry(SubScreen, (MERIDIAN_MENU_ENTRY *)SubEntry);
            }

            SubEntry = CopyLoaderEntry(Entry);
            if (SubEntry != NULL) {
                SubEntry->me.Title = StrDuplicate(L"Load Instance: Linux for a 20\" iMac (*)");
                SubEntry->LoadOptions = StrDuplicate(L"-d 0 i20");
                SubEntry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_ELILO;
                AddMenuEntry(SubScreen, (MERIDIAN_MENU_ENTRY *)SubEntry);
            }

            SubEntry = CopyLoaderEntry(Entry);
            if (SubEntry != NULL) {
                SubEntry->me.Title = StrDuplicate(L"Load Instance: Linux for a Mac Mini (*)");
                SubEntry->LoadOptions = StrDuplicate(L"-d 0 mini");
                SubEntry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_ELILO;
                AddMenuEntry(SubScreen, (MERIDIAN_MENU_ENTRY *)SubEntry);
            }

            AddMenuInfoLine(SubScreen, L"NOTE: This is an example", FALSE);
            AddMenuInfoLine(SubScreen, L"Entries marked with (*) may not work", FALSE);
        }
        else if (Entry->OSType == 'X') {
            LOG_SEP(L"X");

            Entry->LoadOptions = StrDuplicate(L"-s -h");

            AddMacSubEntry(Entry, SubScreen, L"Load Instance: Windows on Hard Disk", L"-s -h",
                           GlobalConfig.GraphicsFor & GRAPHICS_FOR_WINDOWS);
            AddMacSubEntry(Entry, SubScreen, L"Load Instance: Windows on Optical Disc", L"-s -c",
                           GlobalConfig.GraphicsFor & GRAPHICS_FOR_WINDOWS);
            AddMacSubEntry(Entry, SubScreen, L"Load Instance: XOM in Text Mode", L"-v",
                           GlobalConfig.GraphicsFor & GRAPHICS_FOR_WINDOWS);
        }

        do {
            LOG_SEP(L"X");
            if (GenerateReturn) {
                if (!GetMenuEntryReturn(&SubScreen)) {
                    FreeMenuScreen(&SubScreen);

                    break;
                }
            }
            Entry->me.SubScreen = SubScreen;
        } while (0);

        LOG_DECREMENT();
        LOG_SEP(L"X");
    }
}

VOID SetLoaderDefaults(IN LOADER_ENTRY *Entry, IN CHAR16 *LoaderPath, IN MERIDIAN_VOLUME *Volume)
{
    UINTN i;
    CHAR16 *PathOnly;
    CHAR16 *NameClues;
    CHAR16 *VentoyName;
    BOOLEAN FoundVentoy;

#if MERIDIAN_DEBUG > 0
    DEBUG_LOG(1, LOG_LINE_NORMAL, L"Set Loader Defaults");
#endif

    LOG_SEP(L"X");
    LOG_INCREMENT();

    if (LoaderPath == NULL) {
        NameClues = NULL;
        PathOnly = NULL;
    }
    else {
        NameClues = Basename(LoaderPath);
        PathOnly = FindPath(LoaderPath);

        if (Entry->OSType == 'L' && MrdStrIncludesCI(NameClues, L"grub")) {

            Entry->OSType = 'G';
        }
        else if (Entry->OSType == 'M') {
            if (MrdStrIncludesCI(NameClues, L"opencore")) {

                Entry->OSType = 'O';
                HasOpenCore = TRUE;
            }
            else {

                HasMacOS = TRUE;
            }
        }
    }

    FoundVentoy = FALSE;
    if (AllowGraphicsMode) {
        if (Volume->DiskKind == DISK_KIND_NET) {

            MergeStrings(&NameClues, Entry->me.Title, L' ');
        }
        else if (GlobalConfig.HandleVentoy) {

            CHAR16 *Probe;

            Probe = NULL;
            if (Volume->FsName != NULL && Volume->FsName[0] != L'\0' &&
                !(GlobalConfig.SyncAPFS && MrdStrIncludesCI(Volume->FsName, L"PreBoot"))) {
                Probe = Volume->FsName;
            }
            else if (Volume->VolName != NULL && Volume->VolName[0] != L'\0') {
                Probe = Volume->VolName;
            }

            i = 0;
            while (Probe != NULL && !FoundVentoy) {
                VentoyName = FindCommaDelimited(VENTOY_NAMES, i++);
                if (VentoyName == NULL) {
                    break;
                }
                if (MrdStrStartsWithCI(VentoyName, Probe)) {
                    FoundVentoy = TRUE;
                }
                MRD_FREE_POOL(VentoyName);
            }

            if (!FoundVentoy && Volume->PartName != NULL && Volume->PartName[0] != L'\0') {
                i = 0;
                while (!FoundVentoy) {
                    VentoyName = FindCommaDelimited(VENTOY_NAMES, i++);
                    if (VentoyName == NULL) {
                        break;
                    }
                    if (MrdStrStartsWithCI(VentoyName, Volume->PartName)) {
                        FoundVentoy = TRUE;
                    }
                    MRD_FREE_POOL(VentoyName);
                }
            }
        }
    }

    if (!FoundVentoy && GlobalConfig.ScanLimine && MrdStrEndsWithCI(L".elf", LoaderPath) &&
        IsLimineExecutable(Volume, LoaderPath)) {
        Entry->BootProtocol = MERIDIAN_PROTO_LIMINE;
        Entry->OSType = 'I';

        if (!Entry->UseGraphicsMode) {
            Entry->UseGraphicsMode = (GlobalConfig.GraphicsFor & GRAPHICS_FOR_LINUX) != 0;
        }
    }

    else if (!FoundVentoy) {
        if (MrdStrIncludesCI(LoaderPath, MACOSX_LOADER_PATH)) {
            Entry->OSType = 'M';
            if (!Entry->UseGraphicsMode) {
                Entry->UseGraphicsMode = (GlobalConfig.GraphicsFor & GRAPHICS_FOR_OSX);
            }
        }
        else if (MrdStrEqualsCI(NameClues, L"bootmgfw.efi") ||
                 MrdStrEqualsCI(NameClues, L"bootmgr.efi") ||
                 MrdStrEqualsCI(NameClues, L"cdboot.efi") ||
                 MrdStrEqualsCI(NameClues, L"bkpbootmgfw.efi")) {
            Entry->OSType = 'W';
            if (!Entry->UseGraphicsMode) {
                Entry->UseGraphicsMode = (GlobalConfig.GraphicsFor & GRAPHICS_FOR_WINDOWS);
            }
        }
        else if (MrdStrIncludesCI(NameClues, L"grub")) {
            Entry->OSType = 'G';
            if (!Entry->UseGraphicsMode) {
                Entry->UseGraphicsMode = (GlobalConfig.GraphicsFor & GRAPHICS_FOR_GRUB);
            }
        }
        else if (MrdStrIncludesCI(NameClues, L"SystemD") ||
                 MrdStrIncludesCI(NameClues, L"gummiboot")) {
            Entry->OSType = 'S';
            if (!Entry->UseGraphicsMode) {
                Entry->UseGraphicsMode = (GlobalConfig.GraphicsFor & GRAPHICS_FOR_SYSTEMD);
            }
        }
        else if (IsListItemSubstringIn(NameClues, GlobalConfig.LinuxPrefixes)) {
            if (Volume->DiskKind != DISK_KIND_NET) {

                MRD_FREE_POOL(Entry->LoadOptions);
                Entry->LoadOptions = GetMainLinuxOptions(LoaderPath, Volume);
            }
            Entry->OSType = 'L';
            if (!Entry->UseGraphicsMode) {
                Entry->UseGraphicsMode = (GlobalConfig.GraphicsFor & GRAPHICS_FOR_LINUX);
            }
        }
        else if (MrdStrEqualsCI(NameClues, L"opencore.efi")) {
            Entry->OSType = 'O';
            HasOpenCore = TRUE;
            if (!Entry->UseGraphicsMode) {

                Entry->UseGraphicsMode = (GlobalConfig.GraphicsFor & GRAPHICS_FOR_OPENCORE) != 0;
            }
        }
        else if (MrdStrEqualsCI(NameClues, L"clover.efi")) {
            Entry->OSType = 'C';
            if (!Entry->UseGraphicsMode) {
                Entry->UseGraphicsMode = (GlobalConfig.GraphicsFor & GRAPHICS_FOR_CLOVER);
            }
        }
        else if (IsBsdRootLoaderPath(LoaderPath) ||
                 MrdStrIncludesCI(LoaderPath, L"\\FreeBSD\\") ||
                 MrdStrIncludesCI(LoaderPath, L"\\ghostbsd\\") ||
                 MrdStrIncludesCI(LoaderPath, L"\\NetBSD\\") ||
                 MrdStrIncludesCI(LoaderPath, L"\\OpenBSD\\")) {

            Entry->OSType = 'B';
        }
        else if (MrdStrIncludesCI(LoaderPath, L"\\DragonFly\\")) {
            Entry->OSType = 'B';

            if (Entry->LoadOptions == NULL) {
                Entry->LoadOptions = DragonFlyCurrdevOption();
            }
        }
        else if (MrdStrIncludesCI(LoaderPath, L"\\HAIKU\\")) {
            Entry->OSType = 'H';
        }
        else if (MrdStrIncludesCI(LoaderPath, L"\\9front\\")) {
            Entry->OSType = 'P';
        }
        else if (MrdStrEqualsCI(NameClues, L"e.efi") || MrdStrEqualsCI(NameClues, L"elilo.efi") ||
                 MrdStrIncludesCI(NameClues, L"elilo")) {
            Entry->OSType = 'E';
            if (!Entry->UseGraphicsMode) {
                Entry->UseGraphicsMode = (GlobalConfig.GraphicsFor & GRAPHICS_FOR_ELILO);
            }
        }
        else if (MrdStrEqualsCI(NameClues, L"xom.efi")) {
            Entry->OSType = 'X';
            if (!Entry->UseGraphicsMode) {
                Entry->UseGraphicsMode = (GlobalConfig.GraphicsFor & GRAPHICS_FOR_WINDOWS);
            }
        }
        else if (MrdStrIncludesCI(NameClues, L"ipxe")) {
            Entry->OSType = 'N';
        }

        if (!Entry->UseGraphicsMode &&
            (Entry->OSType == 'B' || Entry->OSType == 'P' || Entry->OSType == 'H')) {

            Entry->UseGraphicsMode = (GlobalConfig.GraphicsFor & GRAPHICS_FOR_BSD) != 0;
        }
    }

    if (Entry->OSType == 'L' || Entry->OSType == 'E' || Entry->OSType == 'G' ||
        Entry->OSType == 'S') {
        if (GlobalConfig.ToolLocationsExtra == NULL) {
            GlobalConfig.ToolLocationsExtra = StrDuplicate(PathOnly);
        }
        else {
            MergeUniqueStrings(&GlobalConfig.ToolLocationsExtra, PathOnly, L',');
        }
    }

    MRD_FREE_POOL(PathOnly);
    MRD_FREE_POOL(NameClues);

    LOG_DECREMENT();
    LOG_SEP(L"X");
}

LOADER_ENTRY *AddEfiLoaderEntry(IN EFI_DEVICE_PATH_PROTOCOL *EfiLoaderPath, IN CHAR16 *LoaderTitle,
                                IN UINT16 EfiBootNum, IN UINTN Row, IN UINTN TypeTag)
{
    CHAR16 *TempStr;
    CHAR16 *FullTitle;
    LOADER_ENTRY *MenuEntry;

    MenuEntry = InitializeLoaderEntry(NULL);
    if (MenuEntry == NULL) {
        return NULL;
    }

    FullTitle = (LoaderTitle == NULL) ? NULL : PoolPrint(L"Reboot to %s", LoaderTitle);

    MenuEntry->DiscoveryType = DISCOVERY_TYPE_AUTO;
    MenuEntry->me.Title = StrDuplicate((FullTitle != NULL) ? FullTitle : L"Instance: Unknown");
    MenuEntry->Title = StrDuplicate((LoaderTitle != NULL) ? LoaderTitle : L"Instance: Unknown");
    MenuEntry->EfiLoaderPath = DuplicateDevicePath(EfiLoaderPath);
    TempStr = DevicePathToStr(EfiLoaderPath);

#if MERIDIAN_DEBUG > 0
    if (TypeTag != TAG_SHELL) {
        DEBUG_LOG(1, LOG_LINE_NORMAL, L"Append uEFI Loader Entry:- '%s'", MenuEntry->Title);
        DEBUG_LOG(1, LOG_THREE_STAR_MID, L"uEFI Loader Path:- '%s'", TempStr);
    }
#endif

    MRD_FREE_POOL(TempStr);

    MenuEntry->EfiBootNum = EfiBootNum;

    MenuEntry->Volume = NULL;
    MenuEntry->LoaderPath = NULL;
    MenuEntry->LoadOptions = NULL;
    MenuEntry->InitrdPath = NULL;
    MenuEntry->Enabled = TRUE;

    switch (TypeTag) {
    case TAG_SHELL:

        MenuEntry->me.Row = ShellEntryItemsCount;

        MenuEntry->me.Tag = TAG_BASE;
        AddListElement((VOID ***)&ShellEntryItems, &ShellEntryItemsCount, MenuEntry);
        break;
    default:

        MenuEntry->me.Row = Row;

        MenuEntry->me.Tag = TAG_FIRMWARE_LOADER;

        AddMenuEntry(MainMenu, (MERIDIAN_MENU_ENTRY *)MenuEntry);
    }

    return MenuEntry;
}

LOADER_ENTRY *AddLoaderEntry(IN OUT CHAR16 *LoaderPath, IN CHAR16 *LoaderTitle,
                             IN MERIDIAN_VOLUME *Volume, IN BOOLEAN SubScreenReturn,
                             IN BOOLEAN CheckLinux, IN CHAR16 *OverrideOptions OPTIONAL)
{
    UINTN i;
    CHAR16 *KernFile;
    CHAR16 *NameClues;
    CHAR16 *DisplayName;
    CHAR16 *SearchName;
    CHAR16 *LinuxName;
    CHAR16 *ShowName;
    CHAR16 *TmpName;
    CHAR16 *BsdEspName;
    BOOLEAN Found;
    BOOLEAN IsStub;
    BOOLEAN GotSysD;
    BOOLEAN GotGrub;
    BOOLEAN GotElilo;
    BOOLEAN GotPreBoot;
    LOADER_ENTRY *LoaderEntry;

    if (!VolumeScanAllowed(Volume, TRUE, FALSE)) {
        return NULL;
    }

    LoaderEntry = InitializeLoaderEntry(NULL);
    if (LoaderEntry == NULL) {

        return NULL;
    }

    CleanUpPathNameSlashes(LoaderPath);

    ShowName = NULL;

    if (LoaderTitle != NULL) {
        LoaderEntry->Title = StrDuplicate(
            (MrdStrEqualsCI(LoaderTitle, FALLBACK_BASENAME)) ? L"UEFI Fallback File" : LoaderTitle);
    }
    else {
        Found = FALSE;
        if (CheckLinux) {
            GotGrub = (MrdStrIncludesCI(LoaderPath, L"Grub")) ? TRUE : FALSE;

            GotSysD = (MrdStrIncludesCI(LoaderPath, L"SystemD") ||
                       MrdStrIncludesCI(LoaderPath, L"GummiBoot"))
                          ? TRUE
                          : FALSE;

            GotElilo = (MrdStrIncludesCI(LoaderPath, L"Elilo")) ? TRUE : FALSE;

            IsStub = FALSE;
            if (!GotGrub && !GotSysD && !GotElilo) {
                i = 0;
                while (!IsStub) {
                    SearchName = FindCommaDelimited(GlobalConfig.LinuxPrefixes, i++);
                    if (SearchName == NULL)
                        break;

                    if (MrdStrIncludesCI(LoaderPath, SearchName)) {
                        IsStub = TRUE;
                        LoaderEntry->OSType = 'L';
                    }
                    MRD_FREE_POOL(SearchName);
                }
            }

            i = 0;
            while (!Found) {
                LinuxName = FindCommaDelimited(MAIN_LINUX_DISTROS, i++);
                if (LinuxName == NULL)
                    break;

                if (!IsStub) {
                    SearchName = PoolPrint(L"\\%s", LinuxName);
                    if (MrdStrIncludesCI(LoaderPath, SearchName)) {
                        Found = TRUE;
                        ShowName = GetShowName(LinuxName);

                        LoaderEntry->Title =
                            (!GotGrub && !GotSysD && !GotElilo)
                                ? PoolPrint(L"Instance: Linux - %s", ShowName)
                            : (GotGrub) ? PoolPrint(L"Instance: Linux via Grub - %s", ShowName)
                            : (GotSysD) ? PoolPrint(L"Instance: Linux via SDBoot - %s", ShowName)
                                        : PoolPrint(L"Instance: Linux via Elilo - %s", ShowName);
                    }

                    MRD_FREE_POOL(SearchName);
                }

                KernFile = NULL;
                TmpName = L" *!@!* FIND BUG *!@!*";

                if (!Found) {
                    if (LoaderPath != NULL) {
                        MRD_FREE_POOL(LinuxName);
                        GuessLinuxDistribution(&LinuxName, Volume, LoaderPath, TRUE);
                    }

                    if (LinuxName != NULL) {
                        if (!GlobalConfig.FoldLinuxKernels) {
                            KernFile = Basename(LoaderPath);
                            if (!MrdStrFind(KernFile, L".")) {
                                TmpName = KernFile;
                            }
                            else {
                                NameClues = StripSetExtension(L".signed", KernFile);

                                MRD_FREE_POOL(KernFile);
                                KernFile = StripEfiExtension(NameClues);

                                TmpName = StripSetExtension(L".signed", KernFile);

                                MRD_FREE_POOL(NameClues);
                                MRD_FREE_POOL(KernFile);
                                KernFile = TmpName;
                            }

                            i = 0;
                            Found = FALSE;
                            while (!Found) {
                                SearchName = FindCommaDelimited(GlobalConfig.LinuxPrefixes, i++);
                                if (SearchName == NULL)
                                    break;

                                NameClues = PoolPrint(L"%s-", SearchName);
                                if (MrdStrStartsWithCI(NameClues, KernFile)) {
                                    Found = TRUE;

                                    TmpName = GetSubStrAfter(NameClues, KernFile);
                                    if (!MrdStrFind(TmpName, L"-")) {
                                        TmpName = KernFile;
                                    }

                                    if (MrdStrStartsWithCI(L"linux-", TmpName)) {
                                        MRD_FREE_POOL(NameClues);
                                        NameClues = GetSubStrAfter(L"linux-", KernFile);
                                        if (MrdStrFind(NameClues, L"-")) {
                                            TmpName = NameClues;
                                        }

                                        MRD_SOFT_FREE(NameClues);
                                    }
                                }

                                MRD_FREE_POOL(NameClues);
                                MRD_FREE_POOL(SearchName);
                            }
                        }

                        Found = TRUE;
                        ShowName = GetShowName(LinuxName);

                        LoaderEntry->Title =
                            (IsStub) ? (KernFile == NULL)
                                           ? PoolPrint(L"Instance: Linux via Stub - %s", ShowName)
                                           : PoolPrint(L"Instance: Linux via Stub - %s ::: %s",
                                                       ShowName, TmpName)
                            : (!GotGrub && !GotSysD && !GotElilo)
                                ? PoolPrint(L"Instance: Linux - %s", ShowName)
                            : (GotGrub) ? PoolPrint(L"Instance: Linux via Grub - %s", ShowName)
                            : (GotSysD) ? PoolPrint(L"Instance: Linux via SDBoot - %s", ShowName)
                                        : PoolPrint(L"Instance: Linux via Elilo - %s", ShowName);
                    }
                }

                MRD_FREE_POOL(KernFile);
                MRD_FREE_POOL(LinuxName);
                MRD_FREE_POOL(SearchName);
            }
        }

        if (!Found) {
            if (LoaderEntry->OSType == 'L') {
                LoaderEntry->Title = PoolPrint(L"Instance: Linux - %s", LoaderPath);
            }
            else {
                NameClues = Basename(LoaderPath);

                BsdEspName = BsdEspLoaderTitle(LoaderPath);
                if (BsdEspName != NULL) {
                    LoaderEntry->OSType = 'B';
                    LoaderEntry->Title = StrDuplicate(BsdEspName);
                }
                else if (MrdStrEqualsCI(NameClues, FALLBACK_BASENAME)) {

                    MRD_FREE_POOL(NameClues);
                    NameClues = FindLastDirName(LoaderPath);

                    LoaderEntry->Title =
                        PoolPrint(L"%s File in '%s' Dir", FALLBACK_BASENAME, NameClues);
                }
                else if (MrdStrEqualsCI(NameClues, L"bootmgfw.efi")) {
                    LoaderEntry->OSType = 'W';
                    LoaderEntry->Title = PoolPrint(L"Instance: Windows (UEFI) - %s", LoaderPath);
                }
                else if (MrdStrIncludesCI(NameClues, L"Grub")) {
                    LoaderEntry->OSType = 'G';
                    LoaderEntry->Title = PoolPrint(L"Instance: Grub - %s", LoaderPath);
                }
                else if (MrdStrIncludesCI(NameClues, L"SystemD")) {
                    LoaderEntry->OSType = 'S';
                    LoaderEntry->Title = PoolPrint(L"Instance: SDBoot - %s", LoaderPath);
                }
                else if (MrdStrEqualsCI(NameClues, L"OpenCore.efi")) {
                    LoaderEntry->OSType = 'O';
                    HasOpenCore = TRUE;
                    LoaderEntry->Title = PoolPrint(L"Instance: OpenCore - %s", LoaderPath);
                }
                else if (MrdStrEqualsCI(NameClues, L"Clover.efi")) {
                    LoaderEntry->OSType = 'C';
                    LoaderEntry->Title = PoolPrint(L"Instance: Clover - %s", LoaderPath);
                }
                else {
                    LoaderEntry->Title = StrDuplicate(LoaderPath);
                }
                MRD_FREE_POOL(NameClues);
            }
        }
    }

    DisplayName = NULL;

    if (Volume->FSType == FS_TYPE_APFS && GlobalConfig.SyncAPFS) {
        if (Volume->VolRole == APFS_VOLUME_ROLE_PREBOOT) {
            DisplayName = GetVolumeGroupName(LoaderPath, Volume);
            if (DisplayName == NULL) {
                return NULL;
            }

            GotPreBoot = MrdStrIncludesCI(DisplayName, L"PreBoot");
            if (GotPreBoot) {
                MRD_FREE_POOL(DisplayName);
            }
            else {
                GotPreBoot = MrdStrIncludesCI(DisplayName, L"PreBoot");
                if (GotPreBoot) {
                    MRD_FREE_POOL(DisplayName);
                }
                else {
                    if (!SingleAPFS && IsListItem(DisplayName, GlobalConfig.DontScanVolumes)) {
                        MRD_FREE_POOL(DisplayName);

                        return NULL;
                    }

                    MRD_FREE_POOL(Volume->VolName);
                    Volume->VolName = PoolPrint(L"PreBoot - %s", DisplayName);
                }
            }
        }
    }

#if MERIDIAN_DEBUG > 0
    if (DisplayName != NULL) {
        DEBUG_LOG(1, LOG_THREE_STAR_MID, L"Synced PreBoot:- '%s'", DisplayName);
    }
    DEBUG_LOG(1, LOG_LINE_NORMAL, L"Add Loader Entry:- '%s'", LoaderEntry->Title);
    DEBUG_LOG(1, LOG_LINE_NORMAL, L"uEFI Loader File:- '%s'", LoaderPath);
#endif

    TmpName = (DisplayName != NULL) ? DisplayName : Volume->VolName;
    LoaderEntry->me.Title =
        (DisplayName != NULL || Volume->VolName != NULL)
            ? BuildLoaderTitle(LoaderEntry->Title, TmpName, Volume->FSType)
            : PoolPrint(L"Load %s", (LoaderTitle != NULL) ? LoaderTitle : LoaderPath);

    LoaderEntry->me.Row = 0;
    LoaderEntry->DiscoveryType = DISCOVERY_TYPE_AUTO;
    LoaderEntry->LoaderPath =
        (LoaderPath != NULL && (LoaderPath[0] != L'\\')) ? StrDuplicate(L"\\") : NULL;

    MergeUniqueStrings(&(LoaderEntry->LoaderPath), LoaderPath, 0);

    LoaderEntry->Volume = Volume;
    SetLoaderDefaults(LoaderEntry, LoaderPath, Volume);

    if (OverrideOptions != NULL) {

        MRD_FREE_POOL(LoaderEntry->LoadOptions);
        LoaderEntry->LoadOptions = StrDuplicate(OverrideOptions);
    }

    GenerateSubScreen(LoaderEntry, Volume, SubScreenReturn);
    AddMenuEntry(MainMenu, (MERIDIAN_MENU_ENTRY *)LoaderEntry);

#if MERIDIAN_DEBUG > 0
    TmpName = (DisplayName != NULL)       ? DisplayName
              : (Volume->VolName != NULL) ? Volume->VolName
                                          : LoaderEntry->LoaderPath;
    INFO_LOG("%s  - Found %s%s%s%s%s", OffsetNext, LoaderEntry->Title,
             SetVolJoin(LoaderEntry->Title, FALSE),
             SetVolKind(LoaderEntry->Title, TmpName, Volume->FSType),
             SetVolFlag(LoaderEntry->Title, TmpName),
             SetVolType(LoaderEntry->Title, TmpName, Volume->FSType));

    DEBUG_LOG(1, LOG_THREE_STAR_END, L"Successfully Created Menu Entry for %s", LoaderEntry->Title);
#endif

    MRD_FREE_POOL(DisplayName);

    return LoaderEntry;
}
