// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#ifndef __SYSINFO_H_
#define __SYSINFO_H_

#include "tiano_includes.h"

typedef enum
{
    FW_VENDOR_UNKNOWN = 0,
    FW_VENDOR_APPLE,
    FW_VENDOR_AMI,
    FW_VENDOR_COREBOOT,
    FW_VENDOR_INSYDE,
    FW_VENDOR_PHOENIX,
    FW_VENDOR_EDK2,
} FIRMWARE_VENDOR_TYPE;

typedef enum
{
    DISK_BUS_UNKNOWN = 0,
    DISK_BUS_NVME,
    DISK_BUS_SATA,
    DISK_BUS_USB,
    DISK_BUS_SCSI,
    DISK_BUS_FIREWIRE,
    DISK_BUS_VIRTUAL,
} DISK_BUS_TYPE;

typedef struct
{
    DISK_BUS_TYPE BusType;
    UINT64 SizeBytes;
    BOOLEAN Removable;
    BOOLEAN Present;
    CHAR8 Model[41];
    CHAR8 Serial[21];
} MERIDIAN_DISK_INFO;

typedef struct
{
    UINT64 SizeMB;
    UINT16 SpeedMTs;
    UINT8 MemoryType;
    UINT8 FormFactor;
    BOOLEAN IsEcc;
    BOOLEAN Populated;
    BOOLEAN Soldered;
    CHAR8 Slot[32];
    CHAR8 Maker[64];
} MERIDIAN_DIMM_INFO;

typedef struct
{
    BOOLEAN Present;
    BOOLEAN Charging;
    BOOLEAN FullyCharged;
    BOOLEAN FullyDischarged;
    UINT8 Percent;
    UINT16 MinutesLeft;
} MERIDIAN_BATTERY_INFO;

typedef struct
{
    BOOLEAN Present;
    CHAR8 Model[128];
    UINT16 SpeedMHz;
    UINT8 Cores;
    UINT8 Threads;
} MERIDIAN_CPU_INFO;

typedef struct
{
    BOOLEAN Present;
    UINT16 VendorId;
    UINT16 DeviceId;
    CHAR8 VendorName[16];
    UINT64 VramBytes;
} MERIDIAN_GPU_INFO;

typedef struct
{
    BOOLEAN Present;
    BOOLEAN FromEdid;

    CHAR8 Name[14];
    UINT16 HorzRes;
    UINT16 VertRes;

    UINT16 EdidSize;
    UINT8 EdidRaw[512];
} MERIDIAN_DISPLAY_INFO;

typedef struct
{
    BOOLEAN Present;
    BOOLEAN IsV2;
    BOOLEAN Active;
} MERIDIAN_TPM_INFO;

typedef struct
{
    BOOLEAN Present;
    CHAR8 Manufacturer[64];
    CHAR8 ProductName[64];
    CHAR8 BiosVendor[64];
    CHAR8 BiosVersion[32];
} MERIDIAN_SYSTEM_INFO;

extern FIRMWARE_VENDOR_TYPE MeridianFirmwareVendor;
extern UINTN MeridianDiskCount;
extern MERIDIAN_DISK_INFO *MeridianDisks;
extern UINTN MeridianDimmCount;
extern MERIDIAN_DIMM_INFO *MeridianDimms;
extern UINT64 MeridianTotalRamBytes;

extern BOOLEAN MeridianMemorySoldered;
extern UINT64 MeridianSolderedTotalMB;
extern UINT8 MeridianSolderedMemType;
extern MERIDIAN_BATTERY_INFO MeridianBattery;
extern MERIDIAN_CPU_INFO MeridianCpu;
extern UINTN MeridianGpuCount;
extern MERIDIAN_GPU_INFO *MeridianGpus;
extern MERIDIAN_DISPLAY_INFO MeridianDisplay;
extern MERIDIAN_TPM_INFO MeridianTpm;
extern MERIDIAN_SYSTEM_INFO MeridianSystem;
extern CHAR16 *MeridianSystemLabel;

FIRMWARE_VENDOR_TYPE DetectFirmwareVendor(VOID);
VOID ScanDisks(VOID);
VOID ScanMemory(VOID);
VOID ScanBattery(VOID);
VOID ScanCpu(VOID);
VOID ScanGpu(VOID);
VOID LogDeviceInventory(VOID);
VOID PermafrostTame(VOID);
VOID ScanDisplay(VOID);

VOID DumpDisplayEdid(VOID);

VOID DumpDisplayModes(VOID);
VOID ScanTpm(VOID);
VOID ScanSystemInfo(VOID);

CHAR16 *FirmwareVendorName(VOID);
CHAR16 *SystemInfoString(VOID);
CHAR16 *DiskBusName(IN DISK_BUS_TYPE BusType);
CHAR16 *MemoryTypeName(IN UINT8 MemType);
CHAR8 *MemoryTypeNameA(IN UINT8 MemType);
CHAR16 *GpuVendorName(IN UINT16 VendorId);

#endif
