// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#include "global.h"
#include "display.h"
#include "screenmgt.h"
#include "apple.h"
#include "config.h"
#include "lib.h"
#include "mystrings.h"

EFI_GUID gEfiUgaDrawProtocolGuid = EFI_UGA_DRAW_PROTOCOL_GUID;

EFI_UGA_DRAW_PROTOCOL *UGADraw = NULL;
EFI_GRAPHICS_OUTPUT_PROTOCOL *GOPDraw = NULL;

static EFI_CONSOLE_CONTROL_PROTOCOL *ConsoleControl = NULL;
static EFI_GUID ConsoleControlProtocolGuid = EFI_CONSOLE_CONTROL_PROTOCOL_GUID;
static BOOLEAN HasGraphics = FALSE;
static BOOLEAN GraphicsEnabled = FALSE;

static EFI_HANDLE GopHandle = NULL;
static UINTN DisplayWidth = 800;
static UINTN DisplayHeight = 600;

static VOID UpdateGopSize(IN EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop)
{
    if (Gop == NULL || Gop->Mode == NULL || Gop->Mode->Info == NULL) {
        return;
    }

    DisplayWidth = Gop->Mode->Info->HorizontalResolution;
    DisplayHeight = Gop->Mode->Info->VerticalResolution;
}

static BOOLEAN UseGop(IN EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop)
{
    if (Gop == NULL || Gop->Blt == NULL || Gop->Mode == NULL || Gop->Mode->Info == NULL ||
        Gop->Mode->Info->HorizontalResolution == 0 || Gop->Mode->Info->VerticalResolution == 0) {
        return FALSE;
    }

    GOPDraw = Gop;
    UpdateGopSize(GOPDraw);
    HasGraphics = TRUE;

    return TRUE;
}

static BOOLEAN LocateGop(VOID)
{
    EFI_STATUS Status;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop;
    EFI_HANDLE *HandleBuffer;
    UINTN HandleCount;
    UINTN Index;

    Gop = NULL;
    if (gST != NULL && gST->ConsoleOutHandle != NULL) {
        Status = gBS->HandleProtocol(gST->ConsoleOutHandle, &gEfiGraphicsOutputProtocolGuid,
                                     (VOID **)&Gop);
        if (!EFI_ERROR(Status) && UseGop(Gop)) {
            GopHandle = gST->ConsoleOutHandle;
            return TRUE;
        }
    }

    HandleBuffer = NULL;
    Status = gBS->LocateHandleBuffer(ByProtocol, &gEfiGraphicsOutputProtocolGuid, NULL,
                                     &HandleCount, &HandleBuffer);
    if (EFI_ERROR(Status)) {
        return FALSE;
    }

    for (Index = 0; Index < HandleCount; Index++) {
        Gop = NULL;
        Status = gBS->HandleProtocol(HandleBuffer[Index], &gEfiGraphicsOutputProtocolGuid,
                                     (VOID **)&Gop);
        if (!EFI_ERROR(Status) && UseGop(Gop)) {
            GopHandle = HandleBuffer[Index];
            MRD_FREE_POOL(HandleBuffer);
            return TRUE;
        }
    }

    MRD_FREE_POOL(HandleBuffer);
    return FALSE;
}

static VOID LocateConsoleControl(VOID)
{
    EFI_STATUS Status;

    ConsoleControl = NULL;
    Status = gBS->LocateProtocol(&ConsoleControlProtocolGuid, NULL, (VOID **)&ConsoleControl);
    if (EFI_ERROR(Status)) {
        ConsoleControl = NULL;
    }
}

UINTN egCountAppleFramebuffers(VOID)
{
    EFI_STATUS Status;
    EFI_HANDLE *HandleBuffer;
    UINTN HandleCount;

    if (!AppleFirmware) {
        return 0;
    }

    HandleBuffer = NULL;
    Status = gBS->LocateHandleBuffer(ByProtocol, &gAppleFramebufferInfoProtocolGuid, NULL,
                                     &HandleCount, &HandleBuffer);
    if (EFI_ERROR(Status)) {
        return 0;
    }

    MRD_FREE_POOL(HandleBuffer);
    return HandleCount;
}

