// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2020-2026 Dayo Akanji
// SPDX-FileCopyrightText: 2012-2023 Roderick W. Smith
// SPDX-FileCopyrightText: 2006-2010 Christoph Pfisterer

#include "global.h"
#include "lib.h"
#include "mok.h"
#include "menu.h"
#include "scan.h"
#include "apple.h"
#include "config.h"
#include "install.h"
#include "sysinfo.h"
#include "screenmgt.h"
#include "mystrings.h"
#include "launch_efi.h"
#include "driver_support.h"
#include "security_policy.h"
#include "main.h"
#include "version.h"

#define BOOT_FIX_STR_01 L"Disable AMFI Check"
#define BOOT_FIX_STR_02 L"Disable Compatibility Check"
#define BOOT_FIX_STR_03 L"Disable nvRAM Panic Logging"

extern BOOLEAN VarNoCheckCompat, VarNoCheckAMFI, VarDisablePanicLog;

static EFI_STATUS FilterCSR(VOID)
{
    EFI_STATUS Status;

    Status = (!GlobalConfig.NormaliseCSR) ? EFI_NOT_STARTED : NormaliseCSR();

#if MERIDIAN_DEBUG > 0
    DEBUG_LOG(1, LOG_THREE_STAR_MID, L"Normalise CSR ... %r", Status);
    INFO_LOG("%s    * Status:- '%r ... Normalise CSR'", OffsetNext, Status);
#endif

    return Status;
}

static BOOLEAN CheckToggledCSR(VOID)
{
    BOOLEAN Active;
    BOOLEAN Retval;

    Active = (MrdStrFind(gCsrStatus, L"Enabled")) ? FALSE : TRUE;

    Retval = TRUE;
    if (GlobalConfig.DynamicCSR == -1) {

        if (!Active) {

            Retval = FALSE;
        }
    }
    else {
        if (GlobalConfig.DynamicCSR == 1) {

            if (Active) {

                Retval = FALSE;
            }
        }
    }

    return Retval;
}

VOID AlignCSR(VOID)
{
#if MERIDIAN_DEBUG > 0
    CHAR16 *TmpStr;
    CHAR16 *MsgStr;
#endif

    EFI_STATUS Status;
    UINT32 CsrStatus;
    BOOLEAN HandledCSR;
    BOOLEAN RotatedCSR;

    static BOOLEAN RunOnce = FALSE;

    if (RunOnce || GlobalConfig.CsrValues == NULL) {

        GlobalConfig.DynamicCSR = 0;
    }
    RunOnce = TRUE;

    if (GlobalConfig.DynamicCSR == 0) {

        return;
    }

#if MERIDIAN_DEBUG > 0
    MsgStr = StrDuplicate(L"E N F O R C E   C S R   P O L I C Y");
    DEBUG_LOG(1, LOG_LINE_SEPARATOR, L"%s", MsgStr);
    INFO_LOG("%s", MsgStr);
    MRD_FREE_POOL(MsgStr);

    DEBUG_LOG(1, LOG_LINE_NORMAL, L"Vet and Set SIP/SSV");
#endif

    do {
        RotatedCSR = HandledCSR = FALSE;

        if (!HasMacOS) {
#if MERIDIAN_DEBUG > 0
            Status = EFI_NOT_STARTED;
#endif

            break;
        }

        if (GlobalConfig.DynamicCSR != 1 && GlobalConfig.DynamicCSR != -1) {
#if MERIDIAN_DEBUG > 0
            Status = EFI_INVALID_PARAMETER;
#endif

            break;
        }

        Status = GetCsrStatus(&CsrStatus);
        if (EFI_ERROR(Status)) {

            break;
        }
        RecordgCsrStatus(CsrStatus, FALSE);
        HandledCSR = TRUE;

        RotatedCSR = CheckToggledCSR();
        if (RotatedCSR) {
#if MERIDIAN_DEBUG > 0
            Status = EFI_ALREADY_STARTED;
#endif

            break;
        }

        RotateCsrValue(FALSE);
        RotatedCSR = CheckToggledCSR();

        if (!RotatedCSR) {

            RotateCsrValue(FALSE);
            RotatedCSR = CheckToggledCSR();

            if (!RotatedCSR) {

                RotateCsrValue(FALSE);
                RotatedCSR = CheckToggledCSR();
            }
        }
    } while (0);

    if (!RotatedCSR || !HandledCSR) {
        GlobalConfig.DynamicCSR = 0;
    }

#if MERIDIAN_DEBUG > 0
    if (RotatedCSR && HandledCSR) {
        TmpStr = (GlobalConfig.DynamicCSR == 1) ? L"Enable" : L"Disable";
    }
    else if (!HandledCSR) {

        TmpStr = L"Adjust";
    }
    else {

        Status = EFI_NOT_READY;
        TmpStr = L"Could *NOT* Definitively Set";
    }

    MsgStr = PoolPrint(L"%s SIP/SSV ... %r", TmpStr, Status);
    DEBUG_LOG(1, LOG_STAR_HEAD_SEP, L"%s", MsgStr);
    DEBUG_LOG(1, LOG_BLANK_LINE_SEP, L"X");
    INFO_LOG("\n");
    INFO_LOG("INFO: %s", MsgStr);
    INFO_LOG("\n\n");
    MRD_FREE_POOL(MsgStr);
#endif
}

