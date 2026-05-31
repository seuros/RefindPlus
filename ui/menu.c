// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2020-2026 Dayo Akanji
// SPDX-FileCopyrightText: 2021 Joe van Tunen
// SPDX-FileCopyrightText: 2012-2020 Roderick W. Smith
// SPDX-FileCopyrightText: 2006 Christoph Pfisterer

#include "global.h"
#include "menu.h"
#include "lib.h"
#include "scan.h"
#include "config.h"
#include "mystrings.h"
#include "screenmgt.h"
#include "sysinfo.h"
#include "conn_bridge.h"

EFI_EVENT *WaitList = NULL;
UINTN WaitListLength = 0;

BOOLEAN SubScreenBoot = FALSE;

MERIDIAN_MENU_ENTRY MenuEntryNo = {L"No", TAG_RETURN, 1, NULL};
MERIDIAN_MENU_ENTRY MenuEntryYes = {L"Yes", TAG_RETURN, 1, NULL};

extern EFI_GUID MeridianGuid;

#if MERIDIAN_DEBUG > 0
VOID LogExit(IN UINTN MenuExit, IN const char FunctionName[], IN CHAR16 *ChosenOptionTitle)
{
    DEBUG_LOG(1, LOG_LINE_NORMAL, L"Returned '%d' (%s) from Menu Screen Option in '%a' Call ... %s",
              MenuExit, MenuExitInfo(MenuExit), FunctionName, ChosenOptionTitle);
}
#endif

static VOID FreeLoaderEntry(IN LOADER_ENTRY **Entry)
{
    LOG_SEP(L"X");
    LOG_INCREMENT();

    if (*Entry == NULL) {
        LOG_DECREMENT();
        LOG_SEP(L"X");

        return;
    }

    FreeMenuScreen(&(*Entry)->me.SubScreen);

    MRD_FREE_POOL((*Entry)->me.Title);

    MRD_FREE_POOL((*Entry)->Title);
    MRD_FREE_POOL((*Entry)->LoaderPath);
    MRD_FREE_POOL((*Entry)->InitrdPath);
    MRD_FREE_POOL((*Entry)->LoadOptions);
    MRD_FREE_POOL((*Entry)->EfiLoaderPath);

    MRD_FREE_POOL(*Entry);

    LOG_DECREMENT();
    LOG_SEP(L"X");
}

CHAR16 *MenuExitInfo(IN UINTN MenuExit)
{
    CHAR16 *MenuExitData;

    switch (MenuExit) {
    case 1:
        MenuExitData = L"ENTER";
        break;
    case 2:
        MenuExitData = L"ESCAPE";
        break;
    case 3:
        MenuExitData = L"DETAILS";
        break;
    case 4:
        MenuExitData = L"TIMEOUT";
        break;
    case 5:
        MenuExitData = L"EJECT";
        break;
    case 6:
        MenuExitData = L"REMOVE";
        break;
    default:
        MenuExitData = L"RETURN";
    }

    return MenuExitData;
}

VOID AddMenuInfoLine(IN MERIDIAN_MENU_SCREEN *Screen, IN CHAR16 *InfoLine, IN BOOLEAN CanFree)
{
#if MERIDIAN_DEBUG > 0
    DEBUG_LOG(1, LOG_LINE_NORMAL, L"Screen Menu Info Line:- '%s'", InfoLine);
#endif

    AddListElement((VOID ***)&(Screen->InfoLines), &(Screen->InfoLineCount),
                   (CanFree) ? InfoLine : StrDuplicate(InfoLine));
}

VOID AddSubMenuEntry(IN MERIDIAN_MENU_SCREEN *SubScreen, IN MERIDIAN_MENU_ENTRY *SubEntry)
{
    if (SubScreen == NULL || SubEntry == NULL) {

        return;
    }

#if MERIDIAN_DEBUG > 0
    DEBUG_LOG(1, LOG_LINE_NORMAL, L"Set SubMenu Entry in %s - %s%s", SubScreen->Title,
              SubEntry->Title, SetVolType(NULL, SubEntry->Title, 0));
#endif

    AddListElement((VOID ***)&(SubScreen->Entries), &(SubScreen->EntryCount), SubEntry);
}

VOID AddMenuEntry(IN MERIDIAN_MENU_SCREEN *Screen, IN MERIDIAN_MENU_ENTRY *Entry)
{
    if (Screen == NULL || Entry == NULL) {

        return;
    }

#if MERIDIAN_DEBUG > 0
    DEBUG_LOG(1, LOG_LINE_NORMAL, L"Append Menu Entry to %s %s %s%s", Screen->Title,
              (MrdStrEqualsCI(Screen->Title, MAIN_MENU_NAME)) ? L"-" : L" - ", Entry->Title,
              SetVolType(NULL, Entry->Title, 0));

#endif

    AddListElement((VOID ***)&(Screen->Entries), &(Screen->EntryCount), Entry);
}

