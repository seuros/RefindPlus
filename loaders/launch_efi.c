// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2020-2026 Dayo Akanji
// SPDX-FileCopyrightText: 2012-2024 Roderick W. Smith
// SPDX-FileCopyrightText: 2006-2010 Christoph Pfisterer

#include "global.h"
#include "lib.h"
#include "apple/smc.h"
#include "mok.h"
#include "scan.h"
#include "menu.h"
#include "apple.h"
#include "linux.h"
#include "install.h"
#include "mystrings.h"
#include "screenmgt.h"
#include "conn_bridge.h"
#include "launch_efi.h"
#include "driver_support.h"
#include "limine_boot.h"
#include <Protocol/Tcg2Protocol.h>
#include <IndustryStandard/UefiTcgPlatform.h>
#include <Library/UefiBootManagerLib.h>

#if defined(EFIX64)
#define EFI_STUB_ARCH 0x0000866400004550
#elif defined(EFIAARCH64)
#define EFI_STUB_ARCH 0x0000aa6400004550
#else
#define EFI_STUB_ARCH 0x0000000000004550
#endif

#define APPLE_FAT_BINARY 0x0ef1fab9

#define LINUX_PE_MAGIC 0x818223cd
#define LINUX_PE_OFFSET 0x38
#define BASIC_PE_OFFSET 0x3c

#define EFI_HEADER_SIZE 4096

CHAR16 *BootSelection = NULL;

LOADER_ENTRY *BootSelectionEntry = NULL;
CHAR16 *ValidText = L"Invalid Loader";
extern BOOLEAN IsBoot;
extern BOOLEAN ShimFound;
extern BOOLEAN SecureFlag;

static VOID WarnSecureBootError(CHAR16 *Name, BOOLEAN Verbose)
{
    CHAR16 *LoaderName;
    CHAR16 *MsgStrA;
    CHAR16 *MsgStrB;
    CHAR16 *MsgStrC;
    CHAR16 *MsgStrD;
    CHAR16 *MsgStrE;

    if (Name != NULL) {
        LoaderName = PoolPrint(L"'%s'", Name);
    }
    else {
        LoaderName = StrDuplicate(L"the Loader");
    }

    SwitchToText(FALSE);

    MsgStrA = PoolPrint(L"Secure Boot Validation Failure While Starting %s", LoaderName);
    PrintUglyError(MsgStrA, NEXTLINE);

    if (SecureFlag && Verbose) {
        MsgStrB = PoolPrint(
            L"This computer is configured with Secure Boot active but %s has failed validation",
            LoaderName);
        MsgStrC = PoolPrint(L" * Sign %s with a Machine Owner Key (MOK)", LoaderName);
        MsgStrD = PoolPrint(
            L" * Use a MOK utility to add a MOK with which %s has already been signed", LoaderName);
        MsgStrE = PoolPrint(
            L" * Use a MOK utility to register %s ('Enrol Hash') without signing it", LoaderName);

        PrintUglyText(MsgStrB, NEXTLINE);
        PrintUglyText(L"You can:", NEXTLINE);
        PrintUglyText(L" * Launch another boot loader", NEXTLINE);
        PrintUglyText(L" * Disable Secure Boot in your firmware", NEXTLINE);
        PrintUglyText(MsgStrC, NEXTLINE);
        PrintUglyText(MsgStrD, NEXTLINE);
        PrintUglyText(MsgStrE, NEXTLINE);
        PrintUglyText(L"See http://www.rodsbooks.com/refind/secureboot.html for more information",
                      NEXTLINE);

        MRD_FREE_POOL(MsgStrE);
        MRD_FREE_POOL(MsgStrD);
        MRD_FREE_POOL(MsgStrC);
        MRD_FREE_POOL(MsgStrB);
    }
    PauseForKey();
    SwitchToGraphics();

    MRD_FREE_POOL(MsgStrA);
    MRD_FREE_POOL(LoaderName);
}

static VOID DoEnableAndLockVMX(VOID)
{
#if defined(EFIX64)
    UINT32 msr;
    UINT32 low_bits;
    UINT32 high_bits;

#if MERIDIAN_DEBUG > 0
    DEBUG_LOG(1, LOG_LINE_NORMAL, L"Attempt to Enable and Lock VMX");
#endif

    msr = 0x3a;
    low_bits = high_bits = 0;
    __asm__ volatile("rdmsr" : "=a"(low_bits), "=d"(high_bits) : "c"(msr));

    if ((low_bits & 1) == 0) {
        high_bits = 0;
        low_bits = 0x05;
        msr = 0x3a;
        __asm__ volatile("wrmsr" : : "c"(msr), "a"(low_bits), "d"(high_bits));
    }
#endif
}

