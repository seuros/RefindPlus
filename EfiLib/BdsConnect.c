// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2020-2025 Dayo Akanji
// SPDX-FileCopyrightText: Intel Corporation

#include "Platform.h"
#include "lib.h"
#include "screenmgt.h"
#include "mystrings.h"
#include "launch_efi.h"
#include "display.h"

#if MERIDIAN_DEBUG > 0
#include <Library/HandleParsingLib.h>
#endif

#define IS_PCI_GFX(_p) IS_CLASS2(_p, PCI_CLASS_DISPLAY, PCI_CLASS_DISPLAY_OTHER)

BOOLEAN FoundGOP        = FALSE;
BOOLEAN ReLoaded        = FALSE;
BOOLEAN ForceRescanDXE  = FALSE;
BOOLEAN AcquireErrorGOP = FALSE;
BOOLEAN ObtainHandleGOP = FALSE;
BOOLEAN DetectedDevices = FALSE;
BOOLEAN DevicePresence  = FALSE;

UINTN   AllHandleCount;

extern EFI_STATUS AmendSysTable (VOID);
extern EFI_STATUS AcquireGOP (VOID);
extern EFI_STATUS ReissueGOP (VOID);

extern BOOLEAN SetSysTab;

static
EFI_STATUS EFIAPI MeridianConnectController (
    IN  EFI_HANDLE                ControllerHandle,
    IN  EFI_HANDLE               *DriverImageHandle   OPTIONAL,
    IN  EFI_DEVICE_PATH_PROTOCOL *RemainingDevicePath OPTIONAL,
    IN  BOOLEAN                   Recursive
) {
    EFI_STATUS   Status;
    VOID        *DevicePath;

    if (ControllerHandle == NULL) {

        return EFI_INVALID_PARAMETER;
    }

    Status = gBS->HandleProtocol(ControllerHandle, &gEfiDevicePathProtocolGuid, &DevicePath);
    if (EFI_ERROR(Status)) {

        return EFI_NOT_STARTED;
    }

    Status = gBS->ConnectController(ControllerHandle, DriverImageHandle, RemainingDevicePath, Recursive);

    return Status;
}

EFI_STATUS ScanDeviceHandles (
    EFI_HANDLE   ControllerHandle,
    UINTN       *HandleCount,
    EFI_HANDLE **HandleBuffer,
    UINT32     **HandleType
) {
    EFI_STATUS                            Status;
    EFI_GUID                            **ProtocolGuidArray;
    UINTN                                 k;
    UINTN                                 ArrayCount;
    UINTN                                 ProtocolIndex;
    UINTN                                 OpenInfoCount;
    UINTN OpenInfoIndex;
    EFI_OPEN_PROTOCOL_INFORMATION_ENTRY  *OpenInfo;

    *HandleCount  = 0;
    *HandleBuffer = NULL;
    *HandleType   = NULL;

    Status = gBS->LocateHandleBuffer(ByProtocol, &gEfiDevicePathProtocolGuid, NULL, HandleCount, HandleBuffer);
    if (EFI_ERROR(Status)) {
        *HandleCount  = 0;

        return Status;
    }

    *HandleType = AllocatePool (
        *HandleCount * sizeof (UINT32)
    );
    if (*HandleType == NULL) {
        MRD_FREE_POOL(*HandleBuffer);
        *HandleCount  = 0;

        return Status;
    }

    for (k = 0; k < *HandleCount; k++) {
        (*HandleType)[k] = EFI_HANDLE_TYPE_UNKNOWN;

        Status = gBS->ProtocolsPerHandle((*HandleBuffer)[k], &ProtocolGuidArray, &ArrayCount);
        if (EFI_ERROR(Status)) continue;

        for (ProtocolIndex = 0; ProtocolIndex < ArrayCount; ProtocolIndex++) {

            if (CompareGuid(ProtocolGuidArray[ProtocolIndex], &gEfiLoadedImageProtocolGuid)) {
                (*HandleType)[k] |= EFI_HANDLE_TYPE_IMAGE_HANDLE;
            }
            else if (CompareGuid(ProtocolGuidArray[ProtocolIndex],
                                 &gEfiDriverBindingProtocolGuid)) {
                (*HandleType)[k] |= EFI_HANDLE_TYPE_DRIVER_BINDING_HANDLE;
            }
            else if (CompareGuid(ProtocolGuidArray[ProtocolIndex], &gEfiDevicePathProtocolGuid)) {
                (*HandleType)[k] |= EFI_HANDLE_TYPE_DEVICE_HANDLE;
            }

            Status = gBS->OpenProtocolInformation((*HandleBuffer)[k], ProtocolGuidArray[ProtocolIndex], &OpenInfo, &OpenInfoCount);
            if (EFI_ERROR(Status)) continue;

            for (OpenInfoIndex = 0; OpenInfoIndex < OpenInfoCount; OpenInfoIndex++) {
                if (OpenInfo[OpenInfoIndex].ControllerHandle == ControllerHandle &&
                    (OpenInfo[OpenInfoIndex].Attributes & EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER) ==
                        EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER) {
                    (*HandleType)[k] |= EFI_HANDLE_TYPE_PARENT_HANDLE;
                }
            }

            MRD_FREE_POOL(OpenInfo);
        }

        MRD_FREE_POOL(ProtocolGuidArray);
    }

    return EFI_SUCCESS;
}

