// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#include "smc.h"
#include "../sysinfo.h"

#include <Library/IoLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>

#define APPLE_SMC_IO_PROTOCOL_GUID                                                                 \
    {0xB91867EA, 0x9ED9, 0x4B71, {0xA0, 0xAC, 0x3B, 0x8B, 0x4D, 0x2F, 0x0C, 0xB9}}

typedef struct _APPLE_SMC_IO_PROTOCOL APPLE_SMC_IO_PROTOCOL;
typedef EFI_STATUS(EFIAPI *APPLE_SMC_READ_VALUE)(IN APPLE_SMC_IO_PROTOCOL *This, IN UINT32 Key,
                                                 IN UINT8 Size, OUT UINT8 *Value);

struct _APPLE_SMC_IO_PROTOCOL
{
    UINT64 Version;
    VOID *Unknown1;
    VOID *Unknown2;
    APPLE_SMC_READ_VALUE ReadValue;
};

#define SMC_MMIO_DATA 0x0000
#define SMC_MMIO_KEY_NAME 0x0078
#define SMC_MMIO_DATA_LEN 0x007D
#define SMC_MMIO_SMC_ID 0x007E
#define SMC_MMIO_CMD 0x007F
#define SMC_MMIO_STATUS 0x4005
#define SMC_MMIO_READY 0x20
#define SMC_MMIO_MAX_WAIT 24

#define SMC_MMIO_BASE_CANDIDATE 0xFE0B0000ULL

#define SMC_PORT_BASE 0x300
#define SMC_PORT_DATA 0x00
#define SMC_PORT_CMD 0x04
#define SMC_PORT_STMASK 0x0f
#define SMC_PORT_ST_ACK 0x0c
#define SMC_PORT_ST_AWAIT 0x04
#define SMC_PORT_ST_READY 0x05

#define SMC_CMD_READ 0x10
#define SMC_CMD_WRITE 0x11

#define MBSS_RESTART 0x03
#define MBSS_POWEROFF 0x01

static struct
{
    BOOLEAN Init;
    APPLE_SMC_IO_PROTOCOL *Proto;
    UINTN MmioBase;
    BOOLEAN Port;
} gSmc;

static BOOLEAN MmioWait(UINTN Base)
{
    UINTN i, Delay = 10;
    for (i = 0; i < SMC_MMIO_MAX_WAIT; i++) {
        if (MmioRead8(Base + SMC_MMIO_STATUS) & SMC_MMIO_READY) {
            return TRUE;
        }
        gBS->Stall(Delay);
        if (Delay < 3200) {
            Delay *= 2;
        }
    }
    return FALSE;
}

static BOOLEAN MmioReadKey(UINTN Base, CONST CHAR8 Key[4], UINT8 *Buf, UINT8 Len)
{
    UINT32 KeyInt;
    UINT8 i, RLen;

    if (MmioRead8(Base + SMC_MMIO_STATUS)) {
        MmioWrite8(Base + SMC_MMIO_STATUS, 0);
    }
    CopyMem(&KeyInt, Key, 4);
    MmioWrite32(Base + SMC_MMIO_KEY_NAME, KeyInt);
    MmioWrite8(Base + SMC_MMIO_SMC_ID, 0);
    MmioWrite8(Base + SMC_MMIO_CMD, SMC_CMD_READ);
    if (!MmioWait(Base) || MmioRead8(Base + SMC_MMIO_CMD) != 0) {
        return FALSE;
    }
    RLen = MmioRead8(Base + SMC_MMIO_DATA_LEN);
    if (RLen > Len) {
        RLen = Len;
    }
    for (i = 0; i < RLen; i++) {
        Buf[i] = MmioRead8(Base + SMC_MMIO_DATA + i);
    }
    return TRUE;
}

static BOOLEAN MmioWriteKey(UINTN Base, CONST CHAR8 Key[4], CONST UINT8 *Buf, UINT8 Len)
{
    UINT32 KeyInt;
    UINT8 i;

    MmioWrite8(Base + SMC_MMIO_STATUS, 0);
    for (i = 0; i < Len; i++) {
        MmioWrite8(Base + SMC_MMIO_DATA + i, Buf[i]);
    }
    CopyMem(&KeyInt, Key, 4);
    MmioWrite32(Base + SMC_MMIO_KEY_NAME, KeyInt);
    MmioWrite8(Base + SMC_MMIO_DATA_LEN, Len);
    MmioWrite8(Base + SMC_MMIO_SMC_ID, 0);
    MmioWrite8(Base + SMC_MMIO_CMD, SMC_CMD_WRITE);
    if (!MmioWait(Base)) {
        return FALSE;
    }
    return MmioRead8(Base + SMC_MMIO_CMD) == 0;
}

static BOOLEAN MmioIsT2(UINTN Base)
{
    UINT8 Ldkn = 0;
    if (MmioRead8(Base + SMC_MMIO_STATUS) == 0xFF) {
        return FALSE;
    }
    return MmioReadKey(Base, "LDKN", &Ldkn, 1) && Ldkn >= 2;
}

static BOOLEAN PortWait(UINT8 Want, UINTN Tries)
{
    UINTN i;
    Want &= SMC_PORT_STMASK;
    for (i = 0; i < Tries; i++) {
        if ((IoRead8(SMC_PORT_BASE + SMC_PORT_CMD) & SMC_PORT_STMASK) == Want) {
            return TRUE;
        }
        gBS->Stall(10);
    }
    return FALSE;
}

static BOOLEAN PortCommand(UINT8 Cmd)
{
    UINTN i;
    for (i = 0; i < 10; i++) {
        IoWrite8(SMC_PORT_BASE + SMC_PORT_CMD, Cmd);
        if (PortWait(SMC_PORT_ST_ACK, 100)) {
            return TRUE;
        }
    }
    return FALSE;
}

