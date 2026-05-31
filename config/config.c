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

#define LAST_MINUTE (1439)

UINTN TotalEntryCount = 0;
UINTN ValidEntryCount = 0;

BOOLEAN OuterLoop = TRUE;
BOOLEAN SetShowTools = FALSE;
BOOLEAN ManualInclude = FALSE;
BOOLEAN ForceTextOnly = FALSE;
BOOLEAN BaseRescanDXE = FALSE;
BOOLEAN UserDefinedRez = FALSE;

#if MERIDIAN_DEBUG > 0
static BOOLEAN LogUpdate(CHAR16 *TokenName, BOOLEAN MuteFlagOn, BOOLEAN UseTypeReset)
{
    if (MuteFlagOn)
        MuteLogger = FALSE;
    if (UseTypeReset) {
        INFO_LOG("%s  - Reset:- '%s'", OffsetNext, TokenName);
    }
    else {
        INFO_LOG("%s ** Avoid:- '%s'", OffsetNext, TokenName);
    }
    if (MuteFlagOn)
        MuteLogger = TRUE;

    return TRUE;
}
#endif

static VOID SetDefaultByTime(IN CHAR16 **TokenList, OUT CHAR16 **Default)
{
    EFI_STATUS Status;
    UINTN StartTime;
    UINTN EndTime;
    UINTN Now;
    CHAR16 *MsgStr;
    EFI_TIME CurrentTime;
    BOOLEAN SetIt;

    StartTime = HandleTime(TokenList[2]);
    EndTime = HandleTime(TokenList[3]);

    if (StartTime <= LAST_MINUTE && EndTime <= LAST_MINUTE) {
        Status = gRT->GetTime(&CurrentTime, NULL);
        if (EFI_ERROR(Status)) {
            return;
        }

        Now = CurrentTime.Hour * 60 + CurrentTime.Minute;
        if (Now > LAST_MINUTE) {

            MsgStr = PoolPrint(L"ERROR: Impossible System Time:- %d:%d", CurrentTime.Hour,
                               CurrentTime.Minute);

#if MERIDIAN_DEBUG > 0
            INFO_LOG("  - %s", MsgStr);
            INFO_LOG("\n");
#endif

            Print(L"%s\n", MsgStr);
            MRD_FREE_POOL(MsgStr);

            return;
        }

        SetIt = FALSE;
        if (StartTime < EndTime) {

            if (Now >= StartTime && Now <= EndTime) {
                SetIt = TRUE;
            }
        }
        else {

            if (Now >= StartTime || Now <= EndTime) {
                SetIt = TRUE;
            }
        }

        if (SetIt) {
            MRD_FREE_POOL(*Default);
            *Default = StrDuplicate(TokenList[1]);
        }
    }
}

