// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2020-2025 Dayo Akanji
// SPDX-FileCopyrightText: 2006-2009 Christoph Pfisterer

#ifndef __SCREEN_H_
#define __SCREEN_H_

#include "tiano_includes.h"

#include "display.h"

#define DONT_CHANGE_TEXT_MODE 1024

#define ATTR_BASIC (EFI_LIGHTGRAY | EFI_BACKGROUND_BLACK)
#define ATTR_ERROR (EFI_YELLOW | EFI_BACKGROUND_BLACK)
#define ATTR_CHOICE_BASIC ATTR_BASIC
#define ATTR_CHOICE_CURRENT (EFI_WHITE | EFI_BACKGROUND_GREEN)
#define ATTR_SCROLLARROW (EFI_LIGHTGREEN | EFI_BACKGROUND_BLACK)

#define LAYOUT_BANNER_YGAP 32

#define CENTER (0)
#define BOTTOM (1)
#define TOP (2)
#define NEXTLINE (3)

extern UINTN ConWidth;
extern UINTN ConHeight;
extern UINTN ScreenW;
extern UINTN ScreenH;
extern BOOLEAN AllowGraphicsMode;

EFI_STATUS SwitchToGraphics(VOID);

BOOLEAN ReadAllKeyStrokes(VOID);
BOOLEAN CheckError(IN EFI_STATUS Status, IN CHAR16 *where);
BOOLEAN CheckFatalError(IN EFI_STATUS Status, IN CHAR16 *where);

VOID InitScreen(VOID);
VOID SetupScreen(VOID);
VOID PauseForKey(VOID);
VOID MeridianDeadLoop(VOID);
VOID TerminateScreen(VOID);
VOID FinishExternalScreen(VOID);
VOID HaltSeconds(UINTN Seconds);
VOID PauseSeconds(UINTN Seconds);
VOID BeginTextScreen(IN CHAR16 *Title);
VOID BltClearScreen(IN BOOLEAN ShowBanner);
VOID SwitchToText(IN BOOLEAN CursorEnabled);
VOID FinishTextScreen(IN BOOLEAN WaitAlways);
VOID SwitchToGraphicsAndClear(IN BOOLEAN ShowBanner);
VOID BeginExternalScreen(IN BOOLEAN UseGraphicsMode, IN CHAR16 *Title);
VOID PrintUglyText(IN CHAR16 *Text, IN UINTN PositionCode);
VOID PrintUglyTextMuted(IN CHAR16 *Text, IN UINTN PositionCode, IN UINTN Attribute);
VOID PrintUglyError(IN CHAR16 *Text, IN UINTN PositionCode);
#endif
