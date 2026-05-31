// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2020-2025 Dayo Akanji
// SPDX-FileCopyrightText: 2011 Slice (Clover)

#include "tiano_includes.h"
#include "global.h"
#include "MemLogLib.h"

#include <Protocol/SerialIo.h>
#include <Library/UefiBootServicesTableLib.h>

#if MERIDIAN_DEBUG > 0

#include <Protocol/SimpleFileSystem.h>
#include <Protocol/LoadedImage.h>
#include <Guid/FileInfo.h>
#include "lib.h"
#include "screenmgt.h"
#include "mystrings.h"

#define LibOpenRoot EfiLibOpenRoot

extern EFI_GUID gEfiMiscSubClassGuid;

extern INT16 NowYear;
extern INT16 NowMonth;
extern INT16 NowDay;
extern INT16 NowHour;
extern INT16 NowMinute;
extern INT16 NowSecond;

CHAR16 *PadStr = NULL;
CHAR16 *gLogTemp = NULL;
CHAR16 *mDebugLog = NULL;

BOOLEAN TimeStamp = TRUE;
BOOLEAN UseMsgLog = FALSE;

EFI_FILE_PROTOCOL *mRootDir = NULL;

static CHAR16 *GetAltMonth(VOID)
{
    CHAR16 *AltMonth;

    switch (NowMonth) {
    case 1:
        AltMonth = L"b";
        break;
    case 2:
        AltMonth = L"c";
        break;
    case 3:
        AltMonth = L"f";
        break;
    case 4:
        AltMonth = L"h";
        break;
    case 5:
        AltMonth = L"j";
        break;
    case 6:
        AltMonth = L"k";
        break;
    case 7:
        AltMonth = L"n";
        break;
    case 8:
        AltMonth = L"p";
        break;
    case 9:
        AltMonth = L"r";
        break;
    case 10:
        AltMonth = L"t";
        break;
    case 11:
        AltMonth = L"v";
        break;
    default:
        AltMonth = L"x";
    }

    return AltMonth;
}

static CHAR16 *GetAltHour(VOID)
{
    CHAR16 *AltHour;

    switch (NowHour) {
    case 0:
        AltHour = L"a";
        break;
    case 1:
        AltHour = L"b";
        break;
    case 2:
        AltHour = L"c";
        break;
    case 3:
        AltHour = L"d";
        break;
    case 4:
        AltHour = L"e";
        break;
    case 5:
        AltHour = L"f";
        break;
    case 6:
        AltHour = L"g";
        break;
    case 7:
        AltHour = L"h";
        break;
    case 8:
        AltHour = L"i";
        break;
    case 9:
        AltHour = L"j";
        break;
    case 10:
        AltHour = L"k";
        break;
    case 11:
        AltHour = L"m";
        break;
    case 12:
        AltHour = L"n";
        break;
    case 13:
        AltHour = L"p";
        break;
    case 14:
        AltHour = L"q";
        break;
    case 15:
        AltHour = L"r";
        break;
    case 16:
        AltHour = L"s";
        break;
    case 17:
        AltHour = L"t";
        break;
    case 18:
        AltHour = L"u";
        break;
    case 19:
        AltHour = L"v";
        break;
    case 20:
        AltHour = L"w";
        break;
    case 21:
        AltHour = L"x";
        break;
    case 22:
        AltHour = L"y";
        break;
    default:
        AltHour = L"z";
    }

    return AltHour;
}

static CHAR16 *GetDateString(VOID)
{
    INT16 ourYear;
    CHAR16 *ourMonth;
    CHAR16 *ourHour;

    static CHAR16 *DateStr = NULL;

    if (DateStr != NULL) {
        return DateStr;
    }

    ourYear = (NowYear % 100);
    ourMonth = GetAltMonth();
    ourHour = GetAltHour();

    DateStr = PoolPrint(L"%02d%s%02d%s%02d%02d", ourYear, ourMonth, NowDay, ourHour, NowMinute,
                        NowSecond);

    return DateStr;
}

