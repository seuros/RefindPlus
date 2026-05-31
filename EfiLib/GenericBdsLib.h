// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: Intel Corporation

#ifndef _GENERIC_BDS_LIB_H_
#define _GENERIC_BDS_LIB_H_

#define VAR_LEGACY_DEV_ORDER L"LegacyDevOrder"

#define FRONT_PAGE_QUESTION_ID  0x0000
#define FRONT_PAGE_DATA_WIDTH   0x01

#define CONSOLE_OUT 0x00000001
#define STD_ERROR   0x00000002
#define CONSOLE_IN  0x00000004
#define CONSOLE_ALL (CONSOLE_OUT | CONSOLE_IN | STD_ERROR)

#define LOAD_OPTION_ACTIVE              0x00000001
#define LOAD_OPTION_FORCE_RECONNECT     0x00000002

#define LOAD_OPTION_HIDDEN              0x00000008
#define LOAD_OPTION_CATEGORY            0x00001F00

#define LOAD_OPTION_CATEGORY_BOOT       0x00000000
#define LOAD_OPTION_CATEGORY_APP        0x00000100

#define EFI_BOOT_OPTION_SUPPORT_KEY     0x00000001
#define EFI_BOOT_OPTION_SUPPORT_APP     0x00000002

#define IS_LOAD_OPTION_TYPE(_c, _Mask)  (BOOLEAN) (((_c) & (_Mask)) != 0)

#define MAX_CHAR            480
#define MAX_CHAR_SIZE       (MAX_CHAR * 2)

#define BOOT_OPTION_MAX_CHAR 10

#define BDS_LOAD_OPTION_SIGNATURE SIGNATURE_32 ('B', 'd', 'C', 'O')

typedef struct {

  UINTN                     Signature;
  LIST_ENTRY                Link;

  EFI_DEVICE_PATH_PROTOCOL  *DevicePath;

  CHAR16                    *OptionName;
  UINTN                     OptionNumber;
  UINT16                    BootCurrent;
  UINT32                    Attribute;
  CHAR16                    *Description;
  VOID                      *LoadOptions;
  UINT32                    LoadOptionsSize;
  CHAR16                    *StatusString;

} BDS_COMMON_OPTION;

typedef struct {
  EFI_DEVICE_PATH_PROTOCOL  *DevicePath;
  UINTN                     ConnectType;
} BDS_CONSOLE_CONNECT_ENTRY;

VOID
EFIAPI
BdsLibBootNext (
  VOID
  );

EFI_STATUS
EFIAPI
BdsLibBootViaBootOption (
  IN  BDS_COMMON_OPTION             * Option,
  IN  EFI_DEVICE_PATH_PROTOCOL      * DevicePath,
  OUT UINTN                         *ExitDataSize,
  OUT CHAR16                        **ExitData OPTIONAL
  );

EFI_STATUS
EFIAPI
BdsLibEnumerateAllBootOption (
  IN OUT LIST_ENTRY          *BdsBootOptionList
  );

VOID
EFIAPI
BdsLibBuildOptionFromHandle (
  IN  EFI_HANDLE                 Handle,
  IN  LIST_ENTRY                 *BdsBootOptionList,
  IN  CHAR16                     *String
  );

VOID
EFIAPI
BdsLibBuildOptionFromShell (
  IN EFI_HANDLE                  Handle,
  IN OUT LIST_ENTRY              *BdsBootOptionList
  );

VOID
EFIAPI
BdsLibLoadDrivers (
  IN LIST_ENTRY                   *BdsDriverLists
  );

EFI_STATUS
EFIAPI
BdsLibBuildOptionFromVar (
  IN  LIST_ENTRY                      *BdsCommonOptionList,
  IN  CHAR16                          *VariableName
  );

VOID *
BdsLibGetVariableAndSize (
  IN  CHAR16              *Name,
  IN  EFI_GUID            *VendorGuid,
  OUT UINTN               *VariableSize
  );