static
#if MERIDIAN_DEBUG < 1
    VOID ExitOuter(VOID)
{
#else
    VOID ExitOuter(BOOLEAN ValidInclude, BOOLEAN NotRunBefore)
{
    EFI_STATUS Status;
#endif

    if (GlobalConfig.DontScanVolumes == NULL) {
        GlobalConfig.DontScanVolumes = StrDuplicate(DONT_SCAN_VOLUMES);
    }
    if (GlobalConfig.WindowsRecoveryFiles == NULL) {
        GlobalConfig.WindowsRecoveryFiles = StrDuplicate(WINDOWS_RECOVERY_FILES);
    }
    if (GlobalConfig.MacOSRecoveryFiles == NULL) {
        GlobalConfig.MacOSRecoveryFiles = StrDuplicate(MACOS_RECOVERY_FILES);
    }
    if (GlobalConfig.DefaultSelection == NULL) {
        GlobalConfig.DefaultSelection = StrDuplicate(L"+");
    }

    SyncShowTools();
    SyncToolPaths();
    SyncAlsoScan();
    SyncDontScanDirs();
    SyncDontScanFiles();
    SyncLinuxPrefixes();

    if (AppleFirmware) {

        GlobalConfig.RescanDXE = BaseRescanDXE;
    }
    else {
        GlobalConfig.SetAppleFB = FALSE;
    }

    if (GlobalConfig.SyncTrust != ENFORCE_TRUST_NONE) {
        GlobalConfig.DirectBoot = FALSE;
        GlobalConfig.Timeout = 0;
    }

#if MERIDIAN_DEBUG > 0
    if (NotRunBefore)
        MuteLogger = FALSE;
#endif

#if MERIDIAN_DEBUG > 0
    INFO_LOG("\n");
    Status = (ValidInclude) ? EFI_SUCCESS : EFI_WARN_STALE_DATA;
    INFO_LOG("Process Configuration Options ... %r", Status);
    INFO_LOG("\n\n");
#endif

    BaseRescanDXE = FALSE;
}

static VOID BadFlag(CHAR16 *Flag, CHAR16 *TokenName, BOOLEAN NotRunBefore)
{
    CHAR16 *MsgStr;

    MsgStr = PoolPrint(L"WARN: Invalid '%s' Token:- '%s'", TokenName, Flag);

#if MERIDIAN_DEBUG > 0
    if (NotRunBefore)
        MuteLogger = FALSE;
    INFO_LOG("%s  - %s", OffsetNext, MsgStr);
    if (NotRunBefore)
        MuteLogger = TRUE;
#endif

    SwitchToText(FALSE);
    PrintUglyText(MsgStr, NEXTLINE);
    PauseForKey();
    MRD_FREE_POOL(MsgStr);
}

static CHAR8 *MrdSmbiosString(IN SMBIOS_STRUCTURE *Hdr, IN UINTN Index)
{
    CHAR8 *Ptr;
    UINTN i;

    if (Index == 0) {
        return NULL;
    }
    Ptr = (CHAR8 *)Hdr + Hdr->Length;
    for (i = 1; i < Index; i++) {
        while (*Ptr != '\0') {
            Ptr++;
        }
        Ptr++;
        if (*Ptr == '\0') {
            return NULL;
        }
    }
    return Ptr;
}

static CHAR8 *ReadSmbiosConfigPayload(VOID)
{
    EFI_STATUS Status;
    EFI_SMBIOS_PROTOCOL *Smbios;
    EFI_SMBIOS_HANDLE Handle;
    EFI_SMBIOS_TABLE_HEADER *Record;
    CHAR8 *Conf;
    BOOLEAN Found;
    CONST CHAR8 *Prefix = "meridian:config:";
    UINTN PrefixLen = AsciiStrLen(Prefix);

    Status = gBS->LocateProtocol(&gEfiSmbiosProtocolGuid, NULL, (VOID **)&Smbios);
    if (EFI_ERROR(Status)) {
        return NULL;
    }

    Conf = AllocateZeroPool(4096);
    if (Conf == NULL) {
        return NULL;
    }
    Found = FALSE;

    Handle = SMBIOS_HANDLE_PI_RESERVED;
    while (!EFI_ERROR(Smbios->GetNext(Smbios, &Handle, NULL, &Record, NULL))) {
        SMBIOS_TABLE_TYPE11 *T11;
        UINTN n;

        if (Record->Type != SMBIOS_TYPE_OEM_STRINGS) {
            continue;
        }
        T11 = (SMBIOS_TABLE_TYPE11 *)Record;
        for (n = 1; n <= T11->StringCount; n++) {
            CHAR8 *Str = MrdSmbiosString(&T11->Hdr, n);
            if (Str == NULL || *Str == '\0') {
                continue;
            }
            if (AsciiStrnCmp(Str, Prefix, PrefixLen) == 0) {
                CHAR8 *Line = Str + PrefixLen;
                if (AsciiStrLen(Conf) + AsciiStrLen(Line) + 2 < 4096) {
                    AsciiStrCatS(Conf, 4096, Line);
                    AsciiStrCatS(Conf, 4096, "\n");
                    Found = TRUE;
                }
            }
        }
    }

    if (!Found) {
        MRD_FREE_POOL(Conf);
        return NULL;
    }
    return Conf;
}

VOID ApplySmbiosConfig(VOID)
{
    CHAR8 *Payload = ReadSmbiosConfigPayload();

    if (Payload == NULL) {
        return;
    }

#if MERIDIAN_DEBUG > 0
    INFO_LOG("\n");
    INFO_LOG("INFO: Applying SMBIOS-Embedded Config (%d bytes):\n%a", (UINTN)AsciiStrLen(Payload),
             Payload);
#endif

    MrdSaveFile(SelfDir, L"smbios.conf", (UINT8 *)Payload, AsciiStrLen(Payload));
    MRD_FREE_POOL(Payload);
    ReadConfig(L"smbios.conf");
}

static VOID ConfAppendList(IN OUT CHAR8 *Buf, IN UINTN BufSize, IN CONST CHAR8 *Key,
                           IN CHAR16 *Value)
{
    CHAR8 Line[512];

    if (Value == NULL || Value[0] == L'\0') {
        return;
    }
    AsciiSPrint(Line, sizeof(Line), "%a %s\n", Key, Value);
    AsciiStrCatS(Buf, BufSize, Line);
}

VOID BootstrapMissingConfig(VOID)
{
    CHAR8 *Conf;

    if (SelfDir == NULL) {
        return;
    }

    if (GlobalConfig.ConfigFilename == NULL ||
        !MrdStrEqualsCI(GlobalConfig.ConfigFilename, L"config.conf")) {
        return;
    }
    if (FileExists(SelfDir, L"config.conf")) {
        return;
    }

    Conf = AllocateZeroPool(4096);
    if (Conf == NULL) {
        return;
    }
    AsciiSPrint(Conf, 4096,
                "# Meridian configuration (config.conf)\n"
                "#\n"
                "# Auto-generated from Meridian's effective defaults on first boot, as no\n"
                "# config.conf was found. Edit freely; only regenerated if\n"
                "# this file is removed. See the documentation for all available tokens.\n"
                "\n"
                "timeout %d\n"
                "scan_delay %d\n",
                GlobalConfig.Timeout, GlobalConfig.ScanDelay);

    ConfAppendList(Conf, 4096, "linux_prefixes", GlobalConfig.LinuxPrefixes);
    ConfAppendList(Conf, 4096, "also_scan_dirs", GlobalConfig.AlsoScan);
    ConfAppendList(Conf, 4096, "dont_scan_dirs", GlobalConfig.DontScanDirs);
    ConfAppendList(Conf, 4096, "dont_scan_files", GlobalConfig.DontScanFiles);
    ConfAppendList(Conf, 4096, "dont_scan_volumes", GlobalConfig.DontScanVolumes);

    AsciiStrCatS(Conf, 4096,
                 "\n"
                 "# Other available options -- uncomment and edit to override defaults:\n"
                 "#\n"
                 "# default_selection \"Linux\"     # entry to pre-select / auto-boot\n"
                 "# textonly                        # force the text menu (no graphics)\n"
                 "# resolution         1920 1080    # exact mode, or: resolution max\n"
                 "# scanfor            internal,external,optical,manual\n"
                 "# also_scan_dirs     boot\n"
                 "# dont_scan_dirs     EFI/Meridian/tools\n"
                 "# dont_scan_files    shim.efi,PreLoader.efi\n"
                 "# dont_scan_volumes  Recovery\n"
                 "# linux_prefixes     vmlinuz,bzImage,kernel\n"
                 "# showtools          shell,memtest,gdisk,reboot,shutdown,firmware\n");

    MrdSaveFile(SelfDir, L"config.conf", (UINT8 *)Conf, AsciiStrLen(Conf));
#if MERIDIAN_DEBUG > 0
    INFO_LOG("INFO: Generated 'config.conf' from effective defaults\n\n");
#endif
    MRD_FREE_POOL(Conf);
}

VOID ReadConfig(CHAR16 *FileName)
{
#if MERIDIAN_DEBUG > 0
    BOOLEAN UpdatedToken;

    static BOOLEAN ValidInclude = TRUE;
    static BOOLEAN FirstInclude = TRUE;
#endif

    EFI_STATUS Status;
    MERIDIAN_FILE *File;
    BOOLEAN DoneTool;
    BOOLEAN DoneManual;
    BOOLEAN CheckManual;
    BOOLEAN GotHideuiAll;
    BOOLEAN GotNoneHideui;
    BOOLEAN OutLoopHideui;
    BOOLEAN GotSyncTrustAll;
    BOOLEAN GotNoneSyncTrust;
    BOOLEAN OutLoopSyncTrust;
    BOOLEAN GotNoBootLogoAll;
    BOOLEAN GotNoneNoBootLogo;
    BOOLEAN OutLoopNoBootLogo;
    BOOLEAN GotGraphicsForAll;
    BOOLEAN GotNoneGraphicsFor;
    BOOLEAN OutLoopGraphicsFor;
    BOOLEAN DeclineSetting;
    CHAR16 **TokenList;
    CHAR16 *Flag;
    UINTN i, j;
    UINTN TokenCount;
    UINTN InvalidEntries;

    static UINTN ReadLoops = 0;
    static BOOLEAN NotRunBefore = TRUE;

#if MERIDIAN_DEBUG < 1
#define UPDATE_SOME_VARIABLES(CurrentReadLoop, NewNotRunBefore, NewValidInclude, NewFirstInclude)  \
    do {                                                                                           \
        if (!OuterLoop) {                                                                          \
            ReadLoops = (CurrentReadLoop) - 1;                                                     \
        }                                                                                          \
        else {                                                                                     \
            ExitOuter();                                                                           \
            ReadLoops = 0;                                                                         \
        }                                                                                          \
    } while (0)
#else
#define RESET_MISC(NewNotRunBefore, NewValidInclude, NewFirstInclude)                              \
    do {                                                                                           \
        NotRunBefore = (NewNotRunBefore);                                                          \
        ValidInclude = (NewValidInclude);                                                          \
        FirstInclude = (NewFirstInclude);                                                          \
    } while (0)
#define UPDATE_SOME_VARIABLES(CurrentReadLoop, NewNotRunBefore, NewValidInclude, NewFirstInclude)  \
    do {                                                                                           \
        if (!OuterLoop) {                                                                          \
            ReadLoops = (CurrentReadLoop) - 1;                                                     \
        }                                                                                          \
        else {                                                                                     \
            INFO_LOG(" ... Use Default Settings ***");                                             \
            INFO_LOG("\n\n");                                                                      \
            ExitOuter(ValidInclude, NotRunBefore);                                                 \
            ReadLoops = 0;                                                                         \
            RESET_MISC((NewNotRunBefore), (NewValidInclude), (NewFirstInclude));                   \
        }                                                                                          \
    } while (0)
#endif

    if (ReadLoops > 1) {
        ReadLoops = ReadLoops - 1;

#if MERIDIAN_DEBUG > 0
        if (NotRunBefore)
            MuteLogger = FALSE;
        INFO_LOG("%s  ** Ignore Tertiary Config ... %s", OffsetNext, FileName);
        ValidInclude = FALSE;

#endif

        return;
    }
    ReadLoops = ReadLoops + 1;

#if MERIDIAN_DEBUG > 0
    if (NotRunBefore)
        MuteLogger = FALSE;
    if (!OuterLoop) {
        UpdatedToken = FALSE;
    }
    else {
        INFO_LOG("R E A D   C O N F I G   T O K E N S");
    }
    if (NotRunBefore)
        MuteLogger = TRUE;
#endif

    if (OuterLoop) {
        GlobalConfig.GraphicsFor = GRAPHICS_FOR_EVERYTHING;
    }

    if (!FileExists(SelfDir, FileName)) {
#if MERIDIAN_DEBUG > 0
        ValidInclude = FALSE;
        if (NotRunBefore)
            MuteLogger = FALSE;
        INFO_LOG("%s", OffsetNext);
        if (!OuterLoop) {
            INFO_LOG("  - ");
            Flag = L"";
        }
        else {
            INFO_LOG("*** ");
            Flag = L"Configuration";
        }
        INFO_LOG("WARN: %sFile *NOT* Found", Flag);
#endif

        UPDATE_SOME_VARIABLES(ReadLoops, FALSE, TRUE, TRUE);

        return;
    }

    File = AllocateZeroPool(sizeof(MERIDIAN_FILE));
    if (File == NULL) {
        return;
    }

    Status = MeridianReadFile(SelfDir, FileName, File, &i);
    if (EFI_ERROR(Status)) {
#if MERIDIAN_DEBUG > 0
        if (NotRunBefore)
            MuteLogger = FALSE;
        INFO_LOG("%s", OffsetNext);
        if (!OuterLoop) {
            ValidInclude = FALSE;
            INFO_LOG("  - ");
        }
        else {
            INFO_LOG("*** ");
        }
        INFO_LOG("WARN: Invalid Configuration File ... Abort File Load", OffsetNext);
#endif

        UPDATE_SOME_VARIABLES(ReadLoops, FALSE, TRUE, TRUE);

        return;
    }

    CheckManual = DoneManual = FALSE;
    GotNoneHideui = OutLoopHideui = FALSE;
    GotNoneSyncTrust = OutLoopSyncTrust = FALSE;
    GotNoneNoBootLogo = OutLoopNoBootLogo = FALSE;
    GotNoneGraphicsFor = OutLoopGraphicsFor = FALSE;
#if MERIDIAN_DEBUG > 0
    if (!OuterLoop) {
        CheckManual = TRUE;
    }
#endif

    while (1) {
        TokenCount = ReadTokenLine(File, &TokenList);
        if (TokenCount == 0) {
            FreeTokenLine(&TokenList, &TokenCount);

            break;
        }

        if (MrdStrEqualsCI(TokenList[0], L"timeout")) {
#if MERIDIAN_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate(TokenList[0], NotRunBefore, TRUE);
            }
#endif

            HandleSignedInt(TokenList, TokenCount, &(GlobalConfig.Timeout));

            GlobalConfig.DirectBoot = (GlobalConfig.Timeout < 0) ? TRUE : FALSE;
        }
        else if (!GotNoneHideui && MrdStrEqualsCI(TokenList[0], L"hideui")) {
            if (!OuterLoop && !OutLoopHideui) {
#if MERIDIAN_DEBUG > 0
                UpdatedToken = LogUpdate(TokenList[0], NotRunBefore, TRUE);
#endif

                OutLoopHideui = TRUE;
                GlobalConfig.HideUIFlags = HIDEUI_FLAG_NONE;
            }

            GotHideuiAll = FALSE;

            for (i = 1; i < TokenCount; i++) {
                Flag = TokenList[i];
                if (MrdStrEqualsCI(Flag, L"none")) {

                    GotNoneHideui = TRUE;
                    GlobalConfig.HideUIFlags = HIDEUI_FLAG_NONE;
                    break;
                }
                else if (!GotHideuiAll) {

                    if (MrdStrEqualsCI(Flag, L"all")) {
                        GotHideuiAll = TRUE;
                        GlobalConfig.HideUIFlags = HIDEUI_FLAG_ALL;
                    }
                    else {
                        if (MrdStrEqualsCI(Flag, L"label"))
                            GlobalConfig.HideUIFlags |= HIDEUI_FLAG_LABEL;
                        else if (MrdStrEqualsCI(Flag, L"hints"))
                            GlobalConfig.HideUIFlags |= HIDEUI_FLAG_HINTS;
                        else if (MrdStrEqualsCI(Flag, L"banner"))
                            GlobalConfig.HideUIFlags |= HIDEUI_FLAG_BANNER;
                        else if (MrdStrEqualsCI(Flag, L"hwtest"))
                            GlobalConfig.HideUIFlags |= HIDEUI_FLAG_HWTEST;
                        else if (MrdStrEqualsCI(Flag, L"arrows"))
                            GlobalConfig.HideUIFlags |= HIDEUI_FLAG_ARROWS;
                        else if (MrdStrEqualsCI(Flag, L"editor"))
                            GlobalConfig.HideUIFlags |= HIDEUI_FLAG_EDITOR;
                        else if (MrdStrEqualsCI(Flag, L"safemode"))
                            GlobalConfig.HideUIFlags |= HIDEUI_FLAG_SAFEMODE;
                        else if (MrdStrEqualsCI(Flag, L"singleuser"))
                            GlobalConfig.HideUIFlags |= HIDEUI_FLAG_SINGLEUSER;
                        else
                            BadFlag(Flag, TokenList[0], NotRunBefore);
                    }
                }
            }
        }
        else if (!GotNoneGraphicsFor && MrdStrEqualsCI(TokenList[0], L"use_graphics_for")) {
            if (!OutLoopGraphicsFor) {

                GlobalConfig.GraphicsFor = GRAPHICS_FOR_NONE;

                if (!OuterLoop) {
                    OutLoopGraphicsFor = TRUE;

#if MERIDIAN_DEBUG > 0
                    UpdatedToken = LogUpdate(TokenList[0], NotRunBefore, TRUE);
#endif
                }
            }

            if (TokenCount == 2 || (TokenCount > 2 && !MrdStrEqualsCI(TokenList[1], L"+"))) {
                GlobalConfig.GraphicsFor = GRAPHICS_FOR_NONE;
            }

            if (TokenCount > 1) {
                GotGraphicsForAll = FALSE;
            }
            else {
                GotGraphicsForAll = TRUE;
                GlobalConfig.GraphicsFor = GRAPHICS_FOR_EVERYTHING;
            }

            for (i = 1; i < TokenCount; i++) {
                Flag = TokenList[i];
                if (MrdStrEqualsCI(Flag, L"none")) {

                    GotNoneGraphicsFor = TRUE;
                    GlobalConfig.GraphicsFor = GRAPHICS_FOR_NONE;
                    break;
                }

                if (!GotGraphicsForAll) {

                    if (MrdStrEqualsCI(Flag, L"everything")) {
                        GotGraphicsForAll = TRUE;
                        GlobalConfig.GraphicsFor = GRAPHICS_FOR_EVERYTHING;
                    }
                    else {
                        if (0)
                            ;
                        else if (MrdStrEqualsCI(Flag, L"osx"))
                            GlobalConfig.GraphicsFor |= GRAPHICS_FOR_OSX;
                        else if (MrdStrEqualsCI(Flag, L"grub"))
                            GlobalConfig.GraphicsFor |= GRAPHICS_FOR_GRUB;
                        else if (MrdStrEqualsCI(Flag, L"tools"))
                            GlobalConfig.GraphicsFor |= GRAPHICS_FOR_TOOLS;
                        else if (MrdStrEqualsCI(Flag, L"linux"))
                            GlobalConfig.GraphicsFor |= GRAPHICS_FOR_LINUX;
                        else if (MrdStrEqualsCI(Flag, L"elilo"))
                            GlobalConfig.GraphicsFor |= GRAPHICS_FOR_ELILO;
                        else if (MrdStrEqualsCI(Flag, L"clover"))
                            GlobalConfig.GraphicsFor |= GRAPHICS_FOR_CLOVER;
                        else if (MrdStrEqualsCI(Flag, L"systemd"))
                            GlobalConfig.GraphicsFor |= GRAPHICS_FOR_SYSTEMD;
                        else if (MrdStrEqualsCI(Flag, L"windows"))
                            GlobalConfig.GraphicsFor |= GRAPHICS_FOR_WINDOWS;
                        else if (MrdStrEqualsCI(Flag, L"opencore"))
                            GlobalConfig.GraphicsFor |= GRAPHICS_FOR_OPENCORE;
                        else if (MrdStrEqualsCI(Flag, L"bsd"))
                            GlobalConfig.GraphicsFor |= GRAPHICS_FOR_BSD;
                    }
                }
            }
        }
        else if (!GotNoneSyncTrust && MrdStrEqualsCI(TokenList[0], L"sync_trust")) {
#if MERIDIAN_DEBUG > 0
            if (!OuterLoop && !OutLoopSyncTrust) {
                UpdatedToken = LogUpdate(TokenList[0], NotRunBefore, TRUE);
            }
#endif

            GotSyncTrustAll = FALSE;
            if (!OuterLoop && !OutLoopSyncTrust) {

                OutLoopSyncTrust = TRUE;
                GlobalConfig.SyncTrust = ENFORCE_TRUST_NONE;
            }

            for (i = 1; i < TokenCount; i++) {
                Flag = TokenList[i];
                if (MrdStrEqualsCI(Flag, L"none")) {

                    GotNoneSyncTrust = TRUE;
                    GlobalConfig.SyncTrust = ENFORCE_TRUST_NONE;
                    break;
                }

                if (!GotSyncTrustAll) {

                    if (MrdStrEqualsCI(Flag, L"every")) {
                        GotSyncTrustAll = TRUE;
                        GlobalConfig.SyncTrust = ENFORCE_TRUST_EVERY;
                    }
                    else {
                        if (0)
                            ;
                        else if (MrdStrEqualsCI(Flag, L"macos"))
                            GlobalConfig.SyncTrust |= ENFORCE_TRUST_MACOS;
                        else if (MrdStrEqualsCI(Flag, L"linux"))
                            GlobalConfig.SyncTrust |= ENFORCE_TRUST_LINUX;
                        else if (MrdStrEqualsCI(Flag, L"windows"))
                            GlobalConfig.SyncTrust |= ENFORCE_TRUST_WINDOWS;
                        else if (MrdStrEqualsCI(Flag, L"opencore"))
                            GlobalConfig.SyncTrust |= ENFORCE_TRUST_OPENCORE;
                        else if (MrdStrEqualsCI(Flag, L"clover"))
                            GlobalConfig.SyncTrust |= ENFORCE_TRUST_CLOVER;
                        else if (MrdStrEqualsCI(Flag, L"similar"))
                            GlobalConfig.SyncTrust |= ENFORCE_TRUST_OTHERS;
                        else if (MrdStrEqualsCI(Flag, L"verify"))
                            GlobalConfig.SyncTrust |= REQUIRE_TRUST_VERIFY;
                        else
                            BadFlag(Flag, TokenList[0], NotRunBefore);
                    }
                }
            }
        }
        else if (MrdStrEqualsCI(TokenList[0], L"scanfor")) {
#if MERIDIAN_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate(TokenList[0], NotRunBefore, TRUE);
            }
#endif

            for (i = 0; i < NUM_SCAN_OPTIONS; i++) {
                GlobalConfig.ScanFor[i] = (i < TokenCount) ? TokenList[i][0] : ' ';
            }
        }
        else if (MrdStrEqualsCI(TokenList[0], L"also_scan_dirs")) {
#if MERIDIAN_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate(TokenList[0], NotRunBefore, TRUE);
            }
#endif

            HandleStrings(TokenList, TokenCount, &(GlobalConfig.AlsoScan));
        }
        else if (MrdStrEqualsCI(TokenList[0], L"also_scan_tool_dirs")) {
#if MERIDIAN_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate(TokenList[0], NotRunBefore, TRUE);
            }
