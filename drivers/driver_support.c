// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2020-2025 Dayo Akanji
// SPDX-FileCopyrightText: 2012-2017 Roderick W. Smith
// SPDX-FileCopyrightText: 2006-2010 Christoph Pfisterer
// SPDX-FileCopyrightText: Intel Corporation

#include "driver_support.h"
#include "mystrings.h"
#include "lib.h"
#include "screenmgt.h"
#include "launch_efi.h"
#include <Library/DevicePathLib.h>

#define DRIVER_DIRS MERIDIAN_ESP_FS_DIR L"," MERIDIAN_ESP_DRIVERS_DIR

#define MRD_EFI_SIMPLE_FILE_SYSTEM_PROTOCOL EFI_SIMPLE_FILE_SYSTEM_PROTOCOL
#define MRD_EFI_FILE_PROTOCOL EFI_FILE_PROTOCOL
#define MRD_EFI_BLOCK_IO_PROTOCOL EFI_BLOCK_IO_PROTOCOL

VOID ConnectAllDriversToAllControllers(VOID)
{
    static BOOLEAN ResetGOP = TRUE;

    BdsLibConnectAllDriversToAllControllers(ResetGOP);

    ResetGOP = FALSE;

    {
        EFI_HANDLE *EveryHandle = NULL;
        UINTN EveryCount = 0;
        EFI_STATUS EveryStatus =
            gBS->LocateHandleBuffer(AllHandles, NULL, NULL, &EveryCount, &EveryHandle);
        if (!EFI_ERROR(EveryStatus) && EveryHandle != NULL) {
            UINTN EveryIndex;
            UINTN EveryConnected = 0;
            EFI_STATUS EveryConnStatus;
            for (EveryIndex = 0; EveryIndex < EveryCount; EveryIndex++) {
                EFI_DEVICE_PATH_PROTOCOL *EveryDp = DevicePathFromHandle(EveryHandle[EveryIndex]);

                if (EveryDp == NULL) {
                    continue;
                }

#if MERIDIAN_DEBUG > 0

                INFO_LOG("\n  EveryHandle Connect [%d/%d]", (UINTN)(EveryIndex + 1), EveryCount);
#endif

                {
                    VOID *EveryPciIo = NULL;
                    if (EFI_ERROR(gBS->HandleProtocol(EveryHandle[EveryIndex],
                                                      &gEfiPciIoProtocolGuid, &EveryPciIo))) {
#if MERIDIAN_DEBUG > 0
                        INFO_LOG(" ... Skipped (No PciIo)");
#endif
                        continue;
                    }
                }

                if (!IsDevicePathValid(EveryDp, 0)) {
#if MERIDIAN_DEBUG > 0
                    INFO_LOG(" ... Skipped (Invalid Device Path)");
#endif
                    continue;
                }

#if MERIDIAN_DEBUG > 0

                {
                    CHAR16 *EveryDpStr = ConvertDevicePathToText(EveryDp, FALSE, FALSE);
                    if (EveryDpStr != NULL) {
                        INFO_LOG(" :- '%s'", EveryDpStr);
                    }
                    MRD_FREE_POOL(EveryDpStr);
                }
#endif

                if (EveryDp != NULL) {
                    EFI_DEVICE_PATH_PROTOCOL *DpNode;
                    BOOLEAN SkipHandle = FALSE;
                    for (DpNode = EveryDp; !IsDevicePathEnd(DpNode);
                         DpNode = NextDevicePathNode(DpNode)) {
                        if (DevicePathType(DpNode) == MESSAGING_DEVICE_PATH &&
                            DevicePathSubType(DpNode) == MSG_UART_DP) {
                            SkipHandle = TRUE;
                            break;
                        }
                        if (DevicePathType(DpNode) == ACPI_DEVICE_PATH &&
                            DevicePathSubType(DpNode) == ACPI_DP &&
                            ((ACPI_HID_DEVICE_PATH *)DpNode)->HID == EISA_PNP_ID(0x0501)) {
                            SkipHandle = TRUE;
                            break;
                        }
                    }
                    if (SkipHandle) {
#if MERIDIAN_DEBUG > 0
                        INFO_LOG(" ... Skipped (Serial/UART)");
#endif
                        continue;
                    }
                }

                EveryConnStatus = gBS->ConnectController(EveryHandle[EveryIndex], NULL, NULL, TRUE);
                if (!EFI_ERROR(EveryConnStatus)) {
                    EveryConnected++;
                }
            }
#if MERIDIAN_DEBUG > 0
            INFO_LOG("\nINFO: ConnectAll ... EveryHandle pass: %d handles, %d newly connected",
                     EveryCount, EveryConnected);
#endif
            (VOID) EveryConnected;
            MRD_FREE_POOL(EveryHandle);
        }
    }

    {
        EFI_HANDLE *BlockHandles = NULL;
        UINTN BlockHandleCount = 0;
        EFI_STATUS BlockStatus = gBS->LocateHandleBuffer(ByProtocol, &gEfiBlockIoProtocolGuid, NULL,
                                                         &BlockHandleCount, &BlockHandles);
        if (!EFI_ERROR(BlockStatus) && BlockHandles != NULL) {
            UINTN BlockIndex;
            for (BlockIndex = 0; BlockIndex < BlockHandleCount; BlockIndex++) {

                gBS->ConnectController(BlockHandles[BlockIndex], NULL, NULL, TRUE);
            }
            MRD_FREE_POOL(BlockHandles);
        }
    }
}

