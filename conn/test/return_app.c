// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#include <Uefi.h>

EFI_STATUS EFIAPI UefiMain(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    (void)ImageHandle;
    SystemTable->ConOut->OutputString(SystemTable->ConOut,
        (CHAR16 *)L"CONN-CHAINLOAD-OK\r\n");
    return EFI_SUCCESS;
}
