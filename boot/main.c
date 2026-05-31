// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2020-2026 Dayo Akanji
// SPDX-FileCopyrightText: 2021 Joe van Tunen
// SPDX-FileCopyrightText: 2012-2023 Roderick W. Smith
// SPDX-FileCopyrightText: 2006-2010 Christoph Pfisterer

#include "global.h"
#include "lib.h"
#include "conn_bridge.h"
#include "mok.h"
#include "menu.h"
#include "scan.h"
#include "cache.h"
#include "fw_strategy.h"
#include "apple.h"
#include "config.h"
#include "install.h"
#include "sysinfo.h"
#include "apple/smc.h"
#include "reset_reason.h"
#include "netinfo.h"
#include "main.h"
#include "screenmgt.h"
#include "mystrings.h"
#include "launch_efi.h"
#include "driver_support.h"
#include "security_policy.h"
#include "display.h"
#include "version.h"

#ifndef __MERIDIAN_SBAT_
#define __MERIDIAN_SBAT_
#if defined(__APPLE__)
__attribute__((used, section("__SBAT,__sbat"), visibility("default")))
#else
__attribute__((used, section(".sbat"), visibility("default")))
#endif

const char sbat_section[] =
    "sbat,1,SBAT Version,sbat,1,https://github.com/rhboot/shim/blob/main/SBAT.md\n"
    "meridian,1,Abdelkader Boudih,meridian," VERSION_STRING_ASCII
    ",https://github.com/seuros/meridian\n"
    "meridian.user,1,A User,meridian_user,1.0.0,https://example.org/\n";

__attribute__((used)) volatile static const void *force_sbat_link = &sbat_section;
#endif

#define LibLocateProtocol EfiLibLocateProtocol

#define MERIDIAN_NVRAM_VARIABLES L"PreviousBoot,PreviousBootId"

extern VOID InitBooterLog(VOID);

extern EFI_STATUS AmendSysTable(VOID);
extern EFI_STATUS MeridianApfsConnectDevices(VOID);
extern EFI_STATUS EFIAPI NvmExpressLoad(IN EFI_HANDLE ImageHandle,
                                        IN EFI_SYSTEM_TABLE *SystemTable);

extern EFI_FILE_PROTOCOL *gVarsDir;

extern BOOLEAN HasMacOS;
extern BOOLEAN SetSysTab;
extern BOOLEAN ShimFound;
extern BOOLEAN SecureFlag;
extern BOOLEAN SelfVolSet;
extern BOOLEAN SelfVolRun;
extern BOOLEAN ForceTextOnly;
extern BOOLEAN NormaliseCall;
extern BOOLEAN SubScreenBoot;
extern BOOLEAN ForceRescanDXE;
extern BOOLEAN GraphicsScreenDirty;

extern EFI_UGA_DRAW_PROTOCOL *UGADraw;
extern EFI_GRAPHICS_OUTPUT_PROTOCOL *GOPDraw;

static VOID InitMainMenu(VOID)
{
    MERIDIAN_MENU_SCREEN MainMenuSrc = {MAIN_MENU_NAME,
                                        0,
                                        NULL,
                                        0,
                                        NULL,
                                        0,
                                        L"Automatic Boot",
                                        SUBSCREEN_HINT1,
                                        L"Press 'Insert', 'Tab', or 'F2' for more options. Press "
                                        L"'Esc' or 'Backspace' to refresh the screen"};

    MainMenu = CopyMenuScreen(&MainMenuSrc);
    MainMenu->TimeoutSeconds = GlobalConfig.Timeout;
}

VOID StoreLoaderName(IN CHAR16 *Name)
{
    UINTN NameSize;

    if (GlobalConfig.TransientBoot) {
        EfivarSetRaw(&MeridianGuid, L"PreviousBoot", NULL, 0, TRUE);

        return;
    }

    if (Name == NULL) {

        return;
    }

    NameSize = sizeof(CHAR16) * (StrLen(Name) + 1);
    EfivarSetRaw(&MeridianGuid, L"PreviousBoot", Name, NameSize, TRUE);
}

CHAR16 *BuildEntryIdentity(IN LOADER_ENTRY *Entry)
{
    CHAR16 *GuidStr;
    CHAR16 *Identity;

    if (Entry == NULL || Entry->Volume == NULL) {
        return NULL;
    }

    GuidStr = GuidAsString(&(Entry->Volume->PartGuid));
    if (GuidStr == NULL) {
        return NULL;
    }

    Identity = PoolPrint(L"%s|%s", GuidStr, (Entry->LoaderPath != NULL) ? Entry->LoaderPath : L"");
    MRD_FREE_POOL(GuidStr);

    return Identity;
}

VOID StoreLoaderIdentity(IN LOADER_ENTRY *Entry)
{
    CHAR16 *Identity;

    if (GlobalConfig.TransientBoot) {
        EfivarSetRaw(&MeridianGuid, L"PreviousBootId", NULL, 0, TRUE);

        return;
    }

    Identity = BuildEntryIdentity(Entry);
    if (Identity == NULL) {
        EfivarSetRaw(&MeridianGuid, L"PreviousBootId", NULL, 0, TRUE);

        return;
    }

    EfivarSetRaw(&MeridianGuid, L"PreviousBootId", Identity,
                 sizeof(CHAR16) * (StrLen(Identity) + 1), TRUE);
    MRD_FREE_POOL(Identity);
}

CHAR16 *GetPreviousBootIdentity(VOID)
{
    EFI_STATUS Status;
    CHAR16 *Identity;
    UINTN Size;

    Identity = NULL;
    Size = 0;
    Status = EfivarGetRaw(&MeridianGuid, L"PreviousBootId", (VOID **)&Identity, &Size);
    if (EFI_ERROR(Status) || Identity == NULL) {
        return NULL;
    }

    if (Size < sizeof(CHAR16) || Identity[(Size / sizeof(CHAR16)) - 1] != L'\0') {
        MRD_FREE_POOL(Identity);

        return NULL;
    }

    return Identity;
}

BOOLEAN EntryMatchesIdentity(IN MERIDIAN_MENU_ENTRY *Entry, IN CHAR16 *Identity)
{
    CHAR16 *EntryIdentity;
    BOOLEAN Match;

    if (Entry == NULL || Identity == NULL || Entry->Tag != TAG_LOADER) {
        return FALSE;
    }

    EntryIdentity = BuildEntryIdentity((LOADER_ENTRY *)Entry);
    if (EntryIdentity == NULL) {
        return FALSE;
    }

    Match = MrdStrEqualsCI(EntryIdentity, Identity);
    MRD_FREE_POOL(EntryIdentity);

    return Match;
}

VOID RescanAll(BOOLEAN Reconnect)
{
#if MERIDIAN_DEBUG > 0
    BOOLEAN CheckMute = FALSE;
    CHAR16 *MsgStr;
#endif

#if MERIDIAN_DEBUG > 0
    MsgStr = L"R E S C A N   K E Y   I T E M S";
    DEBUG_LOG(1, LOG_STAR_SEPARATOR, L"%s", MsgStr);

    MRD_MUTELOGGER_SET;
#endif

    FreeList((VOID ***)&(MainMenu->Entries), &MainMenu->EntryCount);
    MainMenu->Entries = NULL;
    MainMenu->EntryCount = 0;

    if (Reconnect) {

        ForceRescanDXE = TRUE;
        ConnectAllDriversToAllControllers();
        ForceRescanDXE = GlobalConfig.RescanDXE;

        ScanVolumes();
    }

    ReadConfig(GlobalConfig.ConfigFilename);

    if (OverrideSB) {
        GlobalConfig.DirectBoot = FALSE;
        GlobalConfig.TextOnly = TRUE;
        GlobalConfig.Timeout = MainMenu->TimeoutSeconds = 0;
    }

    ScanForBootloaders();

    ScanForTools();

#if MERIDIAN_DEBUG > 0
    MRD_MUTELOGGER_OFF;

    DEBUG_LOG(1, LOG_BLANK_LINE_SEP, L"X");
#endif

    if (GlobalConfig.MenuCache) {
        DEBUG_LOG(1, LOG_LINE_NORMAL, L"Menu Cache: forced rescan -> rewriting cache");
        CacheStoreMenu(MainMenu, ComputeTopologyFingerprint());
    }
}

static VOID InitializeLib(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable)
{
    gImageHandle = ImageHandle;
    gST = SystemTable;
    gBS = SystemTable->BootServices;
    gRT = SystemTable->RuntimeServices;
    gDS = NULL;
    EfiGetSystemConfigurationTable(&gEfiDxeServicesTableGuid, (VOID **)&gDS);

    gBS->HandleProtocol = HandleProtocolEx;
    gBS->Hdr.CRC32 = 0;
    gBS->CalculateCrc32(gBS, gBS->Hdr.HeaderSize, &gBS->Hdr.CRC32);
}

static BOOLEAN SecureBootSetup(VOID)
{
    EFI_STATUS Status;
    BOOLEAN Retval;

    SecureFlag = secure_mode();
    ShimFound = ShimLoaded();
    Retval = FALSE;
    SecureBootFailure = FALSE;

    if (SecureFlag && ShimFound) {
        Status = security_policy_install();
        if (!EFI_ERROR(Status)) {
            Retval = TRUE;
        }
        else {
            SecureBootFailure = TRUE;
        }
    }

    return Retval;
}

