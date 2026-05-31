// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2020-2026 Dayo Akanji
// SPDX-FileCopyrightText: 2021 Joe van Tunen
// SPDX-FileCopyrightText: 2012-2023 Roderick W. Smith
// SPDX-FileCopyrightText: 2006-2010 Christoph Pfisterer

#include "global.h"
#include "screenmgt.h"

INT16 NowYear = 0;
INT16 NowMonth = 0;
INT16 NowDay = 0;
INT16 NowHour = 0;
INT16 NowMinute = 0;
INT16 NowSecond = 0;

MERIDIAN_MENU_SCREEN *MainMenu = NULL;

MERIDIAN_CONFIG GlobalConfig = {
    .DirectBoot = FALSE,
    .MenuCache = TRUE,
    .CustomScreenBG = FALSE,
    .TextOnly = FALSE,
    .EnableAndLockVMX = FALSE,
    .PersistBootArgs = FALSE,
    .TransientBoot = FALSE,
    .ReloadGOP = TRUE,
    .UseDirectGop = FALSE,
    .NormaliseCSR = FALSE,
    .ShutdownAfterTimeout = FALSE,
    .Install = FALSE,
    .WriteSystemdVars = FALSE,
    .HandleVentoy = FALSE,
    .MitigatePrimedBuffer = FALSE,
    .ScanLimine = FALSE,
    .ScanBtrfsSnapshots = FALSE,
    .ContinueOnWarning = FALSE,
    .ForceTRIM = FALSE,
    .DisableCheckAMFI = FALSE,
    .DisableCheckCompat = FALSE,
    .DisableNvramPanicLog = FALSE,
    .GzippedLoaders = FALSE,
    .SupplyUEFI = FALSE,
    .SupplyNVME = FALSE,
    .SupplyAPFS = TRUE,
    .SyncAPFS = TRUE,
    .ScanAllESP = TRUE,
    .ScanAllLinux = TRUE,
    .FoldLinuxKernels = TRUE,
    .BootLogoClear = TRUE,
    .RescanDXE = TRUE,
    .SetAppleFB = TRUE,
    .GraphicsFor = GRAPHICS_FOR_NONE,
    .RequestedTextMode = DONT_CHANGE_TEXT_MODE,
    .RequestedScreenWidth = 0,
    .RequestedScreenHeight = 0,
    .DisableBootLogo = 0,
    .HideUIFlags = 0,
    .SyncTrust = 0,
    .MaxTags = 0,
    .ScanDelay = 0,
    .SyncNVram = 0,
    .ScreensaverTime = 60,
    .Timeout = 5,
    .DynamicCSR = 0,
    .ScreenR = -1,
    .ScreenG = -1,
    .ScreenB = -1,
    .DiscoveredRoot = NULL,
    .SelfDevicePath = NULL,
    .ToolLocations = NULL,
    .ToolLocationsExtra = NULL,
    .ConfigFilename = NULL,
    .DefaultSelection = L"+",
    .AlsoScan = NULL,
    .DontScanVolumes = NULL,
    .DontScanDirs = NULL,
    .DontScanFiles = NULL,
    .DontScanTools = NULL,
    .DontScanFirmware = NULL,
    .WindowsRecoveryFiles = NULL,
    .MacOSRecoveryFiles = NULL,
    .FollowSymlinks = NULL,
    .DriverDirs = NULL,
    .SetBootArgs = NULL,
    .LinuxPrefixes = NULL,
    .LinuxMatchPatterns = NULL,
    .ExtraKernelVersionStrings = NULL,
    .SpoofOSXVersion = NULL,
    .CsrValues = NULL,
    .ShowTools = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}};

UINTN AppleFramebuffers = 0;
UINTN EfiMajorVersion = 0;
UINT32 AccessFlagsBoot = ACCESS_FLAGS_BOOT;
UINT32 AccessFlagsFull = ACCESS_FLAGS_FULL;
CHAR16 *OffsetNext = L"\n                   ";
CHAR16 *VendorInfo = NULL;
CHAR16 *AllToolLocations = NULL;
BOOLEAN gKernelStarted = FALSE;
BOOLEAN KeepTrustChain = FALSE;
BOOLEAN IsBoot = FALSE;
BOOLEAN ProtectOn = FALSE;
BOOLEAN OverrideSB = FALSE;
BOOLEAN OneMainLoop = FALSE;
BOOLEAN BlockRescan = FALSE;
BOOLEAN NativeLogger = FALSE;
BOOLEAN AppleFirmware = FALSE;
BOOLEAN FlushFailedTag = FALSE;
BOOLEAN FlushFailReset = FALSE;
BOOLEAN WarnVersionEFI = FALSE;
BOOLEAN WarnRevisionUEFI = FALSE;
BOOLEAN WarnMissingQVInfo = FALSE;
BOOLEAN VarNoCheckCompat = FALSE;
BOOLEAN VarNoCheckAMFI = FALSE;
BOOLEAN VarDisablePanicLog = FALSE;
BOOLEAN SecureBootFailure = FALSE;
EG_PIXEL BGColorFail = COLOR_RED;
EG_PIXEL BGColorWarn = COLOR_AMBER;
EG_PIXEL BGColorBase = COLOR_LIGHTBLUE;
EFI_GUID MeridianGuid = MERIDIAN_GUID_VALUE;
EFI_GUID OpenCoreVendorGuid = OPENCORE_VENDOR_GUID;
EFI_GUID MicrosoftVendorGuid = MICROSOFT_VENDOR_GUID;
EFI_SET_VARIABLE OrigSetVariableRT = NULL;
EFI_OPEN_PROTOCOL OrigOpenProtocolBS = NULL;

UINTN RecoveryMacEntryItemsCount = 0;
UINTN RecoveryWinEntryItemsCount = 0;
UINTN MemTestEntryItemsCount = 0;
UINTN NetBootEntryItemsCount = 0;
UINTN ShellEntryItemsCount = 0;
UINTN MOKEntryItemsCount = 0;
UINTN GDiskEntryItemsCount = 0;
UINTN GPTSyncEntryItemsCount = 0;
UINTN FwUpdateEntryItemsCount = 0;
UINTN CydiaEntryItemsCount = 0;

LOADER_ENTRY **RecoveryMacEntryItems = NULL;
LOADER_ENTRY **RecoveryWinEntryItems = NULL;
LOADER_ENTRY **MemTestEntryItems = NULL;
LOADER_ENTRY **NetBootEntryItems = NULL;
LOADER_ENTRY **ShellEntryItems = NULL;
LOADER_ENTRY **MOKEntryItems = NULL;
LOADER_ENTRY **GDiskEntryItems = NULL;
LOADER_ENTRY **GPTSyncEntryItems = NULL;
LOADER_ENTRY **FwUpdateEntryItems = NULL;
LOADER_ENTRY **CydiaEntryItems = NULL;
