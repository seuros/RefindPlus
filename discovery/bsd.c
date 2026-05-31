// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2020-2026 Dayo Akanji
// SPDX-FileCopyrightText: 2012-2023 Roderick W. Smith

#include "global.h"
#include "config.h"
#include "lib.h"
#include "menu.h"
#include "scan.h"
#include "linux.h"
#include "apple.h"
#include "mystrings.h"
#include "launch_efi.h"

#define BSD_KIND_UNKNOWN (0)
#define BSD_KIND_FREEBSD (1)
#define BSD_KIND_OPENBSD (2)
#define BSD_KIND_DRAGONFLY (3)
#define BSD_KIND_NETBSD (4)
#define BSD_KIND_GHOSTBSD (5)

#define BSD_FREEBSD_LOADER_PATH L"boot\\loader.efi"
#define BSD_FREEBSD_LOADER_DIR L"boot"
#define BSD_FREEBSD_LOADER_NAME L"loader.efi"

#if defined(EFIX64)
#define BSD_OPENBSD_LOADER_PATH L"usr\\mdec\\BOOTX64.EFI"
#define BSD_OPENBSD_LOADER_NAME L"BOOTX64.EFI"
#elif defined(EFIAARCH64)
#define BSD_OPENBSD_LOADER_PATH L"usr\\mdec\\BOOTAA64.EFI"
#define BSD_OPENBSD_LOADER_NAME L"BOOTAA64.EFI"
#else
#define BSD_OPENBSD_LOADER_PATH L"usr\\mdec\\BOOT.EFI"
#define BSD_OPENBSD_LOADER_NAME L"BOOT.EFI"
#endif
#define BSD_OPENBSD_LOADER_DIR L"usr\\mdec"

static EFI_GUID GuidFreeBsdUfs = {
    0x516E7CB6, 0x6ECF, 0x11D6, {0x8F, 0xF8, 0x00, 0x02, 0x2D, 0x09, 0x71, 0x2B}};

BOOLEAN IsInstallerMac(MERIDIAN_VOLUME *Volume)
{
    BOOLEAN MacInstaller;

    MacInstaller = (MrdStrIncludesCI(Volume->VolName, L"Install Mac OS") ||
                    MrdStrIncludesCI(Volume->VolName, L"Install macOS") ||
                    MrdStrIncludesCI(Volume->VolName, L"Install OS X") ||
                    MrdStrIncludesCI(Volume->VolName, L"OS X Install") ||
                    MrdStrIncludesCI(Volume->VolName, L"macOS Install") ||
                    MrdStrIncludesCI(Volume->VolName, L"Mac OS Install"));

    return MacInstaller;
}

static UINTN BsdKindFromName(IN CHAR16 *Name)
{
    if (Name == NULL) {
        return BSD_KIND_UNKNOWN;
    }

    if (MrdStrIncludesCI(Name, L"dragonfly")) {
        return BSD_KIND_DRAGONFLY;
    }

    if (MrdStrIncludesCI(Name, L"openbsd")) {
        return BSD_KIND_OPENBSD;
    }

    if (MrdStrIncludesCI(Name, L"netbsd")) {
        return BSD_KIND_NETBSD;
    }

    if (MrdStrIncludesCI(Name, L"ghostbsd")) {
        return BSD_KIND_GHOSTBSD;
    }

    if (MrdStrIncludesCI(Name, L"freebsd")) {
        return BSD_KIND_FREEBSD;
    }

    return BSD_KIND_UNKNOWN;
}

UINTN DetectBsdKind(IN MERIDIAN_VOLUME *Volume)
{
    EFI_STATUS Status;
    MERIDIAN_FILE *File;
    UINTN Kind;
    UINTN FileSize;
    UINTN TokenCount;
    CHAR16 **TokenList;

    if (Volume == NULL || Volume->RootDir == NULL) {
        return BSD_KIND_UNKNOWN;
    }

    Kind = BSD_KIND_UNKNOWN;
    if (FileExists(Volume->RootDir, L"etc\\os-release")) {
        File = AllocateZeroPool(sizeof(MERIDIAN_FILE));
        if (File != NULL) {
            FileSize = 0;
            Status = MeridianReadFile(Volume->RootDir, L"etc\\os-release", File, &FileSize);
            if (!EFI_ERROR(Status)) {
                while (Kind == BSD_KIND_UNKNOWN) {
                    TokenCount = ReadTokenLine(File, &TokenList);
                    if (TokenCount == 0)
                        break;

                    if (TokenCount > 1 && (MrdStrEqualsCI(TokenList[0], L"ID") ||
                                           MrdStrEqualsCI(TokenList[0], L"NAME") ||
                                           MrdStrEqualsCI(TokenList[0], L"PRETTY_NAME"))) {
                        Kind = BsdKindFromName(TokenList[1]);
                    }

                    FreeTokenLine(&TokenList, &TokenCount);
                }
            }

            MRD_FREE_FILE(File);
        }
    }

    if (Kind == BSD_KIND_UNKNOWN) {
        if (FileExists(Volume->RootDir, BSD_OPENBSD_LOADER_PATH) &&
            (FileExists(Volume->RootDir, L"bsd") || FileExists(Volume->RootDir, L"bsd.rd"))) {
            Kind = BSD_KIND_OPENBSD;
        }
        else if (FileExists(Volume->RootDir, BSD_FREEBSD_LOADER_PATH)) {
            Kind = BSD_KIND_FREEBSD;
        }
    }

    return Kind;
}

