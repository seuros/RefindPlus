// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2021-2026 Dayo Akanji
// SPDX-FileCopyrightText: 2012 Roderick W. Smith
// SPDX-FileCopyrightText: 2006-2009 Christoph Pfisterer

#ifndef __MERIDIAN_MENU_H_
#define __MERIDIAN_MENU_H_

#include "tiano_includes.h"
#include "global.h"

#define MENU_EXIT_ZERO (0)
#define MENU_EXIT_ENTER (1)
#define MENU_EXIT_ESCAPE (2)
#define MENU_EXIT_DETAILS (3)
#define MENU_EXIT_TIMEOUT (4)
#define MENU_EXIT_EJECT (5)
#define MENU_EXIT_DELETE (6)
#define MENU_EXIT_SCREENSHOT (7)
#define MENU_EXIT_SHOWSCREEN (8)

typedef struct
{
    INTN CurrentSelection, PreviousSelection, MaxIndex;
    INTN FirstVisible, LastVisible, MaxVisible;
    INTN FinalRow0, InitialRow1;
    INTN ScrollMode;
    BOOLEAN PaintAll, PaintSelection;
} SCROLL_STATE;

#define SCROLL_LINE_UP (0)
#define SCROLL_LINE_DOWN (1)
#define SCROLL_PAGE_UP (2)
#define SCROLL_PAGE_DOWN (3)
#define SCROLL_FIRST (4)
#define SCROLL_LAST (5)
#define SCROLL_NONE (6)
#define SCROLL_LINE_RIGHT (7)
#define SCROLL_LINE_LEFT (8)

#define SCROLL_MODE_TEXT (0)
#define SCROLL_MODE_ICONS (1)

#define POINTER_NO_ITEM (-1)
#define POINTER_LEFT_ARROW (-2)
#define POINTER_RIGHT_ARROW (-3)

#define INPUT_KEY (0)
#define INPUT_TIMEOUT (2)
#define INPUT_TIMER_ERROR (3)

#define TAG_NO (0)
#define TAG_YES (1)

#define MAX_LINE_LENGTH (65)

#define TRUSTED_BOOT_CONFIRM L"Confirm Trusted Boot Fix"

struct _meridian_menu_screen;

typedef VOID (*MENU_STYLE_FUNC)(IN MERIDIAN_MENU_SCREEN *Screen, IN SCROLL_STATE *State,
                                IN UINTN Function, IN CHAR16 *ParamText);

VOID GenerateWaitList(VOID);
VOID FreeBdsOption(BDS_COMMON_OPTION **BdsOption);
VOID FreeMenuScreen(IN MERIDIAN_MENU_SCREEN **Screen);
VOID FreeMenuEntry(IN OUT MERIDIAN_MENU_ENTRY **Entry);
VOID AddMenuEntry(IN MERIDIAN_MENU_SCREEN *Screen, IN MERIDIAN_MENU_ENTRY *Entry);
VOID AddMenuEntryCopy(IN MERIDIAN_MENU_SCREEN *Screen, IN MERIDIAN_MENU_ENTRY *Entry);
VOID AddSubMenuEntry(IN MERIDIAN_MENU_SCREEN *SubScreen, IN MERIDIAN_MENU_ENTRY *SubEntry);
VOID AddMenuInfoLine(IN MERIDIAN_MENU_SCREEN *Screen, IN CHAR16 *InfoLine, IN BOOLEAN CanFree);
VOID DisplaySimpleMessage(CHAR16 *Message, CHAR16 *Title OPTIONAL);
#if MERIDIAN_DEBUG > 0
VOID LogExit(IN UINTN MenuExit, IN const char FunctionName[], IN CHAR16 *ChosenOptionTitle);
#endif

UINTN ComputeRow0PosY(IN BOOLEAN ApplyOffset);
UINTN WaitForInput(IN UINTN Timeout);
UINTN AbortSyncTrust(VOID);
UINTN DrawMenuScreen(IN MERIDIAN_MENU_SCREEN *Screen, IN MENU_STYLE_FUNC StyleFunc,
                     IN OUT INTN *DefaultEntryIndex, OUT MERIDIAN_MENU_ENTRY **ChosenOption);

CHAR16 *MenuExitInfo(IN UINTN MenuExit);

BOOLEAN GetMenuEntryYesNo(IN OUT MERIDIAN_MENU_SCREEN **Screen);
BOOLEAN GetMenuEntryReturn(IN OUT MERIDIAN_MENU_SCREEN **Screen);
BOOLEAN ConfirmSyncNVram(VOID);
BOOLEAN ConfirmRotate(VOID);

BDS_COMMON_OPTION *CopyBdsOption(BDS_COMMON_OPTION *BdsOption);

#endif
