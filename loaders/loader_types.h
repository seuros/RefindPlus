// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2020-2026 Dayo Akanji
// SPDX-FileCopyrightText: 2012-2023 Roderick W. Smith
// SPDX-FileCopyrightText: 2006-2009 Christoph Pfisterer

#ifndef __MERIDIAN_LOADER_TYPES_H_
#define __MERIDIAN_LOADER_TYPES_H_

#include "tiano_includes.h"
#include "volume_types.h"
#include "menu_types.h"

#define MERIDIAN_PROTO_EFI 0
#define MERIDIAN_PROTO_LIMINE 1

typedef struct
{
    MERIDIAN_MENU_ENTRY me;
    CHAR16 *Title;
    CHAR16 *LoaderPath;
    MERIDIAN_VOLUME *Volume;
    BOOLEAN UseGraphicsMode;
    BOOLEAN Enabled;
    CHAR16 *LoadOptions;
    CHAR16 *InitrdPath;
    CHAR8 OSType;
    UINTN BootProtocol;
    UINTN DiscoveryType;
    EFI_DEVICE_PATH_PROTOCOL *EfiLoaderPath;
    UINT16 EfiBootNum;
} LOADER_ENTRY;

#endif
