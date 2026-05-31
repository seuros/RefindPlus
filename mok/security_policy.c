// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2024-2025 Dayo Akanji
// SPDX-FileCopyrightText: 2012 James Bottomley <James.Bottomley@HansenPartnership.com>

#include <global.h>

#include "mok.h"
#include "guid.h"
#include "simple_file.h"
#include "lib.h"

#include <security_policy.h>

struct _EFI_SECURITY2_PROTOCOL;
struct _EFI_SECURITY_PROTOCOL;
struct _EFI_DEVICE_PATH_PROTOCOL;
typedef struct _EFI_SECURITY2_PROTOCOL EFI_SECURITY2_PROTOCOL;
typedef struct _EFI_SECURITY_PROTOCOL EFI_SECURITY_PROTOCOL;

#if defined(EFIX64)
#define MSABI __attribute__((ms_abi))
#else
#define MSABI
#endif

typedef EFI_STATUS (MSABI *EFI_SECURITY_FILE_AUTHENTICATION_STATE) (
    const EFI_SECURITY_PROTOCOL *This,
    UINT32 AuthenticationStatus,
    const EFI_DEVICE_PATH_PROTOCOL *File
);
typedef EFI_STATUS (MSABI *EFI_SECURITY2_FILE_AUTHENTICATION) (
    const EFI_SECURITY2_PROTOCOL *This,
    const EFI_DEVICE_PATH_PROTOCOL *DevicePath,
    VOID *FileBuffer,
    UINTN FileSize,
    BOOLEAN  BootPolicy
);

struct _EFI_SECURITY2_PROTOCOL {
    EFI_SECURITY2_FILE_AUTHENTICATION FileAuthentication;
};

struct _EFI_SECURITY_PROTOCOL {
    EFI_SECURITY_FILE_AUTHENTICATION_STATE  FileAuthenticationState;
};

static EFI_SECURITY_FILE_AUTHENTICATION_STATE esfas = NULL;
static EFI_SECURITY2_FILE_AUTHENTICATION es2fa = NULL;

static
MSABI EFI_STATUS security2_policy_authentication (
    const EFI_SECURITY2_PROTOCOL *This,
    const EFI_DEVICE_PATH_PROTOCOL *DevicePath,
    VOID *FileBuffer,
    UINTN FileSize,
    BOOLEAN  BootPolicy
) {
    EFI_STATUS Status;

    Status = es2fa(This, DevicePath, FileBuffer, FileSize, BootPolicy);

    if (!EFI_ERROR(Status)) {
        return Status;
    }

    if (ShimValidate(FileBuffer, FileSize)) {
        Status = EFI_SUCCESS;
    }

    return Status;
}

static
MSABI EFI_STATUS security_policy_authentication (
    const EFI_SECURITY_PROTOCOL *This,
    UINT32 AuthenticationStatus,
    const EFI_DEVICE_PATH_PROTOCOL *DevicePathConst
) {
    EFI_STATUS                  Status;
    EFI_DEVICE_PATH_PROTOCOL   *DevPath;
    EFI_DEVICE_PATH_PROTOCOL   *OrigDevPath;
    EFI_HANDLE                  h;
    EFI_FILE_PROTOCOL          *f;
    VOID                       *FileBuffer;
    UINTN                       FileSize;
    CHAR16                     *DevPathStr;

    if (DevicePathConst == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    DevPath = OrigDevPath = DuplicateDevicePath (
        (EFI_DEVICE_PATH_PROTOCOL *) DevicePathConst
    );

    Status = gBS->LocateDevicePath(&SIMPLE_FS_PROTOCOL, &DevPath, &h);
    if (EFI_ERROR(Status)) {
        goto out;
    }

    DevPathStr = DevicePathToStr(DevPath);

    Status = simple_file_open_by_handle(h, DevPathStr, &f, MeridianReadOnly);
    MRD_FREE_POOL(DevPathStr);
    if (EFI_ERROR(Status)) {
        goto out;
    }

    Status = simple_file_read_all(f, &FileSize, &FileBuffer);
    simple_file_close(f);
    if (EFI_ERROR(Status))
    goto out;

    if (ShimValidate(FileBuffer, FileSize)) {
        Status = EFI_SUCCESS;
    }
    else {

        Status = esfas(This, AuthenticationStatus, DevicePathConst);
    }
    FreePool(FileBuffer);

    out:
    MRD_FREE_POOL(OrigDevPath);
    return Status;
}

EFI_STATUS security_policy_install(void) {
    EFI_SECURITY_PROTOCOL *security_protocol;
    EFI_SECURITY2_PROTOCOL *security2_protocol = NULL;
    EFI_STATUS status;

    if (esfas) {

        return EFI_ALREADY_STARTED;
    }

    gBS->LocateProtocol(&SECURITY2_PROTOCOL_GUID, NULL, (VOID **) &security2_protocol);

    status = gBS->LocateProtocol(&SECURITY_PROTOCOL_GUID, NULL, (VOID **) &security_protocol);
    if (status != EFI_SUCCESS)

    return status;

    if (security2_protocol) {
        es2fa = security2_protocol->FileAuthentication;
        security2_protocol->FileAuthentication = security2_policy_authentication;
    }

    esfas = security_protocol->FileAuthenticationState;
    security_protocol->FileAuthenticationState = security_policy_authentication;

    return EFI_SUCCESS;
}

EFI_STATUS security_policy_uninstall(void) {
    EFI_STATUS status;

    if (esfas) {
        EFI_SECURITY_PROTOCOL *security_protocol;

        status = gBS->LocateProtocol(&SECURITY_PROTOCOL_GUID, NULL, (VOID **) &security_protocol);

        if (status != EFI_SUCCESS) {
            return status;
        }

        security_protocol->FileAuthenticationState = esfas;
        esfas = NULL;
    }
    else {

        return EFI_NOT_STARTED;
    }

    if (es2fa) {
        EFI_SECURITY2_PROTOCOL *security2_protocol;

        status = gBS->LocateProtocol(&SECURITY2_PROTOCOL_GUID, NULL, (VOID **) &security2_protocol);

        if (status != EFI_SUCCESS) {
            return status;
        }

        security2_protocol->FileAuthentication = es2fa;
        es2fa = NULL;
    }

    return EFI_SUCCESS;
}