static EFI_STATUS RecoveryBootAPFS(IN LOADER_ENTRY *Entry)
{
    EFI_STATUS Status;
    CHAR16 *VarName;
    CHAR16 *InitNVRAM;
    CHAR16 *NameNVRAM;
    CHAR8 *DataNVRAM;
    UINTN OurSize;
    UINTN EntrySize;
    BOOLEAN AlreadyExists;
    EFI_DEVICE_PATH_PROTOCOL *DevicePath;

    InitNVRAM = L"RecoveryModeDisk";
    NameNVRAM = L"internet-recovery-mode";

    DataNVRAM = AllocateZeroPool((StrLen(InitNVRAM) + 1) * sizeof(CHAR8));
    if (DataNVRAM == NULL) {
        return EFI_OUT_OF_RESOURCES;
    }

    UnicodeStrToAsciiStrS(InitNVRAM, DataNVRAM, StrLen(InitNVRAM) + 1);

    Status = EfivarSetRaw(&AppleBootGuid, NameNVRAM, DataNVRAM, AsciiStrSize(DataNVRAM), TRUE);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    NameNVRAM = L"RecoveryBootInitiator";
    OurSize = StrSize(DevicePathToStr(Entry->Volume->DevicePath));
    Status =
        EfivarSetRaw(&AppleBootGuid, NameNVRAM, (VOID **)&Entry->Volume->DevicePath, OurSize, TRUE);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    DevicePath = NULL;
    MRD_FREE_POOL(Entry->EfiLoaderPath);
    Entry->EfiLoaderPath = FileDevicePath(Entry->Volume->DeviceHandle, Entry->LoaderPath);

    Status = ConstructBootEntry(Entry->Volume->DeviceHandle, Entry->LoaderPath,
                                Entry->Volume->VolName, (CHAR8 **)&DevicePath, &EntrySize);
    if (!EFI_ERROR(Status)) {
        AlreadyExists = FALSE;
        Entry->EfiBootNum = FindBootNum(DevicePath, EntrySize, &AlreadyExists);
        VarName = PoolPrint(L"Boot%04x", Entry->EfiBootNum);

        if (!AlreadyExists) {
            Status = EfivarSetRaw(&GlobalGuid, VarName, DevicePath, EntrySize, TRUE);
        }
        if (!EFI_ERROR(Status)) {

            MeridianStall(50);

            Status =
                EfivarSetRaw(&GlobalGuid, L"BootNext", &(Entry->EfiBootNum), sizeof(UINT16), TRUE);
        }

        MRD_FREE_POOL(VarName);
    }

    MRD_FREE_POOL(DevicePath);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    MeridianStall(50);

    MrdAppleSmcNotifyReset(TRUE);
    gRT->ResetSystem(EfiResetCold, EFI_SUCCESS, 0, NULL);

    return EFI_LOAD_ERROR;
}

#if MERIDIAN_DEBUG > 0
static VOID LogAssumedValid(CHAR16 *FileName, BOOLEAN FireWire)
{
    BOOLEAN Known;

#if !defined(EFIX64) && !defined(EFIAARCH64)
    ValidText = L"EFI File *IS ASSUMED* Valid";
    Known = FALSE;
#else
    Known = TRUE;
    ValidText = L"EFI File *IS CONSIDERED* Valid";
#endif

    DEBUG_LOG(1, LOG_THREE_STAR_MID, L"%s%s:- '%s%s'", ValidText,
              (FireWire) ? L" on Apple Firmware (FireWire Workaround)" : L"",
              (FileName != NULL) ? FileName : L"NULL File",
              (!Known) ? L" ... System Arch Unknown" : L"");
    if (IsBoot) {
        INFO_LOG("\n");
        INFO_LOG("%s ... Loading", ValidText);
    }
}
#endif