VOID AddMenuEntryCopy(IN MERIDIAN_MENU_SCREEN *Screen, IN MERIDIAN_MENU_ENTRY *Entry)
{
    if (Screen == NULL || Entry == NULL) {

        return;
    }

    AddMenuEntry(Screen, CopyMenuEntry(Entry));
}

UINTN DrawMenuScreen(IN MERIDIAN_MENU_SCREEN *Screen, IN MENU_STYLE_FUNC StyleFunc,
                     IN OUT INTN *DefaultEntryIndex, OUT MERIDIAN_MENU_ENTRY **ChosenOption)
{
    (VOID) StyleFunc;

    if (Screen == NULL) {
        return MENU_EXIT_ZERO;
    }
    if (Screen->InfoLineCount > 2) {
        return ConnRunInfoScreen(Screen, DefaultEntryIndex, ChosenOption);
    }
    return ConnRunSubScreen(Screen, DefaultEntryIndex, ChosenOption);
}

UINTN ComputeRow0PosY(IN BOOLEAN ApplyOffset)
{
    (VOID) ApplyOffset;

    return ScreenH / 2;
}

VOID GenerateWaitList(VOID)
{
    if (WaitList != NULL) {

        return;
    }

    WaitListLength = 2;

    WaitList = AllocatePool(WaitListLength * sizeof(EFI_EVENT));
    if (WaitList == NULL) {

        return;
    }

    WaitList[0] = gST->ConIn->WaitForKey;
}

UINTN WaitForInput(IN UINTN Timeout)
{
    EFI_STATUS Status;
    UINTN Length;
    UINTN Index;
    EFI_EVENT TimerEvent;

    GenerateWaitList();

    Length = WaitListLength;
    TimerEvent = NULL;

    Status = gBS->CreateEvent(EVT_TIMER, 0, NULL, NULL, &TimerEvent);
    if (Timeout == 0) {
        Length--;
    }
    else {
        if (EFI_ERROR(Status)) {

            MeridianStall(10);

            return INPUT_TIMER_ERROR;
        }

        gBS->SetTimer(TimerEvent, TimerRelative, Timeout * 10000);
        WaitList[Length - 1] = TimerEvent;
    }

    Index = INPUT_TIMEOUT;
    Status = gBS->WaitForEvent(Length, WaitList, &Index);
    gBS->CloseEvent(TimerEvent);

    if (EFI_ERROR(Status)) {

        MeridianStall(20);

        return INPUT_TIMER_ERROR;
    }

    if (Index == 0) {

        return INPUT_KEY;
    }

    return INPUT_TIMEOUT;
}

VOID FreeMenuScreen(IN MERIDIAN_MENU_SCREEN **Screen)
{
#if MERIDIAN_DEBUG > 1
    UINTN j;
#endif

    UINTN i;

    LOG_SEP(L"X");
    LOG_INCREMENT();

    if (Screen == NULL || *Screen == NULL) {
        LOG_DECREMENT();
        LOG_SEP(L"X");

        return;
    }

    MRD_FREE_POOL((*Screen)->Title);

    if ((*Screen)->InfoLines) {
        LOG_SEP(L"X");
#if MERIDIAN_DEBUG > 1
        j = 0;
#endif
        for (i = 0; i < (*Screen)->InfoLineCount; i++) {
#if MERIDIAN_DEBUG > 1
            j++;
#endif
            MRD_FREE_POOL((*Screen)->InfoLines[i]);
        }
        LOG_SEP(L"X");
        (*Screen)->InfoLineCount = 0;

        MRD_FREE_POOL((*Screen)->InfoLines);
    }

    if ((*Screen)->Entries) {
#if MERIDIAN_DEBUG > 1
        j = 0;
#endif
        for (i = 0; i < (*Screen)->EntryCount; i++) {
#if MERIDIAN_DEBUG > 1
            j++;
#endif
            LOG_SEP(L"X");
            FreeMenuEntry(&(*Screen)->Entries[i]);
            LOG_SEP(L"X");
        }
        (*Screen)->EntryCount = 0;

        MRD_FREE_POOL((*Screen)->Entries);
    }

    MRD_FREE_POOL((*Screen)->TimeoutText);
    MRD_FREE_POOL((*Screen)->Hint1);
    MRD_FREE_POOL((*Screen)->Hint2);
    MRD_FREE_POOL(*Screen);

    LOG_DECREMENT();
    LOG_SEP(L"X");
}