BOOLEAN egInitUGADraw(BOOLEAN LogOutput)
{
    EFI_STATUS Status;
    EFI_UGA_DRAW_PROTOCOL *Uga;
    UINT32 Width;
    UINT32 Height;
    UINT32 Depth;
    UINT32 RefreshRate;

    (VOID) LogOutput;

    Uga = NULL;
    if (gST != NULL && gST->ConsoleOutHandle != NULL) {
        Status =
            gBS->HandleProtocol(gST->ConsoleOutHandle, &gEfiUgaDrawProtocolGuid, (VOID **)&Uga);
        if (!EFI_ERROR(Status)) {
            UGADraw = Uga;
        }
    }

    if (UGADraw == NULL) {
        Status = gBS->LocateProtocol(&gEfiUgaDrawProtocolGuid, NULL, (VOID **)&UGADraw);
        if (EFI_ERROR(Status)) {
            UGADraw = NULL;
            return FALSE;
        }
    }

    Width = Height = Depth = RefreshRate = 0;
    Status = UGADraw->GetMode(UGADraw, &Width, &Height, &Depth, &RefreshRate);
    if (EFI_ERROR(Status) || Width == 0 || Height == 0) {
        UGADraw = NULL;
        return FALSE;
    }

    DisplayWidth = Width;
    DisplayHeight = Height;
    HasGraphics = TRUE;

    return TRUE;
}

VOID egInitScreen(VOID)
{
    GOPDraw = NULL;
    UGADraw = NULL;
    HasGraphics = FALSE;
    GraphicsEnabled = FALSE;
    GopHandle = NULL;

    LocateConsoleControl();

    if (LocateGop()) {
        return;
    }

    egInitUGADraw(FALSE);
}

VOID egGetScreenSize(OUT UINTN *ScreenWidth, OUT UINTN *ScreenHeight)
{
    if (GOPDraw != NULL) {
        UpdateGopSize(GOPDraw);
    }

    if (ScreenWidth != NULL) {
        *ScreenWidth = DisplayWidth;
    }
    if (ScreenHeight != NULL) {
        *ScreenHeight = DisplayHeight;
    }
}

BOOLEAN egHasGraphicsMode(VOID) { return HasGraphics; }

BOOLEAN egGetResFromMode(UINTN *ModeWidth, UINTN *Height)
{
    EFI_STATUS Status;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info;
    UINTN Size;

    if (GOPDraw == NULL || ModeWidth == NULL || Height == NULL) {
        return FALSE;
    }

    Info = NULL;
    Status = GOPDraw->QueryMode(GOPDraw, (UINT32)*ModeWidth, &Size, &Info);
    if (EFI_ERROR(Status) || Info == NULL) {
        return FALSE;
    }

    *ModeWidth = Info->HorizontalResolution;
    *Height = Info->VerticalResolution;
    MRD_FREE_POOL(Info);

    return TRUE;
}

BOOLEAN egSetScreenSize(IN OUT UINTN *ScreenWidth, IN OUT UINTN *ScreenHeight)
{
    EFI_STATUS Status;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info;
    UINTN Size;
    UINT32 Mode;

    if (GOPDraw == NULL || ScreenWidth == NULL || ScreenHeight == NULL) {
        return FALSE;
    }

    if (*ScreenHeight == 0) {
        Mode = (UINT32)*ScreenWidth;
        if (Mode >= GOPDraw->Mode->MaxMode) {
            return FALSE;
        }

        Status = GOPDraw->SetMode(GOPDraw, Mode);
        if (EFI_ERROR(Status)) {
            return FALSE;
        }

        UpdateGopSize(GOPDraw);
        *ScreenWidth = DisplayWidth;
        *ScreenHeight = DisplayHeight;
        return TRUE;
    }

    for (Mode = 0; Mode < GOPDraw->Mode->MaxMode; Mode++) {
        Info = NULL;
        Status = GOPDraw->QueryMode(GOPDraw, Mode, &Size, &Info);
        if (EFI_ERROR(Status) || Info == NULL) {
            continue;
        }

        if (Info->HorizontalResolution == *ScreenWidth &&
            Info->VerticalResolution == *ScreenHeight) {
            MRD_FREE_POOL(Info);
            Status = GOPDraw->SetMode(GOPDraw, Mode);
            if (EFI_ERROR(Status)) {
                return FALSE;
            }

            UpdateGopSize(GOPDraw);
            *ScreenWidth = DisplayWidth;
            *ScreenHeight = DisplayHeight;
            return TRUE;
        }

        MRD_FREE_POOL(Info);
    }

    return FALSE;
}

