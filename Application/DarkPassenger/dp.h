// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#ifndef MERIDIAN_DARK_PASSENGER_H
#define MERIDIAN_DARK_PASSENGER_H

#include <Uefi.h>
#include <Protocol/PciIo.h>

#define LPC_RCBA 0xF0
#define RCBA_BASE_MASK 0xFFFFC000u
#define RCBA_EN (1u << 0)

typedef struct
{
    CHAR8 *Buf;
    UINTN Off;
    UINTN Cap;
    BOOLEAN AllowActivate;
    EFI_PCI_IO_PROTOCOL *Lpc;
    UINTN RcbaBase;
} DP_CTX;

typedef struct
{
    const CHAR8 *Name;
    const CHAR8 *Desc;
    BOOLEAN (*Applies)(DP_CTX *Ctx);
    EFI_STATUS (*Recon)(DP_CTX *Ctx);
    EFI_STATUS (*Activate)(DP_CTX *Ctx);
    EFI_STATUS (*Describe)(DP_CTX *Ctx);
} DARK_PASSENGER;

VOID EFIAPI DpEmit(DP_CTX *Ctx, const CHAR8 *Fmt, ...);

EFI_STATUS DpSaveFile(CHAR16 *FileName, UINT8 *Data, UINTN Size);

VOID DpDumpAcpi(DP_CTX *Ctx);

EFI_STATUS DpInstallAcpiTable(DP_CTX *Ctx, const VOID *Table, UINTN Size);

EFI_PCI_IO_PROTOCOL *DpFindFn(UINTN Bus, UINTN Dev, UINTN Fn);

BOOLEAN DpIobpPoll(UINTN Base);
UINT32 DpIobpRead(UINTN Base, UINT32 Address, BOOLEAN *Ok);
VOID DpIobpWrite(UINTN Base, UINT32 Address, UINT32 Data);
VOID DpIobpClearBit(UINTN Base, UINT32 Address, UINT32 Bit);

UINT32 DpPciMem32Top(VOID);

BOOLEAN DpDwI2cPing(UINT32 Bar, UINT8 Addr);
VOID DpDwI2cProbe(DP_CTX *Ctx, EFI_PCI_IO_PROTOCOL *P, UINT32 Bar, const CHAR8 *Name);

extern const DARK_PASSENGER *const gDpRegistry[];
extern const UINTN gDpRegistryCount;

extern const DARK_PASSENGER gDpLpssLynxPoint;

#endif