BOOLEAN IsValidLoader(EFI_FILE_PROTOCOL *RootDir, CHAR16 *FileName)
{
    if (AppleFirmware && (RootDir == NULL || FileName == NULL)) {

#if MERIDIAN_DEBUG > 0
        LogAssumedValid(FileName, TRUE);
#endif

        return TRUE;
    }

#if !defined(EFIX64) && !defined(EFIAARCH64)
#if MERIDIAN_DEBUG > 0
    LogAssumedValid(FileName, FALSE);
#endif

    return TRUE;
#else
#if MERIDIAN_DEBUG > 0
    CHAR16 *AbortReason;
#endif

    EFI_STATUS Status;
    BOOLEAN ValidFile;
    BOOLEAN AppleBinaryPlain;
    BOOLEAN LimineBinary;
    BOOLEAN AppleBinaryFat;
    UINT32 LinuxMagicPE;
    UINT32 BaseMagicPE;
    UINTN LoadedSize;
    CHAR8 *Header;
    UINT64 PESig;
    EFI_FILE_HANDLE FileHandle;

    Header = AllocatePool(EFI_HEADER_SIZE);
    if (Header == NULL) {

        ValidText = L"EFI File *IS ASSUMED* Invalid";

#if MERIDIAN_DEBUG > 0
        AbortReason = L":- 'Unable to Allocate Memory'";
        DEBUG_LOG(1, LOG_THREE_STAR_MID, L"%s ... Aborting%s", ValidText, AbortReason);
        INFO_LOG("\n\n");
        INFO_LOG("INFO: %s ... Aborting%s", ValidText, AbortReason);
        INFO_LOG("\n\n");
#endif

        return FALSE;
    }

    do {
        ValidFile = AppleBinaryFat = AppleBinaryPlain = LimineBinary = FALSE;

#if MERIDIAN_DEBUG > 0
        AbortReason = L"";
#endif

        if (!FileExists(RootDir, FileName)) {
#if MERIDIAN_DEBUG > 0
            AbortReason = L":- 'File *NOT* Found'";
#endif

            break;
        }

        Status = RootDir->Open(RootDir, &FileHandle, FileName, MeridianReadOnly, 0);
        if (EFI_ERROR(Status)) {
#if MERIDIAN_DEBUG > 0
            AbortReason = L":- 'File Handle *IS NOT* Accessible'";
#endif

            break;
        }

        LoadedSize = EFI_HEADER_SIZE;

        Status = FileHandle->Read(FileHandle, &LoadedSize, Header);
        FileHandle->Close(FileHandle);
        if (EFI_ERROR(Status)) {
#if MERIDIAN_DEBUG > 0
            AbortReason = L":- 'File *IS NOT* Readable'";
#endif

            break;
        }

        if (GlobalConfig.GzippedLoaders && Header[0] == (CHAR8)0x1F && Header[1] == (CHAR8)0x8B) {
#if MERIDIAN_DEBUG > 0
            AbortReason = L":- 'GZipped Binary'";
#endif

            break;
        }

        do {

            ValidFile = (Header[0] == 'M' && Header[1] == 'Z' && LoadedSize == EFI_HEADER_SIZE);
            if (ValidFile) {

                PESig = EFI_STUB_ARCH;
                gBS->CopyMem(&BaseMagicPE, &Header[BASIC_PE_OFFSET],
                             sizeof(UINT32));
                ValidFile = (BaseMagicPE < (EFI_HEADER_SIZE - 8) &&
                             CompareMem(&Header[BaseMagicPE], &PESig, 6) == 0);
                if (ValidFile)
                    break;
            }

            gBS->CopyMem(&LinuxMagicPE, &Header[LINUX_PE_OFFSET],
                         sizeof(UINT32));
            ValidFile = (LinuxMagicPE == LINUX_PE_MAGIC);
        } while (0);

        if (ValidFile) {

            break;
        }

        ValidFile = AppleBinaryFat = (*((UINT32 *)Header) == APPLE_FAT_BINARY);
        if (ValidFile) {

            break;
        }

        if (GlobalConfig.ScanLimine) {
            ValidFile = LimineBinary =
                (Header[0] == (CHAR8)0x7f && Header[1] == 'E' && Header[2] == 'L' &&
                 Header[3] == 'F' && Header[4] == 2  &&
                 Header[5] == 1  && Header[18] == 62 );
            if (ValidFile) {
                break;
            }
        }

        ValidFile = AppleBinaryPlain = AppleFirmware;
        if (ValidFile) {

            break;
        }

#if MERIDIAN_DEBUG > 0
        AbortReason = L":- 'Unknown Binary Type'";
#endif
    } while (0);

    ValidText = (!ValidFile)       ? L"EFI File *IS NOT* Valid"
                : (LimineBinary)   ? L"ELF64 Binary *MAY BE* a Limine Kernel"
                : (AppleBinaryFat) ? L"EFI File (Apple 'Fat' Binary) *IS ASSUMED* to be Valid"
                : (AppleBinaryPlain)
                    ? L"EFI File ('Plain' Binary) *IS ASSUMED* to be Valid on Apple Firmware"
                    : L"EFI File is Valid";

#if MERIDIAN_DEBUG > 0
    if (!ValidFile) {
        DEBUG_LOG(1, LOG_THREE_STAR_MID, L"%s:- '%s' ... Aborting%s", ValidText,
                  (FileName != NULL) ? FileName : L"NULL File", AbortReason);
    }
    else {
        DEBUG_LOG(1, LOG_THREE_STAR_MID, L"%s:- '%s'", ValidText,
                  (FileName != NULL) ? FileName : L"NULL File");
    }
#endif

    if (IsBoot) {

        IsBoot = ValidFile;

#if MERIDIAN_DEBUG > 0
        if (ValidFile) {
            INFO_LOG("\n");
            INFO_LOG("%s ... Loading", ValidText);
        }
        else {
            INFO_LOG("\n\n");
            INFO_LOG("INFO: %s ... Aborting%s", ValidText, AbortReason);
            INFO_LOG("\n\n");
        }
#endif
    }

    MRD_FREE_POOL(Header);

    return ValidFile;
#endif
}

