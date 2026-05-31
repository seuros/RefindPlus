// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#ifndef __MERIDIAN_DISPLAY_H_
#define __MERIDIAN_DISPLAY_H_

#include "tiano_includes.h"
#include "display_types.h"

#define EFI_UGA_DRAW_PROTOCOL_GUID                                                                 \
    {0x982c298b, 0xf4fa, 0x41cb, {0xb8, 0x38, 0x77, 0xaa, 0x68, 0x8f, 0xb8, 0x39}}

#define EFI_CONSOLE_CONTROL_PROTOCOL_GUID                                                          \
    {0xf42f7782, 0x012e, 0x4c12, {0x99, 0x56, 0x49, 0xf9, 0x43, 0x04, 0xf7, 0x21}}

typedef struct _EFI_UGA_DRAW_PROTOCOL EFI_UGA_DRAW_PROTOCOL;

typedef EFI_STATUS(EFIAPI *EFI_UGA_DRAW_PROTOCOL_GET_MODE)(IN EFI_UGA_DRAW_PROTOCOL *This,
                                                           OUT UINT32 *HorizontalResolution,
                                                           OUT UINT32 *VerticalResolution,
                                                           OUT UINT32 *ColorDepth,
                                                           OUT UINT32 *RefreshRate);

typedef EFI_STATUS(EFIAPI *EFI_UGA_DRAW_PROTOCOL_SET_MODE)(IN EFI_UGA_DRAW_PROTOCOL *This,
                                                           IN UINT32 HorizontalResolution,
                                                           IN UINT32 VerticalResolution,
                                                           IN UINT32 ColorDepth,
                                                           IN UINT32 RefreshRate);

typedef struct
{
    UINT8 Blue;
    UINT8 Green;
    UINT8 Red;
    UINT8 Reserved;
} EFI_UGA_PIXEL;

typedef enum
{
    EfiUgaVideoFill,
    EfiUgaVideoToBltBuffer,
    EfiUgaBltBufferToVideo,
    EfiUgaVideoToVideo,
    EfiUgaBltMax
} EFI_UGA_BLT_OPERATION;

typedef EFI_STATUS(EFIAPI *EFI_UGA_DRAW_PROTOCOL_BLT)(IN EFI_UGA_DRAW_PROTOCOL *This,
                                                      IN EFI_UGA_PIXEL *BltBuffer OPTIONAL,
                                                      IN EFI_UGA_BLT_OPERATION BltOperation,
                                                      IN UINTN SourceX, IN UINTN SourceY,
                                                      IN UINTN DestinationX, IN UINTN DestinationY,
                                                      IN UINTN Width, IN UINTN Height,
                                                      IN UINTN Delta OPTIONAL);

struct _EFI_UGA_DRAW_PROTOCOL
{
    EFI_UGA_DRAW_PROTOCOL_GET_MODE GetMode;
    EFI_UGA_DRAW_PROTOCOL_SET_MODE SetMode;
    EFI_UGA_DRAW_PROTOCOL_BLT Blt;
};

typedef struct _EFI_CONSOLE_CONTROL_PROTOCOL EFI_CONSOLE_CONTROL_PROTOCOL;

typedef enum
{
    EfiConsoleControlScreenText,
    EfiConsoleControlScreenGraphics,
    EfiConsoleControlScreenMaxValue
} EFI_CONSOLE_CONTROL_SCREEN_MODE;

typedef EFI_STATUS(EFIAPI *EFI_CONSOLE_CONTROL_PROTOCOL_GET_MODE)(
    IN EFI_CONSOLE_CONTROL_PROTOCOL *This, OUT EFI_CONSOLE_CONTROL_SCREEN_MODE *Mode,
    OUT BOOLEAN *UgaExists OPTIONAL, OUT BOOLEAN *StdInLocked OPTIONAL);

typedef EFI_STATUS(EFIAPI *EFI_CONSOLE_CONTROL_PROTOCOL_SET_MODE)(
    IN EFI_CONSOLE_CONTROL_PROTOCOL *This, IN EFI_CONSOLE_CONTROL_SCREEN_MODE Mode);

typedef EFI_STATUS(EFIAPI *EFI_CONSOLE_CONTROL_PROTOCOL_LOCK_STD_IN)(
    IN EFI_CONSOLE_CONTROL_PROTOCOL *This, IN CHAR16 *Password);

struct _EFI_CONSOLE_CONTROL_PROTOCOL
{
    EFI_CONSOLE_CONTROL_PROTOCOL_GET_MODE GetMode;
    EFI_CONSOLE_CONTROL_PROTOCOL_SET_MODE SetMode;
    EFI_CONSOLE_CONTROL_PROTOCOL_LOCK_STD_IN LockStdIn;
};

extern EFI_GUID gEfiUgaDrawProtocolGuid;

extern EFI_UGA_DRAW_PROTOCOL *UGADraw;
extern EFI_GRAPHICS_OUTPUT_PROTOCOL *GOPDraw;

BOOLEAN egHasGraphicsMode(VOID);
BOOLEAN egIsGraphicsModeEnabled(VOID);
BOOLEAN egSetTextMode(UINT32 RequestedMode);
BOOLEAN egGetResFromMode(UINTN *ModeWidth, UINTN *Height);
BOOLEAN egSetScreenSize(IN OUT UINTN *ScreenWidth, IN OUT UINTN *ScreenHeight);
BOOLEAN egSetNativeResolution(VOID);
BOOLEAN egInitUGADraw(BOOLEAN LogOutput);
UINTN egCountAppleFramebuffers(VOID);
VOID egInitScreen(VOID);
VOID egGetScreenSize(OUT UINTN *ScreenWidth, OUT UINTN *ScreenHeight);
VOID egSetGraphicsModeEnabled(IN BOOLEAN Enable);
VOID egClearScreen(IN EG_PIXEL *Color);
VOID egDisplayMessage(CHAR16 *Text, EG_PIXEL *MessageBG, UINTN PositionCode, UINTN PauseLength,
                      CHAR16 *PauseType OPTIONAL);

#endif