VOID FreeMenuEntry(MERIDIAN_MENU_ENTRY **Entry)
{
#if MERIDIAN_DEBUG > 1
    CHAR16 *TagType;
#endif

    typedef enum
    {
        EntryTypeMenuEntry,
        EntryTypeLoaderEntry,
        EntryTypeLegacyEntry,
    } ENTRY_TYPE;
    ENTRY_TYPE EntryType;

    if (*Entry == NULL) {

        return;
    }

    LOG_SEP(L"X");
    LOG_INCREMENT();

    switch ((*Entry)->Tag) {
    case TAG_TOOL:
        EntryType = EntryTypeLoaderEntry;
        break;
    case TAG_LOADER:
        EntryType = EntryTypeLoaderEntry;
        break;
    case TAG_RESET_NVRAM:
        EntryType = EntryTypeLoaderEntry;
        break;
    case TAG_FIRMWARE_LOADER:
        EntryType = EntryTypeLoaderEntry;
        break;
    default:
        EntryType = EntryTypeMenuEntry;
        break;
    }

#if MERIDIAN_DEBUG > 1
    switch ((*Entry)->Tag) {
    case TAG_TOOL:
        TagType = L"TAG_TOOL";
        break;
    case TAG_LOADER:
        TagType = L"TAG_LOADER";
        break;
    case TAG_RESET_NVRAM:
        TagType = L"TAG_RESET_NVRAM";
        break;
    case TAG_FIRMWARE_LOADER:
        TagType = L"TAG_FIRMWARE_LOADER";
        break;
    default:
        TagType = L"DEFAULT";
        break;
    }
#endif

    if (EntryType == EntryTypeLoaderEntry) {
        FreeLoaderEntry((LOADER_ENTRY **)Entry);
    }
    else {
        MRD_FREE_POOL((*Entry)->Title);

        FreeMenuScreen(&(*Entry)->SubScreen);
    }

    MRD_FREE_POOL(*Entry);

    LOG_DECREMENT();
    LOG_SEP(L"X");
}

BDS_COMMON_OPTION *CopyBdsOption(BDS_COMMON_OPTION *BdsOption)
{
    BDS_COMMON_OPTION *NewBdsOption;

    if (BdsOption == NULL) {

        return NULL;
    }

    NewBdsOption = AllocateCopyPool(sizeof(*BdsOption), BdsOption);
    if (NewBdsOption == NULL) {

        return NULL;
    }

    if (BdsOption->DevicePath) {
        NewBdsOption->DevicePath =
            AllocateCopyPool(GetDevicePathSize(BdsOption->DevicePath), BdsOption->DevicePath);
    }

    if (BdsOption->OptionName) {
        NewBdsOption->OptionName =
            AllocateCopyPool(StrSize(BdsOption->OptionName), BdsOption->OptionName);
    }

    if (BdsOption->Description) {
        NewBdsOption->Description =
            AllocateCopyPool(StrSize(BdsOption->Description), BdsOption->Description);
    }

    if (BdsOption->LoadOptions) {
        NewBdsOption->LoadOptions =
            AllocateCopyPool(BdsOption->LoadOptionsSize, BdsOption->LoadOptions);
    }

    if (BdsOption->StatusString) {
        NewBdsOption->StatusString =
            AllocateCopyPool(StrSize(BdsOption->StatusString), BdsOption->StatusString);
    }

    return NewBdsOption;
}

VOID FreeBdsOption(BDS_COMMON_OPTION **BdsOption)
{
    if (BdsOption == NULL || *BdsOption == NULL) {

        return;
    }

    MRD_FREE_POOL((*BdsOption)->DevicePath);
    MRD_FREE_POOL((*BdsOption)->OptionName);
    MRD_FREE_POOL((*BdsOption)->Description);
    MRD_FREE_POOL((*BdsOption)->LoadOptions);
    MRD_FREE_POOL((*BdsOption)->StatusString);
    MRD_FREE_POOL(*BdsOption);
}

BOOLEAN GetMenuEntryReturn(IN OUT MERIDIAN_MENU_SCREEN **Screen)
{
    MERIDIAN_MENU_ENTRY *MenuEntryReturn;

    if (Screen == NULL || *Screen == NULL) {

        return FALSE;
    }

    MenuEntryReturn = AllocateZeroPool(sizeof(MERIDIAN_MENU_ENTRY));
    if (MenuEntryReturn == NULL) {

        return FALSE;
    }

    MenuEntryReturn->Title = StrDuplicate(L"Return to Main Menu");
    MenuEntryReturn->Tag = TAG_RETURN;
    AddMenuEntry(*Screen, MenuEntryReturn);

    return TRUE;
}