#endif

            HandleStrings(TokenList, TokenCount, &(GlobalConfig.ToolLocationsExtra));
        }
        else if (MrdStrEqualsCI(TokenList[0], L"dont_scan_volumes")) {
#if MERIDIAN_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate(TokenList[0], NotRunBefore, TRUE);
            }
#endif

            MRD_FREE_POOL(GlobalConfig.DontScanVolumes);
            for (i = 1; i < TokenCount; i++) {
                MergeStrings(&GlobalConfig.DontScanVolumes, TokenList[i], L',');
            }
        }
        else if (MrdStrEqualsCI(TokenList[0], L"dont_scan_files")) {
#if MERIDIAN_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate(TokenList[0], NotRunBefore, TRUE);
            }
#endif

            HandleStrings(TokenList, TokenCount, &(GlobalConfig.DontScanFiles));
        }
        else if (MrdStrEqualsCI(TokenList[0], L"dont_scan_dirs")) {
#if MERIDIAN_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate(TokenList[0], NotRunBefore, TRUE);
            }
#endif

            HandleStrings(TokenList, TokenCount, &(GlobalConfig.DontScanDirs));
        }
        else if (MrdStrEqualsCI(TokenList[0], L"dont_scan_tools")) {
#if MERIDIAN_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate(TokenList[0], NotRunBefore, TRUE);
            }
