// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2020-2025 Dayo Akanji
// SPDX-FileCopyrightText: 2021 Joe van Tunen
// SPDX-FileCopyrightText: 2012-2020 Roderick W. Smith
// SPDX-FileCopyrightText: 2006-2009 Christoph Pfisterer

#ifndef __LIB_H_
#define __LIB_H_

#include "tiano_includes.h"

#include "global.h"
#include "meridian_funcs.h"

typedef struct
{
    EFI_STATUS LastStatus;
    EFI_FILE_HANDLE DirHandle;
    BOOLEAN CloseDirHandle;

    EFI_FILE_INFO **Cache;
    UINTN CacheCount;
    UINTN CacheIndex;
} MERIDIAN_DIR_ITER;

#define DISK_KIND_INTERNAL (0)
#define DISK_KIND_EXTERNAL (1)
#define DISK_KIND_OPTICAL (2)
#define DISK_KIND_NET (3)

#define VOL_UNREADABLE 999

#define IS_EXTENDED_PART_TYPE(type) ((type) == 0x05 || (type) == 0x0f || (type) == 0x85)

#define GPT_READ_ONLY 0x1000000000000000
#define GPT_NO_AUTOMOUNT 0x8000000000000000

#define IGNORE_PARTITION_NAMES L"Microsoft basic data,Linux filesystem,Apple HFS/HFS+"

#if MERIDIAN_DEBUG > 0
#define NVRAM_LOG_GET L"Get Item from"
#define NVRAM_LOG_SET L"Put Item into"

#define NVRAM_TITLE L"Variable Storage"

#define NVRAM_HARDWARE (NVRAM_TITLE L" (Hardware)")
#define NVRAM_EMULATED (NVRAM_TITLE L" (Emulated)")
#endif

INTN FindMem(IN VOID *Buffer, IN UINTN BufferLength, IN VOID *SearchString,
             IN UINTN SearchStringLength);

EFI_STATUS FindVarsDir(VOID);
EFI_STATUS ReinitMeridianLib(VOID);
EFI_STATUS InitMeridianLib(IN EFI_HANDLE ImageHandle);
EFI_STATUS DirIterClose(IN OUT MERIDIAN_DIR_ITER *DirIter);
EFI_STATUS MrdFindESP(OUT EFI_FILE_HANDLE *RootDir);
EFI_STATUS MrdLoadFile(IN EFI_FILE_PROTOCOL *BaseDir, IN CHAR16 *FileName, OUT UINT8 **FileData,
                       OUT UINTN *FileDataLength);
EFI_STATUS MrdSaveFile(IN EFI_FILE_PROTOCOL *BaseDir OPTIONAL, IN CHAR16 *FileName,
                       IN UINT8 *FileData, IN UINTN FileDataLength);

VOID ClearDirCache(VOID);
EFI_STATUS EfivarGetRaw(IN EFI_GUID *VendorGUID, IN CHAR16 *VariableName, OUT VOID **VariableData,
                        OUT UINTN *VariableSize OPTIONAL);
EFI_STATUS EfivarSetRaw(IN EFI_GUID *VendorGUID, IN CHAR16 *VariableName, IN VOID *VariableData,
                        IN UINTN VariableSize, IN BOOLEAN Persistent);

VOID ScanVolumes(VOID);
VOID ReinitVolumes(VOID);

VOID UninitVolume(IN OUT MERIDIAN_VOLUME **Volume);
VOID ReinitVolume(IN OUT MERIDIAN_VOLUME **Volume);
VOID UninitMeridianLib(VOID);
VOID FreeSyncVolumes(VOID);
VOID FreeVolume(MERIDIAN_VOLUME **Volume);
VOID SanitiseVolumeName(MERIDIAN_VOLUME **Volume);
VOID EraseUint32List(IN UINT32_LIST **TheList);
VOID CleanUpPathNameSlashes(IN OUT CHAR16 *PathName);
VOID FreeList(IN OUT VOID ***ListPtr, IN OUT UINTN *ElementCount);
VOID AddListElement(IN OUT VOID ***ListPtr, IN OUT UINTN *ElementCount, IN VOID *NewElement);
VOID SplitPathName(IN CHAR16 *InPath, IN OUT CHAR16 **VolName, IN OUT CHAR16 **Path,
                   IN OUT CHAR16 **Filename);
VOID DirIterOpen(IN EFI_FILE_PROTOCOL *BaseDir, IN CHAR16 *RelativePath OPTIONAL,
                 OUT MERIDIAN_DIR_ITER *DirIter);
VOID FindVolumeAndFilename(IN EFI_DEVICE_PATH_PROTOCOL *loadpath,
                           OUT MERIDIAN_VOLUME **DeviceVolume, OUT CHAR16 **loader);

CHAR16 *Basename(IN CHAR16 *Path);
CHAR16 *FindPath(IN CHAR16 *FullPath);
CHAR16 *FindExtension(IN CHAR16 *Path);
CHAR16 *FindLastDirName(IN CHAR16 *Path);
CHAR16 *StripEfiExtension(IN CHAR16 *FileName);
CHAR16 *StripSetExtension(IN CHAR16 *Extension, IN CHAR16 *FileName);
CHAR16 *GetVolumeName(IN MERIDIAN_VOLUME *Volume);
CHAR16 *SplitDeviceString(IN OUT CHAR16 *InString);
CHAR16 *MeridianGetBootPathName(IN EFI_DEVICE_PATH_PROTOCOL *DevicePath);

BOOLEAN EjectMedia(VOID);
BOOLEAN HasWindowsBiosBootFiles(IN MERIDIAN_VOLUME *Volume);
BOOLEAN GuidsAreEqual(IN EFI_GUID *Guid1, IN EFI_GUID *Guid2);
BOOLEAN MeridianMetaiMatch(IN CHAR16 *String, IN CHAR16 *Pattern);
BOOLEAN FindVolume(IN MERIDIAN_VOLUME **Volume, IN CHAR16 *Identifier);
BOOLEAN FileExists(IN EFI_FILE_PROTOCOL *BaseDir, IN CHAR16 *RelativePath);
BOOLEAN SplitVolumeAndFilename(IN OUT CHAR16 **Path, OUT CHAR16 **VolName);
BOOLEAN VolumeScanAllowed(IN MERIDIAN_VOLUME *Volume, IN BOOLEAN SkipVentoy,
                          IN BOOLEAN SkipRootDir);
BOOLEAN VolumeMatchesDescription(IN MERIDIAN_VOLUME *Volume, IN CHAR16 *Description);
BOOLEAN FilenameIn(IN MERIDIAN_VOLUME *Volume, IN CHAR16 *Directory, IN CHAR16 *Filename,
                   IN CHAR16 *List);
BOOLEAN DirIterNext(IN OUT MERIDIAN_DIR_ITER *DirIter, IN UINTN FilterMode,
                    IN CHAR16 *FilePattern OPTIONAL, OUT EFI_FILE_INFO **DirEntry);

MERIDIAN_VOLUME *CopyVolume(IN MERIDIAN_VOLUME *VolumeToCopy);
#endif