static EFI_FILE_PROTOCOL *OpenLogFile(VOID)
{
    EFI_STATUS Status;
    CHAR16 *DateStr;
    EFI_FILE_PROTOCOL *LogProtocol;

    if (mRootDir == NULL) {
        return NULL;
    }

    if (mDebugLog == NULL) {
        DateStr = GetDateString();

        mDebugLog = PoolPrint(L"%s.log", DateStr);
        MRD_FREE_POOL(DateStr);
    }

    Status = mRootDir->Open(mRootDir, &LogProtocol, mDebugLog, MeridianReadWrite, 0);
    if (Status == EFI_NOT_FOUND) {

        mRootDir->Open(mRootDir, &LogProtocol, mDebugLog, MeridianReadWriteCreate, 0);
    }

    return LogProtocol;
}

static EFI_STATUS HandleDir(EFI_FILE_PROTOCOL *Entity OPTIONAL)
{
    EFI_STATUS Status;

    Status = mRootDir->Close(mRootDir);
    if (Entity == NULL) {
        Status = EFI_NOT_READY;
    }

    return Status;
}

static EFI_FILE_PROTOCOL *GetDebugLogFile(VOID)
{
    EFI_STATUS Status;
    EFI_FILE_PROTOCOL *LogProtocol;
    EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;

    Status = gBS->HandleProtocol(gImageHandle, &gEfiLoadedImageProtocolGuid, (VOID **)&LoadedImage);
    if (EFI_ERROR(Status) || LoadedImage->DeviceHandle == NULL) {
        return NULL;
    }

    mRootDir = LibOpenRoot(LoadedImage->DeviceHandle);
    if (mRootDir != NULL) {
        LogProtocol = OpenLogFile();
        Status = HandleDir(LogProtocol);
    }
    else {
        Status = EFI_NOT_READY;

        gST->ConOut->SetAttribute(gST->ConOut, ATTR_ERROR);
        PrintUglyText(L"Default ESP for Meridian Debug Log Storage:- 'Not Ready'", NEXTLINE);

        gST->ConOut->SetAttribute(gST->ConOut, ATTR_BASIC);
        PrintUglyText(L"Meridian will now try other ESPs ... if available'", NEXTLINE);
        PrintUglyText(L"Debug log file, if created, *WILL NOT* be in the default ESP'", NEXTLINE);

        PauseSeconds(4);
    }

    if (EFI_ERROR(Status)) {

        mRootDir = NULL;
        Status = MrdFindESP(&mRootDir);
        if (!EFI_ERROR(Status)) {
            LogProtocol = OpenLogFile();
            Status = HandleDir(LogProtocol);
        }

        if (EFI_ERROR(Status)) {
            mRootDir = LogProtocol = NULL;

            gST->ConOut->SetAttribute(gST->ConOut, ATTR_ERROR);
            PrintUglyText(L"Alernative ESP for Meridian Debug Log Storage:- 'Not Ready'", NEXTLINE);

            gST->ConOut->SetAttribute(gST->ConOut, ATTR_BASIC);
            PrintUglyText(L"Meridian *WILL NOT* create a debug log file", NEXTLINE);

            PauseSeconds(4);
        }
    }

    return LogProtocol;
}

static VOID SaveMessageToDebugLogFile(IN CHAR8 *LastMessage)
{
    UINTN TextLen;
    CHAR8 *Text;
    EFI_FILE_INFO *Info;
    EFI_FILE_HANDLE LogFile;

    static BOOLEAN FirstTimeSave = FALSE;

    LogFile = GetDebugLogFile();
    if (LogFile == NULL) {
        return;
    }

    Info = EfiLibFileInfo(LogFile);
    if (Info) {

        Text = (FirstTimeSave) ? GetMemLogBuffer() : LastMessage;
        TextLen = (FirstTimeSave) ? GetMemLogLen() : AsciiStrLen(LastMessage);

        LogFile->SetPosition(LogFile, Info->FileSize);

        LogFile->Write(LogFile, &TextLen, Text);

        FirstTimeSave = FALSE;
    }

    LogFile->Close(LogFile);
}

