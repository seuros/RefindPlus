// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2020-2026 Dayo Akanji
// SPDX-FileCopyrightText: 2021 Joe van Tunen
// SPDX-FileCopyrightText: 2012-2021 Roderick W. Smith
// SPDX-FileCopyrightText: 2006 Christoph Pfisterer

#include "global.h"
#include "screenmgt.h"
#include "config.h"
#include "lib.h"
#include "menu.h"
#include "mystrings.h"
#include "conn_bridge.h"

UINTN ConWidth = 80;
UINTN ConHeight = 25;

UINTN ScreenW = 0;
UINTN ScreenH = 0;
UINTN ScreenLongest = 0;
UINTN ScreenShortest = 0;

BOOLEAN GraphicsScreenDirty = FALSE;
BOOLEAN AllowGraphicsMode = FALSE;
BOOLEAN ClearedBuffer = FALSE;
BOOLEAN haveError = FALSE;

EG_PIXEL BlackPixel = {0x00, 0x00, 0x00, 0};

EG_PIXEL MenuBackgroundPixel = {0x00, 0x00, 0x00, 0};

extern BOOLEAN IsBoot;
extern BOOLEAN FlushFailedTag;
extern BOOLEAN UserDefinedRez;
extern EFI_GRAPHICS_OUTPUT_PROTOCOL *GOPDraw;

#if MERIDIAN_DEBUG > 0
static VOID LogClearScreen(CHAR16 *LineSpace) { INFO_LOG("%s  - Clear Screen", LineSpace); }
#endif

VOID InitScreen(VOID)
{
#if MERIDIAN_DEBUG > 1
    BOOLEAN HybridLogger = FALSE;
#endif

    LOG_SEP(L"X");
    LOG_INCREMENT();

    egInitScreen();

#if MERIDIAN_DEBUG > 1
    MRD_HYBRIDLOGGER_SET;
#endif

    if (egHasGraphicsMode()) {
#if MERIDIAN_DEBUG > 1
        DEBUG_LOG(1, LOG_THREE_STAR_MID, L"Graphics Mode Detected ... Get Resolution");
#endif

        egGetScreenSize(&ScreenW, &ScreenH);
        AllowGraphicsMode = TRUE;
    }
    else {
#if MERIDIAN_DEBUG > 1
        DEBUG_LOG(1, LOG_THREE_STAR_MID, L"Graphics Mode *NOT* Detected ... Set Text Mode");
#endif

        AllowGraphicsMode = FALSE;
        egSetTextMode(GlobalConfig.RequestedTextMode);

        egSetGraphicsModeEnabled(FALSE);
    }

#if MERIDIAN_DEBUG > 1
    MRD_HYBRIDLOGGER_OFF;
#endif

    GraphicsScreenDirty = TRUE;

    gST->ConOut->EnableCursor(gST->ConOut, FALSE);

    if (gST->ConOut->QueryMode(gST->ConOut, gST->ConOut->Mode->Mode, &ConWidth, &ConHeight) !=
        EFI_SUCCESS) {

        ConWidth = 80;
        ConHeight = 25;
    }

    if (GlobalConfig.TextOnly || !AllowGraphicsMode) {
        AllowGraphicsMode = FALSE;
    }

    LOG_DECREMENT();
    LOG_SEP(L"X");
}

