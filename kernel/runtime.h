// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#ifndef __MERIDIAN_RUNTIME_H_
#define __MERIDIAN_RUNTIME_H_

#include "tiano_includes.h"
#include "config_types.h"
#include "loader_types.h"

extern INT16 NowYear;
extern INT16 NowMonth;
extern INT16 NowDay;
extern INT16 NowHour;
extern INT16 NowMinute;
extern INT16 NowSecond;

extern CHAR16 *OffsetNext;
extern CHAR16 *SelfDirPath;
extern CHAR16 *SelfBinaryDirPath;
extern CHAR16 *SelfBaseName;
extern CHAR16 *SelfToolPath;
extern CHAR16 *VendorInfo;
extern CHAR16 *AllToolLocations;

extern UINTN PadPosition;
extern UINTN VolumesCount;
extern UINTN RecoveryVolumesAPFSCount;
extern UINTN RecoveryVolumesHFSCount;
extern UINTN SkipApfsVolumesCount;
extern UINTN PreBootVolumesCount;
extern UINTN SystemVolumesCount;
extern UINTN DataVolumesCount;
extern UINTN AppleFramebuffers;
extern UINTN EfiMajorVersion;
extern UINTN RecoveryMacEntryItemsCount;
extern UINTN RecoveryWinEntryItemsCount;
extern UINTN MemTestEntryItemsCount;
extern UINTN NetBootEntryItemsCount;
extern UINTN ShellEntryItemsCount;
extern UINTN MOKEntryItemsCount;
extern UINTN GDiskEntryItemsCount;
extern UINTN GPTSyncEntryItemsCount;
extern UINTN FwUpdateEntryItemsCount;

extern UINT32 AccessFlagsFull;
extern UINT32 AccessFlagsBoot;

extern UINT64 MeridianReadOnly;
extern UINT64 MeridianReadWrite;
extern UINT64 MeridianReadWriteCreate;

extern BOOLEAN SingleAPFS;
extern BOOLEAN MuteLogger;
extern BOOLEAN NativeLogger;
extern BOOLEAN AppleFirmware;
extern BOOLEAN DevicePresence;
extern BOOLEAN gKernelStarted;
extern BOOLEAN KeepTrustChain;
extern BOOLEAN IsBoot;
extern BOOLEAN ProtectOn;
extern BOOLEAN OverrideSB;
extern BOOLEAN OneMainLoop;
extern BOOLEAN BlockRescan;
extern BOOLEAN FlushFailedTag;
extern BOOLEAN FlushFailReset;
extern BOOLEAN WarnVersionEFI;
extern BOOLEAN WarnRevisionUEFI;
extern BOOLEAN WarnMissingQVInfo;
extern BOOLEAN VarNoCheckCompat;
extern BOOLEAN VarNoCheckAMFI;
extern BOOLEAN VarDisablePanicLog;
extern BOOLEAN SecureBootFailure;

extern EFI_FILE_PROTOCOL *SelfDir;
extern EFI_FILE_PROTOCOL *SelfRootDir;

extern EFI_GUID GuidESP;
extern EFI_GUID GuidNull;
extern EFI_GUID GlobalGuid;
extern EFI_GUID AppleBootGuid;
extern EFI_GUID OpenCoreVendorGuid;
extern EFI_GUID MicrosoftVendorGuid;
extern EFI_GUID MeridianGuid;
extern EFI_GUID gEfiLegacyBootProtocolGuid;

extern EG_PIXEL BGColorBase;
extern EG_PIXEL BGColorWarn;
extern EG_PIXEL BGColorFail;

extern EFI_HANDLE SelfImageHandle;

extern EFI_LOADED_IMAGE_PROTOCOL *SelfLoadedImage;

extern MERIDIAN_VOLUME *SelfVolume;
extern MERIDIAN_VOLUME **Volumes;
extern MERIDIAN_VOLUME **RecoveryVolumesAPFS;
extern MERIDIAN_VOLUME **RecoveryVolumesHFS;
extern MERIDIAN_VOLUME **SkipApfsVolumes;
extern MERIDIAN_VOLUME **PreBootVolumes;
extern MERIDIAN_VOLUME **SystemVolumes;
extern MERIDIAN_VOLUME **DataVolumes;

extern MERIDIAN_CONFIG GlobalConfig;

extern MERIDIAN_MENU_SCREEN *MainMenu;

extern EFI_SET_VARIABLE OrigSetVariableRT;
extern EFI_OPEN_PROTOCOL OrigOpenProtocolBS;

extern LOADER_ENTRY **RecoveryMacEntryItems;
extern LOADER_ENTRY **RecoveryWinEntryItems;
extern LOADER_ENTRY **MemTestEntryItems;
extern LOADER_ENTRY **NetBootEntryItems;
extern LOADER_ENTRY **ShellEntryItems;
extern LOADER_ENTRY **MOKEntryItems;
extern LOADER_ENTRY **GDiskEntryItems;
extern LOADER_ENTRY **GPTSyncEntryItems;
extern LOADER_ENTRY **FwUpdateEntryItems;

#endif
