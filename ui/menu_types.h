// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2020-2026 Dayo Akanji
// SPDX-FileCopyrightText: 2012-2023 Roderick W. Smith
// SPDX-FileCopyrightText: 2006-2009 Christoph Pfisterer

#ifndef __MERIDIAN_MENU_TYPES_H_
#define __MERIDIAN_MENU_TYPES_H_

#include "tiano_includes.h"
#include "display_types.h"

#define TAG_BASE (0)

#define TAG_REBOOT (2)
#define TAG_SHUTDOWN (3)
#define TAG_EXIT (4)
#define TAG_SHELL (5)
#define TAG_GPTSYNC (6)
#define TAG_RECOVERY_MAC (7)
#define TAG_RECOVERY_WIN (8)
#define TAG_MOK (9)
#define TAG_FIRMWARE (10)
#define TAG_MEMTEST (11)
#define TAG_GDISK (12)
#define TAG_NETBOOT (13)
#define TAG_CSR_ROTATE (14)
#define TAG_FWUPDATE (15)
#define TAG_CYDIA (16)
#define TAG_INSTALL (17)
#define TAG_BOOTORDER (18)
#define TAG_CLEAN_NVRAM (19)
#define NUM_TOOLS (20)

#define TAG_TOOL (21)
#define TAG_LOADER (22)
#define TAG_RESET_NVRAM (25)
#define TAG_FIRMWARE_LOADER (26)
#define TAG_SCAN_ALL (27)

#define TAG_SPACER (98)
#define TAG_RETURN (99)

#define NUM_SCAN_OPTIONS (10)

#define COLOR_LIGHTBLUE {255, 175, 100, 0}
#define COLOR_AMBER {255, 177, 0, 0}
#define COLOR_RED {0, 0, 200, 0}

#define DISABLE_BOOTLOGO_OFF (0)
#define DISABLE_BOOTLOGO_LIN (1)
#define DISABLE_BOOTLOGO_WIN (2)
#define DISABLE_BOOTLOGO_ALL (3)

#define GRAPHICS_FOR_NONE (0)
#define GRAPHICS_FOR_OSX (1)
#define GRAPHICS_FOR_LINUX (2)
#define GRAPHICS_FOR_WINDOWS (4)
#define GRAPHICS_FOR_GRUB (8)
#define GRAPHICS_FOR_ELILO (16)
#define GRAPHICS_FOR_TOOLS (32)
#define GRAPHICS_FOR_CLOVER (64)
#define GRAPHICS_FOR_SYSTEMD (128)
#define GRAPHICS_FOR_OPENCORE (256)
#define GRAPHICS_FOR_BSD (512)
#define GRAPHICS_FOR_EVERYTHING (1023)

#define HIDEUI_FLAG_NONE (0)
#define HIDEUI_FLAG_BANNER (1)
#define HIDEUI_FLAG_LABEL (2)
#define HIDEUI_FLAG_SINGLEUSER (4)
#define HIDEUI_FLAG_HWTEST (8)
#define HIDEUI_FLAG_ARROWS (16)
#define HIDEUI_FLAG_HINTS (32)
#define HIDEUI_FLAG_EDITOR (64)
#define HIDEUI_FLAG_SAFEMODE (128)
#define HIDEUI_FLAG_ALL (255)

#define SUBSCREEN_HINT1 L"Use arrow keys to move selection and press 'Enter' to run selected item"
#define SUBSCREEN_HINT2                                                                            \
    L"Press 'Insert' or 'F2' to edit options or press 'Esc' to return to the main screen"
#define SUBSCREEN_HINT2_NO_EDITOR L"Press 'Esc' to return to the main screen"

#define MAIN_MENU_NAME L"Main Menu"
#define SELECT_OPTION_HINT L"Select an option and press 'Enter' to apply the option"
#define RETURN_MAIN_SCREEN_HINT                                                                    \
    L"Press 'ESC', 'BackSpace' or 'SpaceBar' to return to the main screen"

typedef struct _meridian_menu_entry
{
    CHAR16 *Title;
    UINTN Tag;
    UINTN Row;
    struct _meridian_menu_screen *SubScreen;
} MERIDIAN_MENU_ENTRY;

typedef struct _meridian_menu_screen
{
    CHAR16 *Title;
    UINTN InfoLineCount;
    CHAR16 **InfoLines;
    UINTN EntryCount;
    MERIDIAN_MENU_ENTRY **Entries;
    INTN TimeoutSeconds;
    CHAR16 *TimeoutText;
    CHAR16 *Hint1;
    CHAR16 *Hint2;
} MERIDIAN_MENU_SCREEN;

#endif
