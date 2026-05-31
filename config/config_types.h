// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2020-2026 Dayo Akanji
// SPDX-FileCopyrightText: 2012-2023 Roderick W. Smith
// SPDX-FileCopyrightText: 2006-2009 Christoph Pfisterer

#ifndef __MERIDIAN_CONFIG_TYPES_H_
#define __MERIDIAN_CONFIG_TYPES_H_

#include "tiano_includes.h"
#include "volume_types.h"
#include "menu_types.h"

typedef struct _uint32_list
{
    UINT32 Value;
    struct _uint32_list *Next;
} UINT32_LIST;

typedef struct
{
    BOOLEAN DirectBoot;
    BOOLEAN MenuCache;
    BOOLEAN CustomScreenBG;
    BOOLEAN TextOnly;
    BOOLEAN LogToSerial;
    BOOLEAN MeasuredBoot;
    BOOLEAN EnableAndLockVMX;
    BOOLEAN PersistBootArgs;
    BOOLEAN TransientBoot;
    BOOLEAN ReloadGOP;
    BOOLEAN UseDirectGop;
    BOOLEAN NormaliseCSR;
    BOOLEAN ShutdownAfterTimeout;
    BOOLEAN Install;
    BOOLEAN WriteSystemdVars;
    BOOLEAN HandleVentoy;
    BOOLEAN MitigatePrimedBuffer;

    BOOLEAN ScanLimine;

    BOOLEAN ScanBtrfsSnapshots;
    BOOLEAN NetworkProbe;
    BOOLEAN NetworkDhcp;
    BOOLEAN ContinueOnWarning;
    BOOLEAN ForceTRIM;
    BOOLEAN DisableCheckAMFI;
    BOOLEAN DisableCheckCompat;
    BOOLEAN DisableNvramPanicLog;
    BOOLEAN GzippedLoaders;
    BOOLEAN SupplyUEFI;
    BOOLEAN SupplyNVME;
    BOOLEAN SupplyAPFS;
    BOOLEAN SyncAPFS;
    BOOLEAN ScanAllESP;
    BOOLEAN ScanAllLinux;
    BOOLEAN FoldLinuxKernels;
    BOOLEAN BootLogoClear;
    BOOLEAN RescanDXE;
    BOOLEAN SetAppleFB;
    UINTN GraphicsFor;
    UINTN RequestedTextMode;
    UINTN RequestedScreenWidth;
    UINTN RequestedScreenHeight;
    UINTN DisableBootLogo;
    UINTN HideUIFlags;
    UINTN SyncTrust;
    UINTN MaxTags;
    UINTN ScanDelay;
    UINTN SyncNVram;
    INTN ScreensaverTime;
    INTN Timeout;
    INTN DynamicCSR;
    INTN ScreenR;
    INTN ScreenG;
    INTN ScreenB;
    MERIDIAN_VOLUME *DiscoveredRoot;
    EFI_DEVICE_PATH_PROTOCOL *SelfDevicePath;
    CHAR16 *ToolLocations;
    CHAR16 *ToolLocationsExtra;
    CHAR16 *ConfigFilename;
    CHAR16 *DefaultSelection;
    CHAR16 *AlsoScan;
    CHAR16 *DontScanVolumes;
    CHAR16 *DontScanDirs;
    CHAR16 *DontScanFiles;
    CHAR16 *DontScanTools;
    CHAR16 *DontScanFirmware;
    CHAR16 *WindowsRecoveryFiles;
    CHAR16 *MacOSRecoveryFiles;
    CHAR16 *FollowSymlinks;
    CHAR16 *DriverDirs;
    CHAR16 *SetBootArgs;
    CHAR16 *LinuxPrefixes;
    CHAR16 *LinuxMatchPatterns;
    CHAR16 *ExtraKernelVersionStrings;
    CHAR16 *SpoofOSXVersion;
    UINT32_LIST *CsrValues;
    UINTN ShowTools[NUM_TOOLS];
    CHAR8 ScanFor[NUM_SCAN_OPTIONS];
} MERIDIAN_CONFIG;

#endif