#if MERIDIAN_DEBUG > 0
static VOID LogDisableCheck(IN CHAR16 *TypStr, IN EFI_STATUS Result)
{
    CHAR16 *MsgStr;

    MsgStr = PoolPrint(L"Status:- '%r ... %s'", Result, TypStr);
    DEBUG_LOG(1, LOG_THREE_STAR_MID, L"%s", MsgStr);
    INFO_LOG("%s    * %s", OffsetNext, MsgStr);
    MRD_FREE_POOL(MsgStr);
}
#endif

static VOID HandleArgs(IN EFI_STATUS Result)
{
#if MERIDIAN_DEBUG > 0
    if (VarNoCheckAMFI)
        LogDisableCheck(BOOT_FIX_STR_01, Result);
    if (VarNoCheckCompat)
        LogDisableCheck(BOOT_FIX_STR_02, Result);
    if (VarDisablePanicLog)
        LogDisableCheck(BOOT_FIX_STR_03, Result);
#endif

    VarNoCheckAMFI = FALSE;
    VarNoCheckCompat = FALSE;
    VarDisablePanicLog = FALSE;
}

static EFI_STATUS StoreArgs(IN CHAR16 *BootArg)
{
    CHAR8 *DataNVram;

    DataNVram = AllocatePool((StrLen(BootArg) + 1) * sizeof(CHAR8));
    if (DataNVram == NULL) {
        return EFI_OUT_OF_RESOURCES;
    }

    UnicodeStrToAsciiStrS(BootArg, DataNVram, StrLen(BootArg) + 1);

    return StoreBootArgsNvram(DataNVram);
}

