// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2020-2025 Dayo Akanji
// SPDX-FileCopyrightText: 2021 Joe van Tunen
// SPDX-FileCopyrightText: Intel Corporation

EFI_STATUS AmendSysTable (VOID);

#include "global.h"
#include "display.h"
#include "meridian_funcs.h"
#include <Protocol/Runtime.h>
#include <Protocol/SmmBase2.h>
#include <Core/Dxe/Event/Event.h>

BOOLEAN SetSysTab = FALSE;

#define EFI_FIELD_OFFSET(TYPE, Field) ((UINTN) (&(((TYPE *) 0)->Field)))
#define EFI_REVISION_MIN  EFI_2_00_SYSTEM_TABLE_REVISION
#define EFI_REVISION_MOD  EFI_2_30_SYSTEM_TABLE_REVISION

EFI_CPU_ARCH_PROTOCOL   *zCpu       = NULL;
EFI_SMM_BASE2_PROTOCOL  *zSmmBase2  = NULL;

EFI_RUNTIME_ARCH_PROTOCOL zRuntimeTemplate = {
    INITIALIZE_LIST_HEAD_VARIABLE (zRuntimeTemplate.ImageHead),
    INITIALIZE_LIST_HEAD_VARIABLE (zRuntimeTemplate.EventHead),
    sizeof (EFI_MEMORY_DESCRIPTOR) +
        sizeof (UINT64) -
        (sizeof (EFI_MEMORY_DESCRIPTOR) % sizeof (UINT64)),
    EFI_MEMORY_DESCRIPTOR_VERSION, 0,
    NULL, NULL, FALSE, FALSE
};

UINTN                       zEventPending      = 0;
EFI_TPL                     zEfiCurrentTpl     = TPL_APPLICATION;
EFI_LOCK                    zEventQueueLock    = EFI_INITIALIZE_LOCK_VARIABLE (TPL_HIGH_LEVEL);
EFI_RUNTIME_ARCH_PROTOCOL  *zRuntime           = &zRuntimeTemplate;
LIST_ENTRY                  zEventSignalQueue  = INITIALIZE_LIST_HEAD_VARIABLE (zEventSignalQueue);
LIST_ENTRY                  zEventQueue[TPL_HIGH_LEVEL + 1];

UINT32 rEventTable[] = {
    EVT_TIMER|EVT_NOTIFY_SIGNAL,
    EVT_TIMER, EVT_NOTIFY_WAIT, EVT_NOTIFY_SIGNAL,
    EVT_SIGNAL_EXIT_BOOT_SERVICES,
    EVT_SIGNAL_VIRTUAL_ADDRESS_CHANGE, 0x00000000,
    EVT_TIMER|EVT_NOTIFY_WAIT
};

EFI_TPL    EFIAPI OurRaiseTpl (IN EFI_TPL  NewTpl);
VOID       EFIAPI OurRestoreTpl (IN EFI_TPL  NewTpl);
VOID              OurSetInterruptState (IN BOOLEAN  Enable);
VOID              OurDispatchEventNotifies (IN EFI_TPL  Priority);
VOID              OurAcquireLock (IN EFI_LOCK  *Lock);
VOID              OurReleaseLock (IN EFI_LOCK  *Lock);
EFI_STATUS EFIAPI OurCreateEventEx (
    UINT32             Type,
    EFI_TPL            NotifyTpl,
    EFI_EVENT_NOTIFY   NotifyFunction,
    const void        *NotifyContext,
    const EFI_GUID    *EventGroup,
    EFI_EVENT         *Event
);

VOID OurSetInterruptState (
    IN BOOLEAN  Enable
) {
    EFI_STATUS  Status;
    BOOLEAN     InSmm;

    if (zCpu == NULL) {
        return;
    }

    if (!Enable) {
        zCpu->DisableInterrupt (zCpu);
        return;
    }

    if (zSmmBase2 == NULL) {
        zCpu->EnableInterrupt (zCpu);
        return;
    }

    Status = zSmmBase2->InSmm (zSmmBase2, &InSmm);
    if (!EFI_ERROR(Status) && !InSmm) {
        zCpu->EnableInterrupt (zCpu);
    }
}

VOID OurDispatchEventNotifies (
    IN EFI_TPL  Priority
) {
    IEVENT        *Event;
    LIST_ENTRY    *Head;

    OurAcquireLock (&zEventQueueLock);
    ASSERT (zEventQueueLock.OwnerTpl == Priority);
    Head = &zEventQueue[Priority];

    while (!IsListEmpty (Head)) {
        Event = CR(Head->ForwardLink, IEVENT, NotifyLink, EVENT_SIGNATURE);
        RemoveEntryList(&Event->NotifyLink);
        Event->NotifyLink.ForwardLink = NULL;

        if ((Event->Type & EVT_NOTIFY_SIGNAL) != 0) {
            Event->SignalCount = 0;
        }

        OurReleaseLock (&zEventQueueLock);

        ASSERT (Event->NotifyFunction != NULL);

        Event->NotifyFunction(Event, Event->NotifyContext);

        OurAcquireLock (&zEventQueueLock);
    }

    zEventPending &= ~((UINTN) (1) << Priority);
    OurReleaseLock (&zEventQueueLock);
}

