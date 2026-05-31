// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2021-2026 Dayo Akanji
// SPDX-FileCopyrightText: 2012 Roderick W. Smith
// SPDX-FileCopyrightText: 2006-2009 Christoph Pfisterer

#include "global.h"
#include "menu.h"
#include "scan.h"
#include "lib.h"
#include "apple.h"
#include "config.h"
#include "mystrings.h"
#include "screenmgt.h"
#include "sysinfo.h"
#include "version.h"

VOID DisplaySimpleMessage(CHAR16 *Message, CHAR16 *Title OPTIONAL)
{
#if MERIDIAN_DEBUG > 0
    CHAR16 *MsgStr;
    INTN MenuExit;
    BOOLEAN CheckMute = FALSE;
#endif

    INTN DefaultEntry;
    BOOLEAN RetVal;
    MENU_STYLE_FUNC Style;
    MERIDIAN_MENU_SCREEN *SimpleMessageMenu;
    MERIDIAN_MENU_ENTRY *ChosenOption;

    if (Message == NULL) {

        return;
    }

    SimpleMessageMenu = AllocateZeroPool(sizeof(MERIDIAN_MENU_SCREEN));
    if (SimpleMessageMenu == NULL) {

        return;
    }

    if (Title == NULL) {
        Title = L"Information";
    }

    SimpleMessageMenu->Title = StrDuplicate(Title);
    SimpleMessageMenu->Hint1 = StrDuplicate(L"Press 'Enter' to Return to Main Menu");
    SimpleMessageMenu->Hint2 = StrDuplicate(L"");

#if MERIDIAN_DEBUG > 0
    MsgStr = PoolPrint(L"DisplaySimpleMessage:- '%s ::: %s'", Title, Message);
    INFO_LOG("INFO: %s", MsgStr);
    INFO_LOG("\n\n");
    DEBUG_LOG(1, LOG_THREE_STAR_MID, L"%s", MsgStr);
    MRD_FREE_POOL(MsgStr);

    MRD_MUTELOGGER_SET;
#endif
    AddMenuInfoLine(SimpleMessageMenu, Message, FALSE);
#if MERIDIAN_DEBUG > 0
    MRD_MUTELOGGER_OFF;
#endif

    RetVal = GetMenuEntryReturn(&SimpleMessageMenu);
    if (!RetVal) {
        FreeMenuScreen(&SimpleMessageMenu);

        return;
    }

    DefaultEntry = 9999;
    Style = NULL;

#if MERIDIAN_DEBUG > 0

    MenuExit =
#endif
        DrawMenuScreen(SimpleMessageMenu, Style, &DefaultEntry, &ChosenOption);

#if MERIDIAN_DEBUG > 0
    LogExit(MenuExit, __func__, Title);
#endif

    FreeMenuScreen(&SimpleMessageMenu);
}