static VOID SetBootArgs(VOID)
{
#if MERIDIAN_DEBUG > 0
    CHAR16 *MsgStr;
    BOOLEAN LogDisableCheckAMFI;
    BOOLEAN LogDisableCheckCompat;
    BOOLEAN LogDisableNvramPanicLog;
#endif

    EFI_STATUS Status;
    CHAR16 *BootArg;
    CHAR8 *DataNVram;
    VOID *VarData;

    if (GlobalConfig.SetBootArgs == NULL || GlobalConfig.SetBootArgs[0] == L'\0') {
#if MERIDIAN_DEBUG > 0
        Status = EFI_INVALID_PARAMETER;

        MsgStr = PoolPrint(L"Reset Boot Args ... %r", Status);
        DEBUG_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
        INFO_LOG("%s    * %s", OffsetNext, MsgStr);
        MRD_FREE_POOL(MsgStr);
#endif

        return;
    }

    if (MrdStrFind(GlobalConfig.SetBootArgs, L"nvram_paniclog")) {

        VarDisablePanicLog = FALSE;
#if MERIDIAN_DEBUG > 0
        LogDisableNvramPanicLog = GlobalConfig.DisableNvramPanicLog;
    }
    else {
        LogDisableNvramPanicLog = FALSE;
#endif
    }

    if (MrdStrFind(GlobalConfig.SetBootArgs, L"amfi_get_out_of_my_way")) {

        VarNoCheckAMFI = FALSE;
#if MERIDIAN_DEBUG > 0
        LogDisableCheckAMFI = GlobalConfig.DisableCheckAMFI;
    }
    else {
        LogDisableCheckAMFI = FALSE;
#endif
    }

    if (MrdStrFind(GlobalConfig.SetBootArgs, L"-no_compat_check")) {

        VarNoCheckCompat = FALSE;
#if MERIDIAN_DEBUG > 0
        LogDisableCheckCompat = GlobalConfig.DisableCheckCompat;
    }
    else {
        LogDisableCheckCompat = FALSE;
#endif
    }

    if (GlobalConfig.DisableCheckAMFI && GlobalConfig.DisableCheckCompat &&
        GlobalConfig.DisableNvramPanicLog) {

        BootArg = PoolPrint(L"%s nvram_paniclog=0 amfi_get_out_of_my_way=1 -no_compat_check",
                            GlobalConfig.SetBootArgs);
    }
    else if (GlobalConfig.DisableCheckAMFI && GlobalConfig.DisableNvramPanicLog) {

        BootArg =
            PoolPrint(L"%s nvram_paniclog=0 amfi_get_out_of_my_way=1", GlobalConfig.SetBootArgs);
    }
    else if (GlobalConfig.DisableCheckCompat && GlobalConfig.DisableNvramPanicLog) {

        BootArg = PoolPrint(L"%s nvram_paniclog=0 -no_compat_check", GlobalConfig.SetBootArgs);
    }
    else if (GlobalConfig.DisableCheckAMFI && GlobalConfig.DisableCheckCompat) {

        BootArg =
            PoolPrint(L"%s amfi_get_out_of_my_way=1 -no_compat_check", GlobalConfig.SetBootArgs);
    }
    else if (GlobalConfig.DisableNvramPanicLog) {

        BootArg = PoolPrint(L"%s nvram_paniclog=0", GlobalConfig.SetBootArgs);
    }
    else if (GlobalConfig.DisableCheckAMFI) {

        BootArg = PoolPrint(L"%s amfi_get_out_of_my_way=1", GlobalConfig.SetBootArgs);
    }
    else if (GlobalConfig.DisableCheckCompat) {

        BootArg = PoolPrint(L"%s -no_compat_check", GlobalConfig.SetBootArgs);
    }
    else {

        BootArg = StrDuplicate(GlobalConfig.SetBootArgs);
    }

    VarData = NULL;
    DataNVram = AllocatePool(sizeof(CHAR8) * (StrLen(BootArg) + 1));
    Status = (DataNVram != NULL) ? EFI_SUCCESS : EFI_OUT_OF_RESOURCES;
    if (!EFI_ERROR(Status)) {
        /* coverity[check_return: SUPPRESS] */
        GetHardwareNvramVariable(L"boot-args", &AppleBootGuid, &VarData, NULL);

        if (VarData && MrdStrFind(VarData, BootArg)) {
            Status = EFI_ALREADY_STARTED;
        }
        else {

            UnicodeStrToAsciiStrS(BootArg, DataNVram, StrLen(BootArg) + 1);
            Status = StoreBootArgsNvram(DataNVram);
        }
    }

#if MERIDIAN_DEBUG > 0
    if (LogDisableCheckAMFI)
        LogDisableCheck(BOOT_FIX_STR_01, Status);
    if (LogDisableCheckCompat)
        LogDisableCheck(BOOT_FIX_STR_02, Status);
    if (LogDisableNvramPanicLog)
        LogDisableCheck(BOOT_FIX_STR_03, Status);

    MsgStr =
        PoolPrint(L"Status:- '%r ... Set Boot Arguments: %s'", Status, GlobalConfig.SetBootArgs);
    DEBUG_LOG(1, LOG_THREE_STAR_MID, L"%s", MsgStr);
    INFO_LOG("%s    * %s", OffsetNext, MsgStr);
    MRD_FREE_POOL(MsgStr);
#endif

    MRD_FREE_POOL(VarData);
    MRD_FREE_POOL(BootArg);
    MRD_FREE_POOL(DataNVram);
}

static EFI_STATUS FinishMacBootArg(EFI_STATUS Status, CHAR16 *BootArg, VOID *VarData,
                                   BOOLEAN *DisableFlag, IN CONST CHAR16 *LogStr)
{
    if (!EFI_ERROR(Status)) {
        Status = StoreArgs(BootArg);
    }
    MRD_FREE_POOL(BootArg);

    if (!EFI_ERROR(Status)) {
        HandleArgs(Status);
    }
    else {
        *DisableFlag = FALSE;
#if MERIDIAN_DEBUG > 0
        LogDisableCheck((CHAR16 *)LogStr, Status);
#else
        (VOID) LogStr;
#endif
    }

    MRD_FREE_POOL(VarData);
    return Status;
}

typedef CHAR16 *(*MAC_FIX_NOTFOUND)(VOID);
typedef CHAR16 *(*MAC_FIX_MERGE)(CHAR16 *CurArgs);

static EFI_STATUS ApplyMacBootFix(BOOLEAN *DisableFlag, IN CHAR16 *CheckArg,
                                  IN CONST CHAR16 *LogStr, MAC_FIX_NOTFOUND BuildNotFound,
                                  MAC_FIX_MERGE BuildFromCurrent)
{
    EFI_STATUS Status;
    CHAR16 *BootArg;
    CHAR16 *CurArgs;
    VOID *VarData;

    VarData = NULL;
    BootArg = NULL;
    Status = GetHardwareNvramVariable(L"boot-args", &AppleBootGuid, &VarData, NULL);
    if (EFI_ERROR(Status)) {
        if (Status == EFI_NOT_FOUND) {
            Status = EFI_SUCCESS;
            BootArg = BuildNotFound();
        }
    }
    else {
        CurArgs = MrdAsciiToUnicode((CHAR8 *)VarData, 0);
        if (MrdStrIncludesCI(CurArgs, CheckArg)) {
            Status = EFI_ALREADY_STARTED;
        }
        else {
            BootArg = BuildFromCurrent(CurArgs);
        }
        MRD_FREE_POOL(CurArgs);
    }

    return FinishMacBootArg(Status, BootArg, VarData, DisableFlag, LogStr);
}