static EFI_STATUS MeasurePcr(IN EFI_TCG2_PROTOCOL *Tcg2, IN UINT32 Pcr, IN UINT8 *Data,
                             IN UINTN Len, IN CONST CHAR8 *Desc)
{
    EFI_STATUS Status;
    EFI_TCG2_EVENT *Event;
    UINTN DescLen;
    UINTN EventSize;

    if (Data == NULL || Len == 0) {
        return EFI_INVALID_PARAMETER;
    }
    DescLen = AsciiStrLen(Desc) + 1;
    EventSize = OFFSET_OF(EFI_TCG2_EVENT, Event) + DescLen;
    Event = AllocateZeroPool(EventSize);
    if (Event == NULL) {
        return EFI_OUT_OF_RESOURCES;
    }
    Event->Size = (UINT32)EventSize;
    Event->Header.HeaderSize = sizeof(EFI_TCG2_EVENT_HEADER);
    Event->Header.HeaderVersion = EFI_TCG2_EVENT_HEADER_VERSION;
    Event->Header.PCRIndex = Pcr;
    Event->Header.EventType = EV_EFI_BOOT_SERVICES_APPLICATION;
    CopyMem(Event->Event, Desc, DescLen);

    Status =
        Tcg2->HashLogExtendEvent(Tcg2, 0, (EFI_PHYSICAL_ADDRESS)(UINTN)Data, (UINT64)Len, Event);
    MRD_FREE_POOL(Event);
    return Status;
}

static VOID MeasureLoader(IN EFI_HANDLE ImageHandle, IN CHAR16 *Identifier)
{
    EFI_STATUS Status;
    EFI_TCG2_PROTOCOL *Tcg2;
    EFI_TCG2_BOOT_SERVICE_CAPABILITY Cap;
    EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;

    if (!GlobalConfig.MeasuredBoot) {
        return;
    }

    Status = gBS->LocateProtocol(&gEfiTcg2ProtocolGuid, NULL, (VOID **)&Tcg2);
    if (EFI_ERROR(Status) || Tcg2 == NULL) {
#if MERIDIAN_DEBUG > 0
        INFO_LOG("\n");
        INFO_LOG("INFO: Measured Boot ... No TCG2 Protocol ... Skipped");
#endif
        return;
    }

    Cap.Size = sizeof(EFI_TCG2_BOOT_SERVICE_CAPABILITY);
    Status = Tcg2->GetCapability(Tcg2, &Cap);
    if (EFI_ERROR(Status) || !Cap.TPMPresentFlag) {
#if MERIDIAN_DEBUG > 0
        INFO_LOG("\n");
        INFO_LOG("INFO: Measured Boot ... No TPM Present ... Skipped");
#endif
        return;
    }

    if (Identifier != NULL) {
        Status = MeasurePcr(Tcg2, 8, (UINT8 *)Identifier, StrSize(Identifier),
                            "Meridian Loader Identity");
#if MERIDIAN_DEBUG > 0
        INFO_LOG("\n");
        INFO_LOG("INFO: Measured Boot ... PCR[8] Identity:- '%r'", Status);
#endif
    }

    Status = gBS->HandleProtocol(ImageHandle, &gEfiLoadedImageProtocolGuid, (VOID **)&LoadedImage);
    if (!EFI_ERROR(Status) && LoadedImage != NULL && LoadedImage->ImageBase != NULL &&
        LoadedImage->ImageSize > 0) {
        Status = MeasurePcr(Tcg2, 9, (UINT8 *)LoadedImage->ImageBase, (UINTN)LoadedImage->ImageSize,
                            "Meridian Loader Image");
#if MERIDIAN_DEBUG > 0
        INFO_LOG("\n");
        INFO_LOG("INFO: Measured Boot ... PCR[9] Image:- '%r'", Status);
#endif
    }
}