VOID SetupScreen(VOID)
{
#if MERIDIAN_DEBUG > 0
    CHAR16 *MsgStr;
    CHAR16 *TmpStr;
#endif

    UINTN NewWidth;
    UINTN NewHeight;
    BOOLEAN TextOption;
    static BOOLEAN BannerLoaded = FALSE;

#if MERIDIAN_DEBUG > 0
    if (!BannerLoaded) {
        INFO_LOG("D I S P L A Y   T I T L E   B A N N E R");
        INFO_LOG("\n");
    }
#endif

    LOG_SEP(L"X");
    LOG_INCREMENT();

    if (GOPDraw == NULL) {

        UserDefinedRez = FALSE;
        GlobalConfig.RequestedScreenWidth = 0;
        GlobalConfig.RequestedScreenHeight = 0;
    }

    if (!UserDefinedRez) {
        egGetScreenSize(&ScreenW, &ScreenH);
    }

    if (GlobalConfig.RequestedScreenWidth > 0 && GlobalConfig.RequestedScreenHeight == 0) {
#if MERIDIAN_DEBUG > 0
        INFO_LOG("Get Resolution from Mode:");
        INFO_LOG("\n");
#endif

        egGetResFromMode(&(GlobalConfig.RequestedScreenWidth),
                         &(GlobalConfig.RequestedScreenHeight));
    }

    if (GlobalConfig.RequestedScreenWidth > 0 && GlobalConfig.RequestedScreenHeight > 0) {
        ScreenW = (ScreenW < GlobalConfig.RequestedScreenWidth) ? ScreenW
                                                                : GlobalConfig.RequestedScreenWidth;

        ScreenH = (ScreenH < GlobalConfig.RequestedScreenHeight)
                      ? ScreenH
                      : GlobalConfig.RequestedScreenHeight;

#if MERIDIAN_DEBUG > 0
        DEBUG_LOG(1, LOG_LINE_NORMAL, L"Recording Current Resolution as %d x %d", ScreenW, ScreenH);
#endif
    }

    if (UserDefinedRez) {

        TextOption = egSetTextMode(GlobalConfig.RequestedTextMode);
        if (TextOption) {
            egGetScreenSize(&NewWidth, &NewHeight);
            if ((NewWidth > ScreenW) || (NewHeight > ScreenH)) {
                ScreenW = NewWidth;
                ScreenH = NewHeight;
            }

            if (ScreenW > GlobalConfig.RequestedScreenWidth ||
                ScreenH > GlobalConfig.RequestedScreenHeight) {
#if MERIDIAN_DEBUG > 0
                MsgStr = L"Match Requested Resolution to Actual Resolution";
                DEBUG_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
                INFO_LOG("  - %s", MsgStr);
                INFO_LOG("\n");
#endif

                GlobalConfig.RequestedScreenWidth = ScreenW;
                GlobalConfig.RequestedScreenHeight = ScreenH;
            }
        }

        egSetScreenSize(&(GlobalConfig.RequestedScreenWidth),
                        &(GlobalConfig.RequestedScreenHeight));

        if (GlobalConfig.RequestedScreenWidth == 0 && GlobalConfig.RequestedScreenHeight == 0) {
            UserDefinedRez = FALSE;
        }
        else {
            egGetScreenSize(&ScreenW, &ScreenH);
        }
    }

    ScreenLongest = (ScreenW >= ScreenH) ? ScreenW : ScreenH;
    ScreenShortest = (ScreenW <= ScreenH) ? ScreenW : ScreenH;

    if (!AllowGraphicsMode) {
#if MERIDIAN_DEBUG > 0
        if (GlobalConfig.TextOnly) {
            MsgStr = (GlobalConfig.DirectBoot) ? L"'DirectBoot' is Active"
                                               : L"Running in Text Only Mode";
            DEBUG_LOG(1, LOG_THREE_STAR_MID, L"%s", MsgStr);
            INFO_LOG("Skip Title Banner Display ... %s", MsgStr);
        }
        else {
            MsgStr = L"Invalid Screen Mode ... Switch to Text Mode";
            DEBUG_LOG(1, LOG_THREE_STAR_MID, L"%s", MsgStr);
            INFO_LOG("WARN: %s", MsgStr);
        }
        INFO_LOG("\n\n");
#endif

        GlobalConfig.TextOnly = TRUE;
        SwitchToText(FALSE);
    }
    else {
        TextOption = (egIsGraphicsModeEnabled()) ? FALSE : TRUE;

        if (TextOption) {
#if MERIDIAN_DEBUG > 0
            MsgStr = L"Deploying Graphics Mode";
            DEBUG_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
            INFO_LOG("INFO: %s", MsgStr);
            INFO_LOG("\n\n");
#endif

            SwitchToGraphics();
        }

        if (!UserDefinedRez) {
            egSetNativeResolution();
            egGetScreenSize(&ScreenW, &ScreenH);
            ScreenLongest = (ScreenW >= ScreenH) ? ScreenW : ScreenH;
            ScreenShortest = (ScreenW <= ScreenH) ? ScreenW : ScreenH;
        }

        if (TextOption || !BannerLoaded) {
#if MERIDIAN_DEBUG > 0
            MsgStr = (TextOption) ? L"Text Screen Mode Active ... Prepare Graphics Mode Switch"
                                  : L"Graphics FX Mode Active ... Prepare Title Banner Display";
            DEBUG_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
            INFO_LOG("%s:", MsgStr);

            MsgStr = L"Display Mode Resolution";
            DEBUG_LOG(1, LOG_LINE_NORMAL, L"%s:- '%d x %d'", MsgStr, ScreenLongest, ScreenShortest);
            INFO_LOG("%s  - %s:- '%d x %d'", OffsetNext, MsgStr, ScreenLongest, ScreenShortest);

            INFO_LOG("\n\n");
#endif

            if (GlobalConfig.ScreensaverTime == -1) {
#if MERIDIAN_DEBUG > 0
                INFO_LOG("INFO: Changing to Screensaver Display");

                MsgStr = L"Configured to Start with Screensaver";
                DEBUG_LOG(1, LOG_THREE_STAR_MID, L"%s", MsgStr);
                INFO_LOG("%s      %s", OffsetNext, MsgStr);
#endif

                GraphicsScreenDirty = TRUE;
            }
            else {

                ConnBootScreen();

#if MERIDIAN_DEBUG > 0
                TmpStr = L"Title Banner Displayed";
                MsgStr = (TextOption) ? L"Graphics Mode Deployed" : TmpStr;
                DEBUG_LOG(1, LOG_THREE_STAR_MID, L"%s", MsgStr);
                INFO_LOG("INFO: %s", MsgStr);

                if (TextOption) {
                    INFO_LOG("%s      %s", OffsetNext, TmpStr);
                }
#endif
            }

#if MERIDIAN_DEBUG > 0
            if (NativeLogger) {
                INFO_LOG("\n");
            }
            else {
                INFO_LOG("\n\n");
            }
#endif

            BannerLoaded = TRUE;
        }
    }

    LOG_DECREMENT();
    LOG_SEP(L"X");
}