UINTN AbortSyncTrust(VOID)
{
    INTN DefaultEntry;
    UINTN MenuExit;
    BOOLEAN RetVal;
    MENU_STYLE_FUNC Style;
    MERIDIAN_MENU_ENTRY *ChosenOption;
    MERIDIAN_MENU_SCREEN *AbortSyncTrustMenu;

    AbortSyncTrustMenu = AllocateZeroPool(sizeof(MERIDIAN_MENU_SCREEN));
    if (AbortSyncTrustMenu == NULL) {

        return SYNC_TRUST_HALT;
    }

    AbortSyncTrustMenu->Title = StrDuplicate(TRUSTED_BOOT_CONFIRM);
    AbortSyncTrustMenu->Hint1 = StrDuplicate(SELECT_OPTION_HINT);
    AbortSyncTrustMenu->Hint2 = StrDuplicate(RETURN_MAIN_SCREEN_HINT);

    AddMenuInfoLine(AbortSyncTrustMenu,
                    L"The boot \"Chain of Trust\" may be servered on booting with 3rd-party tools.",
                    FALSE);
    AddMenuInfoLine(AbortSyncTrustMenu,
                    L"This typically affects units with T2/TPM chips after an SMC/Similar reset.",
                    FALSE);
    AddMenuInfoLine(AbortSyncTrustMenu, L"", FALSE);
    AddMenuInfoLine(AbortSyncTrustMenu,
                    L"When manifested, the machine will fail to boot and/or become unresponsive.",
                    FALSE);
    AddMenuInfoLine(AbortSyncTrustMenu, L"", FALSE);
    AddMenuInfoLine(AbortSyncTrustMenu,
                    L"Meridian will start a native reboot into your target to avoid the issue.",
                    FALSE);
    AddMenuInfoLine(AbortSyncTrustMenu,
                    L"Would you prefer Meridian to load your selected target directly instead?",
                    FALSE);
    AddMenuInfoLine(AbortSyncTrustMenu, L"", FALSE);

    RetVal = GetMenuEntryYesNo(&AbortSyncTrustMenu);
    if (!RetVal) {
        FreeMenuScreen(&AbortSyncTrustMenu);

        return SYNC_TRUST_HALT;
    }

    DefaultEntry = 9999;
    Style = NULL;
    MenuExit = DrawMenuScreen(AbortSyncTrustMenu, Style, &DefaultEntry, &ChosenOption);

#if MERIDIAN_DEBUG > 0
    LogExit(MenuExit, __func__, ChosenOption->Title);
#endif

    if (MenuExit == MENU_EXIT_ESCAPE) {
        RetVal = SYNC_TRUST_EXIT;
    }
    else if (MenuExit == MENU_EXIT_ENTER && MrdStrEqualsCI(ChosenOption->Title, L"Yes")) {
        RetVal = SYNC_TRUST_SKIP;
    }
    else {
        RetVal = SYNC_TRUST_BOOT;
    }

    FreeMenuScreen(&AbortSyncTrustMenu);

    return RetVal;
}

BOOLEAN ConfirmSyncNVram(VOID)
{
    INTN DefaultEntry;
    UINTN MenuExit;
    BOOLEAN RetVal;
    MENU_STYLE_FUNC Style;
    MERIDIAN_MENU_ENTRY *ChosenOption;
    MERIDIAN_MENU_SCREEN *ConfirmSyncNVramMenu;

    ConfirmSyncNVramMenu = AllocateZeroPool(sizeof(MERIDIAN_MENU_SCREEN));
    if (ConfirmSyncNVramMenu == NULL) {

        return FALSE;
    }

    ConfirmSyncNVramMenu->Title = StrDuplicate(L"Confirm nvRAM Sync");
    ConfirmSyncNVramMenu->Hint1 = StrDuplicate(SELECT_OPTION_HINT);
    ConfirmSyncNVramMenu->Hint2 = StrDuplicate(RETURN_MAIN_SCREEN_HINT);

    AddMenuInfoLine(ConfirmSyncNVramMenu, L"Sync Misc nvRAM Entries?", FALSE);
    AddMenuInfoLine(ConfirmSyncNVramMenu, L"", FALSE);

    RetVal = GetMenuEntryYesNo(&ConfirmSyncNVramMenu);
    if (!RetVal) {
        FreeMenuScreen(&ConfirmSyncNVramMenu);

        return FALSE;
    }

    DefaultEntry = 9999;
    Style = NULL;
    MenuExit = DrawMenuScreen(ConfirmSyncNVramMenu, Style, &DefaultEntry, &ChosenOption);

#if MERIDIAN_DEBUG > 0
    LogExit(MenuExit, __func__, ChosenOption->Title);
#endif

    if (MenuExit == MENU_EXIT_ENTER && MrdStrEqualsCI(ChosenOption->Title, L"Yes")) {
        RetVal = TRUE;
    }
    else {
        RetVal = FALSE;
    }

    FreeMenuScreen(&ConfirmSyncNVramMenu);

    return RetVal;
}