#endif

            HandleStrings(TokenList, TokenCount, &(GlobalConfig.DontScanTools));
        }
        else if (MrdStrEqualsCI(TokenList[0], L"dont_scan_firmware")) {
#if MERIDIAN_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate(TokenList[0], NotRunBefore, TRUE);
            }
#endif

            HandleStrings(TokenList, TokenCount, &(GlobalConfig.DontScanFirmware));
        }
        else if (MrdStrEqualsCI(TokenList[0], L"disable_rescan_dxe")) {
#if MERIDIAN_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate(TokenList[0], NotRunBefore, TRUE);
            }
#endif

            DeclineSetting = HandleBoolean(TokenList, TokenCount);
            GlobalConfig.RescanDXE = (DeclineSetting) ? FALSE : TRUE;

            if (AppleFirmware) {

                BaseRescanDXE = GlobalConfig.RescanDXE;
            }
        }
        else if (TokenCount == 2 && MrdStrEqualsCI(TokenList[0], L"sync_nvram")) {
#if MERIDIAN_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate(TokenList[0], NotRunBefore, TRUE);
            }
#endif

            HandleUnsignedInt(TokenList, TokenCount, &(GlobalConfig.SyncNVram));
        }
        else if (MrdStrEqualsCI(TokenList[0], L"scan_driver_dirs")) {
#if MERIDIAN_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate(TokenList[0], NotRunBefore, TRUE);
            }
