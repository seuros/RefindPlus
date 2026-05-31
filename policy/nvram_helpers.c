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
#include "display.h"
#include "version.h"

extern EFI_SET_VARIABLE OrigSetVariableRT;

#if MERIDIAN_DEBUG > 0
VOID UnexpectedReturn(CHAR16 *ItemType)
{
    CHAR16 *MsgStr;

    MsgStr = PoolPrint(L"Unexpected Return from %s", ItemType);
    DEBUG_LOG(1, LOG_STAR_SEPARATOR, L"%s", MsgStr);
    INFO_LOG("** WARN: %s", MsgStr);
    INFO_LOG("\n\n");
    MRD_FREE_POOL(MsgStr);
}
#endif

VOID InitRotateCSR(VOID)
{
#if MERIDIAN_DEBUG > 0
    CHAR16 *MsgStr;
#endif

    BOOLEAN Confirmed;

    Confirmed = ConfirmRotate();
    if (!Confirmed) {
#if MERIDIAN_DEBUG > 0
        MsgStr = L"Aborted CSR Rotation";
        DEBUG_LOG(1, LOG_BLANK_LINE_SEP, L"X");
        DEBUG_LOG(1, LOG_LINE_THIN_SEP, L"%s", MsgStr);
        DEBUG_LOG(1, LOG_BLANK_LINE_SEP, L"X");
        INFO_LOG("%s  *** %s", OffsetNext, MsgStr);
        INFO_LOG("\n\n");
#endif

        return;
    }

    RotateCsrValue(TRUE);
}

EFI_STATUS StoreBootArgsNvram(IN VOID *VariableData OPTIONAL)
{
    EFI_STATUS Status;
    UINTN VariableSize;
    UINTN OldSize;
    VOID *OldBuf;
    UINT32 Attributes;
    BOOLEAN SettingMatch;

    OldSize = 0;
    OldBuf = NULL;
    Status = EFI_LOAD_ERROR;

    if (VariableData == NULL) {
        VariableSize = 0;
    }
    else {
        VariableSize = AsciiStrSize((CHAR8 *)VariableData);
        if (VariableSize == 0) {
            VariableData = NULL;
        }
    }

    if (VariableSize != 0) {
        Status = GetHardwareNvramVariable(L"boot-args", &AppleBootGuid, &OldBuf, &OldSize);
        if (EFI_ERROR(Status) && Status != EFI_NOT_FOUND) {

            return Status;
        }
    }

    SettingMatch = FALSE;
    if (!EFI_ERROR(Status)) {
        if (VariableSize != 0) {

            SettingMatch =
                (VariableSize == OldSize && CompareMem(VariableData, OldBuf, VariableSize) == 0);
        }
        MRD_FREE_POOL(OldBuf);

        if (SettingMatch) {

            return EFI_ALREADY_STARTED;
        }
    }

    Attributes = EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS;
    if (GlobalConfig.PersistBootArgs) {
        Attributes |= EFI_VARIABLE_NON_VOLATILE;
    }
    Status =
        OrigSetVariableRT(L"boot-args", &AppleBootGuid, Attributes, VariableSize, VariableData);

    return Status;
}

EFI_STATUS TrimCoerce(VOID)
{
#if MERIDIAN_DEBUG > 0
    CHAR16 *MsgStr;
#endif

    EFI_STATUS Status;
    CHAR8 DataNVram[1] = {0x01};

    if (!GlobalConfig.ForceTRIM) {

        return EFI_NOT_STARTED;
    }

    Status = SetHardwareNvramVariable(L"EnableTRIM", &AppleBootGuid, AccessFlagsFull,
                                      AsciiStrSize(DataNVram), DataNVram);

#if MERIDIAN_DEBUG > 0
    MsgStr = PoolPrint(L"Forcefully Enable TRIM ... %r", Status);
    DEBUG_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
    INFO_LOG("%s    * %s", OffsetNext, MsgStr);
    MRD_FREE_POOL(MsgStr);
#endif

    return Status;
}