BOOLEAN egSetNativeResolution(VOID)
{
    EFI_STATUS Status;
    EFI_EDID_ACTIVE_PROTOCOL *EdidActive;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info;
    UINT8 *Edid;
    UINTN Size;
    UINT32 Mode;
    UINT32 BestMode;
    UINT64 BestArea;
    UINT32 CurW;
    UINT32 CurH;
    UINT32 NativeW;
    UINT32 NativeH;

    if (GOPDraw == NULL || GOPDraw->Mode == NULL || GOPDraw->Mode->Info == NULL ||
        GOPDraw->QueryMode == NULL || GOPDraw->SetMode == NULL || GopHandle == NULL) {
#if MERIDIAN_DEBUG > 0
        INFO_LOG("MRD-DISPLAY mode-set skip: %a",
                 (GOPDraw == NULL) ? "no GOP" : "no GOP handle recorded");
#endif
        return FALSE;
    }

    EdidActive = NULL;
    Status = gBS->HandleProtocol(GopHandle, &gEfiEdidActiveProtocolGuid, (VOID **)&EdidActive);
    if (EFI_ERROR(Status) || EdidActive == NULL || EdidActive->SizeOfEdid < 128 ||
        EdidActive->Edid == NULL) {
        EdidActive = NULL;
        Status =
            gBS->HandleProtocol(GopHandle, &gEfiEdidDiscoveredProtocolGuid, (VOID **)&EdidActive);
    }
    if (EFI_ERROR(Status) || EdidActive == NULL || EdidActive->SizeOfEdid < 128 ||
        EdidActive->Edid == NULL) {
#if MERIDIAN_DEBUG > 0
        INFO_LOG("MRD-DISPLAY mode-set skip: no usable EDID on the GOP handle (%r)", Status);
#endif
        return FALSE;
    }

    Edid = EdidActive->Edid;
    if (Edid[54] == 0 && Edid[55] == 0) {
        return FALSE;
    }
    NativeW = (UINT32)(Edid[56] | ((Edid[58] >> 4) << 8));
    NativeH = (UINT32)(Edid[59] | ((Edid[61] >> 4) << 8));
    if (NativeW == 0 || NativeH == 0) {
        return FALSE;
    }

    CurW = GOPDraw->Mode->Info->HorizontalResolution;
    CurH = GOPDraw->Mode->Info->VerticalResolution;

    if (CurW >= NativeW && CurH >= NativeH) {
        return FALSE;
    }

    BestMode = GOPDraw->Mode->MaxMode;
    BestArea = (UINT64)CurW * CurH;
    for (Mode = 0; Mode < GOPDraw->Mode->MaxMode; Mode++) {
        Info = NULL;
        Status = GOPDraw->QueryMode(GOPDraw, Mode, &Size, &Info);
        if (EFI_ERROR(Status) || Info == NULL || Size < sizeof(*Info)) {
            MRD_FREE_POOL(Info);
            continue;
        }

        if (Info->HorizontalResolution > NativeW || Info->VerticalResolution > NativeH ||
            Info->HorizontalResolution < CurW || Info->VerticalResolution < CurH ||
            (Info->HorizontalResolution == CurW && Info->VerticalResolution == CurH)) {
            MRD_FREE_POOL(Info);
            continue;
        }

        if (Info->HorizontalResolution == NativeW && Info->VerticalResolution == NativeH) {

            BestMode = Mode;
            MRD_FREE_POOL(Info);
            break;
        }

        if ((UINT64)Info->HorizontalResolution * Info->VerticalResolution > BestArea) {
            BestMode = Mode;
            BestArea = (UINT64)Info->HorizontalResolution * Info->VerticalResolution;
        }

        MRD_FREE_POOL(Info);
    }

    if (BestMode >= GOPDraw->Mode->MaxMode) {
#if MERIDIAN_DEBUG > 0
        INFO_LOG("MRD-DISPLAY mode-set skip: no candidate mode (cur %dx%d, native %dx%d, "
                 "MaxMode %d)",
                 CurW, CurH, NativeW, NativeH, GOPDraw->Mode->MaxMode);
#endif
        return FALSE;
    }

    Status = GOPDraw->SetMode(GOPDraw, BestMode);
    if (EFI_ERROR(Status)) {
#if MERIDIAN_DEBUG > 0
        INFO_LOG("MRD-DISPLAY mode-set skip: SetMode(%d) failed (%r)", BestMode, Status);
#endif
        return FALSE;
    }

    UpdateGopSize(GOPDraw);

#if MERIDIAN_DEBUG > 0
    INFO_LOG("MRD-DISPLAY mode-set: GOP mode %d -> %dx%d (EDID native %dx%d, was %dx%d)", BestMode,
             DisplayWidth, DisplayHeight, NativeW, NativeH, CurW, CurH);
#endif

    return TRUE;
}