EFI_STATUS StartEFIImage(IN MERIDIAN_VOLUME *Volume, IN CHAR16 *Filename, IN CHAR16 *LoadOptions,
                         IN CHAR16 *ImageTitle, IN CHAR8 OSType, IN BOOLEAN Verbose,
                         IN BOOLEAN IsDriver, IN CONST CHAR16 *SplashHint OPTIONAL,
                         OUT EFI_HANDLE *NewImageHandle OPTIONAL)
{
#if MERIDIAN_DEBUG > 0
    EFI_STATUS Status;
    CHAR16 *ConstMsgStr;
    BOOLEAN CheckMute = FALSE;
#endif

    EFI_STATUS ReturnStatus;
    EFI_GUID SystemdGuid = SYSTEMD_GUID_VALUE;
    CHAR16 *FullLoadOptions;
    CHAR16 *MsgStrTmp;
    CHAR16 *MsgStrEx;
    CHAR16 *MsgStr;
    CHAR16 *EspGUID;
    BOOLEAN LoaderValid;
    EFI_HANDLE ChildImageHandle;
    EFI_HANDLE TempImageHandle;
    EFI_DEVICE_PATH_PROTOCOL *DevicePath;
    EFI_LOADED_IMAGE_PROTOCOL *ChildLoadedImage;

    if (Volume == NULL) {
        ReturnStatus = EFI_INVALID_PARAMETER;

#if MERIDIAN_DEBUG > 0
        MsgStr = PoolPrint(L"'%r' While Starting EFI Image!!", ReturnStatus);
        DEBUG_LOG(1, LOG_STAR_SEPARATOR, L"ERROR: %s", MsgStr);
        INFO_LOG("* ERROR: %s", MsgStr);
        INFO_LOG("\n\n");
        MRD_FREE_POOL(MsgStr);
#endif

        return ReturnStatus;
    }

    if (LoadOptions == NULL) {
        FullLoadOptions = NULL;
    }
    else {
        FullLoadOptions = StrDuplicate(LoadOptions);

        if (OSType == 'M') {
            MergeStrings(&FullLoadOptions, L" ", 0);
        }
    }

    MsgStr = PoolPrint(L"Start '%s' with Load Options:- '%s'", ImageTitle,
                       (FullLoadOptions != NULL) ? FullLoadOptions : L"NULL");

#if MERIDIAN_DEBUG > 0
    DEBUG_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
#endif

    if (Verbose) {
        PrintUglyError(MsgStr, NEXTLINE);
    }

    MRD_FREE_POOL(MsgStr);

    do {
        ChildImageHandle = NULL;
        TempImageHandle = NULL;
        DevicePath = NULL;

        ReturnStatus = EFI_LOAD_ERROR;

        LoaderValid = IsValidLoader(Volume->RootDir, Filename);
        if (!LoaderValid) {
#if MERIDIAN_DEBUG > 0
            MsgStr = StrDuplicate(L"Found Invalid Binary");
            DEBUG_LOG(1, LOG_STAR_SEPARATOR, L"%s", MsgStr);
            INFO_LOG("\n\n");
            INFO_LOG("INFO: %s", MsgStr);
            MRD_FREE_POOL(MsgStr);
#endif

            MsgStr = PoolPrint(L"When Loading %s ... %s", ImageTitle, ValidText);
            ValidText = L"Invalid Binary";
            CheckError(ReturnStatus, MsgStr);
            MRD_FREE_POOL(MsgStr);

            break;
        }

        DevicePath = FileDevicePath(Volume->DeviceHandle, Filename);
        if (DevicePath == NULL) {
            MsgStr = PoolPrint(L"While Fetching DeviceHandle Path to '%s'", Filename);
            CheckError(ReturnStatus, MsgStr);
            MRD_FREE_POOL(MsgStr);

            break;
        }

#if MERIDIAN_DEBUG < 1

        if (!IsDriver && (!AllowGraphicsMode || Verbose)) {

            MeridianStall(50);
        }
#endif

        ReturnStatus =
            gBS->LoadImage(FALSE, SelfImageHandle, DevicePath, NULL, 0, &ChildImageHandle);
        MRD_FREE_POOL(DevicePath);
        if (EFI_ERROR(ReturnStatus)) {
            if (ReturnStatus != EFI_ACCESS_DENIED && ReturnStatus != EFI_SECURITY_VIOLATION) {
                MsgStrEx = PoolPrint(L"Returned While Loading Image:- '%s'", ImageTitle);
                CheckError(ReturnStatus, MsgStrEx);
                MRD_FREE_POOL(MsgStrEx);
            }
            else {
#if MERIDIAN_DEBUG > 0
                MsgStr = StrDuplicate(L"Secure Boot Validation Failure");
                DEBUG_LOG(1, LOG_STAR_SEPARATOR, L"ERROR: %s", MsgStr);
                INFO_LOG("\n\n");
                INFO_LOG("WARN: %s", MsgStr);
                MRD_FREE_POOL(MsgStr);
#endif

                WarnSecureBootError(ImageTitle, Verbose);
            }

            break;
        }

        if (SecureFlag && ShimFound) {

#if MERIDIAN_DEBUG > 0
            MsgStr = StrDuplicate(L"Shim 0.8 'Load/Start Image' Hack Applied");
            DEBUG_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
            INFO_LOG("\n\n");
            INFO_LOG("INFO: %s", MsgStr);
            MRD_FREE_POOL(MsgStr);
#endif

            gBS->LoadImage(FALSE, SelfImageHandle, GlobalConfig.SelfDevicePath, NULL, 0,
                           &TempImageHandle);
        }

        do {
            ChildLoadedImage = NULL;
            ReturnStatus = gBS->HandleProtocol(ChildImageHandle, &LoadedImageProtocol,
                                               (VOID **)&ChildLoadedImage);
            if (EFI_ERROR(ReturnStatus)) {
                CheckError(ReturnStatus, L"While Getting 'Child' LoadedImageProtocol Handle");

                break;
            }

            ChildLoadedImage->LoadOptions = (VOID *)FullLoadOptions;
            ChildLoadedImage->LoadOptionsSize =
                (FullLoadOptions != NULL) ? (UINT32)StrSize(FullLoadOptions) : 0;

            if (IsBoot) {

                BOOLEAN HideSplash =
                    ((GlobalConfig.DisableBootLogo & DISABLE_BOOTLOGO_ALL) ==
                     DISABLE_BOOTLOGO_ALL) ||
                    (OSType == 'L' && (GlobalConfig.DisableBootLogo & DISABLE_BOOTLOGO_LIN)) ||
                    (OSType == 'W' && (GlobalConfig.DisableBootLogo & DISABLE_BOOTLOGO_WIN));

                if (!Verbose && !HideSplash) {

                    ConnLaunchSplash(OSType,
                                     (SplashHint != NULL) ? SplashHint : Volume->OSSplashHint);
                }

                if (GlobalConfig.WriteSystemdVars && OSType == 'L') {

                    EspGUID = GuidAsString(&(SelfVolume->PartGuid));

#if MERIDIAN_DEBUG > 0
                    MsgStr = PoolPrint(L"LoaderDevicePartUUID:- '%s'", EspGUID);
                    DEBUG_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
                    MRD_FREE_POOL(MsgStr);

                    Status =
#endif

                        EfivarSetRaw(&SystemdGuid, L"LoaderDevicePartUUID", EspGUID,
                                     StrLen(EspGUID) * 2 + 2, FALSE);
#if MERIDIAN_DEBUG > 0
                    if (EFI_ERROR(Status) && Status != EFI_ALREADY_STARTED) {
                        MsgStr = PoolPrint(
                            L"'%r' When Setting 'LoaderDevicePartUUID' UEFI Variable", Status);
                        DEBUG_LOG(1, LOG_STAR_SEPARATOR, L"WARN: %s", MsgStr);
                        INFO_LOG("\n\n");
                        INFO_LOG("WARN: %s", MsgStr);
                        MRD_FREE_POOL(MsgStr);
                    }
#endif

                    MRD_FREE_POOL(EspGUID);
                }
            }

            if (BootSelection != NULL) {
                if (IsBoot) {
                    StoreLoaderName(BootSelection);
                    StoreLoaderIdentity(BootSelectionEntry);
                }
                BootSelection = NULL;
                BootSelectionEntry = NULL;
            }

#if MERIDIAN_DEBUG > 0
            if (IsDriver) {
                ConstMsgStr = L"uEFI Driver";
            }
            else {
                ConstMsgStr = L"Child Image";

                DEBUG_LOG(1, LOG_LINE_NORMAL, L"Load %s via Loader:- '%s'", ConstMsgStr,
                          ImageTitle);
                OUT_TAG();
            }
#endif

            if (!IsDriver) {
                MeasureLoader(ChildImageHandle, ImageTitle);
            }

            UninitMeridianLib();

            ReturnStatus = gBS->StartImage(ChildImageHandle, NULL, NULL);

            if (NewImageHandle != NULL) {
                *NewImageHandle = ChildImageHandle;
            }

#if MERIDIAN_DEBUG > 0
            CHAR16 *TmpMsgStr = (EFI_ERROR(ReturnStatus)) ? L"While Loading" : L"Returned by";
            MsgStrEx = PoolPrint(L"'%r' %s %s", ReturnStatus, TmpMsgStr, ConstMsgStr);
            DEBUG_LOG(1, LOG_THREE_STAR_MID, L"%s", MsgStrEx);
            if (!IsDriver) {
                INFO_LOG("%s", MsgStrEx);
                RET_TAG();
            }
            MRD_FREE_POOL(MsgStrEx);
#endif

            if (EFI_ERROR(ReturnStatus)) {
                MsgStrTmp = L"Returned from Child Image";
                if (IsDriver) {
                    MsgStrEx = PoolPrint(L"%s:- '%s'", MsgStrTmp, ImageTitle);
                }
                else {
                    MsgStrEx = StrDuplicate(MsgStrTmp);

#if MERIDIAN_DEBUG > 0
                    MRD_MUTELOGGER_SET;
#endif
                    if (ReturnStatus == EFI_NOT_FOUND && MrdStrIncludesCI(ImageTitle, L"gptsync")) {
                        SwitchToText(FALSE);
                        PauseSeconds(4);
                        PrintUglyText(L"                                            ", NEXTLINE);
                        PrintUglyText(L"                                            ", NEXTLINE);
                        PrintUglyText(L"  Applicable Disks for GPTSync *NOT* Found  ", NEXTLINE);
                        PrintUglyText(L"           Returning to Main Menu           ", NEXTLINE);
                        PrintUglyText(L"                                            ", NEXTLINE);
                        PrintUglyText(L"                                            ", NEXTLINE);
                        PauseSeconds(4);
                    }
#if MERIDIAN_DEBUG > 0
                    MRD_MUTELOGGER_OFF;
#endif
                }

                CheckError(ReturnStatus, MsgStrEx);
                MRD_FREE_POOL(MsgStrEx);

                IsBoot = FALSE;
            }

            ReinitMeridianLib();
        } while (0);

        if (!IsDriver) {
            gBS->UnloadImage(ChildImageHandle);
            gBS->UnloadImage(TempImageHandle);
        }
    } while (0);

    MRD_FREE_POOL(FullLoadOptions);

    if (!IsDriver) {
        FinishExternalScreen();
    }

    return ReturnStatus;
}