VOID WayPointer(IN CHAR16 *Msg)
{

    if (gKernelStarted) {
        return;
    }

    if (Msg == NULL) {
        return;
    }

    gLogTemp = StrDuplicate(Msg);
    DeepLoggger(1, LOG_LINE_BASE, &gLogTemp);
    DEBUG_LOG(1, LOG_BLANK_LINE_SEP, L"X");
}

VOID DeepLoggger(IN INTN level, IN INTN type, IN CHAR16 **Msg)
{
    CHAR8 *FormatMsg;
    CHAR16 *Tmp;
    CHAR16 *OurPad;
#if MERIDIAN_DEBUG < 2
    UINTN Limit;
    CHAR16 *StoreMsg;
    BOOLEAN LongStr;
#endif

    (VOID) level;

    if (*Msg == NULL) {
        return;
    }

    if (type != LOG_LINE_FORENSIC && (MuteLogger || NativeLogger)) {
        MRD_FREE_POOL(*Msg);

        return;
    }

    OurPad = (PadStr != NULL) ? PadStr : L"[ ";

#if MERIDIAN_DEBUG < 2

    Limit = 426;
    LongStr = TruncateString(*Msg, Limit);

    StoreMsg = StrDuplicate(*Msg);
    MRD_FREE_POOL(*Msg);
    *Msg = (LongStr) ? PoolPrint(L"%s ... Snipped!!", StoreMsg) : StrDuplicate(StoreMsg);
    MRD_FREE_POOL(StoreMsg);
#endif

    TimeStamp = FALSE;

    switch (type) {
    case LOG_BLOCK_SEP:
    case LOG_BLANK_LINE_SEP:
        Tmp = StrDuplicate(L"\n");
        break;
    case LOG_BLANK_LINE_TWO:
        Tmp = StrDuplicate(L"\n\n");
        break;
    case LOG_STAR_SEPARATOR:
        Tmp = PoolPrint(L"\n\n* ** ** *** *** ***[ %s ]*** *** *** ** ** *\n\n", *Msg);
        break;
    case LOG_LINE_SEPARATOR:
        Tmp = PoolPrint(L"\n===================[ %s ]===================\n\n", *Msg);
        break;
    case LOG_THREE_STAR_SEP:
        Tmp = PoolPrint(L"\n*** *** --- --- ---[ %s ]--- --- --- *** ***\n", *Msg);
        break;
    case LOG_LINE_THIN_SEP:
        Tmp = PoolPrint(L"\n    ------- - - - -[ %s ]- - - - -------\n", *Msg);
        break;
    case LOG_STAR_HEAD_SEP:
        Tmp = PoolPrint(L"\n           * ** ***[ %s ]*** ** *\n", *Msg);
        break;
    case LOG_STAR_HEAD_SEPX:
        Tmp = PoolPrint(L"           * ** ***[ %s ]*** ** *\n", *Msg);
        break;
    case LOG_THREE_STAR_END:
        Tmp = PoolPrint(L"        *** *** ***[ %s ]*** *** ***\n\n", *Msg);
        break;
    case LOG_THREE_STAR_MID:
        Tmp = PoolPrint(L"                ***[ %s\n", *Msg);
        break;
    case LOG_LINE_FORENSIC:
        Tmp = PoolPrint(L"            !!! ---%s%s\n", OurPad, *Msg);
        break;
    case LOG_LINE_SPECIAL:
        Tmp = PoolPrint(L"\n                   %s", *Msg);
        break;
    case LOG_LINE_SAME:
        Tmp = PoolPrint(L"%s", *Msg);
        break;
    case LOG_LINE_EXIT:
        Tmp = PoolPrint(L"\n%s\n\n", *Msg);
        break;
    case LOG_LINE_BASE:
        Tmp = PoolPrint(L"%s\n", *Msg);
        break;
    default:
        Tmp = PoolPrint(L"%s\n", *Msg);

        TimeStamp = TRUE;
    }

    FormatMsg = AllocatePool((StrLen(Tmp) + 1) * sizeof(CHAR8));
    if (FormatMsg != NULL) {

        UseMsgLog = TRUE;

        UnicodeStrToAsciiStrS(Tmp, FormatMsg, StrLen(Tmp) + 1);
        DebugLog((const CHAR8 *)FormatMsg);
        MRD_FREE_POOL(FormatMsg);

        UseMsgLog = FALSE;
    }

    MRD_FREE_POOL(Tmp);
    MRD_FREE_POOL(*Msg);
}

