// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2020-2025 Dayo Akanji
// SPDX-FileCopyrightText: 2021 Joe van Tunen
// SPDX-FileCopyrightText: 2012-2021 Roderick W. Smith
// SPDX-FileCopyrightText: 2006-2009 Christoph Pfisterer
// SPDX-FileCopyrightText: 2019 vit9696

#include "global.h"
#include "gpt.h"
#include "lib.h"
#include "scan.h"
#include "apple.h"
#include "config.h"
#include "screenmgt.h"
#include "mystrings.h"
#include "RemovableMedia.h"

#define LibLocateHandle gBS->LocateHandleBuffer
#define DevicePathProtocol gEfiDevicePathProtocolGuid
#define BlockIoProtocol gEfiBlockIoProtocolGuid
#define LibFileSystemInfo EfiLibFileSystemInfo
#define LibOpenRoot EfiLibOpenRoot

#define FAT_MAGIC 0xAA55
#define EXT2_SUPER_MAGIC 0xEF53
#define HFSPLUS_MAGIC0 0x4442
#define HFSPLUS_MAGIC1 0x2B48
#define HFSPLUS_MAGIC2 0x5848
#define BTRFS_SIGNATURE "_BHRfS_M"
#define BCACHEFS_SUPER_OFFSET 4096
#define BCACHEFS_MAGIC_OFFSET 24
#define BCACHEFS_UUID_OFFSET 56
#define SAMPLE_SIZE 69632
#define UFS1_SUPER_MAGIC 0x00011954U
#define UFS2_SUPER_MAGIC 0x19540119U
#define UFS_SUPER_MAGIC_OFFSET 1372
#define UFS1_SUPER_OFFSET 8192
#define UFS2_SUPER_OFFSET 65536
#define SECTOR_SIZE 4096
#define BASE_SIZE 512

#define EXT2_SUPER_OFFSET 1024
#define EXT2_MAGIC_OFFSET 56
#define EXT2_COMPAT_OFFSET 92
#define EXT2_INCOMPAT_OFFSET 96
#define EXT2_UUID_OFFSET 104

#define EXT4_INCOMPAT_EXTENTS 0x0040
#define EXT4_INCOMPAT_FLEX_BG 0x0200
#define EXT3_COMPAT_HAS_JOURNAL 0x0004

#define BTRFS_SUPER_OFFSET 65536
#define BTRFS_MAGIC_OFFSET 64

#define JFS_SUPER_OFFSET 32768

#define HFSPLUS_SUPER_OFFSET 1024
#define HFSPLUS_UUID_OFFSET 104

#define MBR_SIG_OFFSET 510
#define NTFS_OEM_ID_OFFSET 3
#define NTFS_SERIAL_OFFSET 0x48
#define FAT32_BPB_SIG_OFFSET 0x52
#define FAT32_SERIAL_OFFSET 0x43
#define FAT16_BPB_SIG_OFFSET 0x36
#define FAT16_SERIAL_OFFSET 0x27

#define NETBSD_MAGIC_OFFSET 1028
#define NETBSD_BOOT_MAGIC 0x7886b6d1U

#define PARTITION_TABLE_TXT L"Found MBR Partition Table"
#define LEGACY_CODE_TXT L"Found Legacy Boot Code"
#define ENTRY_BELOW_TXT L" on Entry Below:"
#define AND_JOIN_TXT L" and "
#define BUT_JOIN_TXT L" but "
#define UNKNOWN_OS L"Unknown OS"

#define NAME_FIX(Name) MrdStrIncludesCI  ((*Volume)->VolName, Name)) VolumeName = Name
#define FAT_NAME(Name) MrdStrStartsWithCI (Name, (*Volume)->VolName)) VolumeName = Name
#define FIX_FLAG(Name) MrdStrEqualsCI   ((*Volume)->VolName, Name)) VolumeName = Name
#define XFS_SIGNATURE "XFSB"
#define JFS_SIGNATURE "JFS1"
#define NTFS_SIGNATURE "NTFS    "
#define FAT12_SIGNATURE "FAT12   "
#define FAT16_SIGNATURE "FAT16   "
#define FAT32_SIGNATURE "FAT32   "

extern EFI_GUID gRootGuid;
extern EFI_DEVICE_PATH_PROTOCOL EndDevicePath[];
extern EFI_GUID GuidHFS, GuidAPFS, GuidSwap, GuidHome, GuidLuks, GuidLinux;
extern EFI_GUID GuidBasicData, GuidApplTvRec, GuidContainHFS, GuidFlagAPFS;
extern EFI_GUID GuidMacRaidOn, GuidMacRaidOff, GuidRecoveryHD, GuidReservedMS, GuidWindowsRE;
extern BOOLEAN FoundExternalDisk, DoneHeadings, SkipSpacing, UseButJoin;
extern BOOLEAN SelfVolSet, SelfVolRun, MediaCheck, ValidAPFS, ScanMBR;
extern BOOLEAN FirstVolume;
extern BOOLEAN ScannedOnce;
extern BOOLEAN FoundMBR;

static UINT8 BcachefsMagic[16] = {0xc6, 0x85, 0x73, 0xf6, 0x66, 0xce, 0x90, 0xa9,
                                  0xd9, 0x6a, 0x60, 0xcf, 0x80, 0x3d, 0xf7, 0xef};