static UINTN ScanDriverDir(IN CHAR16 *Path, OUT EFI_HANDLE **DriversList)
{
    EFI_STATUS Status;
    EFI_STATUS XStatus;
    EFI_GUID **ProtocolGuidArray;
    BOOLEAN DriverBindingFlag;
    BOOLEAN FirstLoop;
    BOOLEAN CheckIter;
    CHAR16 *FileName;
    CHAR16 *ErrMsg;
    UINTN NumFound;
    UINTN DriversArrSizeNew;
    UINTN DriversArrSize;
    UINTN DriversArrNum;
    UINTN ProtocolIndex;
    UINTN ArrayCount;
    EFI_HANDLE *DriversArr;
    EFI_HANDLE DriverHandle;
    EFI_FILE_INFO *DirEntry;
    MERIDIAN_DIR_ITER DirIter;

    CleanUpPathNameSlashes(Path);

#if MERIDIAN_DEBUG > 0
    BRK_MOD("\n");
    INFO_LOG("Scan '%s' Folder:", Path);
#endif

    DirIterOpen(SelfRootDir, Path, &DirIter);

    FirstLoop = TRUE;
    DriversArr = NULL;
    DriversArrSize = DriversArrSizeNew = 16;
    ArrayCount = ProtocolIndex = DriversArrNum = NumFound = 0;
    while (1) {
        CheckIter = DirIterNext(&DirIter, 2, LOADER_MATCH_PATTERNS, &DirEntry);
        if (!CheckIter)
            break;

        if (DirEntry->FileName[0] == '.') {

            MRD_FREE_POOL(DirEntry);
            continue;
        }

        DriverBindingFlag = FALSE;

        NumFound++;
        FileName = PoolPrint(L"%s\\%s", Path, DirEntry->FileName);

        Status = StartEFIImage(SelfVolume, FileName, L"", DirEntry->FileName, 0, FALSE, TRUE, NULL,
                               &DriverHandle);

        MRD_FREE_POOL(DirEntry);

        if (DriverHandle != NULL) {

            XStatus = gBS->ProtocolsPerHandle(DriverHandle, &ProtocolGuidArray, &ArrayCount);
            if (!EFI_ERROR(XStatus)) {
                for (ProtocolIndex = 0; ProtocolIndex < ArrayCount; ProtocolIndex++) {
                    if (CompareGuid(ProtocolGuidArray[ProtocolIndex],
                                    &gEfiDriverBindingProtocolGuid)) {
                        DriverBindingFlag = TRUE;
                        break;
                    }
                }

                if (DriverBindingFlag) {
                    if (FirstLoop) {
                        FirstLoop = FALSE;

                        DriversArr = AllocatePool(sizeof(EFI_HANDLE) * DriversArrSize);
                    }
                    else {

                        DriversArrSizeNew += 16;
                        DriversArr = ReallocatePool(DriversArrSize, DriversArrSizeNew, DriversArr);
                        DriversArrSize = DriversArrSizeNew;
                    }

                    DriversArr[DriversArrNum] = DriverHandle;
                    DriversArrNum++;
                    DriversArr[DriversArrNum] = NULL;
                }

                MRD_FREE_POOL(ProtocolGuidArray);
            }
        }

#if MERIDIAN_DEBUG > 0
        INFO_LOG("%s  - %r ... uEFI Driver:- '%s'",
#if MERIDIAN_DEBUG < 2
                 OffsetNext,
#else
                 L"",
#endif
                 Status, FileName);
        BRK_MAX("\n");
#endif

        MRD_FREE_POOL(FileName);
    }

    Status = DirIterClose(&DirIter);
    if (EFI_ERROR(Status) && Status != EFI_NOT_FOUND) {
        ErrMsg = PoolPrint(L"While Scanning the '%s' Directory for uEFI Drivers", Path);
        CheckError(Status, ErrMsg);
        MRD_FREE_POOL(ErrMsg);
    }

    *DriversList = DriversArr;

    return (NumFound);
}