EFI_STATUS BdsLibConnectMostlyAllEfi (VOID) {
    EFI_STATUS            XStatus;
    EFI_STATUS            Status;
    UINTN                 i, k, m;
    UINTN                 BusPCI;
    UINTN                 GOPCount;
    UINTN                 DevicePCI;
    UINTN                 SegmentPCI;
    UINTN                 FunctionPCI;
    UINTN                 HandleCount;
    UINT32               *HandleType;
    BOOLEAN               Parent;
    BOOLEAN               Device;
    BOOLEAN               DevTag;
    BOOLEAN               VGADevice;
    BOOLEAN               GFXDevice;
    BOOLEAN               MakeConnection;
    PCI_TYPE00            Pci;
    EFI_HANDLE           *AllHandleBuffer;
    EFI_HANDLE           *HandleBuffer;
    EFI_HANDLE           *GOPArray;
    EFI_PCI_IO_PROTOCOL  *PciIo;

    #if MERIDIAN_DEBUG > 0
    CHAR16               *GopDevicePathStr;
    CHAR16               *StrDevicePath;
    CHAR16               *DeviceDataTmp;
    CHAR16               *DeviceData;
    CHAR16               *FillStr;
    CHAR16               *MsgStr;
    CHAR16               *TmpStr;
    UINTN                 HexIndex;
    UINTN                 AllHandleCountTrigger;
    #endif

    DetectedDevices = FALSE;

    #if MERIDIAN_DEBUG > 0
    MsgStr = (
        ReLoaded
    ) ? StrDuplicate (
        L"R E C O N N E C T   D E V I C E   H A N D L E S"
    ) : StrDuplicate (
        L"C O N N E C T   D E V I C E   H A N D L E S"
    );
    DEBUG_LOG(1, LOG_BLANK_LINE_TWO, L"X");
    DEBUG_LOG(1, LOG_LINE_SEPARATOR, L"%s", MsgStr);
    INFO_LOG("%s", MsgStr);
    INFO_LOG("\n");
    MRD_FREE_POOL(MsgStr);
    #endif

    AllHandleBuffer = NULL;
    Status = gBS->LocateHandleBuffer(ByProtocol, &gEfiDevicePathProtocolGuid, NULL, &AllHandleCount, &AllHandleBuffer);
    if (EFI_ERROR(Status)) {
        #if MERIDIAN_DEBUG > 0
        MsgStr = StrDuplicate (
            L"Did Not Find Any Contollers with Device Paths"
        );
        DEBUG_LOG(1, LOG_STAR_SEPARATOR, L"%s", MsgStr);
        INFO_LOG("INFO: %s", MsgStr);
        INFO_LOG("\n\n");
        MRD_FREE_POOL(MsgStr);
        #endif

        return Status;
    }

    #if MERIDIAN_DEBUG > 0
    GopDevicePathStr      = NULL;
    AllHandleCountTrigger = AllHandleCount - 1;
    #endif

    HandleType = NULL;
    GOPArray = HandleBuffer = NULL;
    for (i = 0; i < AllHandleCount; i++) {
        MakeConnection = TRUE;

        #if MERIDIAN_DEBUG > 0
        HexIndex   = ConvertHandleToHandleIndex (AllHandleBuffer[i]);
        DeviceData = NULL;
        #endif

        XStatus = ScanDeviceHandles (
            AllHandleBuffer[i],
            &HandleCount,
            &HandleBuffer,
            &HandleType
        );
        if (EFI_ERROR(XStatus)) {
            #if MERIDIAN_DEBUG > 0
            MsgStr = PoolPrint (
                L"Handle 0x%03X      - ERROR: %r",
                HexIndex, XStatus
            );
            DEBUG_LOG(1, LOG_THREE_STAR_MID, L"%s", MsgStr);
            INFO_LOG("%s", MsgStr);
            MRD_FREE_POOL(MsgStr);
            #endif
        }
        else if (HandleType == NULL) {
            #if MERIDIAN_DEBUG > 0
            MsgStr = PoolPrint (
                L"Handle 0x%03X      - ERROR: Invalid Handle",
                HexIndex
            );
            DEBUG_LOG(1, LOG_THREE_STAR_MID, L"%s", MsgStr);
            INFO_LOG("%s", MsgStr);
            MRD_FREE_POOL(MsgStr);
            #endif
        }
        else {

            Device = TRUE;

            for (k = 0; k < HandleCount; k++) {
                if ((HandleType[k] & EFI_HANDLE_TYPE_IMAGE_HANDLE)      ||
                    (HandleType[k] & EFI_HANDLE_TYPE_DRIVER_BINDING_HANDLE)
                ) {
                    Device = FALSE;

                    break;
                }
            }

            if (!Device) {
                #if MERIDIAN_DEBUG > 0
                MsgStr = PoolPrint (
                    L"Handle 0x%03X     Discounted [Other Item]",
                    HexIndex
                );
                DEBUG_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
                INFO_LOG("%s", MsgStr);
                MRD_FREE_POOL(MsgStr);
                #endif
            }
            else {

                DevicePresence = TRUE;

                Parent = FALSE;

                for (k = 0; k < HandleCount; k++) {
                    if (HandleType[k] & EFI_HANDLE_TYPE_PARENT_HANDLE) {
                        MakeConnection = FALSE;
                        Parent         =  TRUE;

                        break;
                    }
                }

                DevTag = FALSE;

                for (k = 0; k < HandleCount; k++) {
                    if (HandleType[k] & EFI_HANDLE_TYPE_DEVICE_HANDLE) {
                        DevTag = TRUE;

                        break;
                    }
                }

                XStatus = EFI_SUCCESS;

                if (DevTag) {
                    XStatus = gBS->HandleProtocol(AllHandleBuffer[i], &gEfiPciIoProtocolGuid, (void **) &PciIo);
                    if (EFI_ERROR(XStatus)) {
                        #if MERIDIAN_DEBUG > 0
                        DeviceData = StrDuplicate (L"Not PCIe Device");
                        #endif
                    }
                    else {

                        PciIo->GetLocation(PciIo, &SegmentPCI, &BusPCI, &DevicePCI, &FunctionPCI);

                        XStatus = PciIo->Pci.Read(PciIo, EfiPciIoWidthUint32, 0, sizeof (Pci) / sizeof (UINT32), &Pci);
                        if (EFI_ERROR(XStatus)) {
                            MakeConnection = FALSE;

                            #if MERIDIAN_DEBUG > 0
                            DeviceData = StrDuplicate (L"Unreadable Item");
                            #endif
                        }
                        else {
                            VGADevice = IS_PCI_VGA(&Pci);
                            GFXDevice = IS_PCI_GFX(&Pci);

                            if (VGADevice) {

                                MakeConnection = FALSE;

                                #if MERIDIAN_DEBUG > 0
                                DeviceData = StrDuplicate (L"Monitor Display");
                                #endif
                            }
                            else if (GFXDevice) {

                                #if MERIDIAN_DEBUG > 0
                                DeviceData = StrDuplicate (L"GraphicsFX Card");
                                #endif
                            }
                            else {

                                #if MERIDIAN_DEBUG > 0
                                DeviceData = PoolPrint (
                                    L"PCI(%02llX|%02llX:%02llX.%llX)",
                                    SegmentPCI, BusPCI,
                                    DevicePCI, FunctionPCI
                                );
                                #endif
                            }
                        }
                    }
                }

                if (!FoundGOP) {
                    XStatus = gBS->LocateHandleBuffer(ByProtocol, &gEfiGraphicsOutputProtocolGuid, NULL, &GOPCount, &GOPArray);
                    if (!EFI_ERROR(XStatus)) {
                        for (m = 0; m < GOPCount; m++) {
                            if (GOPArray[m] != gST->ConsoleOutHandle) {
                                #if MERIDIAN_DEBUG > 0
                                GopDevicePathStr = ConvertDevicePathToText (
                                    DevicePathFromHandle (GOPArray[m]),
                                    FALSE, FALSE
                                );
                                #endif

                                FoundGOP = TRUE;

                                break;
                            }
                        }
                    }

                    MRD_FREE_POOL(GOPArray);
                }

                #if MERIDIAN_DEBUG > 0
                if (FoundGOP) {
                    DeviceDataTmp = StrDevicePath = NULL;
                    if (GopDevicePathStr != NULL) {
                        StrDevicePath = ConvertDevicePathToText (
                            DevicePathFromHandle (AllHandleBuffer[i]),
                            FALSE, FALSE
                        );

                        if (MrdStrFind (GopDevicePathStr, StrDevicePath)) {
                            DeviceDataTmp = DeviceData;
                            DeviceData    = PoolPrint (
                                L"%s : Leverages GOP",
                                DeviceDataTmp
                            );
                        }
                    }

                    MRD_FREE_POOL(DeviceDataTmp);
                    MRD_FREE_POOL(StrDevicePath);
                }
                #endif

                if (MakeConnection) {
                    XStatus = MeridianConnectController (
                        AllHandleBuffer[i], NULL, NULL, TRUE
                    );
                }

                #if MERIDIAN_DEBUG > 0
                if (DeviceData == NULL) {
                    FillStr = L"";
                    DeviceData = StrDuplicate (L"");
                }
                else {
                    if (MrdStrEqualsCI (DeviceData, L"Monitor Display") ||
                        MrdStrEqualsCI (DeviceData, L"GraphicsFX Card")
                    ) {
                        FillStr = L"  x  ";
                    }
                    else if (Parent) {
                        FillStr = L"     ";
                    }
                    else if (
                        XStatus != EFI_SUCCESS &&
                        XStatus != EFI_NOT_FOUND &&
                        XStatus != EFI_NOT_STARTED
                    ) {
                        FillStr = L"  .  ";
                    }
                    else if (EFI_ERROR(XStatus)) {
                        FillStr = L"     ";
                    }
                    else {
                        FillStr = L"  -  ";
                    }
                }
                #endif

                if (Parent) {

#if MERIDIAN_DEBUG > 1
                    MsgStr = PoolPrint (
                        L"Handle 0x%03X     Skipped [Parent Device]%s%s",
                        HexIndex, FillStr, DeviceData
                    );
                    DEBUG_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
                    INFO_LOG("%s", MsgStr);
                    MRD_FREE_POOL(MsgStr);
                    #endif
                }
                else if (XStatus == EFI_NOT_FOUND) {

#if MERIDIAN_DEBUG > 1
                    MsgStr = PoolPrint (
                        L"Handle 0x%03X     Bypassed [Not Linkable]%s%s",
                        HexIndex, FillStr, DeviceData
                    );
                    DEBUG_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
                    INFO_LOG("%s", MsgStr);
                    MRD_FREE_POOL(MsgStr);
                    #endif
                }
                else if (!EFI_ERROR(XStatus)) {
                    DetectedDevices = TRUE;

                    #if MERIDIAN_DEBUG > 0
                    MsgStr = PoolPrint (
                        L"Handle 0x%03X  *  %r                %s%s",
                        HexIndex, XStatus, FillStr, DeviceData
                    );
                    DEBUG_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
                    INFO_LOG("%s", MsgStr);
                    MRD_FREE_POOL(MsgStr);
                    #endif
                }
                else {
                    #if MERIDIAN_DEBUG > 0

                    if (XStatus == EFI_NOT_STARTED) {
                        MsgStr = PoolPrint (
                            L"Handle 0x%03X     Declined [Empty Device]%s%s",
                            HexIndex, FillStr, DeviceData
                        );
                        DEBUG_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
                        INFO_LOG("%s", MsgStr);
                        MRD_FREE_POOL(MsgStr);
                    }
                    else if (XStatus == EFI_INVALID_PARAMETER) {
                        MsgStr = PoolPrint (
                            L"Handle 0x%03X  .  ERROR: Invalid Param   %s%s",
                            HexIndex, FillStr, DeviceData
                        );
                        DEBUG_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
                        INFO_LOG("%s", MsgStr);
                        MRD_FREE_POOL(MsgStr);
                    }
                    else {
                        TmpStr = PoolPrint (L"WARN: %r", XStatus);
                        MsgStr = PoolPrint (
                            L"Handle 0x%03X  .  %-23s%s%s",
                            HexIndex, TmpStr, FillStr, DeviceData
                        );
                        DEBUG_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
                        INFO_LOG("%s", MsgStr);
                        MRD_FREE_POOL(MsgStr);
                        MRD_FREE_POOL(TmpStr);
                    }

                    #endif
                }
            }
        }

        if (EFI_ERROR(XStatus)) {

            Status = XStatus;
        }

        #if MERIDIAN_DEBUG > 0
        if (i == AllHandleCountTrigger) {
            INFO_LOG("\n\n");
        }
        else {
            INFO_LOG("\n");
        }

        MRD_FREE_POOL(DeviceData);
        #endif

        MRD_FREE_POOL(HandleBuffer);
        MRD_FREE_POOL(HandleType);
    }

    #if MERIDIAN_DEBUG > 0
    MRD_FREE_POOL(GopDevicePathStr);
    #endif

	MRD_FREE_POOL(AllHandleBuffer);

	return Status;
}

