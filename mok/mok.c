// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2021-2024 Dayo Akanji
// SPDX-FileCopyrightText: 2021 Joe van Tunen
// SPDX-FileCopyrightText: Intel Corporation
// SPDX-FileCopyrightText: 2012 Red Hat, Inc <mjg@redhat.com>

#include "global.h"
#include "mok.h"
#include "lib.h"
#include "screenmgt.h"

BOOLEAN ShimFound  = FALSE;
BOOLEAN SecureFlag = FALSE;

BOOLEAN secure_mode (VOID) {
    #if MERIDIAN_DEBUG > 0
    BOOLEAN CheckMute = FALSE;
    #endif

    EFI_STATUS      Status;
    EFI_GUID        GlobalVar = EFI_GLOBAL_VARIABLE;
    UINTN           CharSize;
    UINT8          *SetupMode;
    UINT8          *Sec;

    static BOOLEAN  DoneOnce = FALSE;

    if (DoneOnce) {
        return SecureFlag;
    }

    #if MERIDIAN_DEBUG > 0
    MRD_MUTELOGGER_SET;
    #endif
    Sec = NULL;
    Status = EfivarGetRaw (
        &GlobalVar, L"SecureBoot",
        (VOID **) &Sec, &CharSize
    );

    if (*Sec != 1         ||
        EFI_ERROR(Status) ||
        CharSize != sizeof (CHAR8)
    ) {
        SecureFlag = FALSE;
    }
    else {
        SetupMode = NULL;
        Status = EfivarGetRaw (
            &GlobalVar, L"SetupMode",
            (VOID **) &SetupMode, &CharSize
        );
        if (*SetupMode == 1    &&
            !EFI_ERROR(Status) &&
            CharSize == sizeof (CHAR8)
        ) {
            SecureFlag = FALSE;
        }
        else {
            SecureFlag = TRUE;
        }
        MRD_FREE_POOL(SetupMode);
    }
    #if MERIDIAN_DEBUG > 0
    MRD_MUTELOGGER_OFF;
    #endif

    DoneOnce = TRUE;

    MRD_FREE_POOL(Sec);

    return SecureFlag;
}

BOOLEAN ShimLoaded (VOID) {
    EFI_STATUS   Status;
    SHIM_LOCK   *shim_lock;
    EFI_GUID     ShimLockGuid = SHIM_LOCK_GUID;

    Status = gBS->LocateProtocol(&ShimLockGuid, NULL, (VOID **) &shim_lock);

    ShimFound = (EFI_ERROR(Status)) ? FALSE : TRUE;

    return ShimFound;
}

BOOLEAN ShimValidate (
    VOID   *data,
    UINT32  size
) {
    SHIM_LOCK   *shim_lock;
    EFI_GUID    ShimLockGuid = SHIM_LOCK_GUID;

    if (data != NULL &&
        (
            gBS->LocateProtocol(&ShimLockGuid, NULL, (VOID **) &shim_lock) == EFI_SUCCESS
        )
    ) {
        if (!shim_lock) {
            return FALSE;
        }

        if (shim_lock->shim_verify (data, size) == EFI_SUCCESS) {
            return TRUE;
        }
    }

    return FALSE;
}