VOID SwitchToText(IN BOOLEAN CursorEnabled)
{
#if MERIDIAN_DEBUG > 0
    BOOLEAN TextModeOnEntry;
#endif

    EFI_STATUS Status;

    LOG_SEP(L"X");
    LOG_INCREMENT();

    egSetGraphicsModeEnabled(FALSE);
    gST->ConOut->EnableCursor(gST->ConOut, CursorEnabled);

#if MERIDIAN_DEBUG > 0
    TextModeOnEntry = (egIsGraphicsModeEnabled() && !AllowGraphicsMode && !IsBoot);

    if (TextModeOnEntry) {
        INFO_LOG("Determine Text Console Size:");
        INFO_LOG("\n");
    }
#endif

    Status = gST->ConOut->QueryMode(gST->ConOut, gST->ConOut->Mode->Mode, &ConWidth, &ConHeight);
    if (EFI_ERROR(Status)) {

        ConWidth = 80;
        ConHeight = 25;

#if MERIDIAN_DEBUG > 0
        if (TextModeOnEntry) {
            INFO_LOG("Could *NOT* Get Text Console Size ... Use Default:- '%d x %d'", ConHeight,
                     ConWidth);
        }
#endif
    }
    else {
#if MERIDIAN_DEBUG > 0
        if (TextModeOnEntry) {
            INFO_LOG("Text Console Size:- '%d x %d'", ConWidth, ConHeight);
        }
#endif
    }

#if MERIDIAN_DEBUG > 0
    if (TextModeOnEntry) {
        INFO_LOG("\n\n");
    }
#endif

#if MERIDIAN_DEBUG > 0
    if (TextModeOnEntry) {
        INFO_LOG("INFO: Switched to Text Mode");
        INFO_LOG("\n\n");
    }
#endif

    LOG_DECREMENT();
    LOG_SEP(L"X");
}

EFI_STATUS SwitchToGraphics(VOID)
{
    LOG_SEP(L"X");
    LOG_INCREMENT();

    if (!AllowGraphicsMode) {
        LOG_DECREMENT();
        LOG_SEP(L"X");
        return EFI_NOT_STARTED;
    }

    if (egIsGraphicsModeEnabled()) {
        LOG_DECREMENT();
        LOG_SEP(L"X");
        return EFI_ALREADY_STARTED;
    }

    egSetGraphicsModeEnabled(TRUE);
    GraphicsScreenDirty = TRUE;

    LOG_DECREMENT();
    LOG_SEP(L"X");
    return EFI_SUCCESS;
}

