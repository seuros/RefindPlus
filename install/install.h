// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2020-2021 Dayo Akanji
// SPDX-FileCopyrightText: 2012-2023 Roderick W. Smith

#ifndef __INSTALL_H_
#define __INSTALL_H_

#include "esp_layout.h"

#define INST_DIRECTORIES MERIDIAN_ESP_INSTALL_DIRS
#define INST_DRIVERS_SUBDIR MERIDIAN_ESP_FS_DIR
#define INST_MERIDIAN_NAME L"Meridian.efi"
#if defined(EFIX64)
#define INST_PLATFORM_EXTENSION L"_x64.efi"
#elif defined(EFIAARCH64)
#define INST_PLATFORM_EXTENSION L"_aa64.efi"
#else
#define INST_PLATFORM_EXTENSION L".efi"
#endif

#define EFI_BOOT_OPTION_DO_NOTHING 0
#define EFI_BOOT_OPTION_MAKE_DEFAULT 1
#define EFI_BOOT_OPTION_DELETE 2

#define DevicePathSize GetDevicePathSize

typedef struct
{
    UINT16 BootNum;
    UINT32 Options;
    UINT16 Size;
    CHAR16 *Label;
    EFI_DEVICE_PATH_PROTOCOL *DevPath;
} EFI_BOOT_ENTRY;

typedef struct _boot_entry_list
{
    EFI_BOOT_ENTRY BootEntry;
    struct _boot_entry_list *NextBootEntry;
} BOOT_ENTRY_LIST;

BOOT_ENTRY_LIST *FindBootOrderEntries(VOID);

VOID DeleteBootOrderEntries(BOOT_ENTRY_LIST *Entries);
VOID InstallMeridian(VOID);
VOID ManageBootorder(VOID);

UINTN FindBootNum(EFI_DEVICE_PATH_PROTOCOL *Entry, UINTN Size, BOOLEAN *AlreadyExists);

#endif