UINTN RunTrustSync(LOADER_ENTRY *Entry)
{
#if MERIDIAN_DEBUG > 0
    CHAR16 *MsgStr;
    BOOLEAN CheckMute = FALSE;
#endif

    EFI_STATUS Status;
    CHAR16 *VarName;
    UINTN BootNum;
    UINTN EntrySize;
    UINTN ExitChain;
    BOOLEAN LoaderValid;
    BOOLEAN AlreadyExists;
    EFI_DEVICE_PATH_PROTOCOL *DevicePath;

#if MERIDIAN_DEBUG > 0
    DEBUG_LOG(1, LOG_LINE_NORMAL, L"%s", Entry->me.Title);

    MRD_MUTELOGGER_SET;
#endif
    LoaderValid = IsValidLoader(Entry->Volume->RootDir, Entry->LoaderPath);
#if MERIDIAN_DEBUG > 0
    MRD_MUTELOGGER_OFF;
#endif

    if (!LoaderValid) {
#if MERIDIAN_DEBUG > 0
        MsgStr = StrDuplicate(L"ERROR: Invalid Binary!!");
        DEBUG_LOG(1, LOG_STAR_SEPARATOR, L"%s", MsgStr);
        INFO_LOG("%s    * %s", OffsetNext, MsgStr);
        INFO_LOG("%s", OffsetNext);
        MRD_FREE_POOL(MsgStr);
#endif

        return SYNC_TRUST_HALT;
    }

#if MERIDIAN_DEBUG > 0
    DEBUG_LOG(1, LOG_BLANK_LINE_SEP, L"X");
    DEBUG_LOG(1, LOG_LINE_NORMAL, L"Config Setting Detected:- 'sync_trust'");
#endif

    if (GlobalConfig.SyncTrust & REQUIRE_TRUST_VERIFY) {
#if MERIDIAN_DEBUG > 0
        DEBUG_LOG(1, LOG_LINE_THIN_SEP, L"Prepare Menu Screen");
        DEBUG_LOG(1, LOG_LINE_NORMAL, L"Screen Title:- '%s'", TRUSTED_BOOT_CONFIRM);
#endif

        ExitChain = AbortSyncTrust();
        if (ExitChain != SYNC_TRUST_BOOT) {

            return ExitChain;
        }

#if MERIDIAN_DEBUG > 0
        DEBUG_LOG(1, LOG_BLANK_LINE_SEP, L"X");
#endif
    }

    Status = ConstructBootEntry(Entry->Volume->DeviceHandle, Entry->LoaderPath,
                                Entry->Volume->VolName, (CHAR8 **)&DevicePath, &EntrySize);
    if (EFI_ERROR(Status)) {

        return SYNC_TRUST_HALT;
    }

#if MERIDIAN_DEBUG > 0
    MRD_MUTELOGGER_SET;
#endif

    AlreadyExists = FALSE;
    BootNum = FindBootNum(DevicePath, EntrySize, &AlreadyExists);
    VarName = PoolPrint(L"Boot%04x", BootNum);

#if MERIDIAN_DEBUG > 0
    MRD_MUTELOGGER_OFF;

    DEBUG_LOG(1, LOG_LINE_NORMAL, L"Select Boot#### Variable:- '%s'", VarName);
#endif

    if (!AlreadyExists) {
        Status = EfivarSetRaw(&GlobalGuid, VarName, DevicePath, EntrySize, TRUE);

        MeridianStall(50);

        if (EFI_ERROR(Status)) {
            MRD_FREE_POOL(VarName);
            MRD_FREE_POOL(DevicePath);

            return SYNC_TRUST_HALT;
        }
    }

    Status = EfivarSetRaw(&GlobalGuid, L"BootNext", &BootNum, sizeof(UINT16), TRUE);

    MeridianStall(50);

    MRD_FREE_POOL(VarName);
    MRD_FREE_POOL(DevicePath);

    if (EFI_ERROR(Status)) {

        return SYNC_TRUST_HALT;
    }

#if MERIDIAN_DEBUG > 0
    MsgStr = StrDuplicate(L"Status:- 'Success ... Native Boot Chain Setup'");
    DEBUG_LOG(1, LOG_THREE_STAR_MID, L"%s", MsgStr);
    INFO_LOG("%s    * %s", OffsetNext, MsgStr);
    INFO_LOG("%s", OffsetNext);
    MRD_FREE_POOL(MsgStr);
#endif

    egDisplayMessage(L"Restart in Native Boot Chain", &BGColorBase, CENTER, 3, L"PauseSeconds");

#if MERIDIAN_DEBUG > 0
    MsgStr = StrDuplicate(L"Restart in Native Boot Chain for Loader");
    INFO_LOG("\n");
    INFO_LOG("%s File", MsgStr);
    DEBUG_LOG(1, LOG_BLANK_LINE_SEP, L"X");
    DEBUG_LOG(1, LOG_LINE_NORMAL, L"%s:- '%s'", MsgStr, Entry->LoaderPath);
    MRD_FREE_POOL(MsgStr);
#endif

    StoreLoaderName((Entry->Title != NULL) ? Entry->Title : Entry->me.Title);
    StoreLoaderIdentity(Entry);

#if MERIDIAN_DEBUG > 0
    OUT_TAG();
#endif

    gRT->ResetSystem(EfiResetCold, EFI_SUCCESS, 0, NULL);

#if MERIDIAN_DEBUG > 0
    UnexpectedReturn(L"Trusted Boot Chain Reset");
#endif

    return SYNC_TRUST_HALT;
}

