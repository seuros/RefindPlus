// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#include "Platform.h"

CHAR16 * EFIAPI DevicePathToStr (
    IN EFI_DEVICE_PATH_PROTOCOL *DevPath
) {
    if (DevPath == NULL) {
        return NULL;
    }

    return ConvertDevicePathToText (DevPath, FALSE, FALSE);
}