static UINTN LoadDriversHelper(CHAR16 *Directory, CHAR16 *SelfDirectory, BOOLEAN UserDefined,
                               EFI_HANDLE **DriversListUser)
{
#if MERIDIAN_DEBUG > 0
    CHAR16 *MsgNotFound = L"Not Found or Empty";
#endif

    UINTN CurFound;
    CHAR16 *BaseDirectory;

    CleanUpPathNameSlashes(Directory);
    if (UserDefined && StrLen(Directory) == 0) {
        return 0;
    }

    if (SelfDirectory == NULL || (UserDefined && MrdStrStartsWithCI(SelfDirectory, Directory)) ||
        (UserDefined &&
         (MrdStrStartsWithCI(L"EFI\\", Directory) || MrdStrStartsWithCI(L"\\EFI\\", Directory)))) {
        BaseDirectory = StrDuplicate(Directory);
    }
    else {
        BaseDirectory = StrDuplicate(SelfDirectory);
        MergeStrings(&BaseDirectory, Directory, L'\\');
    }

    CurFound = ScanDriverDir(BaseDirectory, DriversListUser);
#if MERIDIAN_DEBUG > 0
    if (CurFound == 0) {
        DEBUG_LOG(1, LOG_LINE_NORMAL, L"'%s' ... %s Driver Folder:- '%s'", MsgNotFound,
                  UserDefined ? L"User Defined" : L"Program Default", BaseDirectory);
        INFO_LOG("%s  - %s",
#if MERIDIAN_DEBUG < 2
                 OffsetNext,
#else
                 L"",
#endif
                 MsgNotFound);
    }
#endif

    MRD_FREE_POOL(BaseDirectory);

    return CurFound;
}

BOOLEAN LoadDrivers(VOID)
{
#if MERIDIAN_DEBUG > 0
    CHAR16 *MsgStr;
#endif

    UINTN i;
    UINTN NumFound;
    UINTN CurFound;
    CHAR16 *Directory;
    CHAR16 *SelfDirectory;
    BOOLEAN NumDriverFlag;
    EFI_HANDLE *DriversListProg;
    EFI_HANDLE *DriversListUser;

#if MERIDIAN_DEBUG > 0

    DEBUG_LOG(1, LOG_THREE_STAR_SEP, L"Load Provided Drivers in Program Default Folder");
    INFO_LOG("\n\n");
    INFO_LOG("L O A D   P R O V I D E D   D R I V E R S   :::::   P R O G R A M   D E F A U L T   "
             "F O L D E R");
    BRK_MAX("\n");
#endif

    if (SelfDirPath == NULL) {
        SelfDirectory = NULL;
    }
    else {
        SelfDirectory = StrDuplicate(SelfDirPath);
        CleanUpPathNameSlashes(SelfDirectory);
    }

    DriversListProg = NULL;
    NumFound = CurFound = i = 0;
    while (CurFound == 0) {
        Directory = FindCommaDelimited(DRIVER_DIRS, i++);
        if (Directory == NULL)
            break;

        CurFound = LoadDriversHelper(Directory, SelfDirectory, FALSE, &DriversListProg);
        if (CurFound > 0) {

            NumFound += CurFound;
        }

        MRD_FREE_POOL(Directory);
    }

    DriversListUser = NULL;
    if (GlobalConfig.DriverDirs) {
#if MERIDIAN_DEBUG > 0
        DEBUG_LOG(1, LOG_THREE_STAR_SEP, L"Load Provided Drivers in User Defined Folders");
        INFO_LOG("\n\n");
        INFO_LOG("L O A D   P R O V I D E D   D R I V E R S   :::::   U S E R   D E F I N E D   F "
                 "O L D E R S");
        BRK_MAX("\n");
#endif

        i = 0;
        while (1) {
            Directory = FindCommaDelimited(GlobalConfig.DriverDirs, i++);
            if (Directory == NULL)
                break;

            NumFound += LoadDriversHelper(Directory, SelfDirectory, TRUE, &DriversListUser);

            MRD_FREE_POOL(Directory);
        }
    }
    MRD_FREE_POOL(SelfDirectory);

    NumDriverFlag = (NumFound > 0);

#if MERIDIAN_DEBUG > 0
    INFO_LOG("\n\n");
    MsgStr = PoolPrint(L"Processed %d UEFI Driver%s", NumFound, (NumFound == 1) ? L"" : L"s");
    DEBUG_LOG(1, LOG_LINE_THIN_SEP, L"%s", MsgStr);
    MRD_FREE_POOL(MsgStr);
#endif

    ConnectAllDriversToAllControllers();

    return NumDriverFlag;
}