BOOLEAN egSetTextMode(UINT32 RequestedMode)
{
    if (gST == NULL || gST->ConOut == NULL) {
        return FALSE;
    }

    if (RequestedMode == DONT_CHANGE_TEXT_MODE ||
        RequestedMode == (UINT32)gST->ConOut->Mode->Mode) {
        return FALSE;
    }

    return EFI_ERROR(gST->ConOut->SetMode(gST->ConOut, RequestedMode)) ? FALSE : TRUE;
}

BOOLEAN egIsGraphicsModeEnabled(VOID)
{
    EFI_CONSOLE_CONTROL_SCREEN_MODE CurrentMode;

    if (ConsoleControl == NULL) {
        return GraphicsEnabled;
    }

    if (EFI_ERROR(ConsoleControl->GetMode(ConsoleControl, &CurrentMode, NULL, NULL))) {
        return GraphicsEnabled;
    }

    return (CurrentMode == EfiConsoleControlScreenGraphics) ? TRUE : FALSE;
}

VOID egSetGraphicsModeEnabled(IN BOOLEAN Enable)
{
    EFI_CONSOLE_CONTROL_SCREEN_MODE NewMode;

    GraphicsEnabled = Enable;

    if (ConsoleControl == NULL) {
        return;
    }

    NewMode = Enable ? EfiConsoleControlScreenGraphics : EfiConsoleControlScreenText;

    ConsoleControl->SetMode(ConsoleControl, NewMode);
}

VOID egClearScreen(IN EG_PIXEL *Color)
{
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL GopPixel;
    EFI_UGA_PIXEL UgaPixel;

    if (!HasGraphics) {
        if (gST != NULL && gST->ConOut != NULL) {
            gST->ConOut->SetAttribute(gST->ConOut, ATTR_BASIC);
            gST->ConOut->ClearScreen(gST->ConOut);
        }
        return;
    }

    GopPixel.Blue = (Color != NULL) ? Color->b : 0;
    GopPixel.Green = (Color != NULL) ? Color->g : 0;
    GopPixel.Red = (Color != NULL) ? Color->r : 0;
    GopPixel.Reserved = 0;

    if (GOPDraw != NULL) {
        GOPDraw->Blt(GOPDraw, &GopPixel, EfiBltVideoFill, 0, 0, 0, 0, DisplayWidth, DisplayHeight,
                     0);
        return;
    }

    if (UGADraw != NULL) {
        UgaPixel.Blue = GopPixel.Blue;
        UgaPixel.Green = GopPixel.Green;
        UgaPixel.Red = GopPixel.Red;
        UgaPixel.Reserved = 0;
        UGADraw->Blt(UGADraw, &UgaPixel, EfiUgaVideoFill, 0, 0, 0, 0, DisplayWidth, DisplayHeight,
                     0);
    }
}

VOID egDisplayMessage(CHAR16 *Text, EG_PIXEL *MessageBG, UINTN PositionCode, UINTN PauseLength,
                      CHAR16 *PauseType OPTIONAL)
{
    UINTN Row;

    (VOID) MessageBG;
    (VOID) PauseType;

    if (Text == NULL || gST == NULL || gST->ConOut == NULL) {
        return;
    }

    switch (PositionCode) {
    case TOP:
        Row = 1;
        break;
    case CENTER:
        Row = ConHeight / 2;
        break;
    case BOTTOM:
        Row = (ConHeight > 2) ? ConHeight - 2 : 0;
        break;
    default:
        Row = gST->ConOut->Mode->CursorRow + 1;
        break;
    }

    if (Row >= ConHeight) {
        Row = 0;
    }

    gST->ConOut->SetCursorPosition(gST->ConOut, 0, Row);
    gST->ConOut->OutputString(gST->ConOut, Text);
    gST->ConOut->OutputString(gST->ConOut, L"\r\n");

    if (PauseLength > 0) {
        gBS->Stall(PauseLength * 1000000);
    }
}