VOID EFIAPI DebugLog(IN const CHAR8 *FormatString, ...)
{

    if (gKernelStarted) {
        return;
    }

    if (MuteLogger || MERIDIAN_DEBUG < 1 || FormatString == NULL) {
        return;
    }

    if (!UseMsgLog) {
        UseMsgLog = NativeLogger;
    }

    VA_LIST Marker;
    VA_START(Marker, FormatString);
    MemLogVA(TimeStamp, MERIDIAN_DEBUG, FormatString, Marker);
    VA_END(Marker);

    TimeStamp = TRUE;
}

VOID LogPadding(BOOLEAN Increment)
{
    CHAR16 *TmpPad;
    UINTN PadPos;

    if (gKernelStarted) {
        return;
    }

    if (MuteLogger) {
        return;
    }

    if (PadStr == NULL) {
        PadStr = StrDuplicate(L"[ ");

        return;
    }

    TmpPad = StrDuplicate(PadStr);
    PadPos = StrLen(PadStr);
    MRD_FREE_POOL(PadStr);

    if (Increment == TRUE) {
        PadStr = (NativeLogger) ? StrDuplicate(TmpPad) : PoolPrint(L"%s. ", TmpPad);
    }
    else {
        if (NativeLogger) {
            PadStr = StrDuplicate(TmpPad);
        }
        else {
            PadPos -= 2;
            if (PadPos < 3) {
                PadStr = StrDuplicate(L"[ ");
            }
            else {
                TmpPad[PadPos] = L'\0';
                PadStr = StrDuplicate(TmpPad);
            }
        }
    }

    MRD_FREE_POOL(TmpPad);
}

#endif

static EFI_SERIAL_IO_PROTOCOL *gMrdSerialIo = NULL;
static BOOLEAN gMrdSerialChkd = FALSE;

static VOID MirrorToSerial(IN CHAR8 *Msg)
{
    UINTN Len;

    if (Msg == NULL || !GlobalConfig.LogToSerial) {
        return;
    }
    if (!gMrdSerialChkd) {
        gMrdSerialChkd = TRUE;
        if (EFI_ERROR(
                gBS->LocateProtocol(&gEfiSerialIoProtocolGuid, NULL, (VOID **)&gMrdSerialIo))) {
            gMrdSerialIo = NULL;
        }
    }
    if (gMrdSerialIo == NULL) {
        return;
    }
    Len = AsciiStrLen(Msg);
    if (Len == 0) {
        return;
    }
    gMrdSerialIo->Write(gMrdSerialIo, &Len, Msg);
}

static VOID EFIAPI MemLogCallback(IN INTN DebugMode, IN CHAR8 *LastMessage)
{
    MirrorToSerial(LastMessage);

#if MERIDIAN_DEBUG > 0
    if (DebugMode >= 1) {
        SaveMessageToDebugLogFile(LastMessage);
    }
#endif

    return;
}

VOID InitBooterLog(VOID) { SetMemLogCallback(MemLogCallback); }