EFI_STATUS RebootIntoFirmware(VOID)
{
    EFI_STATUS Status;
    CHAR16 *TmpStr;
    CHAR16 *MsgStr;
    UINT64 *ItemBuffer;
    UINT64 osind;

    osind = EFI_OS_INDICATIONS_BOOT_TO_FW_UI;

    Status = EfivarGetRaw(&GlobalGuid, L"OsIndications", (VOID **)&ItemBuffer, NULL);
    if (!EFI_ERROR(Status)) {
        osind |= *ItemBuffer;
    }
    MRD_FREE_POOL(ItemBuffer);

    TmpStr = L"Reboot into Firmware";
#if MERIDIAN_DEBUG > 0
    DEBUG_LOG(1, LOG_THREE_STAR_SEP, L"%s", TmpStr);
#endif

    Status = EfivarSetRaw(&GlobalGuid, L"OsIndications", &osind, sizeof(UINT64), TRUE);
    if (EFI_ERROR(Status)) {
#if MERIDIAN_DEBUG > 0
        TmpStr = L"Aborted ... OsIndications *NOT* Found";
        DEBUG_LOG(1, LOG_LINE_NORMAL, L"%s", TmpStr);
        INFO_LOG("%s    ** %s", OffsetNext, TmpStr);
        INFO_LOG("\n\n");
#endif

        return Status;
    }

#if MERIDIAN_DEBUG > 0
    OUT_TAG();
#endif

    UninitMeridianLib();
    MrdAppleSmcNotifyReset(TRUE);
    gRT->ResetSystem(EfiResetCold, EFI_SUCCESS, 0, NULL);
    ReinitMeridianLib();

    Status = EFI_LOAD_ERROR;
    MsgStr = PoolPrint(L"%s ... %r", TmpStr, Status);

#if MERIDIAN_DEBUG > 0
    DEBUG_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
    INFO_LOG("INFO: %s", MsgStr);
    INFO_LOG("\n\n");
#endif

    PrintUglyError(MsgStr, NEXTLINE);

    PauseForKey();

    MRD_FREE_POOL(MsgStr);

    return Status;
}