static
EFI_STATUS BdsLibConnectAllDriversToAllControllersEx (VOID) {
    EFI_STATUS  Status;
    BOOLEAN     RescanDrivers;

    #if MERIDIAN_DEBUG > 0
    CHAR16  *MsgStr;
    #endif

    RescanDrivers = (GlobalConfig.RescanDXE || ForceRescanDXE);

    if (RescanDrivers) {

        EFI_STATUS   ConnStatus;
        EFI_HANDLE  *ConnHandleBuffer;
        UINTN        ConnHandleCount;
        UINTN        ConnIndex;

        ConnHandleBuffer = NULL;
        ConnHandleCount  = 0;
        ConnStatus = gBS->LocateHandleBuffer(AllHandles, NULL, NULL, &ConnHandleCount, &ConnHandleBuffer);
        if (!EFI_ERROR(ConnStatus)) {
            for (ConnIndex = 0; ConnIndex < ConnHandleCount; ConnIndex++) {
                gBS->ConnectController(ConnHandleBuffer[ConnIndex], NULL, NULL, TRUE);
            }
            MRD_FREE_POOL(ConnHandleBuffer);
        }
    }

    do {
        ObtainHandleGOP = FoundGOP = FALSE;

        BdsLibConnectMostlyAllEfi();

        ObtainHandleGOP = FoundGOP;

        Status = (RescanDrivers && gDS) ? gDS->Dispatch() : EFI_NOT_FOUND;

#if MERIDIAN_DEBUG > 0
        if (EFI_ERROR(Status)) {
            if (!FoundGOP && DetectedDevices) {
                INFO_LOG("INFO: Could *NOT* Identify Path to GOP on Device Handles");
            }
        }
        else {
            MsgStr = StrDuplicate (
                L"Additional DXE Drivers Revealed ... Relink Handles"
            );
            DEBUG_LOG(1, LOG_THREE_STAR_MID, L"%s", MsgStr);
            INFO_LOG("INFO: %s", MsgStr);
            INFO_LOG("\n\n");
            MRD_FREE_POOL(MsgStr);
        }
        #endif
    } while (!EFI_ERROR(Status));

    #if MERIDIAN_DEBUG > 0
    MsgStr = PoolPrint (
        L"Processed %d Handle%s ... Devices:- '%s'",
        AllHandleCount,
        (AllHandleCount == 1) ? L"" : L"s",
        (DevicePresence) ? L"Present" : L"Absent"
    );
    if (!FoundGOP && DetectedDevices) {
        INFO_LOG("%s      %s", OffsetNext, MsgStr);
    }
    else {
        INFO_LOG("INFO: %s", MsgStr);
    }
    DEBUG_LOG(1, LOG_LINE_THIN_SEP, L"%s", MsgStr);
    MRD_FREE_POOL(MsgStr);
    #endif

    Status = (FoundGOP) ? EFI_SUCCESS : EFI_NOT_FOUND;

    return Status;
}