BOOLEAN ConfirmRotate(VOID)
{
    UINT32 CurrentCsr;
    UINT32 TargetCsr;
    UINT32 TempCsr;
    CHAR16 *TmpStrA;
    CHAR16 *TmpStrB;
    INTN DefaultEntry;
    UINTN MenuExit;
    BOOLEAN EmptySIP;
    BOOLEAN RetVal;
    UINT32_LIST *ListItem;
    MENU_STYLE_FUNC Style;
    MERIDIAN_MENU_ENTRY *ChosenOption;
    MERIDIAN_MENU_SCREEN *ConfirmRotateMenu;

#if MERIDIAN_DEBUG > 0
    DEBUG_LOG(1, LOG_LINE_THIN_SEP, L"Prepare Menu Screen");
    DEBUG_LOG(1, LOG_LINE_NORMAL, L"Screen Title:- 'Confirm CSR Rotation'");
#endif

    if (GlobalConfig.CsrValues == NULL) {

        return FALSE;
    }

    ConfirmRotateMenu = AllocateZeroPool(sizeof(MERIDIAN_MENU_SCREEN));
    if (ConfirmRotateMenu == NULL) {

        return TRUE;
    }

    /* coverity[check_return: SUPPRESS] */
    GetCsrStatus(&CurrentCsr);
    RecordgCsrStatus(CurrentCsr, FALSE);
    TmpStrA = PoolPrint(L"From : %s", gCsrStatus);
    EmptySIP = (CurrentCsr == SIP_ENABLED_EX) ? TRUE : FALSE;

    ListItem = GlobalConfig.CsrValues;
    if (EmptySIP) {

        TempCsr = GlobalConfig.CsrValues->Value;
    }
    else {
        while ((ListItem != NULL) && (ListItem->Value != CurrentCsr)) {
            ListItem = ListItem->Next;
        }
    }

    TargetCsr = (ListItem == NULL || ListItem->Next == NULL) ? GlobalConfig.CsrValues->Value
                                                             : ListItem->Next->Value;

    RecordgCsrStatus(TargetCsr, FALSE);

    TmpStrB = PoolPrint(L"To   : %s", gCsrStatus);

    RecordgCsrStatus(CurrentCsr, FALSE);

    ConfirmRotateMenu->Title = StrDuplicate(L"Confirm CSR Rotation");
    ConfirmRotateMenu->Hint1 = StrDuplicate(SELECT_OPTION_HINT);
    ConfirmRotateMenu->Hint2 = StrDuplicate(RETURN_MAIN_SCREEN_HINT);

    AddMenuInfoLine(ConfirmRotateMenu, TmpStrA, FALSE);
    AddMenuInfoLine(ConfirmRotateMenu, TmpStrB, FALSE);
    AddMenuInfoLine(ConfirmRotateMenu, L"", FALSE);

    MRD_FREE_POOL(TmpStrA);
    MRD_FREE_POOL(TmpStrB);

    RetVal = GetMenuEntryYesNo(&ConfirmRotateMenu);
    if (!RetVal) {
        FreeMenuScreen(&ConfirmRotateMenu);

        return FALSE;
    }

    DefaultEntry = 9999;
    Style = NULL;
    MenuExit = DrawMenuScreen(ConfirmRotateMenu, Style, &DefaultEntry, &ChosenOption);

#if MERIDIAN_DEBUG > 0
    LogExit(MenuExit, __func__, ChosenOption->Title);
#endif

    if (MenuExit != MENU_EXIT_ENTER || !MrdStrEqualsCI(ChosenOption->Title, L"Yes")) {
        RetVal = FALSE;
    }
    else {
        RetVal = TRUE;

        if (EmptySIP) {

            EfivarSetRaw(&AppleBootGuid, L"csr-active-config", &TempCsr, sizeof(UINT32), TRUE);
        }
    }

    FreeMenuScreen(&ConfirmRotateMenu);

    return RetVal;
}

BOOLEAN GetMenuEntryYesNo(IN OUT MERIDIAN_MENU_SCREEN **Screen)
{
    MERIDIAN_MENU_ENTRY *MenuEntryYes;
    MERIDIAN_MENU_ENTRY *MenuEntryNo;

    if (Screen == NULL || *Screen == NULL) {

        return FALSE;
    }

    MenuEntryYes = AllocateZeroPool(sizeof(MERIDIAN_MENU_ENTRY));
    if (MenuEntryYes == NULL) {

        return FALSE;
    }

    MenuEntryYes->Title = StrDuplicate(L"Yes");
    MenuEntryYes->Tag = TAG_YES;
    AddMenuEntry(*Screen, MenuEntryYes);

    MenuEntryNo = AllocateZeroPool(sizeof(MERIDIAN_MENU_ENTRY));
    if (MenuEntryNo == NULL) {
        FreeMenuEntry((MERIDIAN_MENU_ENTRY **)MenuEntryYes);

        return FALSE;
    }

    MenuEntryNo->Title = StrDuplicate(L"No");
    MenuEntryNo->Tag = TAG_NO;
    AddMenuEntry(*Screen, MenuEntryNo);

    return TRUE;
}