VOID BeginTextScreen(IN CHAR16 *Title)
{
    LOG_SEP(L"X");
    LOG_INCREMENT();

    SwitchToText(FALSE);
    ConnTextScreenHeader(Title);

    haveError = FALSE;

    LOG_DECREMENT();
    LOG_SEP(L"X");
}

VOID FinishTextScreen(IN BOOLEAN WaitAlways)
{
    LOG_SEP(L"X");
    LOG_INCREMENT();

    if (haveError || WaitAlways) {
        SwitchToText(FALSE);
        PauseForKey();
    }

    haveError = FALSE;

    LOG_DECREMENT();
    LOG_SEP(L"X");
}

VOID BeginExternalScreen(IN BOOLEAN UseGraphicsMode, IN CHAR16 *Title)
{
#if MERIDIAN_DEBUG > 0
    CHAR16 *MsgStr;
    BOOLEAN CheckMute = FALSE;
#endif

    LOG_SEP(L"X");
    LOG_INCREMENT();

    if (GlobalConfig.DirectBoot) {
        LOG_DECREMENT();
        LOG_SEP(L"X");

        haveError = FALSE;

        return;
    }

    if (!AllowGraphicsMode) {
        UseGraphicsMode = FALSE;
    }

    if (UseGraphicsMode) {
#if MERIDIAN_DEBUG > 0
        MsgStr = L"Begin Child Image Display with Screen Mode:- 'Graphics'";
        DEBUG_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
        if (!IsBoot) {
            INFO_LOG("%s    * %s", OffsetNext, MsgStr);
            INFO_LOG("\n\n");
        }
#endif

        SwitchToGraphicsAndClear(FALSE);
    }
    else {
#if MERIDIAN_DEBUG > 0
        MsgStr = L"Begin Child Image Display with Screen Mode:- 'Text'";
        DEBUG_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
        if (!IsBoot) {
            INFO_LOG("%s    * %s", OffsetNext, MsgStr);
        }
#endif

        SwitchToText(UseGraphicsMode);

#if MERIDIAN_DEBUG > 0
        MRD_MUTELOGGER_SET;
#endif
        ConnTextScreenHeader(Title);
#if MERIDIAN_DEBUG > 0
        MRD_MUTELOGGER_OFF;
#endif
    }

    haveError = FALSE;
    LOG_DECREMENT();
    LOG_SEP(L"X");
}

VOID FinishExternalScreen(VOID)
{
    LOG_SEP(L"X");
    LOG_INCREMENT();

    GraphicsScreenDirty = TRUE;

    if (haveError) {
        SwitchToText(FALSE);
        PauseForKey();
    }

    SetupScreen();

    haveError = FALSE;
    LOG_DECREMENT();
    LOG_SEP(L"X");
}

VOID TerminateScreen(VOID)
{

    gST->ConOut->SetAttribute(gST->ConOut, ATTR_BASIC);
    gST->ConOut->ClearScreen(gST->ConOut);

    gST->ConOut->EnableCursor(gST->ConOut, TRUE);
}

BOOLEAN ReadAllKeyStrokes(VOID)
{
#if MERIDIAN_DEBUG > 0
    CHAR16 *MsgStr;
    BOOLEAN EmptyBuffer;
#endif

    EFI_STATUS Status;
    BOOLEAN GotKeyStrokes;
    EFI_INPUT_KEY key;

    static BOOLEAN FirstCall = TRUE;

    GotKeyStrokes = FALSE;

#if MERIDIAN_DEBUG > 0
    EmptyBuffer = FALSE;
#endif

    if (FirstCall || !GlobalConfig.DirectBoot) {
        while (1) {
            Status = gST->ConIn->ReadKeyStroke(gST->ConIn, &key);
            switch (Status) {
            case EFI_SUCCESS:

                ClearedBuffer = TRUE;
                GotKeyStrokes = TRUE;

                break;
            case EFI_DEVICE_ERROR:

                gST->ConIn->Reset(gST->ConIn, FALSE);

            default:
                Status = EFI_ALREADY_STARTED;

#if MERIDIAN_DEBUG > 0
                EmptyBuffer = TRUE;
#endif
            }

            if (EFI_ERROR(Status)) {

                break;
            }
        }
    }

#if MERIDIAN_DEBUG > 0
    if (!FirstCall && GlobalConfig.DirectBoot) {
        Status = EFI_NOT_STARTED;
    }
    else if (GotKeyStrokes) {
        Status = EFI_SUCCESS;
    }
    else if (EmptyBuffer) {
        Status = EFI_ALREADY_STARTED;
    }
    else {
        if (!GlobalConfig.DirectBoot) {
            FlushFailedTag = TRUE;
        }
    }

    MsgStr = PoolPrint(L"Clear Keystroke Buffer ... %r", Status);
    INFO_LOG("INFO: %s", MsgStr);
    INFO_LOG("\n\n");
    DEBUG_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
    MRD_FREE_POOL(MsgStr);
#endif

    FirstCall = FALSE;

    return GotKeyStrokes;
}