EFI_STATUS ApplyGOPFix (VOID) {
    EFI_STATUS Status;
    BOOLEAN    TempRescanDXE;

    #if MERIDIAN_DEBUG > 0
    CHAR16 *MsgStr;
    #endif

    Status = AcquireGOP();
    #if MERIDIAN_DEBUG > 0
    DEBUG_LOG(1, LOG_LINE_THIN_SEP, L"Reload OptionROM");
    MsgStr = PoolPrint (
        L"Status:- '%r ... Acquire OptionROM from Volatile Memory'",
        Status
    );
    DEBUG_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
    INFO_LOG("\n\n");
    INFO_LOG("INFO: %s", MsgStr);
    MRD_FREE_POOL(MsgStr);
    #endif
    if (EFI_ERROR(Status)) {
        AcquireErrorGOP = TRUE;

        return Status;
    }

    Status = AmendSysTable();
    #if MERIDIAN_DEBUG > 0
    MsgStr = PoolPrint (
        L"Status:- '%r ... Amend Boot Services Table'",
        Status
    );
    DEBUG_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
    INFO_LOG("%s      %s", OffsetNext, MsgStr);
    MRD_FREE_POOL(MsgStr);
    #endif
    if (Status == EFI_ALREADY_STARTED && SetSysTab == TRUE) {

        Status = EFI_SUCCESS;
    }
    if (EFI_ERROR(Status)) {
        AcquireErrorGOP = TRUE;

        return Status;
    }

    Status = ReissueGOP();
    if (EFI_ERROR(Status)) {

        return Status;
    }

    #if MERIDIAN_DEBUG > 0
    BRK_MOD("\n\n");
    #endif

    TempRescanDXE = GlobalConfig.RescanDXE;
    GlobalConfig.RescanDXE = FALSE;
    Status = BdsLibConnectAllDriversToAllControllersEx();
    GlobalConfig.RescanDXE = TempRescanDXE;

    return Status;
}

