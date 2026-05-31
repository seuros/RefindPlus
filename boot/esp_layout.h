// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#ifndef __ESP_LAYOUT_H_
#define __ESP_LAYOUT_H_

#define MERIDIAN_ESP_HOME L"EFI\\Meridian"
#define MERIDIAN_ESP_HOME_ABS L"\\EFI\\Meridian"
#define MERIDIAN_ESP_FS_DIR L"fs"
#define MERIDIAN_ESP_DRIVERS_DIR L"drivers"
#define MERIDIAN_ESP_TOOLS_DIR L"tools"
#define MERIDIAN_ESP_REMOTE_DIR L"remote"

#define MERIDIAN_ESP_TOOLS_PATH MERIDIAN_ESP_HOME L"\\tools"
#define MERIDIAN_ESP_REMOTE_PATH MERIDIAN_ESP_HOME L"\\remote"
#define MERIDIAN_ESP_REMOTE_ABS MERIDIAN_ESP_HOME_ABS L"\\remote"

#define MERIDIAN_ESP_INSTALL_DIRS                                                                  \
    L"\\EFI"                                                                                       \
    L"," MERIDIAN_ESP_HOME_ABS L"," MERIDIAN_ESP_HOME_ABS L"\\fs"                                  \
    L"," MERIDIAN_ESP_HOME_ABS L"\\drivers"

#endif