static VOID PauseBreakKey(BOOLEAN *Breakout)
{
#if MERIDIAN_DEBUG > 0
    CONST CHAR16 *Msg = L"Pause Terminated by Keypress";
    DEBUG_LOG(1, LOG_LINE_NORMAL, L"%s", Msg);
    INFO_LOG("%s      * %s", OffsetNext, Msg);
    INFO_LOG("\n\n");
#endif
    *Breakout = TRUE;
}

static VOID PauseBreakTimerError(BOOLEAN *Breakout)
{
#if MERIDIAN_DEBUG > 0
    CONST CHAR16 *Msg = L"Pause Terminated on Timer Error";
    DEBUG_LOG(1, LOG_LINE_NORMAL, L"%s!!", Msg);
    INFO_LOG("%s      * %s", OffsetNext, Msg);
    INFO_LOG("\n\n");
#endif
    *Breakout = TRUE;
}

VOID PrintUglyText(IN CHAR16 *Text, IN UINTN PositionCode)
{
    (VOID) PositionCode;

    if (Text != NULL) {
        Print(Text);
        Print(L"\n");
    }
}

VOID PrintUglyTextMuted(IN CHAR16 *Text, IN UINTN PositionCode, IN UINTN Attribute)
{
#if MERIDIAN_DEBUG > 0
    BOOLEAN CheckMute = FALSE;

    MRD_MUTELOGGER_SET;
#endif
    gST->ConOut->SetAttribute(gST->ConOut, Attribute);
    PrintUglyText(Text, PositionCode);
    gST->ConOut->SetAttribute(gST->ConOut, ATTR_BASIC);
#if MERIDIAN_DEBUG > 0
    MRD_MUTELOGGER_OFF;
#endif
}

VOID PrintUglyError(IN CHAR16 *Text, IN UINTN PositionCode)
{
    PrintUglyTextMuted(Text, PositionCode, ATTR_ERROR);
}

