// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2020-2026 Dayo Akanji
// SPDX-FileCopyrightText: 2012-2023 Roderick W. Smith

#include "global.h"
#include "config.h"
#include "lib.h"
#include "scan.h"
#include "mystrings.h"

CHAR16 *GetVolumeGroupName(IN CHAR16 *LoaderPath, IN MERIDIAN_VOLUME *Volume)
{
    UINTN i;
    UINTN TestLen;
    UINTN NameLen;
    UINTN FlagLen;
    CHAR16 *TempStr;
    CHAR16 *DataTag;
    CHAR16 *VolumeGroupName;

    if (!GlobalConfig.SyncAPFS) {

        return NULL;
    }

    VolumeGroupName = NULL;
    if (SingleAPFS) {
        for (i = 0; i < SystemVolumesCount; i++) {
            if (GuidsAreEqual(&(SystemVolumes[i]->PartGuid), &(Volume->PartGuid))) {
                VolumeGroupName = StrDuplicate(SystemVolumes[i]->VolName);
                break;
            }
        }
    }

    if (VolumeGroupName == NULL) {
        for (i = 0; i < SystemVolumesCount; i++) {
            if (MrdStrIncludesCI(LoaderPath, GuidAsString(&(SystemVolumes[i]->VolUuid)))) {
                VolumeGroupName = StrDuplicate(SystemVolumes[i]->VolName);
                break;
            }
        }
    }

    if (VolumeGroupName == NULL) {
        for (i = 0; i < DataVolumesCount; i++) {
            if (MrdStrIncludesCI(LoaderPath, GuidAsString(&(DataVolumes[i]->VolUuid)))) {

                DataTag = L" - Data";
                TempStr = StrDuplicate(DataVolumes[i]->VolName);
                if (MrdStrIncludesCI(TempStr, DataTag)) {
                    FlagLen = StrLen(DataTag);
                    NameLen = StrLen(TempStr);
                    if (NameLen > FlagLen) {
                        TestLen = NameLen - FlagLen;
                        TruncateString(TempStr, TestLen);
                    }
                }

                VolumeGroupName = PoolPrint(L"%s", TempStr);
                MRD_FREE_POOL(TempStr);
                break;
            }
        }
    }

    return VolumeGroupName;
}

CHAR16 *SetVolKind(IN CHAR16 *OurItem, IN CHAR16 *VolName, IN UINT32 VolFSType)
{
    CHAR16 *RetVal;

    if (0)
        ;
    else if (VolFSType == FS_TYPE_FAT32)
        RetVal = L"";
    else if (VolFSType == FS_TYPE_FAT16)
        RetVal = L"";
    else if (VolFSType == FS_TYPE_FAT12)
        RetVal = L"";
    else if (VolFSType == FS_TYPE_EXFAT)
        RetVal = L"";
    else if (MrdStrEqualsCI(VolName, L"EFI"))
        RetVal = L"";
    else if (MrdStrEqualsCI(VolName, L"ESP"))
        RetVal = L"";
    else if (MrdStrIncludesCI(VolName, L"Volume"))
        RetVal = L"";
    else if (MrdStrIncludesCI(VolName, L"Partition"))
        RetVal = L"";
    else if (MrdStrIncludesCI(VolName, L"XBOOTLDR"))
        RetVal = L"";
    else if (MrdStrEndsWithCI(L"' Dir", OurItem))
        RetVal = L"";
    else if (MrdStrIncludesCI(OurItem, L" via "))
        RetVal = L"";
    else if (MrdStrIncludesCI(OurItem, L"Instance:"))
        RetVal = L"Volume:- ";
    else
        RetVal = L"";

    return RetVal;
}

CHAR16 *SetVolJoin(IN CHAR16 *OurItem, IN BOOLEAN ForBoot)
{
    CHAR16 *RetVal;

    if (0)
        ;
    else if (MrdStrIncludesCI(OurItem, L" Stanza:"))
        RetVal = L"";
    else if (MrdStrIncludesCI(OurItem, L" via Stub"))
        RetVal = L" | ";
    else if (MrdStrEndsWithCI(L"' Dir", OurItem))
        RetVal = L" on ";
    else if (MrdStrIncludesCI(OurItem, L" via "))
        RetVal = L" on ";
    else if (MrdStrEqualsCI(OurItem, L"Legacy Boot"))
        RetVal = L" for ";
    else if (ForBoot)
        RetVal = L" from ";
    else
        RetVal = L" on ";

    return RetVal;
}

CHAR16 *SetVolFlag(IN CHAR16 *OurItem, IN CHAR16 *VolName)
{
    CHAR16 *RetVal;

    if (MrdStrFind(OurItem, L" Stanza:")) {
        RetVal = L"";
    }
    else {
        RetVal = VolName;
    }

    return RetVal;
}

CHAR16 *SetVolType(IN CHAR16 *OurItem OPTIONAL, IN CHAR16 *VolName, IN UINT32 VolFSType)
{
    CHAR16 *RetVal;

    if (0)
        ;
    else if (MrdStrIncludesCI(OurItem, L" Stanza:"))
        RetVal = L"";
    else if (MrdStrIncludesCI(VolName, L"Partition"))
        RetVal = L"";
    else if (MrdStrIncludesCI(VolName, L"Volume"))
        RetVal = L"";
    else if (MrdStrEqualsCI(VolName, L"ESP"))
        RetVal = L"";
    else if (MrdStrEqualsCI(VolName, L"EFI"))
        RetVal = L" System Partition";
    else if (MrdStrIncludesCI(OurItem, L" via Stub")) {
        RetVal = (MrdStrIncludesCI(VolName, L"Linux")) ? L" Partition" : L" Linux Partition";
    }
    else if (MrdStrIncludesCI(OurItem, L"Instance:"))
        RetVal = L"";
    else if (MrdStrIncludesCI(OurItem, L"(Legacy"))
        RetVal = L"";
    else if (MrdStrIncludesCI(OurItem, L"vmLinuz-") || MrdStrIncludesCI(OurItem, L"bzImage-") ||
             MrdStrIncludesCI(OurItem, L"Kernel-") || MrdStrIncludesCI(OurItem, L"Image-")) {
        RetVal = (MrdStrIncludesCI(VolName, L"Linux")) ? L" Partition" : L" Linux Partition";
    }
    else if (MrdStrEqualsCI(OurItem, L"Legacy Boot"))
        RetVal = L" Partition";
    else if (MrdStrEqualsCI(VolName, L"BOOTCAMP"))
        RetVal = L" Partition";
    else if (MrdStrIncludesCI(VolName, L"XBOOTLDR"))
        RetVal = L" Partition";
    else if (VolFSType == FS_TYPE_FAT32)
        RetVal = L" Partition";
    else if (VolFSType == FS_TYPE_FAT16)
        RetVal = L" Partition";
    else if (VolFSType == FS_TYPE_FAT12)
        RetVal = L" Partition";
    else if (VolFSType == FS_TYPE_EXFAT)
        RetVal = L" Partition";
    else
        RetVal = L"";

    return RetVal;
}

CHAR16 *BuildLoaderTitle(IN CHAR16 *Title, IN CHAR16 *VolName, IN UINT32 VolFSType)
{
    return PoolPrint(L"Load %s%s%s%s%s", Title, SetVolJoin(Title, TRUE),
                     SetVolKind(Title, VolName, VolFSType), SetVolFlag(Title, VolName),
                     SetVolType(Title, VolName, VolFSType));
}