EFI_TPL EFIAPI OurRaiseTpl (
    IN EFI_TPL  NewTpl
) {
    EFI_TPL     OldTpl;

    OldTpl = zEfiCurrentTpl;
    if (OldTpl > NewTpl) {
        #if MERIDIAN_DEBUG > 0
        INFO_LOG(
            "FATAL ERROR: RaiseTpl with OldTpl (0x%x) > NewTpl (0x%x)",
            OldTpl,
            NewTpl
        );
        INFO_LOG("\n\n");
        #endif

        ASSERT (FALSE);
    }
    ASSERT (VALID_TPL (NewTpl));

    if (NewTpl >= TPL_HIGH_LEVEL  &&  OldTpl < TPL_HIGH_LEVEL) {
        OurSetInterruptState (FALSE);
    }

    zEfiCurrentTpl = NewTpl;

    return OldTpl;
}

VOID EFIAPI OurRestoreTpl (
    IN EFI_TPL NewTpl
) {
    EFI_TPL    OldTpl;
    EFI_TPL    PendingTpl;

    OldTpl = zEfiCurrentTpl;
    if (NewTpl > OldTpl) {
        #if MERIDIAN_DEBUG > 0
        INFO_LOG(
            "FATAL ERROR: RestoreTpl with NewTpl (0x%x) > OldTpl (0x%x)",
            NewTpl,
            OldTpl
        );
        INFO_LOG("\n");
        #endif

        ASSERT (FALSE);
    }
    ASSERT (VALID_TPL (NewTpl));

    if (OldTpl >= TPL_HIGH_LEVEL  &&  NewTpl < TPL_HIGH_LEVEL) {
        zEfiCurrentTpl = TPL_HIGH_LEVEL;
    }

    while (zEventPending != 0) {
        PendingTpl = (UINTN) HighBitSet64 (zEventPending);
        if (PendingTpl <= NewTpl) {
            break;
        }

        zEfiCurrentTpl = PendingTpl;
        if (zEfiCurrentTpl < TPL_HIGH_LEVEL) {
            OurSetInterruptState (TRUE);
        }
        OurDispatchEventNotifies (zEfiCurrentTpl);
    }

    zEfiCurrentTpl = NewTpl;

    if (zEfiCurrentTpl < TPL_HIGH_LEVEL) {
        OurSetInterruptState (TRUE);
    }
}

VOID OurAcquireLock (
    IN EFI_LOCK  *Lock
) {
    ASSERT (Lock != NULL);
    ASSERT (Lock->Lock == EfiLockReleased);

    Lock->OwnerTpl = OurRaiseTpl (Lock->Tpl);
    Lock->Lock     = EfiLockAcquired;
}

VOID OurReleaseLock (
    IN EFI_LOCK  *Lock
) {
    EFI_TPL Tpl;

    ASSERT (Lock != NULL);
    ASSERT (Lock->Lock == EfiLockAcquired);

    Tpl        = Lock->OwnerTpl;
    Lock->Lock = EfiLockReleased;

    OurRestoreTpl (Tpl);
}