static BOOLEAN SecureBootUninstall(VOID)
{
    EFI_STATUS Status;
    CHAR16 *MsgStr;
    BOOLEAN Success;

    Success = TRUE;
    if (SecureFlag) {
        Status = security_policy_uninstall();
        if (EFI_ERROR(Status)) {
            Success = FALSE;
            BeginTextScreen(L"Secure Boot Policy Failure");

            MsgStr =
                L"Failed to Uninstall MOK Secure Boot Extensions ... Forcing Shutdown in 9 Seconds";

#if MERIDIAN_DEBUG > 0
            INFO_LOG("%s", MsgStr);
            END_TAG();
#endif

            PrintUglyError(MsgStr, NEXTLINE);

            PauseSeconds(9);

            MrdAppleSmcNotifyReset(FALSE);
            gRT->ResetSystem(EfiResetShutdown, EFI_SUCCESS, 0, NULL);
        }
    }

    return Success;
}

static VOID SetConfigFilename(EFI_HANDLE ImageHandle)
{
    EFI_STATUS Status;
    CHAR16 *Options;
    CHAR16 *FileName;
    CHAR16 *SubString;
    CHAR16 *MsgStr;
    EFI_LOADED_IMAGE_PROTOCOL *Info;

    Status = gBS->HandleProtocol(ImageHandle, &LoadedImageProtocol, (VOID **)&Info);
    if (!EFI_ERROR(Status) && Info->LoadOptionsSize > 0) {
        Options = (CHAR16 *)Info->LoadOptions;
        SubString = MrdStrFind(Options, L" -c ");
        if (SubString) {
#if MERIDIAN_DEBUG > 0
            INFO_LOG("Set Config Filename from Command Line Option:");
            INFO_LOG("\n");
#endif

            FileName = StrDuplicate(&SubString[4]);
            LimitStringLength(FileName, 256);

            if (FileName != NULL && FileExists(SelfDir, FileName)) {
                GlobalConfig.ConfigFilename = FileName;

#if MERIDIAN_DEBUG > 0
                INFO_LOG("  - Config File:- '%s'", FileName);
                INFO_LOG("\n\n");
#endif
            }
            else {
                MRD_FREE_POOL(FileName);

                MsgStr = L"** WARN: Specified Config File *NOT* Found";
#if MERIDIAN_DEBUG > 0
                INFO_LOG("%s", MsgStr);
                INFO_LOG("\n");
#endif

                PrintUglyError(MsgStr, NEXTLINE);

                MsgStr = L"         Try Default:- 'config.conf'";
#if MERIDIAN_DEBUG > 0
                INFO_LOG("%s", MsgStr);
                INFO_LOG("\n\n");
#endif

                PrintUglyError(MsgStr, NEXTLINE);

                PauseSeconds(9);
            }
        }
    }

    if (GlobalConfig.ConfigFilename == NULL) {
        GlobalConfig.ConfigFilename = L"config.conf";
    }
}

BOOLEAN DefaultIsPreviousBoot = FALSE;

static VOID AdjustDefaultSelection(VOID)
{
#if MERIDIAN_DEBUG > 0
    CHAR16 *TmpStr;
#endif

    EFI_STATUS Status;
    UINTN i;
    CHAR16 *Element;
    CHAR16 *NewCommaDelimited;
    CHAR16 *PreviousBoot;
    BOOLEAN FoundOnce;
    BOOLEAN FormatLog;
    BOOLEAN Ignore;

#if MERIDIAN_DEBUG > 0
    INFO_LOG("U P D A T E   D E F A U L T   S E L E C T I O N");
#endif

    i = 0;
    FoundOnce = FormatLog = FALSE;
    PreviousBoot = NewCommaDelimited = NULL;
    while (GlobalConfig.DefaultSelection != NULL && !FoundOnce) {
        Element = FindCommaDelimited(GlobalConfig.DefaultSelection, i++);
        if (Element == NULL)
            break;

        Ignore = FALSE;
        if (MrdStrEqualsCI(Element, L"+")) {
            if (GlobalConfig.TransientBoot && GlobalConfig.DefaultSelection != NULL &&
                StrLen(GlobalConfig.DefaultSelection) > 1) {
                Ignore = TRUE;
                FormatLog = FALSE;
            }

            if (!Ignore) {
#if MERIDIAN_DEBUG > 0
                INFO_LOG("\n");
#endif

                FormatLog = TRUE;
                MRD_FREE_POOL(Element);
                Status = EfivarGetRaw(&MeridianGuid, L"PreviousBoot", (VOID **)&PreviousBoot, NULL);
                if (!EFI_ERROR(Status)) {
                    Element = PreviousBoot;
                }
            }
        }

        if (!Ignore && Element != NULL && StrLen(Element) > 0) {
#if MERIDIAN_DEBUG > 0
            if (FormatLog) {
                TmpStr = L"Changed to Previous Selection";
            }
            else {
                INFO_LOG("\n");
                TmpStr = L"Changed to Preferred Selection";
            }
            BRK_MIN("INFO: ");
            INFO_LOG("%s:- '%s'", TmpStr, Element);
#endif

            FoundOnce = TRUE;

            DefaultIsPreviousBoot = (PreviousBoot != NULL && Element == PreviousBoot);
            MergeStrings(&NewCommaDelimited, Element, L',');
        }

        MRD_FREE_POOL(Element);
    }

#if MERIDIAN_DEBUG > 0
    if (!FoundOnce) {
        if (!FormatLog) {
            BRK_MIN("\n");
        }
        BRK_MIN("INFO: ");
        INFO_LOG("No Change to Default Selection");
    }
    BRK_MOD("\n\n");
#endif

    MRD_FREE_POOL(GlobalConfig.DefaultSelection);
    GlobalConfig.DefaultSelection = NewCommaDelimited;
}

#if MERIDIAN_DEBUG > 0
static VOID LogRevisionInfo(EFI_TABLE_HEADER *Hdr, CHAR16 *Name, UINT16 CompileSize,
                            BOOLEAN DoEFICheck)
{
    static BOOLEAN FirstRun = TRUE;

    if (FirstRun) {
        INFO_LOG("\n\n");
    }
    else {
        INFO_LOG("\n");
    }
    FirstRun = FALSE;

    INFO_LOG("%s:- '%-4s %d.%02d'", Name,
             DoEFICheck ? (((Hdr->Revision >> 16U) > 1) ? L"UEFI" : L"EFI") : L"Ver",
             Hdr->Revision >> 16U, Hdr->Revision & 0xffff);

    if (Hdr->HeaderSize == CompileSize) {
        INFO_LOG(" (HeaderSize %d)", Hdr->HeaderSize);
    }
    else {
        INFO_LOG(" (HeaderSize %d ... %d CompileSize)", Hdr->HeaderSize, CompileSize);
    }
}

static VOID LogBasicInfo(BOOLEAN GotMOK)
{
    EFI_STATUS Status;
    CHAR16 *MsgStr;
    UINTN CheckSize;
    UINT64 MaximumVariableSize;
    UINT64 MaximumVariableStorageSize;
    UINT64 RemainingVariableStorageSize;
    EFI_GUID ConsoleControlProtocolGuid = EFI_CONSOLE_CONTROL_PROTOCOL_GUID;

    LogRevisionInfo(&gST->Hdr, L"    System Table", sizeof(*gST), TRUE);
    LogRevisionInfo(&gBS->Hdr, L"   Boot Services", sizeof(*gBS), TRUE);
    LogRevisionInfo(&gRT->Hdr, L"Runtime Services", sizeof(*gRT), TRUE);
    if (gDS != NULL) {
        LogRevisionInfo(&gDS->Hdr, L"    DXE Services", sizeof(*gDS), FALSE);
    }
    else {
        INFO_LOG("\n");
        INFO_LOG("    DXE Services:- 'Not Found' ... Some Functionality Will be Lost!!");
    }
    INFO_LOG("\n\n");

    if (WarnVersionEFI || WarnRevisionUEFI) {
        INFO_LOG("** WARN: Inconsistent %s Detected",
                 (WarnVersionEFI) ? L"EFI Versions" : L"UEFI Revisions");
        INFO_LOG("\n\n");
    }

    INFO_LOG("Non-Volatile Memory:");
    if (!gFwStrategy->HasQueryVariableInfo) {

        INFO_LOG("%s*** Skipped QueryVariableInfo Call ***", OffsetNext);
    }
    else {

        CheckSize =
            MRD_OFFSET_OF(EFI_RUNTIME_SERVICES, QueryVariableInfo) + sizeof(gRT->QueryVariableInfo);
        if (gRT->Hdr.HeaderSize < CheckSize) {
            WarnMissingQVInfo = TRUE;

            INFO_LOG("\n\n");
            INFO_LOG("** WARN: Irregular UEFI 2.x Implementation Detected");
            INFO_LOG("%s         Program Behaviour *IS NOT* Defined", OffsetNext);
        }
        else {
            Status = gRT->QueryVariableInfo(AccessFlagsBoot, &MaximumVariableStorageSize,
                                            &RemainingVariableStorageSize, &MaximumVariableSize);
            if (EFI_ERROR(Status)) {
                INFO_LOG("\n\n");
                INFO_LOG("** WARN: Could *NOT* Retrieve Non-Volatile Memory Details");
            }
            else if (!gFwStrategy->TrustNvramSizes) {

                INFO_LOG("%s** Redacted Invalid Output from QVI **", OffsetNext);
            }
            else {
                INFO_LOG("%s  - Total Storage         : %ld", OffsetNext,
                         MaximumVariableStorageSize);
                INFO_LOG("%s  - Remaining Available   : %ld", OffsetNext,
                         RemainingVariableStorageSize);
                INFO_LOG("%s  - Maximum Variable Size : %ld", OffsetNext, MaximumVariableSize);
            }
        }
    }
    INFO_LOG("\n\n");

    INFO_LOG("ConsoleOut Modes:");

    Status = LibLocateProtocol(&ConsoleControlProtocolGuid, (VOID **)&MsgStr);
    INFO_LOG("%s  - %sMode = Text           : %s", OffsetNext, (AppleFirmware) ? L"ConOut " : L"",
             EFI_ERROR(Status) ? L" NO" : L"YES");
    MRD_FREE_POOL(MsgStr);

    Status = gBS->HandleProtocol(gST->ConsoleOutHandle, &gEfiUgaDrawProtocolGuid, (VOID **)&MsgStr);
    INFO_LOG("%s  - %sMode = Graphics (UGA) : %s", OffsetNext, (AppleFirmware) ? L"ConOut " : L"",
             EFI_ERROR(Status) ? L" NO" : L"YES");
    MRD_FREE_POOL(MsgStr);

    Status = gBS->HandleProtocol(gST->ConsoleOutHandle, &gEfiGraphicsOutputProtocolGuid,
                                 (VOID **)&MsgStr);
    INFO_LOG("%s  - %sMode = Graphics (GOP) : %s", OffsetNext, (AppleFirmware) ? L"ConOut " : L"",
             (EFI_ERROR(Status)) ? L" NO" : L"YES");
    INFO_LOG("\n\n");
    MRD_FREE_POOL(MsgStr);

    INFO_LOG("Apple Framebuffer Count:- '%d'", AppleFramebuffers);
    INFO_LOG("\n\n");

    INFO_LOG("Secure Boot Items:");
    INFO_LOG("%s  - Preloaders:- %s", OffsetNext, (ShimFound) ? L"  'Active'" : L"'Inactive'");
    INFO_LOG("%s  - SecureTags:- %s", OffsetNext, (SecureFlag) ? L"  'Active'" : L"'Inactive'");
    INFO_LOG("%s  - MOK Extras:- %s", OffsetNext, (GotMOK) ? L"  'Active'" : L"'Inactive'");
    INFO_LOG("\n\n");
}
#endif