static CHAR16 *AmfiBuildNotFound(VOID) { return PoolPrint(L"%s=1", L"amfi_get_out_of_my_way"); }

static CHAR16 *AmfiBuildFromCurrent(CHAR16 *CurArgs)
{
    CHAR16 *ArgData = L"amfi_get_out_of_my_way";

    if (VarNoCheckCompat) {
        if (!MrdStrIncludesCI(CurArgs, L"amfi_get_out_of_my_way")) {
            ArgData = L"-no_compat_check amfi_get_out_of_my_way";
        }
    }

    if (VarDisablePanicLog) {
        if (!MrdStrIncludesCI(CurArgs, L"nvram_paniclog")) {
            if (VarNoCheckCompat) {
                ArgData = L"nvram_paniclog=0 -no_compat_check amfi_get_out_of_my_way";
            }
            else {
                ArgData = L"nvram_paniclog=0 amfi_get_out_of_my_way";
            }
        }
    }

    return PoolPrint(L"%s %s=1", CurArgs, ArgData);
}

static EFI_STATUS NoCheckAMFI(VOID)
{
    if (!VarNoCheckAMFI) {

        return EFI_NOT_STARTED;
    }

    return ApplyMacBootFix(&VarNoCheckAMFI, L"amfi_get_out_of_my_way", BOOT_FIX_STR_01,
                           AmfiBuildNotFound, AmfiBuildFromCurrent);
}

static CHAR16 *CompatBuildNotFound(VOID) { return StrDuplicate(L"-no_compat_check"); }

static CHAR16 *CompatBuildFromCurrent(CHAR16 *CurArgs)
{
    CHAR16 *ArgData = L"-no_compat_check";

    if (VarNoCheckAMFI) {
        if (!MrdStrIncludesCI(CurArgs, L"amfi_get_out_of_my_way")) {
            ArgData = L"amfi_get_out_of_my_way=1 -no_compat_check";
        }
    }

    if (VarDisablePanicLog) {
        if (!MrdStrIncludesCI(CurArgs, L"nvram_paniclog")) {
            if (VarNoCheckAMFI) {
                ArgData = L"nvram_paniclog=0 amfi_get_out_of_my_way=1 -no_compat_check";
            }
            else {
                ArgData = L"nvram_paniclog=0 -no_compat_check";
            }
        }
    }

    return PoolPrint(L"%s %s", CurArgs, ArgData);
}

static EFI_STATUS NoCheckCompat(VOID)
{
    if (!VarNoCheckCompat) {

        return EFI_NOT_STARTED;
    }

    return ApplyMacBootFix(&VarNoCheckCompat, L"-no_compat_check", BOOT_FIX_STR_02,
                           CompatBuildNotFound, CompatBuildFromCurrent);
}

static CHAR16 *PanicLogBuildNotFound(VOID) { return PoolPrint(L"%s=0", L"nvram_paniclog"); }

static CHAR16 *PanicLogBuildFromCurrent(CHAR16 *CurArgs)
{
    CHAR16 *ArgData = L"nvram_paniclog";

    if (VarNoCheckAMFI) {
        if (!MrdStrIncludesCI(CurArgs, L"amfi_get_out_of_my_way")) {
            ArgData = L"amfi_get_out_of_my_way=1 nvram_paniclog";
        }
    }

    if (VarNoCheckCompat) {
        if (!MrdStrIncludesCI(CurArgs, L"-no_compat_check")) {
            if (VarNoCheckAMFI) {
                ArgData = L"-no_compat_check amfi_get_out_of_my_way=1 nvram_paniclog";
            }
            else {
                ArgData = L"-no_compat_check nvram_paniclog";
            }
        }
    }

    return PoolPrint(L"%s %s=0", CurArgs, ArgData);
}

static EFI_STATUS NoNvramPanicLog(VOID)
{
    if (!VarDisablePanicLog) {

        return EFI_NOT_STARTED;
    }

    return ApplyMacBootFix(&VarDisablePanicLog, L"nvram_paniclog", BOOT_FIX_STR_03,
                           PanicLogBuildNotFound, PanicLogBuildFromCurrent);
}

VOID RunMacBootSupportFuncs(CHAR16 *SelectionName)
{

    if (GlobalConfig.SetBootArgs != NULL && GlobalConfig.SetBootArgs[0] != L'\0') {
        SetBootArgs();
    }

    FilterCSR();

    NoCheckAMFI();

    NoCheckCompat();

    NoNvramPanicLog();

    TrimCoerce();

    RunNVramSync(SelectionName, TRUE);

    RemapOpenProtocol();
}
