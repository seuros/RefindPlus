// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#include "shim.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    EFI_GRAPHICS_OUTPUT_PROTOCOL              gop;
    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE         mode;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION      info;
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL            *fb;
    UINTN                                     w, h;
} HostGop;

struct ConnHost {
    ConnFw            fw;
    EFI_BOOT_SERVICES bs;
    HostGop           hg;
};

static EFI_STATUS EFIAPI host_alloc(EFI_MEMORY_TYPE type, UINTN size, VOID **buf) {
    (void)type;
    if (buf == NULL) return EFI_INVALID_PARAMETER;
    *buf = malloc(size ? size : 1);
    return *buf ? EFI_SUCCESS : EFI_OUT_OF_RESOURCES;
}
static EFI_STATUS EFIAPI host_free(VOID *buf) { free(buf); return EFI_SUCCESS; }

static EFI_STATUS EFIAPI host_query_mode(EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
        UINT32 ModeNumber, UINTN *SizeOfInfo,
        EFI_GRAPHICS_OUTPUT_MODE_INFORMATION **Info) {
    HostGop *hg = (HostGop *)This;
    if (ModeNumber != 0 || SizeOfInfo == NULL || Info == NULL)
        return EFI_INVALID_PARAMETER;
    *SizeOfInfo = sizeof hg->info;
    *Info = &hg->info;
    return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI host_set_mode(EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
        UINT32 ModeNumber) {
    (void)This;
    return ModeNumber == 0 ? EFI_SUCCESS : EFI_UNSUPPORTED;
}
static EFI_STATUS EFIAPI host_blt(EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
        EFI_GRAPHICS_OUTPUT_BLT_PIXEL *BltBuffer,
        EFI_GRAPHICS_OUTPUT_BLT_OPERATION Op,
        UINTN SrcX, UINTN SrcY, UINTN DstX, UINTN DstY,
        UINTN Width, UINTN Height, UINTN Delta) {
    HostGop *hg = (HostGop *)This;
    if (Width == 0 || Height == 0) return EFI_INVALID_PARAMETER;
    if (DstX + Width > hg->w || DstY + Height > hg->h) return EFI_INVALID_PARAMETER;
    UINTN stride = Delta ? Delta / sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL) : Width;

    switch (Op) {
    case EfiBltVideoFill:
        for (UINTN y = 0; y < Height; y++) {
            EFI_GRAPHICS_OUTPUT_BLT_PIXEL *d = hg->fb + (DstY + y) * hg->w + DstX;
            for (UINTN x = 0; x < Width; x++) d[x] = BltBuffer[0];
        }
        return EFI_SUCCESS;
    case EfiBltBufferToVideo:
        if (BltBuffer == NULL) return EFI_INVALID_PARAMETER;
        for (UINTN y = 0; y < Height; y++)
            memcpy(hg->fb + (DstY + y) * hg->w + DstX,
                   BltBuffer + (SrcY + y) * stride + SrcX,
                   Width * sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL));
        return EFI_SUCCESS;
    case EfiBltVideoToBltBuffer:
        if (BltBuffer == NULL) return EFI_INVALID_PARAMETER;
        for (UINTN y = 0; y < Height; y++)
            memcpy(BltBuffer + (DstY + y) * stride + DstX,
                   hg->fb + (SrcY + y) * hg->w + SrcX,
                   Width * sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL));
        return EFI_SUCCESS;
    default:
        return EFI_UNSUPPORTED;
    }
}

ConnFw *conn_host_init(UINTN w, UINTN h) {
    ConnHost *host = (ConnHost *)calloc(1, sizeof *host);
    if (!host) return NULL;
    host->hg.fb = (EFI_GRAPHICS_OUTPUT_BLT_PIXEL *)calloc(w * h, sizeof(*host->hg.fb));
    if (!host->hg.fb) { free(host); return NULL; }
    host->hg.w = w; host->hg.h = h;

    host->hg.info.Version = 0;
    host->hg.info.HorizontalResolution = (UINT32)w;
    host->hg.info.VerticalResolution   = (UINT32)h;
    host->hg.info.PixelFormat          = PixelBltOnly;
    host->hg.info.PixelsPerScanLine    = (UINT32)w;

    host->hg.mode.MaxMode        = 1;
    host->hg.mode.Mode           = 0;
    host->hg.mode.Info           = &host->hg.info;
    host->hg.mode.SizeOfInfo     = sizeof host->hg.info;
    host->hg.mode.FrameBufferBase = 0;
    host->hg.mode.FrameBufferSize = 0;

    host->hg.gop.QueryMode = host_query_mode;
    host->hg.gop.SetMode   = host_set_mode;
    host->hg.gop.Blt       = host_blt;
    host->hg.gop.Mode      = &host->hg.mode;

    host->bs.AllocatePool = host_alloc;
    host->bs.FreePool     = host_free;

    host->fw.BS  = &host->bs;
    host->fw.Gop = &host->hg.gop;
    return &host->fw;
}

void conn_host_free(ConnFw *fw) {
    if (!fw) return;
    ConnHost *host = (ConnHost *)fw;
    free(host->hg.fb);
    free(host);
}

EFI_GRAPHICS_OUTPUT_BLT_PIXEL *conn_host_framebuffer(ConnFw *fw, UINTN *w, UINTN *h) {
    ConnHost *host = (ConnHost *)fw;
    if (w) *w = host->hg.w;
    if (h) *h = host->hg.h;
    return host->hg.fb;
}