#endif

            HandleStrings(TokenList, TokenCount, &(GlobalConfig.DriverDirs));
        }
        else if (MrdStrEqualsCI(TokenList[0], L"showtools")) {
#if MERIDIAN_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate(TokenList[0], NotRunBefore, TRUE);
            }
#endif

            if (SetShowTools) {

                for (j = 0; j < NUM_TOOLS; j++) {
                    GlobalConfig.ShowTools[j] = TAG_BASE;
                }
            }

            DoneTool = FALSE;
            InvalidEntries = 0;
            i = j = 0;
            while (1) {

                ++i;

                if (i >= TokenCount || i >= (NUM_TOOLS + InvalidEntries)) {

                    break;
                }

                j = (DoneTool) ? j + 1 : 0;

                Flag = TokenList[i];
                if (0)
                    ;
                else if (MrdStrStartsWithCI(L"mok_tool", Flag))
                    GlobalConfig.ShowTools[j] = TAG_MOK;
                else if (MrdStrStartsWithCI(L"exit", Flag))
                    GlobalConfig.ShowTools[j] = TAG_EXIT;
                else if (MrdStrStartsWithCI(L"shell", Flag))
                    GlobalConfig.ShowTools[j] = TAG_SHELL;
                else if (MrdStrStartsWithCI(L"gdisk", Flag))
                    GlobalConfig.ShowTools[j] = TAG_GDISK;
                else if (MrdStrStartsWithCI(L"reboot", Flag))
                    GlobalConfig.ShowTools[j] = TAG_REBOOT;
                else if (MrdStrStartsWithCI(L"gptsync", Flag))
                    GlobalConfig.ShowTools[j] = TAG_GPTSYNC;
                else if (MrdStrStartsWithCI(L"memtest", Flag))
                    GlobalConfig.ShowTools[j] = TAG_MEMTEST;
                else if (MrdStrStartsWithCI(L"cydia", Flag))
                    GlobalConfig.ShowTools[j] = TAG_CYDIA;
                else if (MrdStrStartsWithCI(L"install", Flag))
                    GlobalConfig.ShowTools[j] = TAG_INSTALL;
                else if (MrdStrStartsWithCI(L"netboot", Flag))
                    GlobalConfig.ShowTools[j] = TAG_NETBOOT;
                else if (MrdStrStartsWithCI(L"shutdown", Flag))
                    GlobalConfig.ShowTools[j] = TAG_SHUTDOWN;
                else if (MrdStrStartsWithCI(L"firmware", Flag))
                    GlobalConfig.ShowTools[j] = TAG_FIRMWARE;
                else if (MrdStrStartsWithCI(L"fwupdate", Flag))
                    GlobalConfig.ShowTools[j] = TAG_FWUPDATE;
                else if (MrdStrStartsWithCI(L"bootorder", Flag))
                    GlobalConfig.ShowTools[j] = TAG_BOOTORDER;
                else if (MrdStrStartsWithCI(L"csr_rotate", Flag))
                    GlobalConfig.ShowTools[j] = TAG_CSR_ROTATE;
                else if (MrdStrStartsWithCI(L"clean_nvram", Flag))
                    GlobalConfig.ShowTools[j] = TAG_CLEAN_NVRAM;
                else if (MrdStrStartsWithCI(L"windows_recovery", Flag))
                    GlobalConfig.ShowTools[j] = TAG_RECOVERY_WIN;
                else if (MrdStrStartsWithCI(L"apple_recovery", Flag))
                    GlobalConfig.ShowTools[j] = TAG_RECOVERY_MAC;
                else {
#if MERIDIAN_DEBUG > 0
                    if (NotRunBefore)
                        MuteLogger = FALSE;
                    DEBUG_LOG(1, LOG_THREE_STAR_MID,
                              L"Invalid Config Entry in 'showtools' List:- '%s'!!", Flag);
                    if (NotRunBefore)
                        MuteLogger = TRUE;
#endif

                    j = (DoneTool) ? j - 1 : 0;

                    InvalidEntries += 1;

                    continue;
                }

                if (!DoneTool) {
                    DoneTool = TRUE;
                }

                if (!SetShowTools) {
                    SetShowTools = TRUE;
                }
            }
        }
        else if (MrdStrEqualsCI(TokenList[0], L"default_selection")) {
#if MERIDIAN_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate(TokenList[0], NotRunBefore, TRUE);
            }
#endif

            MRD_FREE_POOL(GlobalConfig.DefaultSelection);
            if (TokenCount == 4) {
                SetDefaultByTime(TokenList, &(GlobalConfig.DefaultSelection));
            }
            else {

                UINTN DefIdx;
                for (DefIdx = 1; DefIdx < TokenCount; DefIdx++) {
                    MergeStrings(&(GlobalConfig.DefaultSelection), TokenList[DefIdx], L',');
                }
            }
        }
        else if ((TokenCount == 2 || TokenCount == 3) &&
                 MrdStrEqualsCI(TokenList[0], L"resolution")) {
#if MERIDIAN_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate(TokenList[0], NotRunBefore, TRUE);
            }