VOID RunNVramSync(CHAR16 *SelectionName, BOOLEAN IsMacOS)
{
#if MERIDIAN_DEBUG > 0
    CHAR16 *MsgStr;
    BOOLEAN CheckMute = FALSE;
#endif

    EFI_STATUS Status;
    BOOLEAN Proceed;
    UINT8 *TmpBuffer;
    CHAR16 *PreviousBoot;

    if (GlobalConfig.SyncNVram < 1) {

        return;
    }

    Proceed = TRUE;

#if MERIDIAN_DEBUG > 0
    MRD_MUTELOGGER_SET;
#endif

    Status =
        EfivarGetRaw(&AppleBootGuid, L"bluetoothInternalControllerinfo", (VOID **)&TmpBuffer, NULL);
    if (Status == EFI_NOT_FOUND) {
        Status = EfivarGetRaw(&AppleBootGuid, L"bluetoothActiveControllerInfo", (VOID **)&TmpBuffer,
                              NULL);
        if (Status == EFI_NOT_FOUND) {
            Status = EfivarGetRaw(&AppleBootGuid, L"bluetoothExternalDongleFailed",
                                  (VOID **)&TmpBuffer, NULL);
            if (Status == EFI_NOT_FOUND) {

                Proceed = FALSE;
            }
        }
    }

    if (Proceed && !GlobalConfig.TransientBoot &&
        (GlobalConfig.SyncNVram == 1 || GlobalConfig.SyncNVram == 2)) {
        Status = EfivarGetRaw(&MeridianGuid, L"PreviousBoot", (VOID **)&PreviousBoot, NULL);
        if (!EFI_ERROR(Status)) {

            if (MrdStrIncludesCI(SelectionName, PreviousBoot) ||
                (IsMacOS && (MrdStrIncludesCI(PreviousBoot, L"Mac OS") ||
                             MrdStrIncludesCI(PreviousBoot, L"macOS")))) {
                Proceed = FALSE;
            }
        }
    }

#if MERIDIAN_DEBUG > 0
    MRD_MUTELOGGER_OFF;
#endif

    if (Proceed && (GlobalConfig.SyncNVram == 2 || GlobalConfig.SyncNVram == 3)) {

        Proceed = ConfirmSyncNVram();
    }

    if (!Proceed) {

        return;
    }

    SetHardwareNvramVariable(L"bluetoothInternalControllerinfo", &AppleBootGuid, AccessFlagsBoot, 0,
                             NULL);
    SetHardwareNvramVariable(L"bluetoothActiveControllerInfo", &AppleBootGuid, AccessFlagsBoot, 0,
                             NULL);
    SetHardwareNvramVariable(L"bluetoothExternalDongleFailed", &AppleBootGuid, AccessFlagsBoot, 0,
                             NULL);

#if MERIDIAN_DEBUG > 0
    MsgStr = L"Status:- 'Success ... Apply nvRAM Sync'";
    DEBUG_LOG(1, LOG_THREE_STAR_MID, L"%s", MsgStr);
    INFO_LOG("%s    * %s", OffsetNext, MsgStr);
#endif
}

EFI_STATUS GetHardwareNvramVariable(IN CHAR16 *VariableName, IN EFI_GUID *VendorGuid,
                                    OUT VOID **VariableData, OUT UINTN *VariableSize OPTIONAL)
{
    EFI_STATUS Status;
    UINTN BufferSize;
    VOID *TmpBuffer;

    BufferSize = 0;
    TmpBuffer = NULL;
    Status = gRT->GetVariable(VariableName, VendorGuid, NULL, &BufferSize, TmpBuffer);
    if (Status != EFI_BUFFER_TOO_SMALL) {
        return EFI_NOT_FOUND;
    }

    TmpBuffer = AllocatePool(BufferSize);
    if (TmpBuffer == NULL) {
        return EFI_OUT_OF_RESOURCES;
    }

    Status = gRT->GetVariable(VariableName, VendorGuid, NULL, &BufferSize, TmpBuffer);
    if (EFI_ERROR(Status) || BufferSize < 1) {
        MRD_FREE_POOL(TmpBuffer);
        *VariableData = NULL;

        if (VariableSize != NULL) {
            *VariableSize = 0;
        }

        return EFI_LOAD_ERROR;
    }

    *VariableData = TmpBuffer;

    if (VariableSize != NULL) {
        *VariableSize = BufferSize;
    }

    return EFI_SUCCESS;
}

EFI_STATUS SetHardwareNvramVariable(IN CHAR16 *VariableName, IN EFI_GUID *VendorGuid,
                                    IN UINT32 Attributes, IN UINTN VariableSize,
                                    IN VOID *VariableData OPTIONAL)
{
    EFI_STATUS Status;
    VOID *OldBuf;
    UINTN OldSize;
    BOOLEAN SettingMatch;

    OldSize = 0;
    OldBuf = NULL;
    Status = EFI_LOAD_ERROR;

    if (VariableData != NULL && VariableSize != 0) {
        Status = GetHardwareNvramVariable(VariableName, VendorGuid, &OldBuf, &OldSize);
        if (EFI_ERROR(Status) && Status != EFI_NOT_FOUND) {

            return Status;
        }
    }

    SettingMatch = FALSE;
    if (!EFI_ERROR(Status)) {
        if (VariableData != NULL && VariableSize != 0) {

            SettingMatch =
                (VariableSize == OldSize && CompareMem(VariableData, OldBuf, VariableSize) == 0);
        }
        MRD_FREE_POOL(OldBuf);

        if (SettingMatch) {

            return EFI_ALREADY_STARTED;
        }
    }

    Status = OrigSetVariableRT(VariableName, VendorGuid, Attributes, VariableSize, VariableData);

    return Status;
}
