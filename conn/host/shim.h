// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#ifndef CONN_HOST_SHIM_H
#define CONN_HOST_SHIM_H

#include "efi_env.h"

typedef struct ConnHost ConnHost;

ConnFw *conn_host_init(UINTN w, UINTN h);
void    conn_host_free(ConnFw *fw);

EFI_GRAPHICS_OUTPUT_BLT_PIXEL *conn_host_framebuffer(ConnFw *fw, UINTN *w, UINTN *h);

#endif