BDS_COMMON_OPTION *
BdsLibVariableToOption (
  IN OUT LIST_ENTRY                   *BdsCommonOptionList,
  IN  CHAR16                          *VariableName
  );

EFI_STATUS
BdsLibConnectDevicePath (
  IN EFI_DEVICE_PATH_PROTOCOL  *DevicePathToConnect
  );

VOID
EFIAPI
BdsLibConnectAllDriversToAllControllers (IN BOOLEAN ResetGOP);

EFI_STATUS
EFIAPI
BdsLibConnectAllDefaultConsoles (
  VOID
  );

EFI_STATUS
EFIAPI
BdsLibUpdateConsoleVariable (
  IN  CHAR16                    *ConVarName,
  IN  EFI_DEVICE_PATH_PROTOCOL  *CustomizedConDevicePath,
  IN  EFI_DEVICE_PATH_PROTOCOL  *ExclusiveDevicePath
  );

EFI_STATUS
EFIAPI
BdsLibConnectConsoleVariable (
  IN  CHAR16                 *ConVarName
  );

EFI_DEVICE_PATH_PROTOCOL *
EFIAPI
BdsLibDelPartMatchInstance (
  IN     EFI_DEVICE_PATH_PROTOCOL  *Multi,
  IN     EFI_DEVICE_PATH_PROTOCOL  *Single
  );

BOOLEAN
EFIAPI
BdsLibMatchDevicePaths (
  IN  EFI_DEVICE_PATH_PROTOCOL  *Multi,
  IN  EFI_DEVICE_PATH_PROTOCOL  *Single
  );

CHAR16 *
EFIAPI
DevicePathToStr (
  IN EFI_DEVICE_PATH_PROTOCOL     *DevPath
  );

typedef struct {
  CHAR16  *Str;
  UINTN   Len;
  UINTN   Maxlen;
} POOL_PRINT;

typedef
VOID
(*DEV_PATH_FUNCTION) (
  IN OUT POOL_PRINT       *Str,
  IN VOID                 *DevPath
  );

typedef struct {
  UINT8             Type;
  UINT8             SubType;
  DEV_PATH_FUNCTION Function;
} DEVICE_PATH_STRING_TABLE;

typedef struct {
  EFI_DEVICE_PATH_PROTOCOL  Header;
  EFI_GUID                  Guid;
  UINT8                     VendorDefinedData[1];
} VENDOR_DEVICE_PATH_WITH_DATA;

typedef struct {
  EFI_DEVICE_PATH_PROTOCOL  Header;
  UINT16                    NetworkProtocol;
  UINT16                    LoginOption;
  UINT64                    Lun;
  UINT16                    TargetPortalGroupTag;
  CHAR16                    TargetName[1];
} ISCSI_DEVICE_PATH_WITH_NAME;

#if defined(MDE_CPU_IA32) || defined(MDE_CPU_X64)
#define REFRESH_LEGACY_BOOT_OPTIONS \
        BdsDeleteAllInvalidLegacyBootOptions ();\
        BdsAddNonExistingLegacyBootOptions (); \
        BdsUpdateLegacyDevOrder ()
#else
#define REFRESH_LEGACY_BOOT_OPTIONS
#endif

EFI_STATUS
BdsDeleteAllInvalidLegacyBootOptions (
  VOID
  );

EFI_STATUS
BdsAddNonExistingLegacyBootOptions (
  VOID
  );

EFI_STATUS
EFIAPI
BdsUpdateLegacyDevOrder (
  VOID
  );

EFI_STATUS
EFIAPI
BdsRefreshBbsTableForBoot (
  IN BDS_COMMON_OPTION        *Entry
  );

EFI_STATUS
BdsDeleteBootOption (
  IN UINTN                       OptionNumber,
  IN OUT UINT16                  *BootOrder,
  IN OUT UINTN                   *BootOrderSize
  );