static BOOLEAN PortSendKeyBytes(CONST CHAR8 Key[4])
{
    UINTN i;
    for (i = 0; i < 4; i++) {
        IoWrite8(SMC_PORT_BASE + SMC_PORT_DATA, (UINT8)Key[i]);
        if (!PortWait(SMC_PORT_ST_AWAIT, 1000)) {
            return FALSE;
        }
    }
    return TRUE;
}

static BOOLEAN PortReadKey(CONST CHAR8 Key[4], UINT8 *Buf, UINT8 Len)
{
    UINTN Try, i;
    for (Try = 0; Try < 10; Try++) {
        if (!PortCommand(SMC_CMD_READ)) {
            continue;
        }
        if (!PortSendKeyBytes(Key)) {
            goto retry;
        }
        IoWrite8(SMC_PORT_BASE + SMC_PORT_DATA, Len);
        for (i = 0; i < Len; i++) {
            if (!PortWait(SMC_PORT_ST_READY, 1000)) {
                goto retry;
            }
            Buf[i] = IoRead8(SMC_PORT_BASE + SMC_PORT_DATA);
        }
        return TRUE;
    retry:;
    }
    return FALSE;
}

static BOOLEAN PortWriteKey(CONST CHAR8 Key[4], CONST UINT8 *Buf, UINT8 Len)
{
    UINTN Try, i;
    for (Try = 0; Try < 10; Try++) {
        if (!PortCommand(SMC_CMD_WRITE)) {
            continue;
        }
        if (!PortSendKeyBytes(Key)) {
            goto retry;
        }
        IoWrite8(SMC_PORT_BASE + SMC_PORT_DATA, Len);
        if (!PortWait(SMC_PORT_ST_AWAIT, 1000)) {
            goto retry;
        }
        for (i = 0; i < Len; i++) {
            IoWrite8(SMC_PORT_BASE + SMC_PORT_DATA, Buf[i]);
            if (i + 1 < Len && !PortWait(SMC_PORT_ST_AWAIT, 1000)) {
                goto retry;
            }
        }
        return TRUE;
    retry:;
    }
    return FALSE;
}

static VOID SmcInit(VOID)
{
    EFI_GUID Guid = APPLE_SMC_IO_PROTOCOL_GUID;
    APPLE_SMC_IO_PROTOCOL *Proto = NULL;
    UINT8 Probe[4];

    if (gSmc.Init) {
        return;
    }
    SetMem(&gSmc, sizeof(gSmc), 0);
    gSmc.Init = TRUE;

    if (MeridianFirmwareVendor != FW_VENDOR_APPLE) {
        return;
    }

    if (!EFI_ERROR(gBS->LocateProtocol(&Guid, NULL, (VOID **)&Proto)) && Proto != NULL &&
        Proto->ReadValue != NULL) {
        if (!EFI_ERROR(Proto->ReadValue(Proto, APPLE_SMC_KEY('#', 'K', 'E', 'Y'), 4, Probe)) ||
            !EFI_ERROR(Proto->ReadValue(Proto, APPLE_SMC_KEY('R', 'E', 'V', ' '), 2, Probe))) {
            gSmc.Proto = Proto;
        }
    }

    if (MmioIsT2(SMC_MMIO_BASE_CANDIDATE)) {
        gSmc.MmioBase = (UINTN)SMC_MMIO_BASE_CANDIDATE;
    }

    if (PortReadKey("#KEY", Probe, 4)) {
        gSmc.Port = TRUE;
    }
}

APPLE_SMC_BACKEND MrdAppleSmcBackend(VOID)
{
    SmcInit();
    if (gSmc.Proto != NULL) {
        return APPLE_SMC_PROTOCOL;
    }
    if (gSmc.MmioBase != 0) {
        return APPLE_SMC_MMIO;
    }
    if (gSmc.Port) {
        return APPLE_SMC_PORT;
    }
    return APPLE_SMC_NONE;
}

BOOLEAN MrdAppleSmcReadKey(CONST CHAR8 Key[4], UINT8 *Buf, UINT8 Len)
{
    if (Len > APPLE_SMC_MAXVAL) {
        return FALSE;
    }
    SmcInit();
    SetMem(Buf, Len, 0);
    if (gSmc.Proto != NULL) {
        UINT32 K = APPLE_SMC_KEY(Key[0], Key[1], Key[2], Key[3]);
        return !EFI_ERROR(gSmc.Proto->ReadValue(gSmc.Proto, K, Len, Buf));
    }
    if (gSmc.MmioBase != 0) {
        return MmioReadKey(gSmc.MmioBase, Key, Buf, Len);
    }
    if (gSmc.Port) {
        return PortReadKey(Key, Buf, Len);
    }
    return FALSE;
}

BOOLEAN MrdAppleSmcWriteKey(CONST CHAR8 Key[4], CONST UINT8 *Buf, UINT8 Len)
{
    if (Len > APPLE_SMC_MAXVAL) {
        return FALSE;
    }
    SmcInit();

    if (gSmc.MmioBase != 0) {
        return MmioWriteKey(gSmc.MmioBase, Key, Buf, Len);
    }
    if (gSmc.Port) {
        return PortWriteKey(Key, Buf, Len);
    }
    return FALSE;
}

VOID MrdAppleSmcNotifyReset(BOOLEAN Restart)
{
    UINT8 Mbss[2];
    if (MeridianFirmwareVendor != FW_VENDOR_APPLE) {
        return;
    }
    Mbss[0] = 0x00;
    Mbss[1] = Restart ? MBSS_RESTART : MBSS_POWEROFF;

    MrdAppleSmcWriteKey("MBSS", Mbss, 2);
}