static CHAR16 *BsdTitleFromKind(IN UINTN Kind)
{
    if (0)
        ;
    else if (Kind == BSD_KIND_OPENBSD)
        return L"Instance: OpenBSD";
    else if (Kind == BSD_KIND_DRAGONFLY)
        return L"Instance: DragonFly BSD";
    else if (Kind == BSD_KIND_NETBSD)
        return L"Instance: NetBSD";
    else if (Kind == BSD_KIND_GHOSTBSD)
        return L"Instance: GhostBSD";
    else
        return L"Instance: FreeBSD";
}

CHAR16 *BsdEspLoaderTitle(IN CHAR16 *LoaderPath)
{
    UINTN Kind;
    CHAR16 *DirName;
    CHAR16 *Title;

    if (LoaderPath == NULL) {
        return NULL;
    }

    DirName = FindLastDirName(LoaderPath);
    if (DirName == NULL) {
        return NULL;
    }

    Kind = BsdKindFromName(DirName);
    Title = (Kind == BSD_KIND_UNKNOWN) ? NULL : BsdTitleFromKind(Kind);
    MRD_FREE_POOL(DirName);

    return Title;
}

BOOLEAN IsBsdRootLoaderPath(IN CHAR16 *LoaderPath)
{
    if (LoaderPath == NULL) {
        return FALSE;
    }

    while (*LoaderPath == L'\\') {
        LoaderPath++;
    }

    return (MrdStrEqualsCI(LoaderPath, BSD_FREEBSD_LOADER_PATH) ||
            MrdStrEqualsCI(LoaderPath, BSD_OPENBSD_LOADER_PATH));
}

BOOLEAN IsBsdAuxLoader(IN MERIDIAN_VOLUME *Volume, IN CHAR16 *FileName)
{
    if (Volume == NULL || Volume->RootDir == NULL || FileName == NULL) {
        return FALSE;
    }
    if (!IsListItem(FileName, BSD_AUX_FILES)) {
        return FALSE;
    }

    return Volume->FSType == FS_TYPE_UFS && DetectBsdKind(Volume) != BSD_KIND_UNKNOWN;
}

static BOOLEAN SameWholeDisk(IN MERIDIAN_VOLUME *VolA, IN MERIDIAN_VOLUME *VolB)
{
    if (VolA == NULL || VolB == NULL) {
        return TRUE;
    }
    if (VolA->WholeDiskBlockIO == NULL || VolB->WholeDiskBlockIO == NULL) {
        return TRUE;
    }

    return (VolA->WholeDiskBlockIO == VolB->WholeDiskBlockIO);
}

static BOOLEAN BsdRootLoaderAvailable(IN UINTN Kind, IN MERIDIAN_VOLUME *EspVolume)
{
#if MERIDIAN_DEBUG > 0
    BOOLEAN CheckMute = FALSE;
#endif

    UINTN Index;
    UINTN VolumeKind;
    CHAR16 *LoaderPath;
    MERIDIAN_VOLUME *Volume;
    BOOLEAN Found;

    if (Kind == BSD_KIND_UNKNOWN) {
        return FALSE;
    }

    Found = FALSE;
    for (Index = 0; Index < VolumesCount; Index++) {
        Volume = Volumes[Index];
        if (Volume == NULL || Volume->RootDir == NULL || Volume->FSType != FS_TYPE_UFS) {
            continue;
        }

        if (!SameWholeDisk(Volume, EspVolume)) {
            continue;
        }

        VolumeKind = DetectBsdKind(Volume);
        if (VolumeKind != Kind) {
            continue;
        }

        LoaderPath = (Kind == BSD_KIND_OPENBSD) ? BSD_OPENBSD_LOADER_PATH : BSD_FREEBSD_LOADER_PATH;

#if MERIDIAN_DEBUG > 0
        MRD_MUTELOGGER_SET;
#endif
        Found =
            (FileExists(Volume->RootDir, LoaderPath) && IsValidLoader(Volume->RootDir, LoaderPath));
#if MERIDIAN_DEBUG > 0
        MRD_MUTELOGGER_OFF;
#endif

        if (Found)
            break;
    }

    return Found;
}