VOID EFIAPI BdsLibConnectAllDriversToAllControllers (
    IN BOOLEAN ResetGOP
) {
    EFI_STATUS Status;

    #if MERIDIAN_DEBUG > 0
    CHAR16 *MsgStr;
    #endif

    ReadAllKeyStrokes();
    if (!AppleFirmware) {

        gST->ConIn->Reset(gST->ConIn, FALSE);
    }

    Status = BdsLibConnectAllDriversToAllControllersEx();
    if (GlobalConfig.ReloadGOP) {
        if (EFI_ERROR(Status) && ResetGOP && !ReLoaded && DetectedDevices) {
            ReLoaded = TRUE;

            #if MERIDIAN_DEBUG > 0

            Status =
            #endif
            ApplyGOPFix();

            #if MERIDIAN_DEBUG > 0
            if (!AcquireErrorGOP) {
                MsgStr = PoolPrint (
                    L"Status:- '%r ... Issue OptionROM from Volatile Memory'",
                    Status
                );
                DEBUG_LOG(1, LOG_STAR_SEPARATOR, L"%s", MsgStr);
                INFO_LOG("%s      %s", OffsetNext, MsgStr);
                MRD_FREE_POOL(MsgStr);
            }
            #endif

            ReLoaded = FALSE;
        }
    }
}