static VOID WarningPostPause(BOOLEAN AllowGraphicsMode)
{
    if (AllowGraphicsMode) {
        INFO_LOG("Restore Graphics Mode");
        INFO_LOG("\n\n");

        GraphicsScreenDirty = TRUE;
        SwitchToGraphicsAndClear(TRUE);
    }
    else {
        INFO_LOG("Proceeding");
        INFO_LOG("\n\n");

        BltClearScreen(FALSE);
    }

    MeridianStall(100);
}

static VOID ResetCall(CHAR16 *TypeStr, BOOLEAN IsRestart)
{
#if MERIDIAN_DEBUG > 0
    CHAR16 *MsgStr;
#endif

    ConnResetTransition(IsRestart);

#if MERIDIAN_DEBUG > 0
    MsgStr = (IsRestart) ? StrDuplicate(L"R U N   S Y S T E M   R E S T A R T")
                         : StrDuplicate(L"R U N   S Y S T E M   S H U T D O W N");
    DEBUG_LOG(1, LOG_BLANK_LINE_TWO, L"X");
    DEBUG_LOG(1, LOG_LINE_SEPARATOR, L"%s", MsgStr);
    INFO_LOG("%s", MsgStr);
    INFO_LOG("\n");
    MRD_FREE_POOL(MsgStr);

    DEBUG_LOG(1, LOG_LINE_NORMAL, L"Run %s", TypeStr);
    INFO_LOG("Run %s", TypeStr);
    END_TAG();
#endif

    TerminateScreen();

    MrdAppleSmcNotifyReset(IsRestart);
    gRT->ResetSystem((IsRestart) ? EfiResetCold : EfiResetShutdown, EFI_SUCCESS, 0, NULL);

#if MERIDIAN_DEBUG > 0
    MsgStr = PoolPrint(L"%s Failed", TypeStr);
    DEBUG_LOG(1, LOG_LINE_NORMAL, L"%s!!", MsgStr);
    INFO_LOG("** WARN: %s", MsgStr);
    MRD_FREE_POOL(MsgStr);
#endif
}

VOID MeridianStall(UINTN StallLoops)
{
    UINTN StallIndex;

    for (StallIndex = 0; StallIndex < StallLoops; ++StallIndex) {
        gBS->Stall(9999);
    }

    gBS->Stall((StallIndex + 1));
}

// Cydia writes its report to its own volume and returns; we then fall through