VOID
EFIAPI
EnableResetReminderFeature (
  VOID
  );

VOID
EFIAPI
DisableResetReminderFeature (
  VOID
  );

VOID
EFIAPI
EnableResetRequired (
  VOID
  );

VOID
EFIAPI
DisableResetRequired (
  VOID
  );

BOOLEAN
EFIAPI
IsResetReminderFeatureEnable (
  VOID
  );

BOOLEAN
EFIAPI
IsResetRequired (
  VOID
  );

VOID
EFIAPI
SetupResetReminder (
  VOID
  );

#define  BDS_EFI_ACPI_FLOPPY_BOOT         0x0201

#define  BDS_EFI_MESSAGE_ATAPI_BOOT       0x0301
#define  BDS_EFI_MESSAGE_SCSI_BOOT        0x0302
#define  BDS_EFI_MESSAGE_USB_DEVICE_BOOT  0x0305
#define  BDS_EFI_MESSAGE_SATA_BOOT        0x0312
#define  BDS_EFI_MESSAGE_MAC_BOOT         0x030b
#define  BDS_EFI_MESSAGE_MISC_BOOT        0x03FF

#define  BDS_EFI_MEDIA_HD_BOOT            0x0401
#define  BDS_EFI_MEDIA_CDROM_BOOT         0x0402

#define  BDS_LEGACY_BBS_BOOT              0x0501

#define  BDS_EFI_UNSUPPORT                0xFFFF

BOOLEAN
EFIAPI
MatchPartitionDevicePathNode (
  IN  EFI_DEVICE_PATH_PROTOCOL   *BlockIoDevicePath,
  IN  HARDDRIVE_DEVICE_PATH      *HardDriveDevicePath
  );

EFI_DEVICE_PATH_PROTOCOL *
EFIAPI
BdsExpandPartitionPartialDevicePathToFull (
  IN  HARDDRIVE_DEVICE_PATH      *HardDriveDevicePath
  );

EFI_HANDLE
EFIAPI
BdsLibGetBootableHandle (
  IN  EFI_DEVICE_PATH_PROTOCOL      *DevicePath
  );

BOOLEAN
EFIAPI
BdsLibIsValidEFIBootOptDevicePath (
  IN EFI_DEVICE_PATH_PROTOCOL     *DevPath,
  IN BOOLEAN                      CheckMedia
  );

BOOLEAN
EFIAPI
BdsLibIsValidEFIBootOptDevicePathExt (
  IN EFI_DEVICE_PATH_PROTOCOL     *DevPath,
  IN BOOLEAN                      CheckMedia,
  IN CHAR16                       *Description
  );

UINT32
EFIAPI
BdsGetBootTypeFromDevicePath (
  IN  EFI_DEVICE_PATH_PROTOCOL     *DevicePath
  );

VOID
EFIAPI
BdsLibSaveMemoryTypeInformation (
  VOID
  );

EFI_STATUS
EFIAPI
BdsLibUpdateFvFileDevicePath (
  IN  OUT EFI_DEVICE_PATH_PROTOCOL      ** DevicePath,
  IN  EFI_GUID                          *FileGuid
  );

EFI_STATUS
EFIAPI
BdsLibConnectUsbDevByShortFormDP(
  IN UINT8                      HostControllerPI,
  IN EFI_DEVICE_PATH_PROTOCOL   *RemainingDevicePath
  );

VOID
DevPathVendor (
  IN OUT POOL_PRINT       *Str,
  IN VOID                 *DevPath
  );

CHAR16 *
EFIAPI
MrdStrAppendFmt (
  IN OUT POOL_PRINT   *Str,
  IN CHAR16           *Fmt,
  ...
  );

EFI_STATUS
EFIAPI
EnableQuietBoot (
  IN  EFI_GUID  *LogoFile
  );

EFI_STATUS
EFIAPI
DisableQuietBoot (
  VOID
  );

#endif