VOID PauseForKey(VOID)
{
#if MERIDIAN_DEBUG > 0
    CHAR16 *MsgStr;
    BOOLEAN CheckMute = FALSE;
#endif

    UINTN i, WaitOut;
    BOOLEAN Breakout = FALSE;

#if MERIDIAN_DEBUG > 0
    MRD_MUTELOGGER_SET;
#endif
    PrintUglyText(L"", NEXTLINE);
    PrintUglyText(L"", NEXTLINE);
    PrintUglyText(L"                                                          ", NEXTLINE);
    PrintUglyText(L"                                                          ", NEXTLINE);
    PrintUglyText(L"             * Paused for Error or Warning *              ", NEXTLINE);
    (GlobalConfig.ContinueOnWarning)
        ? PrintUglyText(L"        Press a Key or Wait 9 Seconds to Continue         ", NEXTLINE)
        : PrintUglyText(L"                 Press a Key to Continue                  ", NEXTLINE);
    PrintUglyText(L"                                                          ", NEXTLINE);
    PrintUglyText(L"                                                          ", NEXTLINE);

    ReadAllKeyStrokes();
#if MERIDIAN_DEBUG > 0
    MRD_MUTELOGGER_OFF;
#endif

    if (GlobalConfig.ContinueOnWarning) {
#if MERIDIAN_DEBUG > 0
        MsgStr = L"Paused for Error/Warning ... Waiting 9 Seconds";
        DEBUG_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
        INFO_LOG("INFO: %s", MsgStr);
#endif

        for (i = 0; i < 9; ++i) {
            WaitOut = WaitForInput(1000);
            if (WaitOut == INPUT_KEY) {
                PauseBreakKey(&Breakout);
            }
            else if (WaitOut == INPUT_TIMER_ERROR) {
                PauseBreakTimerError(&Breakout);
            }

            if (Breakout) {
                break;
            }
        }

#if MERIDIAN_DEBUG > 0
        if (!Breakout) {
            MsgStr = L"Pause Terminated on Timeout";
            DEBUG_LOG(1, LOG_LINE_NORMAL, L"%s!!", MsgStr);
            INFO_LOG("%s      * %s", OffsetNext, MsgStr);
            INFO_LOG("\n\n");
        }
#endif
    }
    else {
#if MERIDIAN_DEBUG > 0
        MsgStr = L"Paused for Error/Warning ... Keypress Required";
        DEBUG_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
        INFO_LOG("INFO: %s", MsgStr);
#endif

        while (1) {
            WaitOut = WaitForInput(1000);
            if (WaitOut == INPUT_KEY) {
                PauseBreakKey(&Breakout);
            }
            else if (WaitOut == INPUT_TIMER_ERROR) {
                PauseBreakTimerError(&Breakout);
            }

            if (Breakout) {
                break;
            }
        }
    }

    GraphicsScreenDirty = TRUE;

#if MERIDIAN_DEBUG > 0
    DEBUG_LOG(1, LOG_LINE_THIN_SEP, L"Resuming After Pause");
#endif
}

VOID PauseSeconds(UINTN Seconds)
{
#if MERIDIAN_DEBUG > 0
    BOOLEAN CheckMute = FALSE;
#endif

    UINTN i, WaitOut;
    BOOLEAN Breakout = FALSE;

#if MERIDIAN_DEBUG > 0
    MRD_MUTELOGGER_SET;
#endif

    ReadAllKeyStrokes();
#if MERIDIAN_DEBUG > 0
    MRD_MUTELOGGER_OFF;
#endif

#if MERIDIAN_DEBUG > 0
    DEBUG_LOG(1, LOG_THREE_STAR_MID, L"Pausing for %d Seconds", Seconds);
#endif

    for (i = 0; i < Seconds; ++i) {
        WaitOut = WaitForInput(1000);
        if (WaitOut == INPUT_KEY) {
#if MERIDIAN_DEBUG > 0
            DEBUG_LOG(1, LOG_LINE_NORMAL, L"Pause Terminated by Keypress");
#endif

            Breakout = TRUE;
        }
        else if (WaitOut == INPUT_TIMER_ERROR) {
#if MERIDIAN_DEBUG > 0
            DEBUG_LOG(1, LOG_LINE_NORMAL, L"Pause Terminated on Timer Error!!");
#endif

            Breakout = TRUE;
        }

        if (Breakout) {
            break;
        }
    }

#if MERIDIAN_DEBUG > 0
    if (!Breakout) {
        DEBUG_LOG(1, LOG_LINE_NORMAL, L"Pause Terminated on Timeout");
    }
#endif

#if MERIDIAN_DEBUG > 0
    MRD_MUTELOGGER_SET;
#endif

    ReadAllKeyStrokes();
#if MERIDIAN_DEBUG > 0
    MRD_MUTELOGGER_OFF;
#endif
}

VOID HaltSeconds(UINTN Seconds)
{
#if MERIDIAN_DEBUG > 0
    BOOLEAN CheckMute = FALSE;
#endif

    UINTN i;

#if MERIDIAN_DEBUG > 0
    DEBUG_LOG(1, LOG_THREE_STAR_MID, L"Halting for %d Seconds", Seconds);
#endif

    for (i = 0; i < Seconds; ++i) {

        MeridianStall(100);
    }

#if MERIDIAN_DEBUG > 0
    DEBUG_LOG(1, LOG_LINE_NORMAL, L"Halt Terminated on Timeout");
#endif

#if MERIDIAN_DEBUG > 0
    MRD_MUTELOGGER_SET;
#endif

    ReadAllKeyStrokes();
#if MERIDIAN_DEBUG > 0
    MRD_MUTELOGGER_OFF;
#endif
}