#endif

            if (MrdStrEqualsCI(TokenList[1], L"max")) {

                UserDefinedRez = FALSE;

                GlobalConfig.RequestedScreenWidth = 0;
                GlobalConfig.RequestedScreenHeight = 0;
            }
            else {
                UserDefinedRez = TRUE;
                GlobalConfig.RequestedScreenWidth = Atoi(TokenList[1]);
                GlobalConfig.RequestedScreenHeight = (TokenCount == 3) ? Atoi(TokenList[2]) : 0;
            }
        }
        else if (MrdStrEqualsCI(TokenList[0], L"screensaver")) {
#if MERIDIAN_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate(TokenList[0], NotRunBefore, TRUE);
            }
#endif

            HandleSignedInt(TokenList, TokenCount, &(GlobalConfig.ScreensaverTime));
        }
        else if (MrdStrEqualsCI(TokenList[0], L"scan_limine")) {
#if MERIDIAN_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate(TokenList[0], NotRunBefore, TRUE);
            }
#endif

            GlobalConfig.ScanLimine = HandleBoolean(TokenList, TokenCount);
        }
        else if (MrdStrEqualsCI(TokenList[0], L"scan_btrfs_snapshots")) {
#if MERIDIAN_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate(TokenList[0], NotRunBefore, TRUE);
            }
#endif

            GlobalConfig.ScanBtrfsSnapshots = HandleBoolean(TokenList, TokenCount);
        }
        else if (MrdStrEqualsCI(TokenList[0], L"network_probe")) {
#if MERIDIAN_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate(TokenList[0], NotRunBefore, TRUE);
            }
#endif

            GlobalConfig.NetworkProbe = HandleBoolean(TokenList, TokenCount);
        }
        else if (MrdStrEqualsCI(TokenList[0], L"network_dhcp")) {
#if MERIDIAN_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate(TokenList[0], NotRunBefore, TRUE);
            }
#endif

            GlobalConfig.NetworkDhcp = HandleBoolean(TokenList, TokenCount);
        }
        else if (MrdStrEqualsCI(TokenList[0], L"textonly")) {
#if MERIDIAN_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate(TokenList[0], NotRunBefore, TRUE);
            }
#endif

            GlobalConfig.TextOnly = HandleBoolean(TokenList, TokenCount);
        }
        else if (MrdStrEqualsCI(TokenList[0], L"serial")) {

#if MERIDIAN_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate(TokenList[0], NotRunBefore, TRUE);
            }
#endif

            GlobalConfig.LogToSerial = HandleBoolean(TokenList, TokenCount);
        }
        else if (MrdStrEqualsCI(TokenList[0], L"measured_boot")) {

#if MERIDIAN_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate(TokenList[0], NotRunBefore, TRUE);
            }
#endif

            GlobalConfig.MeasuredBoot = HandleBoolean(TokenList, TokenCount);
        }
        else if (MrdStrEqualsCI(TokenList[0], L"enable_menu_cache")) {
#if MERIDIAN_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate(TokenList[0], NotRunBefore, TRUE);
            }
#endif

            GlobalConfig.MenuCache = HandleBoolean(TokenList, TokenCount);
        }
        else if (MrdStrEqualsCI(TokenList[0], L"textmode")) {
#if MERIDIAN_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate(TokenList[0], NotRunBefore, TRUE);
            }
#endif

            HandleUnsignedInt(TokenList, TokenCount, &(GlobalConfig.RequestedTextMode));
        }
        else if (MrdStrEqualsCI(TokenList[0], L"scan_all_linux_kernels")) {
#if MERIDIAN_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate(TokenList[0], NotRunBefore, TRUE);
            }
#endif

            GlobalConfig.ScanAllLinux = HandleBoolean(TokenList, TokenCount);
        }
        else if (MrdStrEqualsCI(TokenList[0], L"fold_linux_kernels")) {
#if MERIDIAN_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate(TokenList[0], NotRunBefore, TRUE);
            }
#endif

            GlobalConfig.FoldLinuxKernels = HandleBoolean(TokenList, TokenCount);
        }
        else if (MrdStrEqualsCI(TokenList[0], L"linux_prefixes")) {
#if MERIDIAN_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate(TokenList[0], NotRunBefore, TRUE);
            }
#endif

            HandleStrings(TokenList, TokenCount, &(GlobalConfig.LinuxPrefixes));
        }
        else if (MrdStrEqualsCI(TokenList[0], L"csr_values")) {
#if MERIDIAN_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate(TokenList[0], NotRunBefore, TRUE);
            }
#endif

            HandleHexes(TokenList, TokenCount, CSR_MAX_LEGAL_VALUE, &(GlobalConfig.CsrValues));
        }
        else if (TokenCount == 4 && MrdStrEqualsCI(TokenList[0], L"screen_rgb")) {
#if MERIDIAN_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate(TokenList[0], NotRunBefore, TRUE);
            }
#endif

            GlobalConfig.ScreenR = Atoi(TokenList[1]);
            GlobalConfig.ScreenG = Atoi(TokenList[2]);
            GlobalConfig.ScreenB = Atoi(TokenList[3]);

            GlobalConfig.CustomScreenBG =
                (GlobalConfig.ScreenR >= 0 && GlobalConfig.ScreenR <= 255 &&
                 GlobalConfig.ScreenG >= 0 && GlobalConfig.ScreenG <= 255 &&
                 GlobalConfig.ScreenB >= 0 && GlobalConfig.ScreenB <= 255);
        }
        else if (MrdStrEqualsCI(TokenList[0], L"persist_boot_args")) {
#if MERIDIAN_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate(TokenList[0], NotRunBefore, TRUE);
            }
#endif

            GlobalConfig.PersistBootArgs = HandleBoolean(TokenList, TokenCount);
        }
        else if (MrdStrEqualsCI(TokenList[0], L"transient_boot")) {
#if MERIDIAN_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate(TokenList[0], NotRunBefore, TRUE);
            }
#endif

            GlobalConfig.TransientBoot = HandleBoolean(TokenList, TokenCount);
        }
        else if (MrdStrEqualsCI(TokenList[0], L"disable_reload_gop")) {
#if MERIDIAN_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate(TokenList[0], NotRunBefore, TRUE);
            }
#endif

            DeclineSetting = HandleBoolean(TokenList, TokenCount);
            GlobalConfig.ReloadGOP = (DeclineSetting) ? FALSE : TRUE;
        }
        else if (MrdStrEqualsCI(TokenList[0], L"disable_apfs_load")) {
#if MERIDIAN_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate(TokenList[0], NotRunBefore, TRUE);
            }
