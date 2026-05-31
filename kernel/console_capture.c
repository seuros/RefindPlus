// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#include "console_capture.h"
#include "log.h"
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/SimpleTextOut.h>

static BOOLEAN gActive = FALSE;
static EFI_TEXT_STRING gSavedConOut = NULL;
static EFI_TEXT_STRING gSavedStdErr = NULL;
static EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *gStdErrPatched = NULL;

static EFI_STATUS EFIAPI CapOutputString(IN EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
                                         IN CHAR16 *String)
{
    (VOID) This;
#if MERIDIAN_DEBUG > 0
    if (String != NULL && String[0] != L'\0') {
        DebugLog("%s", String);
    }
#else
    (VOID) String;
#endif
    return EFI_SUCCESS;
}

VOID MrdConsoleCaptureBegin(VOID)
{
    if (gActive || gST == NULL) {
        return;
    }
    gActive = TRUE;

    if (gST->ConOut != NULL && gST->ConOut->OutputString != CapOutputString) {
        gSavedConOut = gST->ConOut->OutputString;
        gST->ConOut->OutputString = CapOutputString;
    }

    if (gST->StdErr != NULL && gST->StdErr != gST->ConOut &&
        gST->StdErr->OutputString != CapOutputString) {
        gSavedStdErr = gST->StdErr->OutputString;
        gStdErrPatched = gST->StdErr;
        gST->StdErr->OutputString = CapOutputString;
    }
}

VOID MrdConsoleCaptureEnd(VOID)
{
    if (!gActive) {
        return;
    }

    if (gSavedConOut != NULL && gST != NULL && gST->ConOut != NULL) {
        gST->ConOut->OutputString = gSavedConOut;
    }
    if (gSavedStdErr != NULL && gStdErrPatched != NULL) {
        gStdErrPatched->OutputString = gSavedStdErr;
    }

    gSavedConOut = NULL;
    gSavedStdErr = NULL;
    gStdErrPatched = NULL;
    gActive = FALSE;
}