EFI_STATUS EFIAPI OurCreateEventEx (
    IN        UINT32             Type,
    IN        EFI_TPL            NotifyTpl,
    IN        EFI_EVENT_NOTIFY   NotifyFunction OPTIONAL,
    IN  const VOID              *NotifyContext  OPTIONAL,
    IN  const EFI_GUID          *EventGroup     OPTIONAL,
    OUT       EFI_EVENT         *Event
) {
    EFI_STATUS         Status;
    IEVENT            *IEvent;
    INTN               Index;

    if ((Type & (EVT_NOTIFY_WAIT | EVT_NOTIFY_SIGNAL)) != 0) {
        if (NotifyTpl != TPL_APPLICATION &&
            NotifyTpl != TPL_CALLBACK    &&
            NotifyTpl != TPL_NOTIFY
        ) {
            return EFI_INVALID_PARAMETER;
        }
    }

    if (Event == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    Status = EFI_INVALID_PARAMETER;
    for (Index = 0; Index < (sizeof (rEventTable) / sizeof (UINT32)); Index++) {
        if (Type == rEventTable[Index]) {
            Status = EFI_SUCCESS;
            break;
        }
    }
    if (EFI_ERROR(Status)) {
        return EFI_INVALID_PARAMETER;
    }

    if (EventGroup != NULL) {

        if ((Type == EVT_SIGNAL_EXIT_BOOT_SERVICES) ||
            (Type == EVT_SIGNAL_VIRTUAL_ADDRESS_CHANGE)
        ) {
            return EFI_INVALID_PARAMETER;
        }

        if (CompareGuid (EventGroup, &gEfiEventExitBootServicesGuid)) {
            Type = EVT_SIGNAL_EXIT_BOOT_SERVICES;
        }
        else {
            if (CompareGuid (EventGroup, &gEfiEventVirtualAddressChangeGuid)) {
                Type = EVT_SIGNAL_VIRTUAL_ADDRESS_CHANGE;
            }
        }
    }
    else {

        if (Type == EVT_SIGNAL_EXIT_BOOT_SERVICES) {
            EventGroup = &gEfiEventExitBootServicesGuid;
        }
        else {
            if (Type == EVT_SIGNAL_VIRTUAL_ADDRESS_CHANGE) {
                EventGroup = &gEfiEventVirtualAddressChangeGuid;
            }
        }
    }

    if ((Type & (EVT_NOTIFY_WAIT | EVT_NOTIFY_SIGNAL)) != 0) {

        if ((NotifyFunction == NULL)       ||
            (NotifyTpl <= TPL_APPLICATION) ||
            (NotifyTpl >= TPL_HIGH_LEVEL)
        ) {
            return EFI_INVALID_PARAMETER;
        }
    }
    else {

        NotifyTpl      = 0;
        NotifyFunction = NULL;
        NotifyContext  = NULL;
    }

    IEvent = ((Type & EVT_RUNTIME) != 0)
        ? AllocateRuntimeZeroPool (sizeof (IEVENT))
        : AllocateZeroPool (sizeof (IEVENT));

    if (IEvent == NULL) {
        return EFI_OUT_OF_RESOURCES;
    }

    IEvent->Signature      = EVENT_SIGNATURE;
    IEvent->Type           = Type;
    IEvent->NotifyTpl      = NotifyTpl;
    IEvent->NotifyFunction = NotifyFunction;
    IEvent->NotifyContext  = (VOID *) NotifyContext;

    if (EventGroup != NULL) {
        CopyGuid (&IEvent->EventGroup, EventGroup);
        IEvent->ExFlag |= EVT_EXFLAG_EVENT_GROUP;
    }

    *Event = IEvent;

    if ((Type & EVT_RUNTIME) != 0) {

        IEvent->RuntimeData.Type           = Type;
        IEvent->RuntimeData.NotifyTpl      = NotifyTpl;
        IEvent->RuntimeData.NotifyFunction = NotifyFunction;
        IEvent->RuntimeData.NotifyContext  = (VOID *) NotifyContext;
        IEvent->RuntimeData.Event          = (EFI_EVENT *) IEvent;
        InsertTailList (&zRuntime->EventHead, &IEvent->RuntimeData.Link);
    }

    OurAcquireLock (&zEventQueueLock);

    if ((Type & EVT_NOTIFY_SIGNAL) != 0x00000000) {

        InsertHeadList (&zEventSignalQueue, &IEvent->SignalLink);
    }

    OurReleaseLock (&zEventQueueLock);

    return EFI_SUCCESS;
}

EFI_STATUS AmendSysTable (VOID) {
    EFI_BOOT_SERVICES *uBS;

    if (gBS->Hdr.Revision >= EFI_REVISION_MIN ||
        gRT->Hdr.Revision >= EFI_REVISION_MIN ||
        gST->Hdr.Revision >= EFI_REVISION_MIN
    ) {

        return EFI_ALREADY_STARTED;
    }

    if (gBS->Hdr.HeaderSize > EFI_FIELD_OFFSET(EFI_BOOT_SERVICES, CreateEventEx)) {

        return EFI_PROTOCOL_ERROR;
    }

    uBS = (EFI_BOOT_SERVICES *) AllocateCopyPool (sizeof (EFI_BOOT_SERVICES), gBS);
    if (uBS == NULL) {

        return EFI_OUT_OF_RESOURCES;
    }

    gST->BootServices    = gBS;
    gST->RuntimeServices = gRT;
    gST->Hdr.HeaderSize  = sizeof (EFI_SYSTEM_TABLE);
    gST->Hdr.Revision    = EFI_REVISION_MOD;
    gST->Hdr.CRC32       = 0;
    gBS->CalculateCrc32(gST, gST->Hdr.HeaderSize, &gST->Hdr.CRC32);

    uBS->CreateEventEx   = OurCreateEventEx;
    uBS->Hdr.HeaderSize  = sizeof (EFI_BOOT_SERVICES);
    uBS->Hdr.Revision    = EFI_REVISION_MOD;
    uBS->Hdr.CRC32       = 0;
    uBS->CalculateCrc32(uBS, uBS->Hdr.HeaderSize, &uBS->Hdr.CRC32);
    gBS = uBS;

    SetSysTab = TRUE;

    return EFI_SUCCESS;
}
