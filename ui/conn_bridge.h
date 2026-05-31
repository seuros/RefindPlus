// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#ifndef __CONN_BRIDGE_H_
#define __CONN_BRIDGE_H_

#include "global.h"

BOOLEAN ConnPreviewRequested(VOID);

VOID ConnPreviewShow(IN MERIDIAN_MENU_SCREEN *Menu);

BOOLEAN ConnMenuRequested(VOID);

UINTN ConnRunMainMenu(IN MERIDIAN_MENU_SCREEN *Menu, IN OUT CHAR16 **SelectionName,
                      OUT MERIDIAN_MENU_ENTRY **ChosenEntry);

VOID ConnResetTransition(BOOLEAN IsRestart);

VOID ConnBootScreen(VOID);

VOID ConnLaunchSplash(IN CHAR8 OSType, IN CONST CHAR16 *Hint);

VOID ConnTextScreenHeader(IN CHAR16 *Title);

BOOLEAN ConnScreensActive(MERIDIAN_MENU_SCREEN *Screen);

UINTN ConnRunSubScreen(IN MERIDIAN_MENU_SCREEN *Screen, IN OUT INTN *DefaultEntryIndex,
                       OUT MERIDIAN_MENU_ENTRY **ChosenEntry);

UINTN ConnRunInfoScreen(IN MERIDIAN_MENU_SCREEN *Screen, IN OUT INTN *DefaultEntryIndex,
                        OUT MERIDIAN_MENU_ENTRY **ChosenEntry);

#endif