VOID RebootIntoLoader(LOADER_ENTRY *Entry)
{
    EFI_STATUS Status;
    CHAR16 *TmpStr;
    CHAR16 *MsgStr;

    TmpStr = L"Reboot into nvRAM Boot Option";

#if MERIDIAN_DEBUG > 0
    DEBUG_LOG(1, LOG_THREE_STAR_SEP, L"%s", TmpStr);
#endif

#if MERIDIAN_DEBUG > 0
    MsgStr = PoolPrint(L"%s:- '%s' (Boot%04x)", TmpStr, Entry->Title, Entry->EfiBootNum);
    DEBUG_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
    INFO_LOG("%s    * %s", OffsetNext, MsgStr);
    MRD_FREE_POOL(MsgStr);
#endif

    Status = EfivarSetRaw(&GlobalGuid, L"BootNext", &(Entry->EfiBootNum), sizeof(UINT16), TRUE);
    if (EFI_ERROR(Status)) {
        MsgStr = PoolPrint(L"'%r' While Running '%s'", Status, TmpStr);

#if MERIDIAN_DEBUG > 0
        DEBUG_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
        INFO_LOG("\n\n");

#endif
        PrintUglyError(MsgStr, NEXTLINE);

        PauseForKey();
        MRD_FREE_POOL(MsgStr);

        return;
    }

    StoreLoaderName((Entry->Title != NULL) ? Entry->Title : Entry->me.Title);
    StoreLoaderIdentity(Entry);

#if MERIDIAN_DEBUG > 0
    OUT_TAG();
#endif

    MrdAppleSmcNotifyReset(TRUE);
    gRT->ResetSystem(EfiResetCold, EFI_SUCCESS, 0, NULL);

    EfivarSetRaw(&GlobalGuid, L"BootNext", NULL, 0, TRUE);

    Status = EFI_LOAD_ERROR;
    MsgStr = PoolPrint(L"%s ... %r", TmpStr, Status);

#if MERIDIAN_DEBUG > 0
    DEBUG_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
    INFO_LOG("INFO: %s", MsgStr);
    RET_TAG();

#endif
    PrintUglyError(MsgStr, NEXTLINE);

    PauseForKey();

    MRD_FREE_POOL(MsgStr);
}

