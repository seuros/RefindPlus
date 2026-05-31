// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#include "conn_core.h"
#include "efi_env.h"
#include <string.h>

typedef struct {
    const char *name, *sub, *part, *loader;
    ConnColor   accent;
    BOOLEAN     locked;
} MockEntry;

static const MockEntry MOCK[] = {
    {"MACOS TAHOE", "26.6.2 \xC2\xB7 APFS \xC2\xB7 MACINTOSH", "D0 P2", "boot.efi", CONN_ICE, FALSE},
    {"ARCH LINUX", "LINUX 7.1.10 \xC2\xB7 EXT4", "D0 P3", "grubx64.efi", CONN_SKY, FALSE},
    {"OMARCHY BY DHH", "4 QUATTRO \xC2\xB7 BTRFS LUKS", "D0 P4", "limine.efi", CONN_CORAL, FALSE},
    {"WINDOWS 11", "25H2 \xC2\xB7 NTFS BOOTMGR", "D0 P1", "bootmgfw.efi", CONN_LAV, TRUE},
    {"FREEBSD", "16.0-CURRENT \xC2\xB7 ZFS", "D0 P5", "loader.efi", CONN_RED, FALSE},
    {"NETBSD", "11.99 CURRENT \xC2\xB7 FFS", "D1 P1", "bootx64.efi", CONN_AMBER, FALSE},
    {"OPENBSD", "7.9 PINKPUFFY \xC2\xB7 FFS", "D1 P2", "bootx64.efi", CONN_HONEY, FALSE},
    {"DRAGONFLY BSD", "6.4.2 \xC2\xB7 HAMMER2", "D1 P3", "loader.efi", CONN_COBALT, FALSE},
    {"HAIKU", "R1/BETA6 \xC2\xB7 BFS SYSTEM", "D1 P4", "haiku_loader.efi", CONN_GOLD, FALSE},
    {"9FRONT", "THIS WAS SUPPOSED TO BE FUN", "D1 P5", "bootx64.efi", CONN_PEACH, FALSE},
};
#define NMOCK ((UINTN)(sizeof(MOCK) / sizeof(MOCK[0])))

static void copy(char *dst, UINTN cap, const char *src) {
    strncpy(dst, src, cap - 1);
    dst[cap - 1] = '\0';
}

EFI_STATUS conn_scan(ConnFw *Fw, ConnEntryList *out, ConnBootTarget *targets) {
    (void)Fw;
    UINTN n = NMOCK < CONN_MAX_ENTRIES ? NMOCK : CONN_MAX_ENTRIES;
    out->rng_nonce[0] = '\0';
    out->rng_source[0] = '\0';
    for (UINTN i = 0; i < n; i++) {
        ConnEntry *e = &out->entry[i];
        copy(e->name,   sizeof e->name,   MOCK[i].name);
        copy(e->sub,    sizeof e->sub,    MOCK[i].sub);
        copy(e->part,   sizeof e->part,   MOCK[i].part);
        copy(e->loader, sizeof e->loader, MOCK[i].loader);
        e->accent = MOCK[i].accent;
        e->locked = MOCK[i].locked;
        targets[i].volume = NULL;
        targets[i].path[0] = 0;
        targets[i].has_guid = FALSE;
    }
    out->count = n;
    out->default_index = 0;
    out->allow_autoboot = (n > 0);
    return EFI_SUCCESS;
}
