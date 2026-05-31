// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2020-2026 Dayo Akanji
// SPDX-FileCopyrightText: 2012-2023 Roderick W. Smith

#ifndef __MERIDIAN_TOOL_DEFAULTS_H_
#define __MERIDIAN_TOOL_DEFAULTS_H_

#include "esp_layout.h"

#if defined(EFIX64)
#define MOK_FILES                                                                                  \
    L"MokManager.efi,HashTool.efi,HashTool-signed.efi,"                                            \
    L"KeyTool.efi,KeyTool-signed.efi,mm.efi,mm_x64.efi,mmx64.efi"
#elif defined(EFIAARCH64)
#define MOK_FILES                                                                                  \
    L"MokManager.efi,HashTool.efi,HashTool-signed.efi,"                                            \
    L"KeyTool.efi,KeyTool-signed.efi,mm.efi,mm_aa64.efi,mmaa64.efi"
#else
#define MOK_FILES                                                                                  \
    L"MokManager.efi,HashTool.efi,HashTool-signed.efi,KeyTool.efi,KeyTool-signed.efi,mm.efi"
#endif

#if defined(EFIX64)
#define FWUPDATE_FILES L"fwup.efi,fwup_x64,fwupx64.efi"
#elif defined(EFIAARCH64)
#define FWUPDATE_FILES L"fwup.efi,fwup_aa64,fwupaa64.efi"
#else
#define FWUPDATE_FILES L"fwup.efi"
#endif

#define MEMTEST_LOCATIONS                                                                          \
    MERIDIAN_ESP_TOOLS_PATH L"\\memtest," MERIDIAN_ESP_TOOLS_PATH L"\\memtest86"                   \
                            L"," MERIDIAN_ESP_TOOLS_PATH L"\\memtest86+"                           \
                            L"," MERIDIAN_ESP_TOOLS_PATH L"\\memtest"                              \
                            L"86p"

#define TOOL_LOCATIONS MERIDIAN_ESP_TOOLS_PATH

#define REMOTE_LOCATIONS MERIDIAN_ESP_REMOTE_PATH

#endif