VOID StartLoader(IN LOADER_ENTRY *Entry, IN CHAR16 *SelectionName, IN BOOLEAN TrustSynced)
{
    CHAR16 *LoaderPath;
    BOOLEAN IsVerbose;

    IsBoot = TRUE;

    BootSelection = (Entry->Title != NULL) ? Entry->Title : SelectionName;

    BootSelectionEntry = Entry;

#if MERIDIAN_DEBUG > 0
    if (TrustSynced) {
        DEBUG_LOG(1, LOG_BLANK_LINE_SEP, L"X");
    }
#endif

    if (GlobalConfig.EnableAndLockVMX) {
        DoEnableAndLockVMX();
    }

    BeginExternalScreen(Entry->UseGraphicsMode, SelectionName);

    LoaderPath = Basename(Entry->LoaderPath);
    IsVerbose = !Entry->UseGraphicsMode;

    if (Entry->BootProtocol == MERIDIAN_PROTO_LIMINE) {

        LaunchLimineKernel(Entry);

        MRD_FREE_POOL(LoaderPath);

        return;
    }

    StartEFIImage(Entry->Volume, Entry->LoaderPath, Entry->LoadOptions, LoaderPath, Entry->OSType,
                  IsVerbose, FALSE, Entry->me.Title, NULL);

    MRD_FREE_POOL(LoaderPath);
}

VOID StartTool(IN LOADER_ENTRY *Entry)
{
    EFI_STATUS Status;
    BOOLEAN IsVerbose;
    BOOLEAN IsRecovAPFS;
    CHAR16 *LoaderPath;
    CHAR16 *MsgStr;

    IsBoot = FALSE;
    LoaderPath = Basename(Entry->LoaderPath);
    MsgStr = PoolPrint(L"Start Child Image (Tool) Loader:- '%s'", Entry->LoaderPath);

#if MERIDIAN_DEBUG > 0
    DEBUG_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
    INFO_LOG("%s    * %s", OffsetNext, MsgStr);
#endif

    IsRecovAPFS = MrdStrIncludesCI(Entry->me.Title, RECOVERY_NAME_APFS);
    if (!IsRecovAPFS) {
        BeginExternalScreen(Entry->UseGraphicsMode, MsgStr);

        IsVerbose = !Entry->UseGraphicsMode;

        if (MrdStrIncludesCI(Entry->me.Title, L"Cydia") &&
            (GlobalConfig.DisableBootLogo & DISABLE_BOOTLOGO_ALL) != DISABLE_BOOTLOGO_ALL) {
            ConnLaunchSplash(Entry->OSType, Entry->me.Title);
        }

        StartEFIImage(Entry->Volume, Entry->LoaderPath, Entry->LoadOptions, LoaderPath,
                      Entry->OSType, IsVerbose, FALSE, NULL, NULL);
    }
    else {
        MRD_FREE_POOL(MsgStr);

        if (SingleAPFS) {

            Status = RecoveryBootAPFS(Entry);
        }
        else {
            Status = EFI_NOT_STARTED;

            MsgStr = StrDuplicate(
                L"APFS Recovery Boot *IS NOT* Available When Multi-Instance Contaners are Present");
        }
        if (EFI_ERROR(Status)) {
            if (SingleAPFS) {

                MsgStr = PoolPrint(L"'%r' While Running '%s'", Status, Entry->me.Title);
            }

#if MERIDIAN_DEBUG > 0
            DEBUG_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
            INFO_LOG("\n");
            INFO_LOG("** WARN: %s", MsgStr);
            INFO_LOG("\n\n");

#endif
            PrintUglyError(MsgStr, NEXTLINE);

            PauseForKey();
        }
    }

    MRD_FREE_POOL(MsgStr);
    MRD_FREE_POOL(LoaderPath);
}
