// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#include "conn_core.h"
#include "efi_env.h"
#include "shim.h"
#include "png.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : "conn-host-png.png";
    int ti = argc > 2 ? atoi(argv[2]) : 0;
    if (ti < 0 || ti >= CONN_THEME_COUNT) {
        fprintf(stderr, "theme index must be 0..%d\n", CONN_THEME_COUNT - 1);
        return 1;
    }

    UINTN w = conn_text_width(), h = conn_text_height();
    ConnFw *fw = conn_host_init(w, h);
    if (!fw) { fprintf(stderr, "shim init failed\n"); return 1; }

    EFI_STATUS st = ConnUiRunThemed(fw, conn_themes[ti]);
    if (EFI_ERROR(st)) {
        fprintf(stderr, "ConnUiRunThemed failed: 0x%lx\n", (unsigned long)st);
        conn_host_free(fw);
        return 1;
    }

    UINTN fw_w, fw_h;
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL *fb = conn_host_framebuffer(fw, &fw_w, &fw_h);

    unsigned char *rgba = (unsigned char *)malloc(fw_w * fw_h * 4);
    if (!rgba) { conn_host_free(fw); return 1; }
    for (UINTN i = 0; i < fw_w * fw_h; i++) {
        rgba[i*4+0] = fb[i].Red;
        rgba[i*4+1] = fb[i].Green;
        rgba[i*4+2] = fb[i].Blue;
        rgba[i*4+3] = 0xFF;
    }

    int err = conn_write_png(path, rgba, (int)fw_w, (int)fw_h);
    free(rgba);
    conn_host_free(fw);
    if (err) { fprintf(stderr, "png write failed\n"); return 1; }
    printf("wrote %s (%lux%lu, %s, via ConnUiRunThemed/GOP Blt)\n",
           path, (unsigned long)fw_w, (unsigned long)fw_h, conn_themes[ti]->name);
    return 0;
}
