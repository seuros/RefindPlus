// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2020-2025 Dayo Akanji
// SPDX-FileCopyrightText: Intel Corporation

#include "tiano_includes.h"

#include "meridian_funcs.h"

EFI_GUID EfiDevicePathProtocolGuid = { 0x09576E91, 0x6D3F, 0x11D2, \
    { 0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B }};

EFI_STATUS BdsLibConnectDevicePath (
    IN EFI_DEVICE_PATH_PROTOCOL  *DevicePathToConnect
) {
    EFI_STATUS                 Status;
    UINTN                      Size;
    EFI_DEVICE_PATH_PROTOCOL  *Instance;
    EFI_DEVICE_PATH_PROTOCOL  *RemainingDevicePath;
    EFI_DEVICE_PATH_PROTOCOL  *CopyOfDevicePath;
    EFI_DEVICE_PATH_PROTOCOL  *DevicePath;
    EFI_DEVICE_PATH_PROTOCOL  *Next;
    EFI_HANDLE                 Handle;
    EFI_HANDLE                 PreviousHandle;

    if (DevicePathToConnect == NULL) {
        return EFI_SUCCESS;
    }

    DevicePath = DuplicateDevicePath (DevicePathToConnect);
    CopyOfDevicePath = DevicePath;
    if (DevicePath == NULL) {
        return EFI_OUT_OF_RESOURCES;
    }

    do {

        Instance = GetNextDevicePathInstance (&DevicePath, &Size);
        if (Instance == NULL) {
            Status = EFI_OUT_OF_RESOURCES;

            break;
        }

        Next = Instance;
        while (!IsDevicePathEndType (Next)) {
            Next = NextDevicePathNode (Next);
        }
        SetDevicePathEndNode (Next);

        PreviousHandle = NULL;
        do {

            RemainingDevicePath = Instance;

            Status = gBS->LocateDevicePath(&EfiDevicePathProtocolGuid, &RemainingDevicePath, &Handle);
            if (EFI_ERROR(Status)) continue;

            if (Handle == PreviousHandle) {

                Status = (gDS != NULL) ? gDS->Dispatch() : EFI_NOT_FOUND;
                if (EFI_ERROR(Status)) continue;
            }

            PreviousHandle = Handle;
            gBS->ConnectController(Handle, NULL, RemainingDevicePath, FALSE);
        } while (
            RemainingDevicePath != NULL &&
            !IsDevicePathEnd (RemainingDevicePath)
        );
    } while (DevicePath != NULL);

    MRD_FREE_POOL(CopyOfDevicePath);

    return Status;
}

BDS_COMMON_OPTION * BdsLibVariableToOption (
    IN OUT LIST_ENTRY *BdsCommonOptionList,
    IN     CHAR16     *VariableName
) {
    UINT32                     Attribute;
    UINT16                     FilePathSize;
    UINT8                     *Variable;
    UINT8                     *TempPtr;
    UINTN                      VariableSize;
    INTN                       i;
    EFI_DEVICE_PATH_PROTOCOL  *DevicePath;
    BDS_COMMON_OPTION         *Option;
    VOID                      *LoadOptions;
    UINT32                     LoadOptionsSize;
    CHAR16                    *Description;

    Variable = BdsLibGetVariableAndSize (
        VariableName,
        &gEfiGlobalVariableGuid,
        &VariableSize
    );
    if (Variable == NULL) {
        return NULL;
    }

    TempPtr    =  Variable;
    Attribute  =  *(UINT32 *) Variable;
    TempPtr   += sizeof (UINT32);

    FilePathSize  =  *(UINT16 *) TempPtr;
    TempPtr      += sizeof (UINT16);

    Description = (CHAR16 *) TempPtr;

    TempPtr += StrSize ((CHAR16 *) TempPtr);

    DevicePath       = (EFI_DEVICE_PATH_PROTOCOL *) TempPtr;
    TempPtr         += FilePathSize;
    LoadOptions      = TempPtr;
    LoadOptionsSize  = (UINT32) (VariableSize - (UINTN) (TempPtr - Variable));

    Option = AllocateZeroPool (sizeof (BDS_COMMON_OPTION));
    if (Option == NULL) {
        MRD_FREE_POOL(Variable);

        return NULL;
    }

    Option->Signature  = BDS_LOAD_OPTION_SIGNATURE;
    Option->DevicePath = AllocateZeroPool (GetDevicePathSize (DevicePath));
    if (Option->DevicePath == NULL) {
        MRD_FREE_POOL(Option);
        MRD_FREE_POOL(Variable);

        return NULL;
    }

    gBS->CopyMem(Option->DevicePath, DevicePath, GetDevicePathSize (DevicePath));
    Option->Attribute   = Attribute;
    Option->Description = AllocateZeroPool (StrSize (Description));
    if (Option->Description == NULL) {
        MRD_FREE_POOL(Option->DevicePath);
        MRD_FREE_POOL(Option);
        MRD_FREE_POOL(Variable);

        return NULL;
    }

    gBS->CopyMem(Option->Description, Description, StrSize (Description));
    Option->LoadOptions = AllocateZeroPool (LoadOptionsSize);
    if (Option->LoadOptions == NULL) {
        MRD_FREE_POOL(Option->DevicePath);
        MRD_FREE_POOL(Option->Description);
        MRD_FREE_POOL(Option);
        MRD_FREE_POOL(Variable);

        return NULL;
    }

    gBS->CopyMem(Option->LoadOptions, LoadOptions, LoadOptionsSize);
    Option->LoadOptionsSize = LoadOptionsSize;

    i = 0;

    #define is(x) (VariableName[i++] == x)
    #define ishex ({CHAR16 c; c = VariableName[i++]; (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F');})
    #define hex ({CHAR16 c; c = VariableName[4 + i]; (c - (c <= '9' ? '0' : 'A' - 10)) << ((3 - i++) * 4);})

    if (is('B') && is('o') && is('o') && is('t') && ishex && ishex && ishex && ishex && is(0)) {
        i = 0;
        Option->BootCurrent = hex + hex + hex + hex;
    }

    #undef is
    #undef ishex

    if ((Option->Attribute & LOAD_OPTION_ACTIVE) == LOAD_OPTION_ACTIVE) {
        InsertTailList (BdsCommonOptionList, &Option->Link);
        MRD_FREE_POOL(Variable);

        return Option;
    }

    MRD_FREE_POOL(Option->DevicePath);
    MRD_FREE_POOL(Option->Description);
    MRD_FREE_POOL(Option->LoadOptions);
    MRD_FREE_POOL(Option);
    MRD_FREE_POOL(Variable);

    return NULL;
}

VOID * BdsLibGetVariableAndSize (
    IN  CHAR16   *Name,
    IN  EFI_GUID *VendorGuid,
    OUT UINTN    *VariableSize
) {
    EFI_STATUS  Status;
    UINTN       BufferSize;
    VOID       *Buffer;

    Buffer = NULL;

    BufferSize = 0;
    Status = gRT->GetVariable(Name, VendorGuid, NULL, &BufferSize, Buffer);
    if (Status == EFI_BUFFER_TOO_SMALL) {

        Buffer = AllocateZeroPool (BufferSize);

        if (Buffer == NULL) {
            return NULL;
        }

        Status = gRT->GetVariable(Name, VendorGuid, NULL, &BufferSize, Buffer);
        if (EFI_ERROR(Status)) {
            BufferSize = 0;
        }
    }

    *VariableSize = BufferSize;

    return Buffer;
}