BOOLEAN IsDuplicateBsdEspDir(IN MERIDIAN_VOLUME *EspVolume, IN CHAR16 *DirName)
{
    UINTN Kind;

    Kind = BsdKindFromName(DirName);

    return BsdRootLoaderAvailable(Kind, EspVolume);
}

CHAR16 *DragonFlyCurrdevOption(VOID)
{
    UINTN Index;
    EFI_HANDLE TargetHandle;
    EFI_STATUS Status;
    UINTN HandleCount;
    EFI_HANDLE *Handles;
    UINTN PartIndex;
    INTN FoundPart;

    TargetHandle = NULL;
    for (Index = 0; Index < VolumesCount; Index++) {
        MERIDIAN_VOLUME *Volume = Volumes[Index];
        if (Volume == NULL || Volume->DeviceHandle == NULL) {
            continue;
        }
        if (Volume->FSType == FS_TYPE_UFS ||
            GuidsAreEqual(&Volume->PartTypeGuid, &GuidFreeBsdUfs)) {
            TargetHandle = Volume->DeviceHandle;
            break;
        }
    }
    if (TargetHandle == NULL) {
        return NULL;
    }

    HandleCount = 0;
    Handles = NULL;
    Status =
        gBS->LocateHandleBuffer(ByProtocol, &gEfiBlockIoProtocolGuid, NULL, &HandleCount, &Handles);
    if (EFI_ERROR(Status) || Handles == NULL) {
        if (Handles != NULL) {
            MRD_FREE_POOL(Handles);
        }
        return NULL;
    }

    PartIndex = 0;
    FoundPart = -1;
    for (Index = 0; Index < HandleCount; Index++) {
        EFI_BLOCK_IO_PROTOCOL *BlockIo = NULL;
        Status = gBS->HandleProtocol(Handles[Index], &gEfiBlockIoProtocolGuid, (VOID **)&BlockIo);
        if (EFI_ERROR(Status) || BlockIo == NULL || BlockIo->Media == NULL) {
            continue;
        }
        if (!BlockIo->Media->LogicalPartition) {
            continue;
        }
        if (Handles[Index] == TargetHandle) {
            FoundPart = (INTN)PartIndex;
            break;
        }
        PartIndex++;
    }

    MRD_FREE_POOL(Handles);

    if (FoundPart < 0) {
        return NULL;
    }

    return PoolPrint(L"currdev=part%u:", (UINTN)FoundPart);
}

BOOLEAN ScanBsdRootLoader(IN MERIDIAN_VOLUME *Volume)
{
#if MERIDIAN_DEBUG > 0
    BOOLEAN CheckMute = FALSE;
#endif

    UINTN Kind;
    CHAR16 *LoaderDir;
    CHAR16 *LoaderName;
    CHAR16 *LoaderPath;
    CHAR16 *LoaderTitle;
    LOADER_ENTRY *LoaderEntry;

    if (Volume == NULL || Volume->RootDir == NULL || Volume->FSType != FS_TYPE_UFS) {
        return FALSE;
    }

    Kind = DetectBsdKind(Volume);

    if (Kind == BSD_KIND_OPENBSD) {
        LoaderDir = BSD_OPENBSD_LOADER_DIR;
        LoaderName = BSD_OPENBSD_LOADER_NAME;
        LoaderPath = StrDuplicate(BSD_OPENBSD_LOADER_PATH);
    }
    else {
        LoaderDir = BSD_FREEBSD_LOADER_DIR;
        LoaderName = BSD_FREEBSD_LOADER_NAME;
        LoaderPath = StrDuplicate(BSD_FREEBSD_LOADER_PATH);
    }

    if (LoaderPath == NULL) {
        return FALSE;
    }

#if MERIDIAN_DEBUG > 0
    MRD_MUTELOGGER_SET;
#endif
    if (!FileExists(Volume->RootDir, LoaderPath) || !IsValidLoader(Volume->RootDir, LoaderPath) ||
        IsListItem(LoaderName, GlobalConfig.DontScanFiles) ||
        FilenameIn(Volume, LoaderDir, LoaderName, GlobalConfig.DontScanFiles)) {
#if MERIDIAN_DEBUG > 0
        MRD_MUTELOGGER_OFF;
#endif
        MRD_FREE_POOL(LoaderPath);

        return FALSE;
    }
#if MERIDIAN_DEBUG > 0
    MRD_MUTELOGGER_OFF;
#endif

    DisplayLoader = TRUE;
    LoaderTitle = BsdTitleFromKind(Kind);

    LoaderEntry = AddLoaderEntry(LoaderPath, LoaderTitle, Volume, TRUE, FALSE, NULL);

    MRD_FREE_POOL(LoaderPath);

    return (LoaderEntry != NULL);
}