VOID SanitiseVolumeName(MERIDIAN_VOLUME **Volume)
{
    CHAR16 *VolumeName;

    VolumeName = NULL;
    if (Volume != NULL && *Volume != NULL && (*Volume)->VolName != NULL) {
        if (0)
            ;
        else if (NAME_FIX(L"EFI System Partition"        );
        else if (NAME_FIX(L"Whole Disk Volume"           );
        else if (NAME_FIX(L"Unknown Volume"              );
        else if (NAME_FIX(L"NTFS Volume"                 );
        else if (NAME_FIX(L"HFS+ Volume"                 );
        else if (FAT_NAME(L"FAT Volume"                  );
        else if (NAME_FIX(L"XFS Volume"                  );
        else if (NAME_FIX(L"PreBoot"                     );
        else if (FIX_FLAG(L"Swap"                        );
        else if (NAME_FIX(L"Ext4 Volume"                 );
        else if (NAME_FIX(L"Ext3 Volume"                 );
        else if (NAME_FIX(L"Ext2 Volume"                 );
        else if (FAT_NAME(L"ExFAT Volume"                );
        else if (NAME_FIX(L"BtrFS Volume"                );
        else if (NAME_FIX(L"ISO-9660 Volume"             );
        else if (NAME_FIX(L"System Reserved"             );
        else if (NAME_FIX(L"Basic Data Partition"        );
        else if (NAME_FIX(L"Microsoft Reserved Partition");
    }

    if (VolumeName != NULL) {
        MRD_FREE_POOL((*Volume)->VolName);
        (*Volume)->VolName = StrDuplicate(VolumeName);
    }
}

MERIDIAN_VOLUME *CopyVolume(IN MERIDIAN_VOLUME *VolumeToCopy)
{
    MERIDIAN_VOLUME *Volume;
    UINTN SizeMBR;

    if (VolumeToCopy == NULL) {
        return NULL;
    }

    UninitVolume(&VolumeToCopy);

    Volume = AllocateCopyPool(sizeof(MERIDIAN_VOLUME), VolumeToCopy);
    if (Volume != NULL) {
        Volume->FsName = StrDuplicate(VolumeToCopy->FsName);
        Volume->VolName = StrDuplicate(VolumeToCopy->VolName);
        Volume->PartName = StrDuplicate(VolumeToCopy->PartName);

        if (VolumeToCopy->DevicePath != NULL) {
            Volume->DevicePath = DuplicateDevicePath(VolumeToCopy->DevicePath);
        }

        if (VolumeToCopy->WholeDiskDevicePath != NULL) {
            Volume->WholeDiskDevicePath = DuplicateDevicePath(VolumeToCopy->WholeDiskDevicePath);
        }

        if (VolumeToCopy->MbrPartitionTable != NULL) {
            SizeMBR = 4 * 16;
            Volume->MbrPartitionTable = AllocatePool(SizeMBR);
            if (Volume->MbrPartitionTable != NULL) {
                gBS->CopyMem(Volume->MbrPartitionTable, VolumeToCopy->MbrPartitionTable, SizeMBR);
            }
        }

        ReinitVolume(&Volume);
    }

    ReinitVolume(&VolumeToCopy);

    return Volume;
}

VOID FreeVolume(MERIDIAN_VOLUME **Volume)
{
    if (Volume == NULL || *Volume == NULL) {
        return;
    }

    UninitVolume(Volume);

    MRD_FREE_POOL((*Volume)->FsName);
    MRD_FREE_POOL((*Volume)->VolName);
    MRD_FREE_POOL((*Volume)->PartName);
    MRD_FREE_POOL((*Volume)->DevicePath);
    MRD_FREE_POOL((*Volume)->MbrPartitionTable);
    MRD_FREE_POOL((*Volume)->WholeDiskDevicePath);

    MRD_FREE_POOL(*Volume);
}

VOID FreeSyncVolumes(VOID)
{
    if (!GlobalConfig.SyncAPFS) {
        return;
    }

    MRD_FREE_POOL(RecoveryVolumesAPFS);
    MRD_FREE_POOL(RecoveryVolumesHFS);
    MRD_FREE_POOL(SkipApfsVolumes);
    MRD_FREE_POOL(PreBootVolumes);
    MRD_FREE_POOL(SystemVolumes);
    MRD_FREE_POOL(DataVolumes);

    RecoveryVolumesAPFSCount = 0;
    RecoveryVolumesHFSCount = 0;
    SkipApfsVolumesCount = 0;
    PreBootVolumesCount = 0;
    SystemVolumesCount = 0;
    DataVolumesCount = 0;
}

static VOID FreeVolumes(VOID)
{
    UINTN i;

    for (i = 0; i < VolumesCount; i++) {
        FreeVolume(&Volumes[i]);
    }

    MRD_FREE_POOL(Volumes);
    VolumesCount = 0;
}

static CHAR16 *FSTypeName(IN MERIDIAN_VOLUME *Volume)
{
    UINTN i;
    CHAR16 *retval;
    CHAR16 *VentoyName;
    BOOLEAN FoundVentoy;

    switch (Volume->FSType) {
    case FS_TYPE_WHOLEDISK:
        retval = L"Whole Disk";
        break;
    case FS_TYPE_HFSPLUS:
        retval = L"HFS+";
        break;
    case FS_TYPE_APFS:
        retval = L"APFS";
        break;
    case FS_TYPE_NTFS:
        retval = L"NTFS";
        break;
    case FS_TYPE_EXT4:
        retval = L"Ext4";
        break;
    case FS_TYPE_EXT3:
        retval = L"Ext3";
        break;
    case FS_TYPE_EXT2:
        retval = L"Ext2";
        break;
    case FS_TYPE_FAT32:
        retval = L"FAT-32";
        break;
    case FS_TYPE_FAT16:
        retval = L"FAT-16";
        break;
    case FS_TYPE_FAT12:
        retval = L"FAT-12";
        break;
    case FS_TYPE_EXFAT:
        retval = L"ExFAT";
        break;
    case FS_TYPE_XFS:
        retval = L"XFS";
        break;
    case FS_TYPE_JFS:
        retval = L"JFS";
        break;
    case FS_TYPE_BTRFS:
        retval = L"BtrFS";
        break;
    case FS_TYPE_ISO9660:
        retval = L"ISO-9660";
        break;
    case FS_TYPE_UFS:
        retval = L"UFS";
        break;
    case FS_TYPE_BCACHEFS:
        retval = L"Bcachefs";
        break;
    default:
        retval = LABEL_UNKNOWN;
        break;
    }

    if (0)
        ;
    else if (MrdStrIncludesCI(Volume->VolName, L"Optical Disc"))
        retval = L"ISO-9660 (Assumed)";
    else if (MrdStrIncludesCI(Volume->VolName, L"APFS/FileVault"))
        retval = L"APFS (Assumed)";
    else if (MrdStrIncludesCI(Volume->VolName, L"Fusion/FileVault"))
        retval = L"HFS+ (Assumed)";

    i = 0;
    FoundVentoy = FALSE;
    while (!FoundVentoy) {
        VentoyName = FindCommaDelimited(VENTOY_NAMES, i++);
        if (VentoyName == NULL)
            break;

        if (MrdStrStartsWithCI(VentoyName, Volume->VolName)) {
            GlobalConfig.HandleVentoy = FoundVentoy = TRUE;
        }

        MRD_FREE_POOL(VentoyName);

        if (FoundVentoy && MrdStrEqualsCI(retval, LABEL_UNKNOWN)) {
            return L"ExFAT (Assumed)";
        }
    }

    if (!MrdStrEqualsCI(retval, LABEL_UNKNOWN)) {
        return retval;
    }

    if (GuidsAreEqual(&(Volume->PartTypeGuid), &GuidBasicData)) {
        retval = (FoundVentoy) ? L"ExFAT (Assumed)" : L"NTFS (Assumed)";
    }
    else if (GuidsAreEqual(&(Volume->PartTypeGuid), &GuidReservedMS))
        retval = L"NTFS (Assumed)";
    else if (GuidsAreEqual(&(Volume->PartTypeGuid), &GuidWindowsRE))
        retval = L"NTFS (Assumed)";
    else if (GuidsAreEqual(&(Volume->PartTypeGuid), &GuidESP))
        retval = L"FAT-32 (Assumed)";
    else if (GuidsAreEqual(&(Volume->PartTypeGuid), &GuidLinux))
        retval = L"Ext4 (Assumed)";
    else if (GuidsAreEqual(&(Volume->PartTypeGuid), &GuidRecoveryHD))
        retval = L"HFS+ (Assumed)";
    else if (GuidsAreEqual(&(Volume->PartTypeGuid), &GuidHFS))
        retval = L"HFS+ (Assumed)";
    else if (GuidsAreEqual(&(Volume->PartTypeGuid), &gRootGuid))
        retval = L"Linux Root";
    else if (GuidsAreEqual(&(Volume->PartTypeGuid), &GuidHome))
        retval = L"Linux Home";
    else if (GuidsAreEqual(&(Volume->PartTypeGuid), &GuidSwap))
        retval = L"Linux Swap";
    else if (GuidsAreEqual(&(Volume->PartTypeGuid), &GuidLuks))
        retval = L"LUKS Encrypted";
    else if (GuidsAreEqual(&(Volume->PartTypeGuid), &GuidMacRaidOn))
        retval = L"Apple Raid (ON)";
    else if (GuidsAreEqual(&(Volume->PartTypeGuid), &GuidMacRaidOff))
        retval = L"Apple Raid (OFF)";

    return retval;
}

static VOID SetFilesystemName(IN OUT MERIDIAN_VOLUME *Volume)
{
    EFI_FILE_SYSTEM_INFO *FileSystemInfoPtr;

    if (Volume == NULL || Volume->RootDir == NULL) {
        return;
    }

    FileSystemInfoPtr = LibFileSystemInfo(Volume->RootDir);
    if (FileSystemInfoPtr == NULL) {
        return;
    }

    if (StrLen(FileSystemInfoPtr->VolumeLabel) > 0) {
        MRD_FREE_POOL(Volume->FsName);
        Volume->FsName = StrDuplicate(FileSystemInfoPtr->VolumeLabel);
    }

    MRD_FREE_POOL(FileSystemInfoPtr);
}

static VOID SetFilesystemData(IN UINT8 *Buffer, IN UINTN BufferSize, IN OUT MERIDIAN_VOLUME *Volume)
{
    EFI_GUID *GuidPathAPFS;
    UINT32 *Ext2Incompat;
    UINT32 *Ext2Compat;
    UINT32 *Magic32;
    UINT16 *Magic16;
    char *MagicString;

    if (Buffer == NULL || Volume == NULL) {
        return;
    }

    gBS->SetMem(&(Volume->VolUuid), sizeof(EFI_GUID), 0);

    Volume->FSType = FS_TYPE_UNKNOWN;

    if (BufferSize >= (EXT2_SUPER_OFFSET + EXT2_INCOMPAT_OFFSET + sizeof(UINT32))) {
        Magic16 = (UINT16 *)(Buffer + EXT2_SUPER_OFFSET + EXT2_MAGIC_OFFSET);
        if (*Magic16 == EXT2_SUPER_MAGIC) {

            Ext2Compat = (UINT32 *)(Buffer + EXT2_SUPER_OFFSET + EXT2_COMPAT_OFFSET);
            Ext2Incompat = (UINT32 *)(Buffer + EXT2_SUPER_OFFSET + EXT2_INCOMPAT_OFFSET);

            if ((*Ext2Incompat & EXT4_INCOMPAT_EXTENTS) ||
                (*Ext2Incompat & EXT4_INCOMPAT_FLEX_BG)) {

                Volume->FSType = FS_TYPE_EXT4;
            }
            else if (*Ext2Compat & EXT3_COMPAT_HAS_JOURNAL) {

                Volume->FSType = FS_TYPE_EXT3;
            }
            else {

                Volume->FSType = FS_TYPE_EXT2;
            }

            gBS->CopyMem(&(Volume->VolUuid), Buffer + EXT2_SUPER_OFFSET + EXT2_UUID_OFFSET,
                         sizeof(EFI_GUID));

            return;
        }
    }

    if (BufferSize >= (BTRFS_SUPER_OFFSET + BTRFS_MAGIC_OFFSET + 8)) {
        MagicString = (char *)(Buffer + BTRFS_SUPER_OFFSET + BTRFS_MAGIC_OFFSET);
        if (CompareMem(MagicString, BTRFS_SIGNATURE, 8) == 0) {
            Volume->FSType = FS_TYPE_BTRFS;

            return;
        }
    }

    if (BufferSize >= (BCACHEFS_SUPER_OFFSET + BCACHEFS_UUID_OFFSET + sizeof(EFI_GUID))) {
        if (CompareMem(Buffer + BCACHEFS_SUPER_OFFSET + BCACHEFS_MAGIC_OFFSET, BcachefsMagic,
                       sizeof(BcachefsMagic)) == 0) {
            Volume->FSType = FS_TYPE_BCACHEFS;
            gBS->CopyMem(&(Volume->VolUuid), Buffer + BCACHEFS_SUPER_OFFSET + BCACHEFS_UUID_OFFSET,
                         sizeof(EFI_GUID));

            return;
        }
    }

    if (BufferSize >= BASE_SIZE) {
        MagicString = (char *)Buffer;
        if (CompareMem(MagicString, XFS_SIGNATURE, 4) == 0) {
            Volume->FSType = FS_TYPE_XFS;

            return;
        }
    }

    if (BufferSize >= (JFS_SUPER_OFFSET + 4)) {
        MagicString = (char *)(Buffer + JFS_SUPER_OFFSET);
        if (CompareMem(MagicString, JFS_SIGNATURE, 4) == 0) {
            Volume->FSType = FS_TYPE_JFS;

            return;
        }
    }

    if (BufferSize >= (UFS2_SUPER_OFFSET + UFS_SUPER_MAGIC_OFFSET + sizeof(UINT32))) {
        Magic32 = (UINT32 *)(Buffer + UFS2_SUPER_OFFSET + UFS_SUPER_MAGIC_OFFSET);
        if (*Magic32 == UFS2_SUPER_MAGIC) {
            Volume->FSType = FS_TYPE_UFS;

            return;
        }
    }

    if (BufferSize >= (UFS1_SUPER_OFFSET + UFS_SUPER_MAGIC_OFFSET + sizeof(UINT32))) {
        Magic32 = (UINT32 *)(Buffer + UFS1_SUPER_OFFSET + UFS_SUPER_MAGIC_OFFSET);
        if (*Magic32 == UFS1_SUPER_MAGIC || *Magic32 == UFS2_SUPER_MAGIC) {
            Volume->FSType = FS_TYPE_UFS;

            return;
        }
    }

    if (BufferSize >= (HFSPLUS_SUPER_OFFSET + sizeof(UINT16))) {
        Magic16 = (UINT16 *)(Buffer + HFSPLUS_SUPER_OFFSET);
        if ((*Magic16 == HFSPLUS_MAGIC0) || (*Magic16 == HFSPLUS_MAGIC1) ||
            (*Magic16 == HFSPLUS_MAGIC2)) {
            if (BufferSize >= (HFSPLUS_SUPER_OFFSET + HFSPLUS_UUID_OFFSET + sizeof(UINT64))) {
                gBS->CopyMem(&(Volume->VolUuid),
                             Buffer + HFSPLUS_SUPER_OFFSET + HFSPLUS_UUID_OFFSET, sizeof(UINT64));
            }

            Volume->FSType = FS_TYPE_HFSPLUS;

            return;
        }
    }

    if (BufferSize >= BASE_SIZE) {

        Magic16 = (UINT16 *)(Buffer + MBR_SIG_OFFSET);
        if (*Magic16 == FAT_MAGIC) {
            MagicString = (char *)Buffer;
            if (CompareMem(MagicString + NTFS_OEM_ID_OFFSET, NTFS_SIGNATURE, 8) == 0) {
                Volume->FSType = FS_TYPE_NTFS;
                gBS->CopyMem(&(Volume->VolUuid), Buffer + NTFS_SERIAL_OFFSET, sizeof(UINT64));
            }
            else if (CompareMem(MagicString + FAT32_BPB_SIG_OFFSET, FAT32_SIGNATURE, 8) == 0) {
                Volume->FSType = FS_TYPE_FAT32;
                gBS->CopyMem(&(Volume->VolUuid), Buffer + FAT32_SERIAL_OFFSET, sizeof(UINT32));
            }
            else if (CompareMem(MagicString + FAT16_BPB_SIG_OFFSET, FAT16_SIGNATURE, 8) == 0) {
                Volume->FSType = FS_TYPE_FAT16;
                gBS->CopyMem(&(Volume->VolUuid), Buffer + FAT16_SERIAL_OFFSET, sizeof(UINT32));
            }
            else if (CompareMem(MagicString + FAT16_BPB_SIG_OFFSET, FAT12_SIGNATURE, 8) == 0) {
                Volume->FSType = FS_TYPE_FAT12;
                gBS->CopyMem(&(Volume->VolUuid), Buffer + FAT16_SERIAL_OFFSET, sizeof(UINT32));
            }
            else if (!Volume->BlockIO->Media->LogicalPartition) {
                Volume->FSType = FS_TYPE_WHOLEDISK;
            }
            else if (FindMem(Buffer, BASE_SIZE, "EXFAT", 5) >= 0) {
                Volume->FSType = FS_TYPE_EXFAT;
            }

            return;
        }
    }

    if (DevicePathType(Volume->DevicePath) == MEDIA_DEVICE_PATH &&
        DevicePathSubType(Volume->DevicePath) == MEDIA_VENDOR_DP) {
        GuidPathAPFS = (EFI_GUID *)((UINT8 *)Volume->DevicePath + 0x04);

        if (GuidsAreEqual(GuidPathAPFS, &GuidFlagAPFS)) {
            Volume->FSType = FS_TYPE_APFS;
            if (GuidsAreEqual(&(Volume->PartTypeGuid), &GuidNull)) {
                Volume->PartTypeGuid = GuidFlagAPFS;
            }

            return;
        }
    }

    if (GuidsAreEqual(&(Volume->PartTypeGuid), &GuidAPFS)) {
        Volume->FSType = FS_TYPE_APFS;

        return;
    }

    if (Volume->BlockIO->Media->BlockSize == 2048) {
        Volume->FSType = FS_TYPE_ISO9660;
    }
}

static VOID ScanVolumeBootcode(IN OUT MERIDIAN_VOLUME *Volume, IN OUT BOOLEAN *Bootable)
{
#if MERIDIAN_DEBUG > 0
    UINTN LogLineType;
    CHAR16 *StrSpacer;
    CHAR16 *MsgStr;
#endif

    EFI_STATUS Status;
    UINTN i, SizeMBR;
    UINT8 Buffer[SAMPLE_SIZE];
    BOOLEAN MbrTableFound;
    MBR_PARTITION_INFO *MbrTable;

    *Bootable = FALSE;
    MediaCheck = FALSE;
    Volume->HasBootCode = FALSE;
    Volume->OSSplashHint = NULL;
    Volume->OSName = NULL;

    if (Volume->BlockIO == NULL) {
#if MERIDIAN_DEBUG > 0
        if (SelfVolRun) {
            MsgStr = L"Found Invalid Volume BlockIO on Item Below";
            INFO_LOG("\n\n");
            INFO_LOG("** WARN:  %s", MsgStr);
            INFO_LOG("\n");
        }
#endif

        return;
    }
    if (Volume->BlockIO->Media->BlockSize > SAMPLE_SIZE) {
#if MERIDIAN_DEBUG > 0
        if (SelfVolRun) {

            MsgStr = L"Found Invalid Boot Code Buffer Size on Item Below";
            INFO_LOG("\n\n");
            INFO_LOG("** WARN:  %s", MsgStr);
        }
#endif

        return;
    }

    Status = Volume->BlockIO->ReadBlocks(Volume->BlockIO, Volume->BlockIO->Media->MediaId,
                                         Volume->BlockIOOffset, SAMPLE_SIZE, Buffer);
    if (EFI_ERROR(Status)) {
#if MERIDIAN_DEBUG > 0
        if (SelfVolRun && ScanMBR) {
            MsgStr = L"Could *NOT* Read Boot Sector on Item Below";
            INFO_LOG("\n\n");
            INFO_LOG("** WARN: '%r' %s", Status, MsgStr);

            ScannedOnce = FALSE;
            CheckError(Status, MsgStr);
            if (Status == EFI_NO_MEDIA) {
                MediaCheck = TRUE;
            }
        }
#endif

        return;
    }

    SetFilesystemData(Buffer, SAMPLE_SIZE, Volume);

    *Bootable = TRUE;

    if ((Buffer[0] != 0) && (*((UINT16 *)(Buffer + MBR_SIG_OFFSET)) == FAT_MAGIC) &&
        (FindMem(Buffer, BASE_SIZE, "EXFAT", 5) == -1)) {
        Volume->HasBootCode = TRUE;
    }

    if (CompareMem(Buffer + 2, "LILO", 4) == 0 || CompareMem(Buffer + 6, "LILO", 4) == 0 ||
        CompareMem(Buffer + 3, "SYSLINUX", 8) == 0 ||
        FindMem(Buffer, SECTOR_SIZE, "ISOLINUX", 8) >= 0) {
        Volume->HasBootCode = TRUE;
        Volume->OSSplashHint = L"linux";
        Volume->OSName = L"Instance: Linux (Legacy)";
    }
    else if (FindMem(Buffer, BASE_SIZE, "Geom\0Hard Disk\0Read\0 Error", 26) >= 0) {
        Volume->HasBootCode = TRUE;
        Volume->OSSplashHint = L"grub,linux";
        Volume->OSName = L"Instance: Linux (Legacy)";
    }
    else if (FindMem(Buffer, SECTOR_SIZE, "Starting the BTX loader", 23) >= 0 ||
             (*((UINT32 *)(Buffer + 502)) == 0 && *((UINT32 *)(Buffer + 506)) == 50000 &&
              *((UINT16 *)(Buffer + MBR_SIG_OFFSET)) == FAT_MAGIC) ||
             ((*((UINT16 *)(Buffer + MBR_SIG_OFFSET)) == FAT_MAGIC) &&
              (FindMem(Buffer, SECTOR_SIZE, "Boot loader too large", 21) >= 0) &&
              (FindMem(Buffer, SECTOR_SIZE, "I/O error loading boot loader", 29) >= 0))) {

        Volume->HasBootCode = TRUE;
        Volume->OSSplashHint = L"freebsd";
        Volume->OSName = L"Instance: FreeBSD (Legacy)";
    }
    else if (FindMem(Buffer, BASE_SIZE, "!Loading", 8) >= 0 ||
             FindMem(Buffer, SECTOR_SIZE, "/cdboot\0/CDBOOT\0", 16) >= 0) {
        Volume->HasBootCode = TRUE;
        Volume->OSSplashHint = L"openbsd";
        Volume->OSName = L"Instance: OpenBSD (Legacy)";
    }
    else if (FindMem(Buffer, BASE_SIZE, "Not a bootxx image", 18) >= 0 ||
             *((UINT32 *)(Buffer + NETBSD_MAGIC_OFFSET)) == NETBSD_BOOT_MAGIC) {
        Volume->HasBootCode = TRUE;
        Volume->OSSplashHint = L"netbsd";
        Volume->OSName = L"Instance: NetBSD (Legacy)";
    }
    else if (FindMem(Buffer, SECTOR_SIZE, "NTLDR", 5) >= 0) {

        Volume->HasBootCode = TRUE;
        Volume->OSSplashHint = L"windows,win,win8";
        Volume->OSName = L"Instance: Windows (Legacy - NT/XP)";
    }
    else if (FindMem(Buffer, SECTOR_SIZE, "BOOTMGR", 7) >= 0) {

        Volume->HasBootCode = TRUE;
        Volume->OSSplashHint = L"windows,win,win8";
        Volume->OSName = L"Instance: Windows (Legacy)";
    }
    else if (FindMem(Buffer, BASE_SIZE, "CPUBOOT SYS", 11) >= 0 ||
             FindMem(Buffer, BASE_SIZE, "KERNEL  SYS", 11) >= 0) {
        Volume->HasBootCode = TRUE;
        Volume->OSSplashHint = L"freedos";
        Volume->OSName = L"Instance: FreeDOS (Legacy)";
    }
    else if (FindMem(Buffer, BASE_SIZE, "OS2LDR", 6) >= 0 ||
             FindMem(Buffer, BASE_SIZE, "OS2BOOT", 7) >= 0) {
        Volume->HasBootCode = TRUE;
        Volume->OSSplashHint = L"ecomstation";
        Volume->OSName = L"Instance: eComStation (Legacy)";
    }
    else if (FindMem(Buffer, BASE_SIZE, "Be Boot Loader", 14) >= 0) {
        Volume->HasBootCode = TRUE;
        Volume->OSSplashHint = L"beos";
        Volume->OSName = L"Instance: BeOS (Legacy)";
    }
    else if (FindMem(Buffer, BASE_SIZE, "yT Boot Loader", 14) >= 0) {
        Volume->HasBootCode = TRUE;
        Volume->OSSplashHint = L"zeta,beos";
        Volume->OSName = L"Instance: ZETA (Legacy)";
    }
    else if (FindMem(Buffer, BASE_SIZE,
                     "\x04"
                     "beos\x06"
                     "system\x05"
                     "zbeos",
                     18) >= 0 ||
             FindMem(Buffer, BASE_SIZE,
                     "\x06"
                     "system\x0c"
                     "haiku_loader",
                     20) >= 0) {
        Volume->HasBootCode = TRUE;
        Volume->OSSplashHint = L"haiku,beos";
        Volume->OSName = L"Instance: Haiku (Legacy)";
    }

    do {
        if (Volume->HasBootCode) {

            if (FindMem(Buffer, BASE_SIZE, "Non-system disk", 15) >= 0) {

                Volume->HasBootCode = FALSE;
            }
            else if (FindMem(Buffer, BASE_SIZE, "Press any key to restart", 24) >= 0) {

                Volume->HasBootCode = FALSE;
            }
            else if (FindMem(Buffer, BASE_SIZE, "This is not a bootable disk", 27) >= 0) {

                Volume->HasBootCode = FALSE;
            }
        }
    } while (0);

    if (!Volume->HasBootCode) {
        *Bootable = FALSE;

        return;
    }

    if (*((UINT16 *)(Buffer + MBR_SIG_OFFSET)) != FAT_MAGIC) {
        Volume->HasBootCode = FALSE;
        *Bootable = FALSE;

        return;
    }

    Volume->HasBootCode = FALSE;
    *Bootable = FALSE;

    return;
    MbrTable = (MBR_PARTITION_INFO *)(Buffer + 446);
    for (i = 0; i < 4; i++) {
        if (MbrTable[i].StartLBA && MbrTable[i].Size) {
            MbrTableFound = TRUE;

            break;
        }
    }
    for (i = 0; i < 4; i++) {
        if (MbrTable[i].Flags != 0x00 && MbrTable[i].Flags != 0x80) {
            MbrTableFound = FALSE;

            break;
        }
    }

    if (MbrTableFound) {
        SizeMBR = 4 * 16;
        Volume->MbrPartitionTable = AllocatePool(SizeMBR);
        if (Volume->MbrPartitionTable == NULL) {
            Volume->HasBootCode = FALSE;
            *Bootable = FALSE;

            return;
        }

        gBS->CopyMem(Volume->MbrPartitionTable, MbrTable, SizeMBR);

#if MERIDIAN_DEBUG > 0
        FoundMBR = TRUE;
#endif
    }

#if MERIDIAN_DEBUG > 0
    if (DoneHeadings && (MbrTableFound || Volume->HasBootCode)) {
        LogLineType = (SkipSpacing) ? LOG_LINE_SAME : LOG_LINE_SPECIAL;

        if (Volume->HasBootCode) {
            StrSpacer = (!SkipSpacing) ? L"" : (UseButJoin) ? BUT_JOIN_TXT : AND_JOIN_TXT;
            DEBUG_LOG(1, LogLineType, L"%s%s", StrSpacer, LEGACY_CODE_TXT);
            UseButJoin = FALSE;
        }

        if (MbrTableFound) {
            if (Volume->HasBootCode) {
                StrSpacer = AND_JOIN_TXT;
                LogLineType = LOG_LINE_SAME;
            }
            else {
                StrSpacer = (!SkipSpacing) ? L"" : (UseButJoin) ? BUT_JOIN_TXT : AND_JOIN_TXT;
            }
            DEBUG_LOG(1, LogLineType, L"%s%s", StrSpacer, PARTITION_TABLE_TXT);
            UseButJoin = FALSE;
        }

        SkipSpacing = TRUE;
    }
#endif
}

static CHAR16 *SizeInIEEEUnits(IN UINT64 SizeInBytes)
{
    UINTN Index;
    UINTN NumPrefixes;
    CHAR16 *Units;
    const CHAR16 *Prefixes = L" KMGTPEZ";
    CHAR16 *TheValue;
    UINT64 SizeInIeee;

    NumPrefixes = StrLen(Prefixes);
    SizeInIeee = SizeInBytes;
    Index = 0;
    while ((SizeInIeee > 1024) && (Index < (NumPrefixes - 1))) {
        Index++;
        SizeInIeee /= 1024;
    }

    if (Prefixes[Index] == ' ') {
        Units = StrDuplicate(L"-byte");
    }
    else {
        Units = StrDuplicate(L"  iB");
        Units[1] = Prefixes[Index];
    }

    TheValue = PoolPrint(L"%ld%s", SizeInIeee, Units);
    MRD_FREE_POOL(Units);

    return TheValue;
}

static VOID SetPartGuidAndName(IN OUT MERIDIAN_VOLUME *Volume,
                               IN OUT EFI_DEVICE_PATH_PROTOCOL *DevicePath)
{
    HARDDRIVE_DEVICE_PATH *HdDevicePath;
    GPT_ENTRY *PartInfo;
    EFI_GUID GuidMBR = MBR_GUID_VALUE;

    if (Volume == NULL || DevicePath == NULL) {
        return;
    }

    if ((DevicePath->Type != MEDIA_DEVICE_PATH) || (DevicePath->SubType != MEDIA_HARDDRIVE_DP)) {
        return;
    }

    HdDevicePath = (HARDDRIVE_DEVICE_PATH *)DevicePath;
    if (HdDevicePath->SignatureType != SIGNATURE_TYPE_GUID) {

        Volume->PartGuid = GuidMBR;

        return;
    }

    Volume->PartGuid = *((EFI_GUID *)HdDevicePath->Signature);
    PartInfo = FindPartWithGuid(&(Volume->PartGuid));
    if (PartInfo == NULL) {
        return;
    }

    Volume->PartName = StrDuplicate(PartInfo->name);
    gBS->CopyMem(&(Volume->PartTypeGuid), PartInfo->type_guid, sizeof(EFI_GUID));

    if (GuidsAreEqual(&(Volume->PartTypeGuid), &gRootGuid) &&
        ((PartInfo->attributes & GPT_NO_AUTOMOUNT) == 0)) {
        GlobalConfig.DiscoveredRoot = Volume;
    }

    Volume->IsMarkedReadOnly = ((PartInfo->attributes & GPT_READ_ONLY) > 0);

    MRD_FREE_POOL(PartInfo);
}

static CHAR16 *GetVolumeNameEx(IN MERIDIAN_VOLUME *Volume)
{
    CHAR16 *SISize;
    CHAR16 *TypeName;
    CHAR16 *FoundName;
    EFI_FILE_SYSTEM_INFO *FileSystemInfoPtr;

    if (Volume->FsName != NULL && Volume->FsName[0] != L'\0' && StrLen(Volume->FsName) != 0) {
        return StrDuplicate(
            (Volume->FSType == FS_TYPE_EXFAT && MrdStrEqualsCI(Volume->FsName, L"FAT"))
                ? L"ExFAT"
                : Volume->FsName);
    }

    if (Volume->PartName != NULL && Volume->PartName[0] != L'\0' && StrLen(Volume->PartName) > 0 &&
        !IsListItem(Volume->PartName, IGNORE_PARTITION_NAMES)) {
        return StrDuplicate(Volume->PartName);
    }

    if (Volume->DiskKind == DISK_KIND_OPTICAL) {
        FoundName = StrDuplicate((Volume->FSType == FS_TYPE_ISO9660) ? L"Optical ISO-9660 Image"
                                                                     : L"Optical Disc Drive");
    }
    else if (MediaCheck) {
        FoundName = StrDuplicate(L"Network Volume (Assumed)");
    }
    else {
        if (0)
            ;
        else if (GuidsAreEqual(&(Volume->PartTypeGuid), &GuidAPFS))
            FoundName = StrDuplicate(L"APFS/FileVault Container");
        else if (GuidsAreEqual(&(Volume->PartTypeGuid), &GuidESP))
            FoundName = StrDuplicate(L"EFI System Partition");
        else if (GuidsAreEqual(&(Volume->PartTypeGuid), &GuidLinux))
            FoundName = StrDuplicate(L"Linux Volume");
        else if (GuidsAreEqual(&(Volume->PartTypeGuid), &GuidRecoveryHD))
            FoundName = StrDuplicate(L"Recovery HD");
        else if (GuidsAreEqual(&(Volume->PartTypeGuid), &GuidReservedMS))
            FoundName = StrDuplicate(L"Microsoft Reserved Partition");
        else if (GuidsAreEqual(&(Volume->PartTypeGuid), &GuidMacRaidOn))
            FoundName = StrDuplicate(L"Apple Raid Partition (Online)");
        else if (GuidsAreEqual(&(Volume->PartTypeGuid), &GuidMacRaidOff))
            FoundName = StrDuplicate(L"Apple Raid Partition (Offline)");
        else if (GuidsAreEqual(&(Volume->PartTypeGuid), &GuidContainHFS))
            FoundName = StrDuplicate(L"Fusion/FileVault Container");
        else if (GuidsAreEqual(&(Volume->PartTypeGuid), &GuidApplTvRec))
            FoundName = StrDuplicate(L"AppleTV Recovery Partition");
        else {

            TypeName = FSTypeName(Volume);

            if (MrdStrEqualsCI(TypeName, L"APFS")) {
                FoundName = StrDuplicate(L"APFS Volume (Assumed)");
            }
            else {
                if (0)
                    ;
                else if (GuidsAreEqual(&(Volume->PartTypeGuid), &GuidHFS))
                    FoundName = StrDuplicate(L"Unidentified HFS+ Partition");
                else if (GuidsAreEqual(&(Volume->PartTypeGuid), &gRootGuid))
                    FoundName = StrDuplicate(L"Linux Root Volume");
                else if (GuidsAreEqual(&(Volume->PartTypeGuid), &GuidHome))
                    FoundName = StrDuplicate(L"Linux Home Volume");
                else if (GuidsAreEqual(&(Volume->PartTypeGuid), &GuidSwap))
                    FoundName = StrDuplicate(L"Linux Swap Volume");
                else if (GuidsAreEqual(&(Volume->PartTypeGuid), &GuidLuks))
                    FoundName = StrDuplicate(L"Encrypted Linux Volume");
                else {

                    FileSystemInfoPtr =
                        (Volume->RootDir != NULL) ? LibFileSystemInfo(Volume->RootDir) : NULL;

                    if (FileSystemInfoPtr == NULL) {
                        FoundName = PoolPrint(L"%s Volume", TypeName);
                    }
                    else {
                        SISize = SizeInIEEEUnits(FileSystemInfoPtr->VolumeSize);
                        FoundName = PoolPrint(L"%s %s Volume", SISize, TypeName);
                        MRD_FREE_POOL(FileSystemInfoPtr);
                        MRD_FREE_POOL(SISize);
                    }
                }
            }
        }
    }

    return FoundName;
}

CHAR16 *GetVolumeName(IN MERIDIAN_VOLUME *Volume)
{
    CHAR16 *FoundName;

    FoundName = GetVolumeNameEx(Volume);
    if (FoundName[0] >= L'a' && FoundName[0] <= L'z') {

        FoundName[0] = FoundName[0] - L'a' + L'A';
    }

    return FoundName;
}

BOOLEAN VolumeScanAllowed(IN MERIDIAN_VOLUME *Volume, IN BOOLEAN SkipVentoy, IN BOOLEAN SkipRootDir)
{
    UINTN i;
    CHAR16 *VolGuid;
    CHAR16 *VentoyName;
    BOOLEAN FoundVentoy;
    BOOLEAN ScanAllowed;

    if (Volume == NULL || Volume->VolName == NULL || !Volume->IsReadable) {
        return FALSE;
    }

    if (!GlobalConfig.ScanAllESP && GuidsAreEqual(&(Volume->PartTypeGuid), &GuidESP) &&
        !GuidsAreEqual(&(Volume->PartGuid), &SelfVolume->PartGuid)) {
        return FALSE;
    }

    if (!SkipRootDir && Volume->RootDir == NULL) {
        return FALSE;
    }

    if (Volume->FSType == FS_TYPE_APFS) {
        if (GlobalConfig.SyncAPFS && Volume->VolRole == APFS_VOLUME_ROLE_PREBOOT) {
            return TRUE;
        }

        if (Volume->VolRole != APFS_VOLUME_ROLE_SYSTEM &&
            Volume->VolRole != APFS_VOLUME_ROLE_PREBOOT &&
            Volume->VolRole != APFS_VOLUME_ROLE_UNDEFINED) {
            return FALSE;
        }
    }

    if (Volume->FSType == FS_TYPE_HFSPLUS &&
        GuidsAreEqual(&(Volume->PartTypeGuid), &GuidRecoveryHD)) {
        return FALSE;
    }

    if (Volume->FSType == FS_TYPE_NTFS) {
        if (MrdStrEqualsCI(Volume->VolName, L"System Reserved") ||
            MrdStrEqualsCI(Volume->VolName, L"System Device Bay") ||
            MrdStrEqualsCI(Volume->VolName, L"Microsoft Reserved Partition")) {
            return FALSE;
        }
    }

    if (IsListItem(Volume->VolName, GlobalConfig.DontScanVolumes) ||
        IsListItem(Volume->FsName, GlobalConfig.DontScanVolumes) ||
        IsListItem(Volume->PartName, GlobalConfig.DontScanVolumes)) {
        return FALSE;
    }

    if (GuidsAreEqual(&(Volume->PartTypeGuid), &GuidWindowsRE)) {
        return FALSE;
    }

    VolGuid = GuidAsString(&(Volume->PartGuid));
    if (IsListItem(VolGuid, GlobalConfig.DontScanVolumes)) {
        ScanAllowed = FALSE;
    }
    else {
        ScanAllowed = TRUE;

        if (!SkipVentoy) {
            i = 0;
            FoundVentoy = FALSE;
            while (GlobalConfig.HandleVentoy && !FoundVentoy) {
                VentoyName = FindCommaDelimited(VENTOY_NAMES, i++);
                if (VentoyName == NULL)
                    break;

                if (MrdStrStartsWithCI(VentoyName, Volume->VolName) ||
                    MrdStrStartsWithCI(VentoyName, Volume->FsName) ||
                    MrdStrStartsWithCI(VentoyName, Volume->PartName)) {
                    FoundVentoy = TRUE;
                }
                MRD_FREE_POOL(VentoyName);
            }

            if (FoundVentoy) {
                if (!FileExists(Volume->RootDir, FALLBACK_FULLNAME)) {
                    ScanAllowed = FALSE;
                }
            }
        }
    }
    MRD_FREE_POOL(VolGuid);

    return ScanAllowed;
}

BOOLEAN HasWindowsBiosBootFiles(IN MERIDIAN_VOLUME *Volume)
{
    if (Volume->RootDir == NULL) {
        Volume->RootDir = LibOpenRoot(Volume->DeviceHandle);
        if (Volume->RootDir == NULL) {
            return TRUE;
        }
    }

    return (FileExists(Volume->RootDir, L"NTLDR") || FileExists(Volume->RootDir, L"bootmgr"));
}

VOID ScanVolume(IN OUT MERIDIAN_VOLUME *Volume)
{
#if MERIDIAN_DEBUG > 0
    UINTN LogLineType;
    CHAR16 *StrSpacer;
    CHAR16 *MsgStr;
    BOOLEAN HybridLogger = FALSE;
#endif

    EFI_STATUS Status;
    CHAR16 *VolGuid;
    EFI_DEVICE_PATH_PROTOCOL *DevicePath;
    EFI_DEVICE_PATH_PROTOCOL *NextDevicePath;
    EFI_DEVICE_PATH_PROTOCOL *DiskDevicePath;
    EFI_DEVICE_PATH_PROTOCOL *RemainingDevicePath;
    EFI_HANDLE WholeDiskHandle;
    UINTN PartialLength;
    BOOLEAN CheckTagAll;
    BOOLEAN CheckTagOff;
    BOOLEAN Bootable;

#if MERIDIAN_DEBUG > 0
    MRD_HYBRIDLOGGER_SET;
#endif

    Volume->DevicePath = DuplicateDevicePath(DevicePathFromHandle(Volume->DeviceHandle));

    Volume->DiskKind = DISK_KIND_INTERNAL;

    Status =
        gBS->HandleProtocol(Volume->DeviceHandle, &BlockIoProtocol, (VOID **)&(Volume->BlockIO));
    if (EFI_ERROR(Status)) {
        Volume->BlockIO = NULL;

#if MERIDIAN_DEBUG > 0
        LogLineType = (HybridLogger) ? LOG_LINE_SPECIAL : LOG_LINE_NORMAL;
        DEBUG_LOG(1, LogLineType, L"Cannot Get BlockIO Protocol in ScanVolume!!");
#endif
    }
    else {
        if (Volume->BlockIO->Media->BlockSize == 2048) {
            Volume->DiskKind = DISK_KIND_OPTICAL;
        }
    }

#if MERIDIAN_DEBUG > 0
    UseButJoin = FALSE;
#endif

    DevicePath = Volume->DevicePath;
    while (DevicePath != NULL && !IsDevicePathEndType(DevicePath)) {
        NextDevicePath = NextDevicePathNode(DevicePath);

        if (DevicePathType(DevicePath) == MEDIA_DEVICE_PATH) {
            SetPartGuidAndName(Volume, DevicePath);
        }

        if (DevicePathType(DevicePath) == MESSAGING_DEVICE_PATH &&
            (DevicePathSubType(DevicePath) == MSG_USB_DP ||
             DevicePathSubType(DevicePath) == MSG_1394_DP ||
             DevicePathSubType(DevicePath) == MSG_USB_CLASS_DP ||
             DevicePathSubType(DevicePath) == MSG_FIBRECHANNEL_DP)) {

            Volume->DiskKind = DISK_KIND_EXTERNAL;
            FoundExternalDisk = TRUE;
        }

        if (DevicePathType(DevicePath) == MEDIA_DEVICE_PATH &&
            DevicePathSubType(DevicePath) == MEDIA_CDROM_DP) {

            Volume->DiskKind = DISK_KIND_OPTICAL;
        }

        if (DevicePathType(DevicePath) == MESSAGING_DEVICE_PATH) {

            PartialLength = (UINT8 *)NextDevicePath - (UINT8 *)(Volume->DevicePath);
            DiskDevicePath =
                (EFI_DEVICE_PATH_PROTOCOL *)AllocatePool(PartialLength + sizeof(EFI_DEVICE_PATH));

            gBS->CopyMem(DiskDevicePath, Volume->DevicePath, PartialLength);

            gBS->CopyMem((UINT8 *)DiskDevicePath + PartialLength, EndDevicePath,
                         sizeof(EFI_DEVICE_PATH));

            RemainingDevicePath = DiskDevicePath;
            Status =
                gBS->LocateDevicePath(&BlockIoProtocol, &RemainingDevicePath, &WholeDiskHandle);
            MRD_FREE_POOL(DiskDevicePath);
            if (EFI_ERROR(Status)) {
#if MERIDIAN_DEBUG > 0
                if (DoneHeadings) {
                    if (HybridLogger) {
                        LogLineType = (SkipSpacing) ? LOG_LINE_SAME : LOG_LINE_SPECIAL;
                        StrSpacer = (SkipSpacing) ? L" ... " : L"";
                    }
                    else {
                        StrSpacer = L"";
                        LogLineType = LOG_LINE_NORMAL;
                    }
                    DEBUG_LOG(1, LogLineType, L"%sCould *NOT* Locate Device Path", StrSpacer);
                    SkipSpacing = TRUE;
                    UseButJoin = TRUE;
                }
#endif
            }
            else {

                Status = gBS->HandleProtocol(WholeDiskHandle, &DevicePathProtocol,
                                             (VOID **)&DiskDevicePath);
                if (EFI_ERROR(Status)) {
#if MERIDIAN_DEBUG > 0
                    if (DoneHeadings) {
                        if (HybridLogger) {
                            LogLineType = (SkipSpacing) ? LOG_LINE_SAME : LOG_LINE_SPECIAL;
                            StrSpacer = (SkipSpacing) ? L" ... " : L"";
                        }
                        else {
                            StrSpacer = L"";
                            LogLineType = LOG_LINE_NORMAL;
                        }
                        DEBUG_LOG(1, LogLineType, L"%sCould *NOT* Get DiskDevicePath", StrSpacer);
                        SkipSpacing = TRUE;
                        UseButJoin = TRUE;
                    }
#endif
                }
                else {
                    Volume->WholeDiskDevicePath = DuplicateDevicePath(DiskDevicePath);
                }

                Status = gBS->HandleProtocol(WholeDiskHandle, &BlockIoProtocol,
                                             (VOID **)&Volume->WholeDiskBlockIO);
                if (!EFI_ERROR(Status)) {

                    if (Volume->WholeDiskBlockIO->Media->BlockSize == 2048) {
                        Volume->DiskKind = DISK_KIND_OPTICAL;
                    }
                }
                else {
                    Volume->WholeDiskBlockIO = NULL;

#if MERIDIAN_DEBUG > 0
                    if (DoneHeadings) {
                        if (HybridLogger) {
                            LogLineType = (SkipSpacing) ? LOG_LINE_SAME : LOG_LINE_SPECIAL;
                            StrSpacer = (SkipSpacing) ? L" ... " : L"";
                        }
                        else {
                            StrSpacer = L"";
                            LogLineType = LOG_LINE_NORMAL;
                        }
                        DEBUG_LOG(1, LogLineType, L"%sCould *NOT* Get WholeDiskBlockIO", StrSpacer);
                        SkipSpacing = TRUE;
                        UseButJoin = TRUE;
                    }
#endif
                }
            }
        }
        DevicePath = NextDevicePath;
    }

    Bootable = FALSE;
    ScanVolumeBootcode(Volume, &Bootable);
    if (Volume->DiskKind == DISK_KIND_OPTICAL) {
        Bootable = TRUE;
    }

    if (!Bootable) {
        if (Volume->HasBootCode) {
#if MERIDIAN_DEBUG > 0
            MsgStr = L"Considered Non-Bootable but Boot Code is Present";
            if (DoneHeadings) {
                if (HybridLogger) {
                    LogLineType = (SkipSpacing) ? LOG_LINE_SAME : LOG_LINE_SPECIAL;
                    StrSpacer = (SkipSpacing) ? L" ... " : L"";
                }
                else {
                    StrSpacer = L"";
                    LogLineType = LOG_LINE_NORMAL;
                }
                DEBUG_LOG(1, LogLineType, L"%s%s!!", StrSpacer, MsgStr);
            }
            INFO_LOG("\n");
            INFO_LOG("** WARN: %s", MsgStr);
            SkipSpacing = TRUE;
#endif

            Volume->HasBootCode = FALSE;
        }
    }

#if MERIDIAN_DEBUG > 0
    MRD_HYBRIDLOGGER_OFF;
#endif

    if (Volume->RootDir == NULL) {
        Volume->RootDir = LibOpenRoot(Volume->DeviceHandle);
    }

    SetFilesystemName(Volume);
    Volume->VolName = GetVolumeName(Volume);
    SanitiseVolumeName(&Volume);

    Volume->IsReadable = (Volume->HasBootCode || Volume->RootDir != NULL) ? TRUE : FALSE;

    Volume->AllowSymlinks = FALSE;

    if (GlobalConfig.FollowSymlinks == NULL) {

        return;
    }

    CheckTagAll = MrdStrEqualsCI(SYM_TAG_ALL, GlobalConfig.FollowSymlinks);
    CheckTagOff = MrdStrEqualsCI(SYM_TAG_OFF, GlobalConfig.FollowSymlinks);
    if (CheckTagAll || CheckTagOff) {

        if (CheckTagAll) {
            Volume->AllowSymlinks = TRUE;
        }

        return;
    }

    VolGuid = GuidAsString(&(Volume->PartGuid));
    if (MrdStrStartsWithCI(SYM_TAG_OFF, GlobalConfig.FollowSymlinks)) {

        if (!IsListItem(Volume->VolName, GlobalConfig.FollowSymlinks) &&
            !IsListItem(Volume->FsName, GlobalConfig.FollowSymlinks) &&
            !IsListItem(Volume->PartName, GlobalConfig.FollowSymlinks) &&
            !IsListItem(VolGuid, GlobalConfig.FollowSymlinks)) {
            Volume->AllowSymlinks = TRUE;
        }
    }
    else {

        if (IsListItem(Volume->VolName, GlobalConfig.FollowSymlinks) ||
            IsListItem(Volume->FsName, GlobalConfig.FollowSymlinks) ||
            IsListItem(Volume->PartName, GlobalConfig.FollowSymlinks) ||
            IsListItem(VolGuid, GlobalConfig.FollowSymlinks)) {
            Volume->AllowSymlinks = TRUE;
        }
    }
    MRD_FREE_POOL(VolGuid);
}

static VOID ScanExtendedPartition(IN OUT MERIDIAN_VOLUME *WholeDiskVolume,
                                  IN MBR_PARTITION_INFO *MbrEntry)
{
    EFI_STATUS Status;
    UINT32 ExtBase;
    UINT32 ExtCurrent;
    UINT32 NextExtCurrent;
    UINTN LogicalPartitionIndex, i;
    UINT8 SectorBuffer[BASE_SIZE];
    BOOLEAN Bootable;
    MERIDIAN_VOLUME *Volume;
    MBR_PARTITION_INFO *EMbrTable;

    ExtBase = MbrEntry->StartLBA;
    for (ExtCurrent = ExtBase; ExtCurrent; ExtCurrent = NextExtCurrent) {

        Status = WholeDiskVolume->BlockIO->ReadBlocks(WholeDiskVolume->BlockIO,
                                                      WholeDiskVolume->BlockIO->Media->MediaId,
                                                      ExtCurrent, BASE_SIZE, SectorBuffer);

        if (EFI_ERROR(Status)) {
#if MERIDIAN_DEBUG > 0
            DEBUG_LOG(1, LOG_BLANK_LINE_SEP, L"X");
            DEBUG_LOG(1, LOG_LINE_NORMAL, L"Error %d Reading Blocks from Disk", Status);
#endif

            break;
        }

        if (*((UINT16 *)(SectorBuffer + MBR_SIG_OFFSET)) != FAT_MAGIC) {
            break;
        }

        EMbrTable = (MBR_PARTITION_INFO *)(SectorBuffer + 446);

        NextExtCurrent = 0;
        LogicalPartitionIndex = 4;
        for (i = 0; i < 4; i++) {
            if (EMbrTable[i].Size == 0 || EMbrTable[i].StartLBA == 0 ||
                (EMbrTable[i].Flags != 0x00 && EMbrTable[i].Flags != 0x80)) {
                break;
            }

            if (IS_EXTENDED_PART_TYPE(EMbrTable[i].Type)) {

                NextExtCurrent = ExtBase + EMbrTable[i].StartLBA;
                break;
            }

            Volume = AllocateZeroPool(sizeof(MERIDIAN_VOLUME));
            Volume->DiskKind = WholeDiskVolume->DiskKind;
            Volume->IsMbrPartition = TRUE;
            Volume->MbrPartitionIndex = LogicalPartitionIndex++;

            Volume->VolName = PoolPrint(L"Partition %d", Volume->MbrPartitionIndex + 1);

            Volume->BlockIO = WholeDiskVolume->BlockIO;
            Volume->BlockIOOffset = ExtCurrent + EMbrTable[i].StartLBA;
            Volume->WholeDiskBlockIO = WholeDiskVolume->BlockIO;

            ScanMBR = TRUE;
            Bootable = FALSE;
            ScanVolumeBootcode(Volume, &Bootable);
            if (!Bootable) {
                Volume->HasBootCode = FALSE;
            }
            ScanMBR = FALSE;

            AddListElement((VOID ***)&Volumes, &VolumesCount, Volume);
        }
    }
}

static VOID VetMultiInstanceAPFS(VOID)
{
#if MERIDIAN_DEBUG > 0
    BOOLEAN AppleRecovery;
    CHAR16 *MsgStrA;
    const CHAR16 *MsgStrB = L"Disabled:- 'Every Instance (APFS) - Apple Recovery Tool'";
    const CHAR16 *MsgStrC = L"Disabled:- 'Synced Volumes (APFS) - Apple Hardware Test'";
#endif

    UINTN i, j;
    BOOLEAN GotSystemVol;
    BOOLEAN ActiveContainer;

#if MERIDIAN_DEBUG > 0

    AppleRecovery = FALSE;
    for (i = 0; i < NUM_TOOLS; i++) {
        switch (GlobalConfig.ShowTools[i]) {
        case TAG_RECOVERY_MAC:
            AppleRecovery = TRUE;
            break;
        default:
            continue;
        }

        if (AppleRecovery) {
            break;
        }
    }
#endif

    if (!GlobalConfig.SyncAPFS) {
#if MERIDIAN_DEBUG > 0
        if (AppleRecovery) {

            MsgStrA = L"SyncAPFS is Inactive";
            DEBUG_LOG(1, LOG_STAR_SEPARATOR, L"%s ... %s", MsgStrA, MsgStrB);
            INFO_LOG("\n\n");
            INFO_LOG("INFO: %s ... %s", MsgStrA, MsgStrB);
            INFO_LOG("\n\n");
        }
#endif

        SingleAPFS = FALSE;

        return;
    }

    for (j = 0; j < PreBootVolumesCount; j++) {
        ActiveContainer = FALSE;

        for (i = 0; i < VolumesCount; i++) {
            GotSystemVol = ((Volumes[i]->VolName != NULL) && (StrLen(Volumes[i]->VolName) != 0) &&
                            (Volumes[i]->VolRole == APFS_VOLUME_ROLE_SYSTEM ||
                             Volumes[i]->VolRole == APFS_VOLUME_ROLE_UNDEFINED) &&
                            GuidsAreEqual(&(PreBootVolumes[j]->PartGuid), &(Volumes[i]->PartGuid)));

            if (GotSystemVol) {
                if (!ActiveContainer) {
                    ActiveContainer = TRUE;
                }
                else {
                    SingleAPFS = FALSE;

                    break;
                }
            }
        }

        if (!SingleAPFS) {

#if MERIDIAN_DEBUG > 0
            MsgStrA = L"Found APFS Container(s) with Multiple Mac OS Instances";
            INFO_LOG("\n\n");
            INFO_LOG("INFO: %s", MsgStrA);

            if (AppleRecovery) {

                DEBUG_LOG(1, LOG_STAR_SEPARATOR, L"%s ... %s", MsgStrA, MsgStrB);
                INFO_LOG("%s      * %s", OffsetNext, MsgStrB);
            }

            DEBUG_LOG(1, LOG_STAR_SEPARATOR, L"%s ... %s", MsgStrA, MsgStrC);
            INFO_LOG("%s      * %s", OffsetNext, MsgStrC);
#endif

            break;
        }
    }
}

static VOID VetSyncAPFS(VOID)
{
#if MERIDIAN_DEBUG > 0
    CHAR16 *MsgStr;
    CHAR16 *TmpMsg;
#endif

    UINTN i, j, k;
    CHAR16 *CheckName;
    CHAR16 *TweakName;
    CHAR16 *TestName;
    CHAR16 *DataName;
    BOOLEAN GotName;

#if MERIDIAN_DEBUG > 0
    TmpMsg = L"S Y N C   A P F S   V O L U M E S";
    DEBUG_LOG(1, LOG_LINE_SEPARATOR, L"%s", TmpMsg);
    INFO_LOG("\n\n");
    INFO_LOG("%s", TmpMsg);
    INFO_LOG("\n");
#endif

    if (PreBootVolumesCount == 0) {
#if MERIDIAN_DEBUG > 0
        TmpMsg = L"Activated 'disable_apfs_sync' ... Could *NOT* Identify APFS PreBoot Volumes";
        DEBUG_LOG(1, LOG_STAR_SEPARATOR, L"%s", TmpMsg);
        INFO_LOG("INFO: %s", TmpMsg);
#endif

        GlobalConfig.SyncAPFS = FALSE;

        return;
    }

    if (!ValidAPFS) {
#if MERIDIAN_DEBUG > 0
        TmpMsg = L"Activated 'disable_apfs_sync' ... Could *NOT* Get VolUUID/PartGUID on One/More "
                 L"Key APFS Volumes";
        DEBUG_LOG(1, LOG_STAR_SEPARATOR, L"%s", TmpMsg);
        INFO_LOG("INFO: %s", TmpMsg);
#endif

        GlobalConfig.SyncAPFS = FALSE;

        return;
    }

#if MERIDIAN_DEBUG > 0
    TmpMsg = L"Remap Potential APFS Volume Groups";
    DEBUG_LOG(1, LOG_THREE_STAR_SEP, L"%s", TmpMsg);
    INFO_LOG("%s:", TmpMsg);

    for (i = 0; i < SystemVolumesCount; i++) {
        MsgStr = PoolPrint(L"Potential System Volume:- '%s'", SystemVolumes[i]->VolName);
        DEBUG_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
        INFO_LOG("%s  - %s", OffsetNext, MsgStr);
        MRD_FREE_POOL(MsgStr);
    }
#endif

    for (i = 0; i < DataVolumesCount; i++) {
        k = 0;
        while (1) {
            DataName = FindCommaDelimited(DATA_NAME_APFS, k++);
            if (DataName == NULL)
                break;

            TestName = PoolPrint(L"- %s", DataName);
            if (TestName != NULL && MrdStrEndsWithCI(TestName, DataVolumes[i]->VolName)) {
                GotName = FALSE;
                for (j = 0; j < SystemVolumesCount; j++) {
                    TweakName = SanitiseString(SystemVolumes[j]->VolName);
                    CheckName = PoolPrint(L"%s - %s", TweakName, DataName);

                    if (MrdStrEqualsCI(DataVolumes[i]->VolName, CheckName)) {
                        GotName = TRUE;
                    }
                    else {
                        if (!MrdStrEqualsCI(SystemVolumes[j]->VolName, TweakName)) {

                            MRD_FREE_POOL(CheckName);
                            CheckName = PoolPrint(L"%s - %s", SystemVolumes[j]->VolName, DataName);

                            if (MrdStrEqualsCI(DataVolumes[i]->VolName, CheckName)) {
                                GotName = TRUE;
                            }
                        }
                    }

                    MRD_FREE_POOL(TweakName);
                    MRD_FREE_POOL(CheckName);

                    if (GotName) {
                        MRD_FREE_POOL(DataVolumes[i]->VolName);
                        DataVolumes[i]->VolName = StrDuplicate(SystemVolumes[j]->VolName);

                        break;
                    }
                }
            }

            MRD_FREE_POOL(TestName);
            MRD_FREE_POOL(DataName);
        }
    }

#if MERIDIAN_DEBUG > 0
    MsgStr = PoolPrint(L"Processed %d Potential APFS Volume Group%s", SystemVolumesCount,
                       (SystemVolumesCount == 1) ? L"" : L"s");
    DEBUG_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);

    if (SystemVolumesCount == 0) {
        INFO_LOG("%s%s", OffsetNext, MsgStr);
    }
    else {
        INFO_LOG("\n\n");
        INFO_LOG("INFO: %s", MsgStr);
    }
    MRD_FREE_POOL(MsgStr);
#endif

    VetMultiInstanceAPFS();
}

#if MERIDIAN_DEBUG > 0
static CHAR16 *GetApfsRoleString(IN APFS_VOLUME_ROLE VolumeRole)
{
    CHAR16 *retval;

    switch (VolumeRole) {
    case APFS_VOLUME_ROLE_UNDEFINED:
        retval = L"0x00 - APFS Undefined";
        break;
    case APFS_VOLUME_ROLE_SYSTEM:
        retval = L"0x01 - APFS System";
        break;
    case APFS_VOLUME_ROLE_USER:
        retval = L"0x02 - APFS User Home";
        break;
    case APFS_VOLUME_ROLE_RECOVERY:
        retval = L"0x04 - APFS Recovery";
        break;
    case APFS_VOLUME_ROLE_VM:
        retval = L"0x08 - APFS VM";
        break;
    case APFS_VOLUME_ROLE_PREBOOT:
        retval = L"0x10 - APFS Pre-Boot";
        break;
    case APFS_VOLUME_ROLE_INSTALLER:
        retval = L"0x20 - APFS Installer";
        break;
    case APFS_VOLUME_ROLE_DATA:
        retval = L"0x40 - APFS Data";
        break;
    case APFS_VOLUME_ROLE_UPDATE:
        retval = L"0xC0 - APFS Snapshot";
        break;
    case APFS_VOL_ROLE_XART:
        retval = L"0x0? - APFS Secured Data";
        break;
    case APFS_VOL_ROLE_HARDWARE:
        retval = L"0x1? - APFS FirmwareData";
        break;
    case APFS_VOL_ROLE_BACKUP:
        retval = L"0x2? - APFS Backup (TM)";
        break;
    case APFS_VOL_ROLE_RESERVED_7:
        retval = L"0x3? - APFS Reserved 07";
        break;
    case APFS_VOL_ROLE_RESERVED_8:
        retval = L"0x4? - APFS Reserved 08";
        break;
    case APFS_VOL_ROLE_ENTERPRISE:
        retval = L"0x5? - APFS Enterprise";
        break;
    case APFS_VOL_ROLE_RESERVED_10:
        retval = L"0x6? - APFS Reserved 10";
        break;
    case APFS_VOL_ROLE_PRELOGIN:
        retval = L"0x7? - APFS Pre-Login";
        break;
    default:
        retval = L"0xFF - APFS Unknown Role";
        break;
    }

    return retval;
}
#endif

VOID ScanVolumes(VOID)
{
#if MERIDIAN_DEBUG > 0
    CHAR16 *MsgStr;
    CHAR16 *TmpMsg;
    CHAR16 *PartName;
    CHAR16 *PartGUID;
    CHAR16 *VolumeUUID;
    CHAR16 *PartTypeGUID;

    const CHAR16 *ITEMVOLA = L"PARTITION TYPE GUID";
    const CHAR16 *ITEMVOLB = L"PARTITION GUID";
    const CHAR16 *ITEMVOLC = L"PARTITION TYPE";
    const CHAR16 *ITEMVOLD = L"VOLUME UUID";
    const CHAR16 *ITEMVOLE = L"VOLUME ROLE";
    const CHAR16 *ITEMVOLF = L"VOLUME NAME";
#endif

    EFI_STATUS Status;
    EFI_HANDLE *Handles;
    MERIDIAN_VOLUME *Volume;
    MERIDIAN_VOLUME *WholeDiskVolume;
    MBR_PARTITION_INFO *MbrTable;
    UINTN i, j;
    UINTN SectorSum;
    UINTN HandleCount;
    UINTN HandleIndex;
    UINTN VolumeIndex;
    UINTN VolumeIndex2;
    UINTN PartitionIndex;
    UINT8 *SectorBuffer1;
    UINT8 *SectorBuffer2;
    CHAR16 *StrSelfUUID;
    CHAR16 *VentoyName;
    CHAR16 *TempUUID;
    CHAR16 *PartType;
    CHAR16 *RoleStr;
    BOOLEAN DupFlag;
    BOOLEAN FoundVentoy;
    EFI_GUID VolumeGuid = NULL_GUID_VALUE;
    EFI_GUID *UuidList;
    APFS_VOLUME_ROLE VolumeRole;

#if MERIDIAN_DEBUG > 0
    TmpMsg = L"A S S E S S   D E T E C T E D   V O L U M E S";
    DEBUG_LOG(1, LOG_LINE_SEPARATOR, L"%s", TmpMsg);
    INFO_LOG("\n\n");
    INFO_LOG("%s", TmpMsg);
#endif

    if (SelfVolRun) {

        FreeVolumes();
        FreeSyncVolumes();
        ForgetPartitionTables();
    }

    HandleCount = 0;
    Status = LibLocateHandle(ByProtocol, &BlockIoProtocol, NULL, &HandleCount, &Handles);
    if (EFI_ERROR(Status)) {
#if MERIDIAN_DEBUG > 0
        MsgStr = PoolPrint(
            L"In ScanVolumes ... '%r' While Listing File Systems (*** FATAL ERROR ***)", Status);
        DEBUG_LOG(1, LOG_LINE_THIN_SEP, L"%s!!", MsgStr);
        INFO_LOG("\n\n");
        INFO_LOG("** %s", MsgStr);
        INFO_LOG("\n\n");
        MRD_FREE_POOL(MsgStr);
#endif

        return;
    }

#if MERIDIAN_DEBUG > 0
    INFO_LOG("\nINFO: ScanVolumes ... %d BlockIo handle(s) located", HandleCount);
#endif

    UuidList = AllocateZeroPool(sizeof(EFI_GUID) * HandleCount);
    if (UuidList == NULL) {
#if MERIDIAN_DEBUG > 0
        Status = EFI_BUFFER_TOO_SMALL;

        MsgStr = PoolPrint(L"In ScanVolumes ... Allocate UuidList:- '%r'", Status);
        DEBUG_LOG(1, LOG_BLANK_LINE_SEP, L"X");
        DEBUG_LOG(1, LOG_LINE_THIN_SEP, L"%s!!", MsgStr);
        INFO_LOG("\n\n");
        INFO_LOG("** WARN: %s", MsgStr);
        INFO_LOG("\n\n");
        MRD_FREE_POOL(MsgStr);
#endif

        MRD_FREE_POOL(Handles);

        return;
    }

    DoneHeadings = FALSE;
    SkipSpacing = FALSE;
    ValidAPFS = TRUE;

#if MERIDIAN_DEBUG > 0
    FoundMBR = FALSE;
    ScannedOnce = FALSE;
    FirstVolume = TRUE;
#endif

    for (HandleIndex = 0; HandleIndex < HandleCount; HandleIndex++) {
        if (!SelfVolRun && SelfLoadedImage->DeviceHandle != Handles[HandleIndex]) {
            continue;
        }

        Volume = AllocateZeroPool(sizeof(MERIDIAN_VOLUME));
        if (Volume == NULL) {
            MRD_FREE_POOL(UuidList);
            MRD_FREE_POOL(Handles);

#if MERIDIAN_DEBUG > 0
            Status = EFI_BUFFER_TOO_SMALL;

            MsgStr = PoolPrint(L"In ScanVolumes ... Allocate Volumes:- '%r'", Status);
            DEBUG_LOG(1, LOG_BLANK_LINE_SEP, L"X");
            DEBUG_LOG(1, LOG_LINE_THIN_SEP, L"%!!s", MsgStr);
            INFO_LOG("\n\n");
            INFO_LOG("** WARN: %s", MsgStr);
            INFO_LOG("\n\n");
            MRD_FREE_POOL(MsgStr);
#endif

            return;
        }

        Volume->VolRole = APFS_VOLUME_ROLE_UNKNOWN;
        Volume->DeviceHandle = Handles[HandleIndex];
        AddPartitionTable(Volume);
        ScanVolume(Volume);

        if (Volume->DeviceHandle == SelfLoadedImage->DeviceHandle) {
            SelfVolume = CopyVolume(Volume);
            SelfVolSet = TRUE;
        }

        if (!SelfVolRun) {
            FreeVolume(&Volume);

            if (SelfVolSet) {

                break;
            }

#if MERIDIAN_DEBUG > 0
            FirstVolume = FALSE;
            ScannedOnce = TRUE;
#endif

            continue;
        }

        if (!SelfVolSet) {
            StrSelfUUID = NULL;
        }
        else {
            StrSelfUUID = GuidAsString(&SelfVolume->VolUuid);
        }

        UuidList[HandleIndex] = Volume->VolUuid;

        for (i = 0; i < HandleIndex; i++) {
            if (!GlobalConfig.ScanAllESP) {
                DupFlag =
                    ((CompareMem(&(Volume->VolUuid), &(UuidList[i]), sizeof(EFI_GUID)) == 0) &&
                     (CompareMem(&(Volume->VolUuid), &GuidNull, sizeof(EFI_GUID)) != 0));
            }
            else {
                DupFlag =
                    ((!GuidsAreEqual(&(Volume->PartTypeGuid), &GuidESP)) &&
                     (CompareMem(&(Volume->VolUuid), &(UuidList[i]), sizeof(EFI_GUID)) == 0) &&
                     (CompareMem(&(Volume->VolUuid), &GuidNull, sizeof(EFI_GUID)) != 0));

                if (!DupFlag && SelfVolSet && SelfVolume != NULL &&
                    !GuidsAreEqual(&(Volume->VolUuid), &GuidNull) &&
                    GuidsAreEqual(&(Volume->PartTypeGuid), &GuidESP)) {
                    TempUUID = GuidAsString(&Volume->VolUuid);
                    DupFlag = MrdStrEqualsCI(StrSelfUUID, TempUUID);
                    MRD_FREE_POOL(TempUUID);
                }
            }

            if (Volume->DeviceHandle == SelfLoadedImage->DeviceHandle) {
                DupFlag = FALSE;
            }

            if (DupFlag) {

                Volume->IsReadable = FALSE;
                break;
            }
        }

        MRD_FREE_POOL(StrSelfUUID);

        AddListElement((VOID ***)&Volumes, &VolumesCount, Volume);

#if MERIDIAN_DEBUG > 0
        if (SkipSpacing) {
            INFO_LOG("%s", ENTRY_BELOW_TXT);
        }

        if (!DoneHeadings) {
            BRK_MOD("\n");
        }
        else if (ScannedOnce) {
            if (!SkipSpacing && (HandleIndex % 4) == 0 && (HandleCount - HandleIndex) > 2) {
                if ((HandleIndex % 24) == 0 && (HandleCount - HandleIndex) > (12 + 2)) {
                    DoneHeadings = FALSE;
                    BRK_MOD("\n\n                   ");
                }
                else {
                    BRK_MOD("\n\n");
                }
            }
            else {
                BRK_MOD("\n");
            }
        }
        SkipSpacing = FALSE;

        if (!DoneHeadings) {
            INFO_LOG("%-39s%-39s%-21s%-39s%-27s%s", ITEMVOLA, ITEMVOLB, ITEMVOLC, ITEMVOLD,
                     ITEMVOLE, ITEMVOLF);
            INFO_LOG("\n");
            DoneHeadings = TRUE;

            if (FirstVolume) {
                if (FoundMBR || Volume->HasBootCode) {
                    if (Volume->HasBootCode) {
                        INFO_LOG("%s for %s", LEGACY_CODE_TXT,
                                 (Volume->OSName != NULL) ? Volume->OSName : UNKNOWN_OS);
                    }
                    if (FoundMBR) {
                        if (Volume->HasBootCode) {
                            INFO_LOG("%s", (UseButJoin) ? BUT_JOIN_TXT : AND_JOIN_TXT);
                        }
                        INFO_LOG("%s", PARTITION_TABLE_TXT);
                    }
                    INFO_LOG("%s", ENTRY_BELOW_TXT);
                    INFO_LOG("\n");
                }
            }
        }
#endif

        PartType = FSTypeName(Volume);
        if (MrdStrEndsWithCI(L"(Assumed)", PartType)) {
            if (0)
                ;
            else if (MrdStrStartsWithCI(L"APFS", PartType))
                Volume->FSType = FS_TYPE_APFS;
            else if (MrdStrStartsWithCI(L"NTFS", PartType))
                Volume->FSType = FS_TYPE_NTFS;
            else if (MrdStrStartsWithCI(L"Ext4", PartType))
                Volume->FSType = FS_TYPE_EXT4;
            else if (MrdStrStartsWithCI(L"ExFAT", PartType))
                Volume->FSType = FS_TYPE_EXFAT;
            else if (MrdStrStartsWithCI(L"FAT-32", PartType))
                Volume->FSType = FS_TYPE_FAT32;
            else if (MrdStrStartsWithCI(L"ISO-9660", PartType))
                Volume->FSType = FS_TYPE_ISO9660;
            else if (MrdStrStartsWithCI(L"HFS+", PartType))
                Volume->FSType = FS_TYPE_HFSPLUS;
        }

        RoleStr = NULL;
        VolumeRole = APFS_VOLUME_ROLE_UNKNOWN;
        if (0)
            ;
        else if (MrdStrIncludesCI(Volume->VolName, L"APFS/FileVault"))
            RoleStr = L"?* Type Entity-Container";
        else if (MrdStrEqualsCI(Volume->VolName, L"Whole Disk Volume"))
            RoleStr = L" * Type Entity-WholeDisk";
        else if (GuidsAreEqual(&(Volume->PartTypeGuid), &GuidESP))
            RoleStr = L" * Part System EFI (ESP)";
        else if (GuidsAreEqual(&(Volume->PartTypeGuid), &GuidLinux))
            RoleStr = L" * Part Linux FileSystem";
        else if (GuidsAreEqual(&(Volume->PartTypeGuid), &gRootGuid))
            RoleStr = L" * Part Linux RootVolume";
        else if (GuidsAreEqual(&(Volume->PartTypeGuid), &GuidHome))
            RoleStr = L" * Part Linux HomeVolume";
        else if (GuidsAreEqual(&(Volume->PartTypeGuid), &GuidSwap))
            RoleStr = L" * Part Linux SwapVolume";
        else if (GuidsAreEqual(&(Volume->PartTypeGuid), &GuidBasicData))
            RoleStr = L" * Type Volume-BasicData";
        else if (MrdStrIncludesCI(Volume->VolName, L"Optical Disc"))
            RoleStr = L" * Type Entity-OpticDisk";
        else if (MrdStrIncludesCI(PartType, L"Apple Raid"))
            RoleStr = L" * Type Entity-AppleRAID";
        else if (AppleFirmware && MrdStrEqualsCI(Volume->VolName, L"BOOTCAMP"))
            RoleStr = L" * Part Windows BootCamp";
        else if (MrdStrEqualsCI(Volume->VolName, L"Boot OS X"))
            RoleStr = L" * Part BootAssist (Mac)";
        else if (GuidsAreEqual(&(Volume->PartTypeGuid), &GuidRecoveryHD))
            RoleStr = L" * Part RecoveryHD (HFS)";
        else if (GuidsAreEqual(&(Volume->PartTypeGuid), &GuidWindowsRE))
            RoleStr = L" * Part RecoveryHD (Win)";
        else if (GuidsAreEqual(&(Volume->PartTypeGuid), &GuidReservedMS))
            RoleStr = L" * Part ReservedHD (Win)";
        else if (GuidsAreEqual(&(Volume->PartTypeGuid), &GuidLuks))
            RoleStr = L" * Type Encrypted Volume";
        else if (MrdStrEqualsCI(Volume->VolName, L"Unknown Volume"))
            RoleStr = L"?? Role Not Known";
        else {
            Status =
                MeridianGetApfsVolumeInfo(Volume->DeviceHandle, NULL, &VolumeGuid, &VolumeRole);

            if (!EFI_ERROR(Status) || Volume->FSType == FS_TYPE_APFS) {
                PartType = L"APFS";
                Volume->FSType = FS_TYPE_APFS;
                Volume->VolUuid = VolumeGuid;
                Volume->VolRole = VolumeRole;

#if MERIDIAN_DEBUG > 0
                RoleStr = GetApfsRoleString(VolumeRole);
#endif

                if (ValidAPFS) {
                    if (Volume->VolRole == APFS_VOLUME_ROLE_RECOVERY) {

                        Volume->IsReadable = FALSE;

                        AddListElement((VOID ***)&RecoveryVolumesAPFS, &RecoveryVolumesAPFSCount,
                                       Volume);
                    }
                    else if (Volume->VolRole == APFS_VOLUME_ROLE_DATA) {

                        Volume->IsReadable = FALSE;

                        AddListElement((VOID ***)&DataVolumes, &DataVolumesCount, Volume);
                    }
                    else if (Volume->VolRole == APFS_VOLUME_ROLE_PREBOOT) {

                        AddListElement((VOID ***)&PreBootVolumes, &PreBootVolumesCount, Volume);
                    }
                    else if (Volume->VolRole == APFS_VOLUME_ROLE_SYSTEM ||
                             Volume->VolRole == APFS_VOLUME_ROLE_UNDEFINED) {

                        AddListElement((VOID ***)&SystemVolumes, &SystemVolumesCount, Volume);

                        if (!ShouldScan(Volume, MACOSX_LOADER_DIR)) {

                            AddListElement((VOID ***)&SkipApfsVolumes, &SkipApfsVolumesCount,
                                           Volume);
                        }
                    }
                    else {

                        Volume->IsReadable = FALSE;
                    }

                    if (Volume->IsReadable && (GuidsAreEqual(&GuidNull, &(Volume->VolUuid)) ||
                                               GuidsAreEqual(&GuidNull, &(Volume->PartGuid)))) {
                        ValidAPFS = FALSE;
                    }
                }
            }
        }

        if (RoleStr != NULL) {
            if (Volume->FSType == FS_TYPE_HFSPLUS &&
                MrdStrEqualsCI(RoleStr, L" * Part RecoveryHD (HFS)")) {

                AddListElement((VOID ***)&RecoveryVolumesHFS, &RecoveryVolumesHFSCount, Volume);
            }
        }
        else {
            if (Volume->FSType == FS_TYPE_NTFS) {
                RoleStr = (MrdStrEqualsCI(Volume->VolName, L"System Reserved"))
                              ? L" * Part SysReserve (Win)"
                          : (MrdStrEqualsCI(Volume->VolName, L"NTFS Volume"))
                              ? L" * Type WinDrive (Other)"
                              : L" * Type WinDrive (Named)";
            }
            else if (Volume->FSType == FS_TYPE_HFSPLUS) {
                if (GuidsAreEqual(&GuidHFS, &(Volume->PartTypeGuid))) {
                    RoleStr = (FileExists(Volume->RootDir, MACOSX_LOADER_PATH))
                                  ? L" * Part MacOS Boot (HFS)"
                                  : L" * Part Other/Data (HFS)";
                }
                else {
                    Volume->FSType = FS_TYPE_UNKNOWN;

#if MERIDIAN_DEBUG > 0
                    PartType = LABEL_UNKNOWN;
#endif
                }
            }
        }

        if (RoleStr == NULL) {
            FoundVentoy = FALSE;
            j = 0;
            while (GlobalConfig.HandleVentoy && !FoundVentoy) {
                VentoyName = FindCommaDelimited(VENTOY_NAMES, j++);
                if (VentoyName == NULL)
                    break;

                if (MrdStrStartsWithCI(VentoyName, Volume->VolName)) {
                    RoleStr = L" * Part Ventoy ISO Boot";
                    FoundVentoy = TRUE;
                }

                MRD_FREE_POOL(VentoyName);
            }

            if (RoleStr == NULL) {
                RoleStr = (Volume->FSType == FS_TYPE_EXFAT) ? L" * Part Data Store"
                                                            : L"** Role Undefined";
            }
        }

#if MERIDIAN_DEBUG > 0

        PartName = StrDuplicate(PartType);
        PartGUID = GuidAsString(&(Volume->PartGuid));
        PartTypeGUID = GuidAsString(&(Volume->PartTypeGuid));
        VolumeUUID = GuidAsString(&(Volume->VolUuid));

        LimitStringLength(PartName, 18);

        MsgStr = PoolPrint(L"%-36s : %-36s : %-18s : %-36s : %-24s : %s", PartTypeGUID, PartGUID,
                           PartName, VolumeUUID, RoleStr, Volume->VolName);

        DEBUG_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
        INFO_LOG("%s", MsgStr);
        MRD_FREE_POOL(MsgStr);

        MRD_FREE_POOL(PartName);
        MRD_FREE_POOL(PartGUID);
        MRD_FREE_POOL(PartTypeGUID);
        MRD_FREE_POOL(VolumeUUID);

        FoundMBR = FALSE;
        UseButJoin = FALSE;
        FirstVolume = FALSE;
        ScannedOnce = TRUE;
#endif
    }

    MRD_FREE_POOL(UuidList);
    MRD_FREE_POOL(Handles);

    if (!SelfVolSet || !SelfVolRun) {
        SelfVolRun = TRUE;

        return;
    }

#if MERIDIAN_DEBUG > 0
    MsgStr = PoolPrint(L"%-39s%-39s%-21s%-39s%-27s%s", ITEMVOLA, ITEMVOLB, ITEMVOLC, ITEMVOLD,
                       ITEMVOLE, ITEMVOLF);
    DEBUG_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
    INFO_LOG("%s", OffsetNext);
    INFO_LOG("%s", MsgStr);
    INFO_LOG("\n\n");
    MRD_FREE_POOL(MsgStr);
#endif

    for (VolumeIndex = 0; VolumeIndex < VolumesCount; VolumeIndex++) {
        Volume = Volumes[VolumeIndex];

        if (Volume->BlockIOOffset == 0 && Volume->MbrPartitionTable != NULL &&
            Volume->WholeDiskBlockIO != NULL && Volume->WholeDiskBlockIO == Volume->BlockIO) {
            MbrTable = Volume->MbrPartitionTable;
            for (PartitionIndex = 0; PartitionIndex < 4; PartitionIndex++) {
                if (IS_EXTENDED_PART_TYPE(MbrTable[PartitionIndex].Type)) {
                    ScanExtendedPartition(Volume, MbrTable + PartitionIndex);
                }
            }
        }

        WholeDiskVolume = NULL;
        if (Volume->BlockIO != NULL && Volume->WholeDiskBlockIO != NULL &&
            Volume->WholeDiskBlockIO != Volume->BlockIO) {
            for (VolumeIndex2 = 0; VolumeIndex2 < VolumesCount; VolumeIndex2++) {
                if (Volumes[VolumeIndex2]->BlockIOOffset == 0 &&
                    Volumes[VolumeIndex2]->BlockIO == Volume->WholeDiskBlockIO) {
                    WholeDiskVolume = Volumes[VolumeIndex2];

                    break;
                }
            }
        }

        if (WholeDiskVolume && WholeDiskVolume->MbrPartitionTable) {

            SectorBuffer1 = AllocatePool(BASE_SIZE);
            SectorBuffer2 = AllocatePool(BASE_SIZE);
            MbrTable = WholeDiskVolume->MbrPartitionTable;
            for (PartitionIndex = 0; PartitionIndex < 4; PartitionIndex++) {

                if ((UINT64)(MbrTable[PartitionIndex].Size) !=
                    Volume->BlockIO->Media->LastBlock + 1) {
                    continue;
                }

                Status =
                    Volume->BlockIO->ReadBlocks(Volume->BlockIO, Volume->BlockIO->Media->MediaId,
                                                Volume->BlockIOOffset, BASE_SIZE, SectorBuffer1);
                if (EFI_ERROR(Status)) {
                    break;
                }

                Status = Volume->WholeDiskBlockIO->ReadBlocks(
                    Volume->WholeDiskBlockIO, Volume->WholeDiskBlockIO->Media->MediaId,
                    MbrTable[PartitionIndex].StartLBA, BASE_SIZE, SectorBuffer2);
                if (EFI_ERROR(Status)) {
                    break;
                }

                if (CompareMem(SectorBuffer1, SectorBuffer2, BASE_SIZE) != 0) {
                    continue;
                }

                SectorSum = 0;
                for (i = 0; i < BASE_SIZE; i++) {
                    SectorSum += SectorBuffer1[i];
                }
                if (SectorSum < 1000) {
                    continue;
                }

                Volume->IsMbrPartition = TRUE;
                Volume->MbrPartitionIndex = PartitionIndex;
                if (Volume->VolName == NULL) {
                    Volume->VolName = PoolPrint(L"Partition %d", PartitionIndex + 1);
                }

                break;
            }

            MRD_FREE_POOL(SectorBuffer1);
            MRD_FREE_POOL(SectorBuffer2);
        }
    }

#if MERIDIAN_DEBUG > 0
    MsgStr = PoolPrint(L"Assessed %d Volume%s", VolumesCount, (VolumesCount == 1) ? L"" : L"s");
    INFO_LOG("INFO: %s", MsgStr);
    DEBUG_LOG(1, LOG_LINE_THIN_SEP, L"%s", MsgStr);
    MRD_FREE_POOL(MsgStr);
#endif

    if (SelfVolRun && GlobalConfig.SyncAPFS) {
        VetSyncAPFS();
    }

#if MERIDIAN_DEBUG > 0
    MuteLogger = FALSE;
    INFO_LOG("\n\n");
#endif
}