#endif

            DeclineSetting = HandleBoolean(TokenList, TokenCount);
            GlobalConfig.SupplyAPFS = (DeclineSetting) ? FALSE : TRUE;
        }
        else if (MrdStrEqualsCI(TokenList[0], L"disable_apfs_sync")) {
#if MERIDIAN_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate(TokenList[0], NotRunBefore, TRUE);
            }
#endif

            DeclineSetting = HandleBoolean(TokenList, TokenCount);
            GlobalConfig.SyncAPFS = (DeclineSetting) ? FALSE : TRUE;
        }
        else if (MrdStrEqualsCI(TokenList[0], L"disable_set_applefb")) {
#if MERIDIAN_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken =
                    LogUpdate(TokenList[0], NotRunBefore, (AppleFirmware) ? TRUE : FALSE);
            }
#endif

            if (AppleFirmware) {
                DeclineSetting = HandleBoolean(TokenList, TokenCount);
                GlobalConfig.SetAppleFB = (DeclineSetting) ? FALSE : TRUE;
            }
        }
        else if (MrdStrEqualsCI(TokenList[0], L"follow_symlinks")) {
#if MERIDIAN_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate(TokenList[0], NotRunBefore, TRUE);
            }
#endif

            MRD_FREE_POOL(GlobalConfig.FollowSymlinks);
            if (TokenCount == 1) {

                GlobalConfig.FollowSymlinks = StrDuplicate(SYM_TAG_ALL);
            }
            else {

                if (!MrdStrEqualsCI(TokenList[1], L"false") &&
                    !MrdStrEqualsCI(TokenList[1], L"off") && !MrdStrEqualsCI(TokenList[1], L"0")) {
                    GlobalConfig.FollowSymlinks =
                        StrDuplicate((TokenCount == 2) ? SYM_TAG_ALL : TokenList[2]);
                }
                else {
                    GlobalConfig.FollowSymlinks =
                        (TokenCount == 2) ? StrDuplicate(SYM_TAG_OFF)
                                          : PoolPrint(L"%s,%s", SYM_TAG_OFF, TokenList[2]);
                }
            }
        }
        else if (MrdStrEqualsCI(TokenList[0], L"csr_normalise")) {
#if MERIDIAN_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate(TokenList[0], NotRunBefore, TRUE);
            }
#endif

            GlobalConfig.NormaliseCSR = HandleBoolean(TokenList, TokenCount);
        }
        else if (MrdStrEqualsCI(TokenList[0], L"csr_dynamic")) {
#if MERIDIAN_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate(TokenList[0], NotRunBefore, TRUE);
            }
#endif

            HandleSignedInt(TokenList, TokenCount, &(GlobalConfig.DynamicCSR));
        }
        else if (MrdStrEqualsCI(TokenList[0], L"disable_nvram_paniclog")) {
#if MERIDIAN_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate(TokenList[0], NotRunBefore, TRUE);
            }
#endif

            GlobalConfig.DisableNvramPanicLog = HandleBoolean(TokenList, TokenCount);
        }
        else if (MrdStrEqualsCI(TokenList[0], L"disable_check_compat")) {
#if MERIDIAN_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate(TokenList[0], NotRunBefore, TRUE);
            }
#endif

            GlobalConfig.DisableCheckCompat = HandleBoolean(TokenList, TokenCount);
        }
        else if (MrdStrEqualsCI(TokenList[0], L"disable_check_amfi")) {
#if MERIDIAN_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate(TokenList[0], NotRunBefore, TRUE);
            }
#endif

            GlobalConfig.DisableCheckAMFI = HandleBoolean(TokenList, TokenCount);
        }
        else if (!GotNoneNoBootLogo && (MrdStrEqualsCI(TokenList[0], L"disable_exitlogo_image") ||
                                        MrdStrEqualsCI(TokenList[0], L"disable_bootlogo_image") ||
                                        MrdStrEqualsCI(TokenList[0], L"disable_bootlogo"))) {
#if MERIDIAN_DEBUG > 0
            if (!OuterLoop && !OutLoopNoBootLogo) {
                UpdatedToken = LogUpdate(TokenList[0], NotRunBefore, TRUE);
            }
#endif

            GotNoBootLogoAll = FALSE;
            if (!OuterLoop && !OutLoopNoBootLogo) {

                OutLoopNoBootLogo = TRUE;
                GlobalConfig.DisableBootLogo = DISABLE_BOOTLOGO_OFF;
            }

            for (i = 1; i < TokenCount; i++) {
                Flag = TokenList[i];
                if (MrdStrStartsWithCI(L"Off", Flag)) {

                    GotNoneNoBootLogo = TRUE;
                    GlobalConfig.DisableBootLogo = DISABLE_BOOTLOGO_OFF;
                    break;
                }

                if (!GotNoBootLogoAll) {

                    if (MrdStrStartsWithCI(L"All", Flag)) {
                        GotNoBootLogoAll = TRUE;
                        GlobalConfig.DisableBootLogo = DISABLE_BOOTLOGO_ALL;
                    }
                    else {
                        if (0)
                            ;
                        else if (MrdStrStartsWithCI(L"Lin", Flag))
                            GlobalConfig.DisableBootLogo |= DISABLE_BOOTLOGO_LIN;
                        else if (MrdStrStartsWithCI(L"Win", Flag))
                            GlobalConfig.DisableBootLogo |= DISABLE_BOOTLOGO_WIN;
                        else
                            BadFlag(Flag, TokenList[0], NotRunBefore);
                    }
                }
            }
        }
        else if (MrdStrEqualsCI(TokenList[0], L"disable_exitlogo_clear") ||
                 MrdStrEqualsCI(TokenList[0], L"disable_bootlogo_clear")) {
#if MERIDIAN_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate(TokenList[0], NotRunBefore, TRUE);
            }
#endif

            DeclineSetting = HandleBoolean(TokenList, TokenCount);
            GlobalConfig.BootLogoClear = (DeclineSetting) ? FALSE : TRUE;
        }
        else if (MrdStrEqualsCI(TokenList[0], L"supply_nvme")) {
#if MERIDIAN_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate(TokenList[0], NotRunBefore, TRUE);
            }
#endif

            GlobalConfig.SupplyNVME = HandleBoolean(TokenList, TokenCount);
        }
        else if (MrdStrEqualsCI(TokenList[0], L"supply_uefi")) {
#if MERIDIAN_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate(TokenList[0], NotRunBefore, TRUE);
            }
#endif

            GlobalConfig.SupplyUEFI = HandleBoolean(TokenList, TokenCount);
        }
        else if (MrdStrEqualsCI(TokenList[0], L"enable_esp_filter") ||
                 MrdStrEqualsCI(TokenList[0], L"disable_esp_filter")) {
#if MERIDIAN_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate(TokenList[0], NotRunBefore, TRUE);
            }