VOID MeridianDeadLoop(VOID)
{
#if MERIDIAN_DEBUG > 0
    BOOLEAN CheckMute = FALSE;
#endif

    UINTN index;

    while (1) {
#if MERIDIAN_DEBUG > 0
        MRD_MUTELOGGER_SET;
#endif
        ReadAllKeyStrokes();

        gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, &index);
#if MERIDIAN_DEBUG > 0
        MRD_MUTELOGGER_OFF;
#endif
    }
}

BOOLEAN CheckFatalError(IN EFI_STATUS Status, IN CHAR16 *where)
{
    CHAR16 *Temp;

    if (!EFI_ERROR(Status)) {
        return FALSE;
    }

    Temp = PoolPrint(L"Fatal Error: '%r' %s", Status, where);

#if MERIDIAN_DEBUG > 0
    INFO_LOG("** FATAL ERROR: '%r' %s", Status, where);
    INFO_LOG("\n");
#endif

    PrintUglyError(Temp, NEXTLINE);
    haveError = TRUE;

#if MERIDIAN_DEBUG > 0
    DEBUG_LOG(1, LOG_STAR_SEPARATOR, Temp);
#endif

    MRD_FREE_POOL(Temp);

    return TRUE;
}

BOOLEAN CheckError(IN EFI_STATUS Status, IN CHAR16 *where)
{
    CHAR16 *Temp;

    if (!EFI_ERROR(Status)) {

        return FALSE;
    }

#if MERIDIAN_DEBUG > 0
    INFO_LOG("\n\n");
#endif

    Temp = PoolPrint(L"Error: '%r' %s", Status, where);

#if MERIDIAN_DEBUG > 0
    INFO_LOG("** WARN: '%r' %s", Status, where);
#endif

#if MERIDIAN_DEBUG > 0
    INFO_LOG("\n\n");
    DEBUG_LOG(1, LOG_STAR_SEPARATOR, Temp);
#endif

    PrintUglyError(Temp, NEXTLINE);

    PauseSeconds(6);

    haveError = (MrdStrFind(where, L"While Reading Boot Sector")) ? FALSE : TRUE;
    haveError = (!haveError && (Status == EFI_VOLUME_FULL)) ? TRUE : FALSE;

    MRD_FREE_POOL(Temp);

    return haveError;
}

VOID SwitchToGraphicsAndClear(IN BOOLEAN ShowBanner)
{
    LOG_SEP(L"X");
    LOG_INCREMENT();

    SwitchToGraphics();

    if (GraphicsScreenDirty) {
        BltClearScreen(ShowBanner);
    }

    LOG_DECREMENT();
    LOG_SEP(L"X");
}

VOID BltClearScreen(BOOLEAN ShowBanner)
{
#if MERIDIAN_DEBUG > 0
    CHAR16 *LineSpace;
#endif

    EG_PIXEL *ClearColor;

    LOG_SEP(L"X");
    LOG_INCREMENT();

    (VOID) ShowBanner;

#if MERIDIAN_DEBUG > 0
    if (!IsBoot) {
        INFO_LOG("Refresh Screen:");
        BRK_MAX("\n");
    }

#if MERIDIAN_DEBUG < 2
    LineSpace = OffsetNext;
#else
    LineSpace = L"";
#endif
#endif

    if (GlobalConfig.DirectBoot || GlobalConfig.ScreensaverTime == -1) {
        MenuBackgroundPixel = BlackPixel;
    }
    else if (!GlobalConfig.CustomScreenBG) {
        MenuBackgroundPixel = BlackPixel;
    }
    else {
        MenuBackgroundPixel.r = GlobalConfig.ScreenR;
        MenuBackgroundPixel.g = GlobalConfig.ScreenG;
        MenuBackgroundPixel.b = GlobalConfig.ScreenB;
        MenuBackgroundPixel.a = 0;
    }

    ClearColor = (GlobalConfig.DirectBoot || GlobalConfig.ScreensaverTime == -1)
                     ? &BlackPixel
                     : &MenuBackgroundPixel;

#if MERIDIAN_DEBUG > 0
    if (!IsBoot) {
        LogClearScreen(LineSpace);
    }
#endif

    egClearScreen(ClearColor);

    GraphicsScreenDirty = FALSE;

    LOG_DECREMENT();
    LOG_SEP(L"X");
}