static VOID RunCydiaAutorunOnce(VOID)
{
    EFI_FILE_PROTOCOL *Marker;
    CHAR16 *CydiaPath;

    if (SelfDir == NULL || SelfVolume == NULL || SelfDirPath == NULL) {
        return;
    }
    if (!FileExists(SelfDir, L"CYDIA-ONCE")) {
        return;
    }

    if (!EFI_ERROR(SelfDir->Open(SelfDir, &Marker, L"CYDIA-ONCE",
                                 EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE, 0))) {
        Marker->Delete(Marker);
    }

    if (!FileExists(SelfDir, L"tools\\cydia_x64.efi")) {
#if MERIDIAN_DEBUG > 0
        INFO_LOG("INFO: CYDIA-ONCE present but 'tools\\cydia_x64.efi' missing ... Skipped");
#endif
        return;
    }

    CydiaPath = PoolPrint(L"\\%s\\tools\\cydia_x64.efi", SelfDirPath);
    if (CydiaPath == NULL) {
        return;
    }

#if MERIDIAN_DEBUG > 0
    INFO_LOG("INFO: CYDIA-ONCE ... running '%s' before autoboot", CydiaPath);
#endif

    StartEFIImage(SelfVolume, CydiaPath, NULL, L"Diagnostics (Cydia)", 0, FALSE, FALSE,
                  L"Diagnostics (Cydia)", NULL);

    MRD_FREE_POOL(CydiaPath);
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
#if MERIDIAN_DEBUG > 0
    CHAR16 *StrSelfGUID;
    CHAR16 *StrSelfUUID;
    BOOLEAN CheckMute = FALSE;
    BOOLEAN ForceNative = FALSE;
#endif

    EFI_STATUS Status;
    EFI_TIME Now;
    UINTN i;
#if defined(EFIX64)
    UINTN k;
#endif
    UINTN Trigger;
#if defined(EFIX64)
    UINT8 ResetNVRam;
#endif
    INTN MenuExit;
    CHAR16 *MsgStr;
    CHAR16 *PartMsg;
    CHAR16 *TypeStr;
#if defined(EFIX64)
    CHAR16 *FileName;
#endif
    CHAR16 *FilePath;
    CHAR16 *SelectionName;
    CHAR16 *VentoyName;
    CHAR16 *EntryTitle;
    CHAR16 *EntryPath = NULL;
#if defined(EFIX64)
    CHAR16 *VarNVram;
#endif
#if defined(EFIX64)
    BOOLEAN FoundTool;
#endif
    BOOLEAN RunOurTool;
    BOOLEAN MokProtocol;
    BOOLEAN FoundVentoy;
    BOOLEAN ForceContinue;
    BOOLEAN SkipTrustChain;
    BOOLEAN MainLoopRunning;
    BOOLEAN FoundInstallerMac;
    LOADER_ENTRY *OurLoaderEntry;
    MERIDIAN_VOLUME *EntryVol;
    EFI_INPUT_KEY key;
    MERIDIAN_MENU_ENTRY *ChosenOption;

    InitializeLib(ImageHandle, SystemTable);
    Status = InitMeridianLib(ImageHandle);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    OrigSetVariableRT = gRT->SetVariable;
    OrigOpenProtocolBS = gBS->OpenProtocol;

    ClearRecoveryBootFlags();

    gRT->GetTime(&Now, NULL);
    NowYear = Now.Year;
    NowMonth = Now.Month;
    NowDay = Now.Day;
    NowHour = Now.Hour;
    NowMinute = Now.Minute;
    NowSecond = Now.Second;

    DetectFirmwareVendor();
    SelectFwStrategy();
    AppleFirmware = (MeridianFirmwareVendor == FW_VENDOR_APPLE);

    if (AppleFirmware) {
        VendorInfo = StrDuplicate(gST->FirmwareVendor);
    }
    else {
        VendorInfo = (gST->FirmwareVendor != NULL)
                         ? PoolPrint(L"%s v%d.%02d", gST->FirmwareVendor,
                                     gST->FirmwareRevision >> 16U, gST->FirmwareRevision & 0xFFFF)
                         : PoolPrint(L"Unknown Vendor v%d.%02d", gST->FirmwareRevision >> 16U,
                                     gST->FirmwareRevision & 0xFFFF);
    }

    HandleAppleGfxRestore();

    gBS->CopyMem(GlobalConfig.ScanFor, "ieom      ", NUM_SCAN_OPTIONS);

    InitBooterLog();

#if MERIDIAN_DEBUG > 0

    MRD_NATIVELOGGER_SET;

    INFO_LOG("I N I T I A T E   B O O T S T R A P   S E Q U E N C E");
    INFO_LOG("\n");
    INFO_LOG("Loading Meridian %s on %s Firmware", MERIDIAN_VERSION, VendorInfo);
    INFO_LOG("\n");
#endif

    MokProtocol = SecureBootSetup();

    AppleFramebuffers = egCountAppleFramebuffers();

    EfiMajorVersion = (gST->Hdr.Revision >> 16U);
    WarnVersionEFI = WarnRevisionUEFI = FALSE;
    if ((gBS->Hdr.Revision >> 16U) != EfiMajorVersion ||
        (gRT->Hdr.Revision >> 16U) != EfiMajorVersion) {
        WarnVersionEFI = TRUE;
    }
    else if ((gST->Hdr.Revision & 0xffff) != (gBS->Hdr.Revision & 0xffff) ||
             (gST->Hdr.Revision & 0xffff) != (gRT->Hdr.Revision & 0xffff) ||
             (gBS->Hdr.Revision & 0xffff) != (gRT->Hdr.Revision & 0xffff)) {

        if (!AppleFirmware) {
            WarnRevisionUEFI = TRUE;
        }
    }

#if MERIDIAN_DEBUG > 0

    LogBasicInfo(MokProtocol);

    MRD_MUTELOGGER_SET;
#endif

    ScanVolumes();
    ScanDisks();
    ScanMemory();
    ScanBattery();
    ScanCpu();
    ScanGpu();
    ScanDisplay();
    ScanTpm();
    ScanSystemInfo();

#if MERIDIAN_DEBUG > 0
    MRD_MUTELOGGER_OFF;
    {
        UINTN MemDbgI;
        INFO_LOG("MRD-MEM aggregate: DimmCount=%d MemorySoldered=%d SolderedTotalMB=%d "
                 "SolderedMemType=0x%02x",
                 (INT32)MeridianDimmCount, MeridianMemorySoldered, (INT32)MeridianSolderedTotalMB,
                 MeridianSolderedMemType);
        for (MemDbgI = 0; MemDbgI < MeridianDimmCount; MemDbgI++) {
            INFO_LOG("MRD-MEM DIMM[%d] Populated=%d SizeMB=%d MemType=0x%02x FormFactor=0x%02x "
                     "Mfr='%a' -> "
                     "Soldered=%d",
                     (INT32)MemDbgI, MeridianDimms[MemDbgI].Populated,
                     (INT32)MeridianDimms[MemDbgI].SizeMB, MeridianDimms[MemDbgI].MemoryType,
                     MeridianDimms[MemDbgI].FormFactor, MeridianDimms[MemDbgI].Maker,
                     MeridianDimms[MemDbgI].Soldered);
        }
    }
    INFO_LOG("MRD-DISPLAY Present=%d %dx%d Name='%a'", MeridianDisplay.Present,
             MeridianDisplay.HorzRes, MeridianDisplay.VertRes, MeridianDisplay.Name);
    DumpDisplayEdid();
    DumpDisplayModes();
    if (!SelfVolSet) {
        MsgStr = StrDuplicate(L"Could *NOT* Set Self Volume!!");
        DEBUG_LOG(1, LOG_STAR_HEAD_SEP, L"%s", MsgStr);
        INFO_LOG("** WARN: %s", MsgStr);
        MRD_FREE_POOL(MsgStr);
    }
    else {
        StrSelfGUID = GuidAsString(&SelfVolume->PartGuid);
        StrSelfUUID = GuidAsString(&SelfVolume->VolUuid);
        INFO_LOG("INFO: Self-Volume Data:- '%s  :::  %s  :::  %s'", SelfVolume->VolName,
                 StrSelfGUID, StrSelfUUID);
        INFO_LOG("%s      Program Filename:- '%s'", OffsetNext,
                 (SelfBaseName != NULL) ? SelfBaseName : L"Not Set");
        INFO_LOG("%s      Install Location:- '%s'", OffsetNext,
                 (SelfDirPath != NULL) ? SelfDirPath : L"Not Set");
        MRD_FREE_POOL(StrSelfGUID);
        MRD_FREE_POOL(StrSelfUUID);
    }
    INFO_LOG("\n\n");
#endif

    MrdDetectBootCause();
    INFO_LOG("Boot cause: %a (%a)\n", MrdBootCauseStr(MeridianBootCause), MrdBootCauseDetail());

    SetConfigFilename(ImageHandle);

    BootstrapMissingConfig();

    if (!FileExists(SelfDir, GlobalConfig.ConfigFilename)) {

#if MERIDIAN_DEBUG > 0
        INFO_LOG("INFO: No 'config.conf' and could not create one ... using defaults\n\n");
#endif
    }

    ReadConfig(GlobalConfig.ConfigFilename);

    ApplySmbiosConfig();

    ScanNetwork(GlobalConfig.NetworkProbe, GlobalConfig.NetworkDhcp, NULL, NULL);

    if (GlobalConfig.AlsoScan == NULL) {
        GlobalConfig.AlsoScan = StrDuplicate(L"boot,@/boot");
    }

    VarNoCheckAMFI = GlobalConfig.DisableCheckAMFI;
    VarNoCheckCompat = GlobalConfig.DisableCheckCompat;
    VarDisablePanicLog = GlobalConfig.DisableNvramPanicLog;

    AllToolLocations = StrDuplicate(GlobalConfig.ToolLocations);
    if (GlobalConfig.ToolLocationsExtra != NULL) {
        MergeUniqueItems(&AllToolLocations, GlobalConfig.ToolLocationsExtra, L',');
    }

    InitMainMenu();

    AdjustDefaultSelection();

#if MERIDIAN_DEBUG > 0
#define TAG_ITEM_A(Item) OffsetNext, Item
#define TAG_ITEM_B(Item) OffsetNext, (Item) ? L"YES" : L"NO"
#define TAG_ITEM_C(Item) OffsetNext, (Item) ? L"Active" : L"Inactive"

    INFO_LOG("L I S T   M I S C   S E T T I N G S");
    INFO_LOG("\n");
    INFO_LOG("INFO: MeridianDBG:- '%d'", MERIDIAN_DEBUG);
    INFO_LOG("%s      ScanDelay:- '%d'", TAG_ITEM_A(GlobalConfig.ScanDelay));
    INFO_LOG("%s      SyncNVram:- '%d'", TAG_ITEM_A(GlobalConfig.SyncNVram));
    INFO_LOG("%s      ReloadGOP:- '%s'", TAG_ITEM_B(GlobalConfig.ReloadGOP));
    INFO_LOG("%s      SyncTrust:- '%03d'", TAG_ITEM_A(GlobalConfig.SyncTrust));
    INFO_LOG("%s      SyncAPFS:- '%s'", TAG_ITEM_C(GlobalConfig.SyncAPFS));

    INFO_LOG("%s      CheckDXE:- '%s'", TAG_ITEM_C(GlobalConfig.RescanDXE));
    INFO_LOG("%s      TextOnly:- ", OffsetNext);
    if (ForceTextOnly) {
        INFO_LOG("'Forced'");
    }
    else {
        INFO_LOG("'%s'", (GlobalConfig.TextOnly) ? L"Active" : L"Inactive");
    }

    INFO_LOG("%s      DirectGOP:- '%s'", TAG_ITEM_C(GlobalConfig.UseDirectGop));
    INFO_LOG("%s      ScanAllESP:- '%s'", TAG_ITEM_C(GlobalConfig.ScanAllESP));
    INFO_LOG("%s      DirectBoot:- '%s'", TAG_ITEM_C(GlobalConfig.DirectBoot));
    INFO_LOG("%s      ExitLogoClear:- '%s'", TAG_ITEM_C(GlobalConfig.BootLogoClear));
    INFO_LOG("%s      TransientBoot:- '%s'", OffsetNext,
             (GlobalConfig.TransientBoot) ? L"Active" : L"Inactive");

    INFO_LOG("%s      FollowSymlinks:- ", OffsetNext);
    if (GlobalConfig.FollowSymlinks == NULL ||
        MrdStrEqualsCI(SYM_TAG_OFF, GlobalConfig.FollowSymlinks)) {
        INFO_LOG("'Inactive'");
    }
    else {
        INFO_LOG("'Active'");
    }
    INFO_LOG("\n\n");

    Status = EFI_NOT_STARTED;
#endif

    if (GlobalConfig.SupplyUEFI) {
        Status = AmendSysTable();
    }

#if MERIDIAN_DEBUG > 0
    INFO_LOG("C O M M E N C E   B A S E   S U P P O R T");
    INFO_LOG("\n");
    INFO_LOG("INFO: Supply Support:- 'UEFI  :  %r'", Status);

    Status = EFI_NOT_STARTED;
#endif

    if (GlobalConfig.SupplyNVME) {
        Status = (!SecureFlag) ? NvmExpressLoad(ImageHandle, SystemTable) : EFI_NOT_READY;
    }

#if MERIDIAN_DEBUG > 0
    INFO_LOG("%s      Supply Support:- 'NVME  :  %r'", OffsetNext, Status);
    INFO_LOG("\n\n");

#endif

    LoadDrivers();

#if MERIDIAN_DEBUG > 0

    Status = EFI_NOT_STARTED;
#endif

    if (GlobalConfig.SupplyAPFS) {
        Status = MeridianApfsConnectDevices();
    }

#if MERIDIAN_DEBUG > 0
    INFO_LOG("\n\n");
    INFO_LOG("C O N C L U D E   B A S E   S U P P O R T");
    INFO_LOG("\n");
    INFO_LOG("INFO: Supply Support:- 'APFS  :  %r'", Status);
#endif

    ScanVolumes();

    if (GlobalConfig.SpoofOSXVersion != NULL && GlobalConfig.SpoofOSXVersion[0] != L'\0') {
        Status = SetAppleOSInfo();

#if MERIDIAN_DEBUG > 0
        INFO_LOG("INFO: Spoof Mac OS Version ... %r", Status);
        INFO_LOG("\n\n");
#endif
    }

    if (GlobalConfig.DirectBoot) {

        if (GlobalConfig.ScreensaverTime != 0) {
            GlobalConfig.ScreensaverTime = 300;
        }

        if (SecureBootFailure || WarnMissingQVInfo) {

            OverrideSB = TRUE;
            GlobalConfig.ContinueOnWarning = TRUE;
        }

#if MERIDIAN_DEBUG > 0
        if (WarnMissingQVInfo) {

            OverrideSB = TRUE;
            GlobalConfig.ContinueOnWarning = TRUE;
        }

        INFO_LOG("C O N F I R M   D I R E C T   B O O T");
        if (OverrideSB) {
            INFO_LOG("\n");
            INFO_LOG("Load Error or Warning Present");
        }
#endif

        if (!OverrideSB) {
            Status = gST->ConIn->ReadKeyStroke(gST->ConIn, &key);
            if (!EFI_ERROR(Status)) {
                if (key.ScanCode == SCAN_ESC || key.UnicodeChar == CHAR_BACKSPACE) {
                    OverrideSB = TRUE;
                    MainMenu->TimeoutSeconds = 0;
                }
            }

#if MERIDIAN_DEBUG > 0
            INFO_LOG("\n");
            INFO_LOG("Read Buffered Keystrokes ... %r", Status);
#endif
        }

        if (OverrideSB) {
            BlockRescan = TRUE;
            GlobalConfig.Timeout = 0;
            GlobalConfig.DirectBoot = FALSE;

#if MERIDIAN_DEBUG > 0
            MsgStr = StrDuplicate(L"Override");
#endif
        }
        else {
#if MERIDIAN_DEBUG > 0
            MsgStr = StrDuplicate(L"Maintain");
#endif

            GlobalConfig.TextOnly = TRUE;
        }

#if MERIDIAN_DEBUG > 0
        INFO_LOG("%s - %s 'DirectBoot'", OffsetNext, MsgStr);
        INFO_LOG("\n\n");
        MRD_FREE_POOL(MsgStr);
#endif
    }

#if MERIDIAN_DEBUG > 0
    MsgStr = StrDuplicate(L"E N A B L E   S C R E E N   O U T P U T");
    DEBUG_LOG(1, LOG_LINE_SEPARATOR, L"%s", MsgStr);
    INFO_LOG("%s", MsgStr);
    INFO_LOG("\n");
    MRD_FREE_POOL(MsgStr);
#endif

    InitScreen();

    gBS->SetWatchdogTimer(0x0000, 0x0000, 0x0000, NULL);

    SetupScreen();

#if MERIDIAN_DEBUG > 0

    MRD_NATIVELOGGER_OFF;
#endif

    if (GlobalConfig.ScanDelay > 0) {
        MsgStr = StrDuplicate(L"Paused for Scan Delay");

        Trigger = 3;
        if (GlobalConfig.ScanDelay > Trigger) {
            PartMsg = PoolPrint(L"%s ... Please Wait", MsgStr);
            egDisplayMessage(PartMsg, &BGColorBase, CENTER, 0, NULL);

            MRD_FREE_POOL(PartMsg);
        }

#if MERIDIAN_DEBUG > 0
        DEBUG_LOG(1, LOG_THREE_STAR_SEP, L"Scan Delay");
        DEBUG_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
        INFO_LOG("%s:", MsgStr);
        INFO_LOG("\n");
#endif

        MRD_FREE_POOL(MsgStr);

        for (i = 0; i < GlobalConfig.ScanDelay; ++i) {
#if MERIDIAN_DEBUG > 0
            DEBUG_LOG(1, LOG_THREE_STAR_MID, L"Loading Paused for 1 Second");
#endif

            MeridianStall(100);
        }

#if MERIDIAN_DEBUG > 0
        MsgStr = PoolPrint(L"Resuming After a %d Second Pause", i);
        DEBUG_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
        INFO_LOG("  - %s", MsgStr);
        INFO_LOG("\n");
        MRD_FREE_POOL(MsgStr);
#endif

        if (GlobalConfig.ScanDelay > Trigger) {
            BltClearScreen(TRUE);
        }
    }

    if (GlobalConfig.MenuCache && !EFI_ERROR(CacheLoadMenu(ComputeTopologyFingerprint()))) {

        ScanForTools();
    }
    else {
        ScanForBootloaders();
        ScanForTools();
        if (GlobalConfig.MenuCache) {
            CacheStoreMenu(MainMenu, ComputeTopologyFingerprint());
        }
        else {

            CacheInvalidate();
        }
    }

    if (GlobalConfig.ShutdownAfterTimeout) {
        MainMenu->TimeoutText = StrDuplicate(L"Shutdown");
    }

#if MERIDIAN_DEBUG > 0
    if (!SecureBootFailure && WarnMissingQVInfo) {
        SwitchToText(FALSE);

        INFO_LOG("D I S P L A Y   U S E R   N O T I C E");
        MsgStr = StrDuplicate(L"Inconsistent UEFI 2.x Implementation");
        DEBUG_LOG(1, LOG_LINE_THIN_SEP, L"Display %s Warning", MsgStr);
        INFO_LOG("\n");
        INFO_LOG("INFO: User Warning:- '%s'", MsgStr);
        MRD_FREE_POOL(MsgStr);

        ForceContinue = (GlobalConfig.ContinueOnWarning) ? FALSE : TRUE;
        GlobalConfig.ContinueOnWarning = TRUE;
        MRD_MUTELOGGER_SET;
        gST->ConOut->SetAttribute(gST->ConOut, ATTR_ERROR);
        PrintUglyText(L"                                                          ", NEXTLINE);
        PrintUglyText(L"           Inconsistent UEFI 2.x Implementation           ", NEXTLINE);
        PrintUglyText(L"                                                          ", NEXTLINE);

        gST->ConOut->SetAttribute(gST->ConOut, ATTR_BASIC);
        PrintUglyText(L"                                                          ", NEXTLINE);
        PrintUglyText(L"            Program Behaviour *IS NOT* Defined            ", NEXTLINE);
        PrintUglyText(L"                                                          ", NEXTLINE);
        PrintUglyText(L"  -- This Notice *IS NOT* Displayed by the 'REL' File --  ", NEXTLINE);
        PrintUglyText(L"                                                          ", NEXTLINE);
        PauseForKey();
        MRD_MUTELOGGER_OFF;
        GlobalConfig.ContinueOnWarning = !ForceContinue;
        ForceContinue = FALSE;

        MsgStr = StrDuplicate(L"Warning Acknowledged or Timed Out");
        DEBUG_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
        DEBUG_LOG(1, LOG_BLANK_LINE_SEP, L"X");
        INFO_LOG("%s      %s ...", OffsetNext, MsgStr);
        MRD_FREE_POOL(MsgStr);

        WarningPostPause(AllowGraphicsMode);
    }
#endif

    if (SecureBootFailure) {
        SwitchToText(FALSE);

#if MERIDIAN_DEBUG > 0
        INFO_LOG("D I S P L A Y   U S E R   N O T I C E");
        MsgStr = StrDuplicate(L"Secure Boot Failure");
        DEBUG_LOG(1, LOG_LINE_THIN_SEP, L"Display %s Warning", MsgStr);
        INFO_LOG("\n");
        INFO_LOG("INFO: User Warning:- '%s'", MsgStr);
        MRD_FREE_POOL(MsgStr);
#endif

        ForceContinue = (GlobalConfig.ContinueOnWarning) ? FALSE : TRUE;
        GlobalConfig.ContinueOnWarning = TRUE;
#if MERIDIAN_DEBUG > 0
        MRD_MUTELOGGER_SET;
#endif
        gST->ConOut->SetAttribute(gST->ConOut, ATTR_ERROR);
        PrintUglyText(L"                                                          ", NEXTLINE);
        PrintUglyText(L"            Secure Boot Configuration Failure             ", NEXTLINE);
        PrintUglyText(L"                                                          ", NEXTLINE);

        gST->ConOut->SetAttribute(gST->ConOut, ATTR_BASIC);
        PrintUglyText(L"                                                          ", NEXTLINE);
        PrintUglyText(L"            Program Behaviour *IS NOT* Defined            ", NEXTLINE);
        PrintUglyText(L"                                                          ", NEXTLINE);
        PauseForKey();
#if MERIDIAN_DEBUG > 0
        MRD_MUTELOGGER_OFF;
#endif
        GlobalConfig.ContinueOnWarning = !ForceContinue;
        ForceContinue = FALSE;

#if MERIDIAN_DEBUG > 0
        MsgStr = StrDuplicate(L"Warning Acknowledged or Timed Out");
        DEBUG_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
        DEBUG_LOG(1, LOG_BLANK_LINE_SEP, L"X");
        INFO_LOG("%s      %s ...", OffsetNext, MsgStr);
        MRD_FREE_POOL(MsgStr);
#endif

        WarningPostPause(AllowGraphicsMode);
    }

    AlignCSR();

    SelectionName = (GlobalConfig.DefaultSelection != NULL)
                        ? StrDuplicate(GlobalConfig.DefaultSelection)
                        : NULL;

    MainLoopRunning = TRUE;
    while (MainLoopRunning) {

        TypeStr = NULL;
        FilePath = NULL;
        ChosenOption = NULL;
        OurLoaderEntry = NULL;
        IsBoot = FALSE;
#if defined(EFIX64)
        FoundTool = FALSE;
#endif
        RunOurTool = FALSE; // NOLINT(clang-analyzer-deadcode.DeadStores): loop-top reset
        OverrideSB = FALSE;
        SubScreenBoot = FALSE;
        KeepTrustChain = FALSE;
        SkipTrustChain = FALSE;

        MRD_FREE_POOL(FilePath);

        // wait for a key, then fall through to the normal menu. Boots nothing.
        {
            static BOOLEAN ConnPreviewDone = FALSE;
            if (!ConnPreviewDone) {
                ConnPreviewDone = TRUE;
                if (ConnPreviewRequested()) {
                    ConnPreviewShow(MainMenu);
                }
            }
        }

        // present, then fall through to the normal menu/autoboot (see helper).
        {
            static BOOLEAN CydiaAutorunDone = FALSE;
            if (!CydiaAutorunDone) {
                CydiaAutorunDone = TRUE;
                RunCydiaAutorunOnce();
            }
        }

        MenuExit = ConnRunMainMenu(MainMenu, &SelectionName, &ChosenOption);
        if (MenuExit != MENU_EXIT_TIMEOUT) {
            OneMainLoop = TRUE;
        }

        if (MenuExit == MENU_EXIT_ESCAPE || MenuExit == MENU_EXIT_SHOWSCREEN) {

            OneMainLoop = TRUE;

#if MERIDIAN_DEBUG > 0
            INFO_LOG("Received User Input:");
            if (MenuExit == MENU_EXIT_ESCAPE) {
                INFO_LOG("%s  - Rescan All ... Escape Key Pressed", OffsetNext);
            }
            else {
                INFO_LOG("%s  - Rescan All ... Key Press Detected", OffsetNext);
            }
            INFO_LOG("\n\n");
#endif

            if (GlobalConfig.DirectBoot) {
                OverrideSB = TRUE;
            }

            RescanAll(TRUE);

            continue;
        }
        BlockRescan = FALSE;

        if (FlushFailedTag && !FlushFailReset) {

            OneMainLoop = TRUE;

#if MERIDIAN_DEBUG > 0
            MsgStr = StrDuplicate(L"FlushFailedTag is Set ... Ignore MenuExit");
            DEBUG_LOG(1, LOG_STAR_SEPARATOR, L"%s", MsgStr);
            INFO_LOG("INFO: %s", MsgStr);
            INFO_LOG("\n\n");
            MRD_FREE_POOL(MsgStr);
#endif

            FlushFailedTag = FALSE;
            FlushFailReset = TRUE;

            continue;
        }

        if (!OneMainLoop && ChosenOption->Tag == TAG_REBOOT) {

            OneMainLoop = TRUE;

#if MERIDIAN_DEBUG > 0
            INFO_LOG("INFO: Invalid Post-Load Reboot Call ... Ignore Reboot Call");
            MsgStr = StrDuplicate(L"Mitigated Potential Persistent Primed Keystroke Buffer");
            DEBUG_LOG(1, LOG_STAR_SEPARATOR, L"%s", MsgStr);
            INFO_LOG("%s      %s", OffsetNext, MsgStr);
            INFO_LOG("\n\n");
            MRD_FREE_POOL(MsgStr);
#endif

            TypeStr = L"Aborted Invalid System Reset Call ... Please Try Again";
            egDisplayMessage(TypeStr, &BGColorFail, CENTER, 4, L"PauseSeconds");

            continue;
        }

        if (MenuExit == MENU_EXIT_TIMEOUT && GlobalConfig.ShutdownAfterTimeout) {
            ChosenOption->Tag = TAG_SHUTDOWN;
        }

        switch (ChosenOption->Tag) {
        case TAG_MOK:
        case TAG_SHELL:
        case TAG_GDISK:
        case TAG_GPTSYNC:
        case TAG_MEMTEST:
        case TAG_CYDIA:
        case TAG_NETBOOT:
        case TAG_FWUPDATE:
        case TAG_RECOVERY_MAC:
        case TAG_RECOVERY_WIN:
            PrepToolMenu(ChosenOption->Tag);

            break;
        case TAG_LOADER:
            OurLoaderEntry = (LOADER_ENTRY *)ChosenOption;
            FoundVentoy = FALSE;
            EntryVol = OurLoaderEntry->Volume;
            EntryTitle = OurLoaderEntry->Title;
            EntryPath = OurLoaderEntry->LoaderPath;

            if (!MrdStrIncludesCI(EntryTitle, L"Mac OS") &&
                !MrdStrIncludesCI(EntryTitle, L"Install") &&
                MrdStrIncludesCI(EntryPath, L"System\\Library\\CoreServices")) {

                EntryTitle =
                    (MrdStrIncludesCI(EntryVol->VolName, L"PreBoot")) ? L"Mac OS" : L"Meridian";
            }

            if (MrdStrIncludesCI(EntryPath, L"EFI\\Microsoft\\Boot")) {

                OurLoaderEntry->OSType = 'W';
                EntryTitle = L"Windows (UEFI)";
                if (!OurLoaderEntry->UseGraphicsMode) {
                    OurLoaderEntry->UseGraphicsMode =
                        (GlobalConfig.GraphicsFor & GRAPHICS_FOR_WINDOWS);
                }
            }

            FoundInstallerMac = (MrdStrIncludesCI(EntryTitle, L"Install OS X") ||
                                 MrdStrIncludesCI(EntryTitle, L"OS X Install")) ||
                                (MrdStrIncludesCI(EntryTitle, L"Install macOS") ||
                                 MrdStrIncludesCI(EntryTitle, L"macOS Install")) ||
                                (MrdStrIncludesCI(EntryTitle, L"Install Mac OS") ||
                                 MrdStrIncludesCI(EntryTitle, L"Mac OS Install")) ||
                                (MrdStrIncludesCI(EntryTitle, L"com.apple.install")) ||
                                (MrdStrIncludesCI(EntryPath, L"Install OS X") ||
                                 MrdStrIncludesCI(EntryPath, L"OS X Install")) ||
                                (MrdStrIncludesCI(EntryPath, L"Install macOS") ||
                                 MrdStrIncludesCI(EntryPath, L"macOS Install")) ||
                                (MrdStrIncludesCI(EntryPath, L"Install Mac OS") ||
                                 MrdStrIncludesCI(EntryPath, L"Mac OS Install")) ||
                                (MrdStrIncludesCI(EntryPath, L"com.apple.install")) ||
                                (MrdStrIncludesCI(EntryVol->VolName, L"Install OS X") ||
                                 MrdStrIncludesCI(EntryVol->VolName, L"OS X Install")) ||
                                (MrdStrIncludesCI(EntryVol->VolName, L"Install macOS") ||
                                 MrdStrIncludesCI(EntryVol->VolName, L"macOS Install")) ||
                                (MrdStrIncludesCI(EntryVol->VolName, L"Install Mac OS") ||
                                 MrdStrIncludesCI(EntryVol->VolName, L"Mac OS Install")) ||
                                (MrdStrIncludesCI(EntryVol->VolName, L"com.apple.install"));

            if (FoundInstallerMac) {
#if MERIDIAN_DEBUG > 0

                INFO_LOG("%s:",
                         (GlobalConfig.DirectBoot) ? L"Run DirectBoot" : L"Received User Input");
                MsgStr = StrDuplicate(L"Load Mac OS Installer");
                DEBUG_LOG(1, LOG_THREE_STAR_SEP, L"%s", MsgStr);
                INFO_LOG("%s  - %s%s '%s'", OffsetNext, MsgStr,
                         (EntryVol->VolName != NULL) ? L" from" : L":-",
                         (EntryVol->VolName != NULL) ? EntryVol->VolName : EntryPath);
                MRD_FREE_POOL(MsgStr);
#endif

                RunMacBootSupportFuncs(SelectionName);

                SkipTrustChain = TRUE;
                if (GlobalConfig.SyncTrust & ENFORCE_TRUST_MACOS) {
                    KeepTrustChain = TRUE;
                }

                if (!OurLoaderEntry->UseGraphicsMode) {
                    OurLoaderEntry->UseGraphicsMode = (GlobalConfig.GraphicsFor & GRAPHICS_FOR_OSX);
                }
            }
            else if (

                OurLoaderEntry->OSType == 'O' ||
                (SubScreenBoot && MrdStrIncludesCI(SelectionName, L"OpenCore")) ||
                MrdStrIncludesCI(EntryTitle, L"OpenCore") ||
                MrdStrIncludesCI(EntryPath, L"\\OC_") || MrdStrIncludesCI(EntryPath, L"\\OC\\") ||
                MrdStrIncludesCI(EntryPath, L"\\OpenCore")) {
#if MERIDIAN_DEBUG > 0

                INFO_LOG("Received User Input:");
                MsgStr = PoolPrint(L"Load Instance: OpenCore");
                DEBUG_LOG(1, LOG_THREE_STAR_SEP, L"%s", MsgStr);
                INFO_LOG("%s  - %s:- '%s'", OffsetNext, MsgStr, EntryPath);
                MRD_FREE_POOL(MsgStr);
#endif

                if (!OurLoaderEntry->UseGraphicsMode) {

                    OurLoaderEntry->UseGraphicsMode =
                        (GlobalConfig.GraphicsFor & GRAPHICS_FOR_OPENCORE) != 0;
                }

                RunNVramSync(SelectionName, FALSE);

                SkipTrustChain = TRUE;
                if (GlobalConfig.SyncTrust & ENFORCE_TRUST_OPENCORE) {
                    KeepTrustChain = TRUE;
                }
            }
            else if (OurLoaderEntry->OSType == 'M' ||
                     (SubScreenBoot && (MrdStrIncludesCI(SelectionName, L"macOS") ||
                                        MrdStrIncludesCI(SelectionName, L"Mac OS"))) ||
                     (MrdStrIncludesCI(EntryTitle, L"macOS") ||
                      MrdStrIncludesCI(EntryTitle, L"Mac OS"))) {
#if MERIDIAN_DEBUG > 0

                INFO_LOG("%s:",
                         (GlobalConfig.DirectBoot) ? L"Run DirectBoot" : L"Received User Input");
                DEBUG_LOG(1, LOG_THREE_STAR_SEP, L"%s", SelectionName);
                INFO_LOG("%s  - %s", OffsetNext, SelectionName);
#endif

                RunMacBootSupportFuncs(SelectionName);

                SkipTrustChain = TRUE;
                if (GlobalConfig.SyncTrust & ENFORCE_TRUST_MACOS) {
                    KeepTrustChain = TRUE;
                }

                if (!OurLoaderEntry->UseGraphicsMode) {
                    OurLoaderEntry->UseGraphicsMode = (GlobalConfig.GraphicsFor & GRAPHICS_FOR_OSX);
                }
            }
            else if (OurLoaderEntry->OSType == 'W' ||
                     (SubScreenBoot && MrdStrIncludesCI(SelectionName, L"Windows")) ||
                     MrdStrIncludesCI(EntryTitle, L"Windows")) {
#if MERIDIAN_DEBUG > 0

                INFO_LOG("%s:",
                         (GlobalConfig.DirectBoot) ? L"Run DirectBoot" : L"Received User Input");
                DEBUG_LOG(1, LOG_THREE_STAR_SEP, L"%s", SelectionName);
                INFO_LOG("%s  - %s", OffsetNext, SelectionName);
#endif

                SkipTrustChain = TRUE;
                if (!AppleFirmware) {
                    if (GlobalConfig.SyncTrust & ENFORCE_TRUST_WINDOWS) {
                        KeepTrustChain = TRUE;
                    }
                }

                if (!OurLoaderEntry->UseGraphicsMode) {
                    OurLoaderEntry->UseGraphicsMode =
                        (GlobalConfig.GraphicsFor & GRAPHICS_FOR_WINDOWS);
                }
            }
            else if (OurLoaderEntry->OSType == 'G' ||
                     (SubScreenBoot && MrdStrIncludesCI(SelectionName, L"Grub")) ||
                     MrdStrIncludesCI(EntryTitle, L"Grub")) {
#if MERIDIAN_DEBUG > 0
                MsgStr = StrDuplicate(L"Load Instance: Linux (Grub)");
                DEBUG_LOG(1, LOG_THREE_STAR_SEP, L"%s", MsgStr);

                INFO_LOG("%s:",
                         (GlobalConfig.DirectBoot) ? L"Run DirectBoot" : L"Received User Input");
                INFO_LOG("%s  - %s:- '%s'", OffsetNext, MsgStr, EntryPath);

                MRD_FREE_POOL(MsgStr);
#endif

                SkipTrustChain = TRUE;
                if (GlobalConfig.SyncTrust & ENFORCE_TRUST_LINUX) {
                    KeepTrustChain = TRUE;
                }

                if (!OurLoaderEntry->UseGraphicsMode) {
                    OurLoaderEntry->UseGraphicsMode =
                        (GlobalConfig.GraphicsFor & GRAPHICS_FOR_GRUB);
                }
            }
            else if (OurLoaderEntry->OSType == 'S' ||
                     (SubScreenBoot && MrdStrIncludesCI(SelectionName, L"SystemD")) ||
                     MrdStrIncludesCI(EntryTitle, L"SystemD") ||
                     (SubScreenBoot && MrdStrIncludesCI(SelectionName, L"GummiBoot")) ||
                     MrdStrIncludesCI(EntryTitle, L"GummiBoot")) {
#if MERIDIAN_DEBUG > 0
                MsgStr = StrDuplicate(L"Load Instance: Linux (SDBoot)");
                DEBUG_LOG(1, LOG_THREE_STAR_SEP, L"%s", MsgStr);

                INFO_LOG("%s:",
                         (GlobalConfig.DirectBoot) ? L"Run DirectBoot" : L"Received User Input");
                INFO_LOG("%s  - %s:- '%s'", OffsetNext, MsgStr, EntryPath);

                MRD_FREE_POOL(MsgStr);
#endif

                SkipTrustChain = TRUE;
                if (GlobalConfig.SyncTrust & ENFORCE_TRUST_LINUX) {
                    KeepTrustChain = TRUE;
                }

                if (!OurLoaderEntry->UseGraphicsMode) {
                    OurLoaderEntry->UseGraphicsMode =
                        (GlobalConfig.GraphicsFor & GRAPHICS_FOR_SYSTEMD);
                }
            }
            else if (OurLoaderEntry->OSType == 'L' ||
                     (SubScreenBoot && MrdStrIncludesCI(SelectionName, L"Linux")) ||
                     MrdStrIncludesCI(EntryTitle, L"Linux")) {
#if MERIDIAN_DEBUG > 0

                INFO_LOG("%s:",
                         (GlobalConfig.DirectBoot) ? L"Run DirectBoot" : L"Received User Input");

                if (0)
                    ;
                else if (MrdStrIncludesCI(EntryPath, L"vmlinuz"))
                    TypeStr = L" (VMLinuz)";
                else if (MrdStrIncludesCI(EntryPath, L"bzImage"))
                    TypeStr = L" (BZImage)";
                else if (MrdStrIncludesCI(EntryPath, L"kernel"))
                    TypeStr = L" (Kernel)";
                else if (MrdStrIncludesCI(EntryPath, L"zImage"))
                    TypeStr = L" (ZImage)";
                else
                    TypeStr = L"";

                MsgStr = PoolPrint(L"Load Instance: Linux%s", TypeStr);
                DEBUG_LOG(1, LOG_THREE_STAR_SEP, L"%s", MsgStr);
                INFO_LOG("%s  - %s", OffsetNext, MsgStr);
                if (EntryVol->VolName == NULL) {
                    INFO_LOG(":- '%s'", EntryPath);
                }
                else {
                    if (MrdStrIncludesCI(EntryVol->VolName, L"Partition")) {
                        INFO_LOG(" from %s", EntryVol->VolName);
                    }
                    else {
                        INFO_LOG(" from '%s' Partition", EntryVol->VolName);
                    }
                }
                MRD_FREE_POOL(MsgStr);
#endif

                SkipTrustChain = TRUE;
                if (GlobalConfig.SyncTrust & ENFORCE_TRUST_LINUX) {
                    KeepTrustChain = TRUE;
                }

                if (!OurLoaderEntry->UseGraphicsMode) {
                    OurLoaderEntry->UseGraphicsMode =
                        (GlobalConfig.GraphicsFor & GRAPHICS_FOR_LINUX);
                }
            }
            else if (OurLoaderEntry->OSType == 'C' ||
                     (SubScreenBoot && MrdStrIncludesCI(SelectionName, L"Clover")) ||
                     MrdStrIncludesCI(EntryTitle, L"Clover") ||
                     MrdStrIncludesCI(EntryPath, L"\\Clover")) {
                if (!OurLoaderEntry->UseGraphicsMode) {
                    OurLoaderEntry->UseGraphicsMode =
                        (GlobalConfig.GraphicsFor & GRAPHICS_FOR_CLOVER);
                }

#if MERIDIAN_DEBUG > 0

                INFO_LOG("%s:",
                         (GlobalConfig.DirectBoot) ? L"Run DirectBoot" : L"Received User Input");
                MsgStr = StrDuplicate(L"Load Instance: Clover");
                DEBUG_LOG(1, LOG_THREE_STAR_SEP, L"%s", MsgStr);
                INFO_LOG("%s  - %s:- '%s'", OffsetNext, MsgStr, EntryPath);
                MRD_FREE_POOL(MsgStr);
#endif

                RunNVramSync(SelectionName, FALSE);

                SkipTrustChain = TRUE;
                if (GlobalConfig.SyncTrust & ENFORCE_TRUST_CLOVER) {
                    KeepTrustChain = TRUE;
                }
            }
            else if (OurLoaderEntry->OSType == 'E') {
                if (!OurLoaderEntry->UseGraphicsMode) {
                    OurLoaderEntry->UseGraphicsMode =
                        (GlobalConfig.GraphicsFor & GRAPHICS_FOR_ELILO);
                }

#if MERIDIAN_DEBUG > 0

                INFO_LOG("%s:",
                         (GlobalConfig.DirectBoot) ? L"Run DirectBoot" : L"Received User Input");
                MsgStr = StrDuplicate(L"Load Instance: Elilo");
                DEBUG_LOG(1, LOG_THREE_STAR_SEP, L"%s", MsgStr);
                INFO_LOG("%s  - %s:- '%s'", OffsetNext, MsgStr, EntryPath);
                MRD_FREE_POOL(MsgStr);
#endif

                SkipTrustChain = TRUE;
                if (GlobalConfig.SyncTrust & ENFORCE_TRUST_OTHERS) {
                    KeepTrustChain = TRUE;
                }
            }
            else {
                i = 0;
                while (GlobalConfig.HandleVentoy && !FoundVentoy) {
                    VentoyName = FindCommaDelimited(VENTOY_NAMES, i++);
                    if (VentoyName == NULL)
                        break;

                    if (MrdStrStartsWithCI(VentoyName, EntryVol->VolName)) {
                        FoundVentoy = TRUE;
                    }
                    MRD_FREE_POOL(VentoyName);
                }

#if MERIDIAN_DEBUG > 0
                if (!FoundVentoy) {
                    MsgStr = PoolPrint(L"Load EFI File:- '%s'", EntryPath);
                }
                else {
                    MsgStr = PoolPrint(L"Load Instance: Ventoy from '%s'", EntryPath);
                }

                DEBUG_LOG(1, LOG_THREE_STAR_SEP, L"%s", MsgStr);

                INFO_LOG("Received User Input:");
                INFO_LOG("%s  - %s", OffsetNext, MsgStr);
                MRD_FREE_POOL(MsgStr);
#endif
            }

            if (!FoundVentoy && !SkipTrustChain &&
                (GlobalConfig.SyncTrust & ENFORCE_TRUST_OTHERS)) {
                KeepTrustChain = TRUE;
            }

            if (!KeepTrustChain) {
                Trigger = SYNC_TRUST_SKIP;
                RunOurTool = FALSE;
            }
            else {

                Trigger = RunTrustSync(OurLoaderEntry);
                RunOurTool = TRUE;
            }

            if (Trigger != SYNC_TRUST_EXIT) {
                if (Trigger == SYNC_TRUST_SKIP) {

                    StartLoader(OurLoaderEntry, SelectionName, RunOurTool);

#if MERIDIAN_DEBUG > 0
                    if (!FoundVentoy) {
                        UnexpectedReturn(L"OS Loader");
                    }
#endif
                }
                else {

                    TypeStr = L"Could *NOT* Load Native Boot Chain";

                    egDisplayMessage(TypeStr, &BGColorFail, CENTER, 4, L"PauseSeconds");

#if MERIDIAN_DEBUG > 0
                    DEBUG_LOG(1, LOG_STAR_SEPARATOR, L"%s", TypeStr);
                    INFO_LOG("%s    * %s", OffsetNext, TypeStr);
                    INFO_LOG("%s", OffsetNext);
#endif
                }
            }

            break;
        case TAG_TOOL:
            TypeStr = L"uEFI Tool";

            OurLoaderEntry = (LOADER_ENTRY *)ChosenOption;
#if MERIDIAN_DEBUG > 0
            MsgStr = PoolPrint(L"Start %s", TypeStr);
            DEBUG_LOG(1, LOG_THREE_STAR_SEP, L"%s", MsgStr);
            INFO_LOG("Received User Input:");
            INFO_LOG("%s  - %s:- '%s'", OffsetNext, MsgStr, EntryPath);
            MRD_FREE_POOL(MsgStr);
#endif

            StartTool(OurLoaderEntry);

            break;
        case TAG_FIRMWARE_LOADER:
            OurLoaderEntry = (LOADER_ENTRY *)ChosenOption;

#if MERIDIAN_DEBUG > 0
            INFO_LOG("%s:", (GlobalConfig.DirectBoot) ? L"Run DirectBoot" : L"Received User Input");
            INFO_LOG("%s  - Reboot into Firmware Loader", OffsetNext);
#endif

            RebootIntoLoader(OurLoaderEntry);

#if MERIDIAN_DEBUG > 0
            UnexpectedReturn(L"Firmware Reboot");
#endif

            break;
        case TAG_EXIT:
#if MERIDIAN_DEBUG > 0
            INFO_LOG("Received User Input:");
            INFO_LOG("%s  - Close Meridian", OffsetNext);
            OUT_TAG();
#endif

            if ((MokProtocol) && !SecureBootUninstall()) {

                MainLoopRunning = FALSE;
            }
            else {
                BeginTextScreen(L" ");
                return EFI_SUCCESS;
            }

            break;
        case TAG_FIRMWARE:
#if MERIDIAN_DEBUG > 0
            INFO_LOG("Received User Input:");
            INFO_LOG("%s  - Reboot into Firmware", OffsetNext);
#endif

            RebootIntoFirmware();

            break;
        case TAG_CSR_ROTATE:
#if MERIDIAN_DEBUG > 0
            INFO_LOG("Received User Input:");
            INFO_LOG("%s  - Show '%s' Menu", OffsetNext, LABEL_CSR_ROTATE);
#endif

            InitRotateCSR();

            break;
        case TAG_INSTALL:
#if MERIDIAN_DEBUG > 0
            INFO_LOG("Received User Input:");
            INFO_LOG("%s  - Install Meridian", OffsetNext);
            INFO_LOG("\n\n");
#endif

            InstallMeridian();

            break;
        case TAG_BOOTORDER:
#if MERIDIAN_DEBUG > 0
            INFO_LOG("Received User Input:");
            INFO_LOG("%s  - Show '%s' Menu", OffsetNext, LABEL_BOOTORDER);
            INFO_LOG("\n\n");
#endif

            ManageBootorder();

#if MERIDIAN_DEBUG > 0
            INFO_LOG("Received User Input:");
            INFO_LOG("%s  - Exit '%s' Menu", OffsetNext, LABEL_BOOTORDER);
            INFO_LOG("\n\n");
#endif

            break;
        case TAG_CLEAN_NVRAM:
#if defined(EFIX64)
            TypeStr = L"Clean/Reset nvRAM";

#if MERIDIAN_DEBUG > 0
            INFO_LOG("Received User Input:");
            INFO_LOG("%s  - Show '%s' Menu", OffsetNext, LABEL_CLEAN_NVRAM);
            INFO_LOG("\n\n");
#endif

            RunOurTool = ShowInfoCleanNvram(TypeStr);
            if (!RunOurTool) {

                break;
            }

            i = 0;
            while (!FoundTool) {
                FilePath = FindCommaDelimited(GlobalConfig.ToolLocations, i++);
                if (FilePath == NULL)
                    break;

                k = 0;
                while (!FoundTool) {
                    FileName = FindCommaDelimited(NVRAMCLEAN_FILES, k++);
                    if (FileName == NULL)
                        break;

                    for (i = 0; i < VolumesCount; i++) {

                        MsgStr = PoolPrint(L"%s\\%s", FilePath, FileName);
                        if (Volumes[i]->RootDir != NULL &&
                            FileExists(Volumes[i]->RootDir, MsgStr)) {
                            OurLoaderEntry = AllocateZeroPool(sizeof(LOADER_ENTRY));
                            if (OurLoaderEntry) {
                                OurLoaderEntry->me.Title = StrDuplicate(TypeStr);
                                OurLoaderEntry->me.Tag = TAG_RESET_NVRAM;
                                OurLoaderEntry->Volume = Volumes[i];
                                OurLoaderEntry->LoaderPath = MsgStr;
                                FoundTool = TRUE;
                            }

                            break;
                        }

                        MRD_FREE_POOL(MsgStr);
                    }

                    MRD_FREE_POOL(FileName);
                }

                MRD_FREE_POOL(FilePath);
            }

            if (!FoundTool) {
#if MERIDIAN_DEBUG > 0
                MsgStr = PoolPrint(L"Could *NOT* Find Tool:- '%s'", TypeStr);
                DEBUG_LOG(1, LOG_THREE_STAR_END, L"%s", MsgStr);
                INFO_LOG("** WARN : %s ... Return to Main Menu", MsgStr);
                INFO_LOG("\n\n");
                MRD_FREE_POOL(MsgStr);
#endif

                break;
            }

            SetHardwareNvramVariable(L"scan-policy", &OpenCoreVendorGuid, AccessFlagsBoot, 0, NULL);
            SetHardwareNvramVariable(L"boot-redirect", &OpenCoreVendorGuid, AccessFlagsBoot, 0,
                                     NULL);
            SetHardwareNvramVariable(L"boot-protect", &OpenCoreVendorGuid, AccessFlagsFull, 0,
                                     NULL);
            if (AppleFirmware) {

                SetHardwareNvramVariable(L"CurrentPolicy", &MicrosoftVendorGuid, AccessFlagsBoot, 0,
                                         NULL);
                SetHardwareNvramVariable(L"CurrentActivePolicy", &MicrosoftVendorGuid,
                                         AccessFlagsFull, 0, NULL);
            }

            HandleToolRun(TypeStr, RunOurTool, OurLoaderEntry);

#if MERIDIAN_DEBUG > 0
            MRD_MUTELOGGER_SET;
#endif

            i = 0;
            Status = (gVarsDir != NULL) ? EFI_SUCCESS : FindVarsDir();
            while (1) {
                VarNVram = FindCommaDelimited(MERIDIAN_NVRAM_VARIABLES, i++);
                if (VarNVram == NULL)
                    break;

                if (!EFI_ERROR(Status)) {
                    MrdSaveFile(gVarsDir, VarNVram, NULL, 0);
                }

                SetHardwareNvramVariable(VarNVram, &MeridianGuid, AccessFlagsFull, 0, NULL);
                MRD_FREE_POOL(VarNVram);
            }

            if (AppleFirmware) {
                ResetNVRam = 1;
                gRT->SetVariable(L"ResetNVRam", &AppleBootGuid, AccessFlagsFull, sizeof(ResetNVRam),
                                 &ResetNVRam);
            }

#if MERIDIAN_DEBUG > 0
            MRD_MUTELOGGER_OFF;

            INFO_LOG("INFO: Cleaned nvRAM");
            INFO_LOG("\n\n");
#endif

#else
            break;
#endif
        case TAG_REBOOT:
            ResetCall(L"System Restart", TRUE);

            MainLoopRunning = FALSE;

            break;
        case TAG_SHUTDOWN:
            ResetCall(L"System Shutdown", FALSE);

            MainLoopRunning = FALSE;

            break;
        case TAG_SCAN_ALL:
            FreeList((VOID ***)&(MainMenu->Entries), &MainMenu->EntryCount);
            MainMenu->Entries = NULL;
            MainMenu->EntryCount = 0;
            ScanForBootloaders();
            ScanForTools();

            if (GlobalConfig.MenuCache) {
                DEBUG_LOG(1, LOG_LINE_NORMAL, L"Menu Cache: forced rescan -> rewriting cache");
                CacheStoreMenu(MainMenu, ComputeTopologyFingerprint());
            }

            break;
        }

        if (GlobalConfig.DirectBoot) {
            GlobalConfig.DirectBoot = FALSE;
            MainMenu->TimeoutSeconds = GlobalConfig.Timeout = 0;
        }

        OneMainLoop = TRUE;
    }

#if MERIDIAN_DEBUG > 0
    DEBUG_LOG(1, LOG_LINE_THIN_SEP, L"Unexpected Main Loop Exit ... Try to Reboot!!");

    INFO_LOG("Fallback: System Restart...");
    INFO_LOG("\n");
    INFO_LOG("Screen Termination:");
    INFO_LOG("\n");
#endif

    TerminateScreen();

#if MERIDIAN_DEBUG > 0
    INFO_LOG("System Reset:");
    INFO_LOG("\n\n");
#endif

    MrdAppleSmcNotifyReset(TRUE);
    gRT->ResetSystem(EfiResetCold, EFI_SUCCESS, 0, NULL);

#if MERIDIAN_DEBUG > 0
    DEBUG_LOG(1, LOG_LINE_THIN_SEP, L"Reset After Unexpected Main Loop Exit:- 'FAILED!!'");
#endif

    SwitchToText(FALSE);

    MsgStr = StrDuplicate(L"E N T E R I N G   D E A D   L O O P");

#if MERIDIAN_DEBUG > 0
    DEBUG_LOG(1, LOG_BLANK_LINE_SEP, L"X");
    DEBUG_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
    INFO_LOG("INFO: %s", MsgStr);
    OUT_TAG();
#endif

    PrintUglyError(MsgStr, NEXTLINE);

    MRD_FREE_POOL(MsgStr);

    PauseForKey();
    MeridianDeadLoop();

    return EFI_SUCCESS;
}