#endif

            DeclineSetting = HandleBoolean(TokenList, TokenCount);
            if (MrdStrEqualsCI(TokenList[0], L"enable_esp_filter")) {
                GlobalConfig.ScanAllESP = (DeclineSetting) ? FALSE : TRUE;
            }
            else {
                GlobalConfig.ScanAllESP = DeclineSetting;
            }
        }
        else if (MrdStrEqualsCI(TokenList[0], L"renderer_direct_gop")) {
#if MERIDIAN_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate(TokenList[0], NotRunBefore, TRUE);
            }
#endif

            GlobalConfig.UseDirectGop = HandleBoolean(TokenList, TokenCount);
        }
        else if (MrdStrEqualsCI(TokenList[0], L"force_trim")) {
#if MERIDIAN_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate(TokenList[0], NotRunBefore, TRUE);
            }
#endif

            GlobalConfig.ForceTRIM = HandleBoolean(TokenList, TokenCount);
        }
        else if (MrdStrEqualsCI(TokenList[0], L"continue_on_warning")) {
#if MERIDIAN_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate(TokenList[0], NotRunBefore, TRUE);
            }
#endif

            GlobalConfig.ContinueOnWarning = HandleBoolean(TokenList, TokenCount);
        }
        else if (TokenCount == 2 && MrdStrEqualsCI(TokenList[0], L"scan_delay")) {
#if MERIDIAN_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate(TokenList[0], NotRunBefore, TRUE);
            }
#endif

            HandleUnsignedInt(TokenList, TokenCount, &(GlobalConfig.ScanDelay));
        }
        else if (MrdStrEqualsCI(TokenList[0], L"windows_recovery_files")) {
#if MERIDIAN_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate(TokenList[0], NotRunBefore, TRUE);
            }
#endif

            HandleStrings(TokenList, TokenCount, &(GlobalConfig.WindowsRecoveryFiles));
        }
        else if (MrdStrEqualsCI(TokenList[0], L"shutdown_after_timeout")) {
#if MERIDIAN_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate(TokenList[0], NotRunBefore, TRUE);
            }
#endif

            GlobalConfig.ShutdownAfterTimeout = HandleBoolean(TokenList, TokenCount);
        }
        else if (MrdStrEqualsCI(TokenList[0], L"set_boot_args")) {
#if MERIDIAN_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate(TokenList[0], NotRunBefore, TRUE);
            }
#endif

            HandleString(TokenList, TokenCount, &(GlobalConfig.SetBootArgs));

            if (MrdStrEqualsCI(GlobalConfig.SetBootArgs, L"-none")) {
                MRD_FREE_POOL(GlobalConfig.SetBootArgs);
            }
        }
        else if (MrdStrEqualsCI(TokenList[0], L"write_systemd_vars")) {
#if MERIDIAN_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate(TokenList[0], NotRunBefore, TRUE);
            }
#endif

            GlobalConfig.WriteSystemdVars = HandleBoolean(TokenList, TokenCount);
        }
        else if (MrdStrEqualsCI(TokenList[0], L"extra_kernel_version_strings")) {
#if MERIDIAN_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate(TokenList[0], NotRunBefore, TRUE);
            }
#endif

            HandleStrings(TokenList, TokenCount, &(GlobalConfig.ExtraKernelVersionStrings));
        }
        else if (MrdStrEqualsCI(TokenList[0], L"max_tags")) {
#if MERIDIAN_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate(TokenList[0], NotRunBefore, TRUE);
            }
#endif

            HandleUnsignedInt(TokenList, TokenCount, &(GlobalConfig.MaxTags));
        }
        else if (MrdStrEqualsCI(TokenList[0], L"enable_and_lock_vmx")) {
#if MERIDIAN_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate(TokenList[0], NotRunBefore, TRUE);
            }
#endif

            GlobalConfig.EnableAndLockVMX = HandleBoolean(TokenList, TokenCount);
        }
        else if (MrdStrEqualsCI(TokenList[0], L"spoof_osx_version")) {
#if MERIDIAN_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken =
                    LogUpdate(TokenList[0], NotRunBefore, (AppleFirmware) ? TRUE : FALSE);
            }
#endif

            if (AppleFirmware) {
                HandleString(TokenList, TokenCount, &(GlobalConfig.SpoofOSXVersion));
            }
        }
        else if (MrdStrEqualsCI(TokenList[0], L"support_gzipped_loaders")) {
#if MERIDIAN_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate(TokenList[0], NotRunBefore, TRUE);
            }
#endif

            GlobalConfig.GzippedLoaders = HandleBoolean(TokenList, TokenCount);
        }
        else if (CheckManual && !DoneManual && TokenCount > 1 &&
                 MrdStrEqualsCI(TokenList[0], L"menuentry")) {

            DoneManual = TRUE;
        }
        else {
            if (OuterLoop && TokenCount == 2 && !MrdStrEqualsCI(TokenList[1], FileName) &&
                MrdStrEqualsCI(TokenList[0], L"include") &&
                MrdStrEqualsCI(FileName, GlobalConfig.ConfigFilename)) {
#if MERIDIAN_DEBUG > 0
                if (NotRunBefore)
                    MuteLogger = FALSE;
                if (FirstInclude) {
                    INFO_LOG("\n");
                    INFO_LOG(
                        "Detected Override File(s) - L O A D   C O N F I G   O V E R R I D E S");
                }
                INFO_LOG("%s%s* Supplementary Configuration ... %s", (FirstInclude) ? L"" : L"\n",
                         OffsetNext, TokenList[1]);
                FirstInclude = FALSE;
                INFO_LOG("%s*** Examine Included File ***", OffsetNext);
                if (NotRunBefore)
                    MuteLogger = TRUE;
#endif

                OuterLoop = FALSE;
                ReadConfig(TokenList[1]);
                OuterLoop = TRUE;

#if MERIDIAN_DEBUG > 0
                if (NotRunBefore)
                    MuteLogger = TRUE;
#endif
            }
        }

        FreeTokenLine(&TokenList, &TokenCount);
    }

    MRD_FREE_FILE(File);

    if (OuterLoop) {
        ExitOuter(
#if MERIDIAN_DEBUG > 0
            ValidInclude, NotRunBefore
#endif
        );
        ReadLoops = 0;

#if MERIDIAN_DEBUG > 0

        RESET_MISC(FALSE, TRUE, TRUE);
#endif
    }
    else {

        ReadLoops = ReadLoops - 1;

#if MERIDIAN_DEBUG > 0
        if (NotRunBefore)
            MuteLogger = FALSE;
        if (!UpdatedToken) {
            if (!DoneManual) {
                INFO_LOG("%s  - Active Tokens *NOT* Found", OffsetNext);
            }
            else {
                INFO_LOG("%s  - Only Got Manual Stanza(s) ... Handle Later", OffsetNext);
            }
        }
        INFO_LOG("%s*** Handled Included File ***", OffsetNext);

#endif
    }
}
