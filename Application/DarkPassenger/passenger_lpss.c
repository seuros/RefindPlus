// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#include "dp.h"

#include <Library/UefiLib.h>
#include <Library/BaseLib.h>
#include <Library/IoLib.h>
#include <Library/UefiBootServicesTableLib.h>

#define LPC_ABASE 0x40
#define LPC_ACPI_CNTL 0x44
#define LPC_GPIOBASE 0x48
#define LPC_BIOS_CNTL 0xDC

#define RCBA_FD 0x3418
#define RCBA_FD2 0x3428

#define SIO_FUNCDIS_DIS (1u << 8)
#define SIO_PORTCTRL_CONF_DIS (1u << 20)
#define SIO_PORTCTRL0_CONF_DIS (1u << 4)

#define PCI_VENDOR_INTEL 0x8086
#define LPC_DID_LYNXPOINT_LP_MIN 0x9C40
#define LPC_DID_LYNXPOINT_LP_MAX 0x9C5F

#ifndef DP_ALLOW_REENABLE
#define DP_ALLOW_REENABLE 0
#endif

typedef struct
{
    UINT32 FuncDis;
    UINT32 PortCtrl;
    UINT32 ConfDis;
    const CHAR8 *Name;
} SIO_FD;

static const SIO_FD SioFd[] = {
    {0xce00aa07, 0xcb000240, SIO_PORTCTRL_CONF_DIS, "DMA   D21:F0"},
    {0xce00aa47, 0xcb000248, SIO_PORTCTRL_CONF_DIS, "I2C0  D21:F1"},
    {0xce00aa87, 0xcb000250, SIO_PORTCTRL_CONF_DIS, "I2C1  D21:F2"},
    {0xce00aac7, 0xcb000258, SIO_PORTCTRL_CONF_DIS, "SPI0  D21:F3"},
    {0xce00ab07, 0xcb000260, SIO_PORTCTRL_CONF_DIS, "SPI1  D21:F4"},
    {0xce00ab47, 0xcb000268, SIO_PORTCTRL_CONF_DIS, "UART0 D21:F5"},
    {0xce00ab87, 0xcb000270, SIO_PORTCTRL_CONF_DIS, "UART1 D21:F6"},
    {0xce00ae07, 0xcb000000, SIO_PORTCTRL0_CONF_DIS, "SDIO  D23:F0"},
};

typedef struct
{
    UINT8 Bus;
    UINT8 Dev;
    UINT8 Fn;
    const CHAR8 *Name;
} DARK_BDF;

static const DARK_BDF DarkList[] = {
    {0x00, 0x15, 0x0, "LPSS SDMA"},  {0x00, 0x15, 0x1, "LPSS I2C0"},
    {0x00, 0x15, 0x2, "LPSS I2C1"},  {0x00, 0x15, 0x3, "LPSS SPI0"},
    {0x00, 0x15, 0x4, "LPSS SPI1"},  {0x00, 0x15, 0x5, "LPSS UART0"},
    {0x00, 0x15, 0x6, "LPSS UART1"}, {0x00, 0x17, 0x0, "LPSS SDIO"},
    {0x00, 0x16, 0x1, "ME func1"},   {0x00, 0x1F, 0x6, "Thermal"},
};

static BOOLEAN LpssApplies(DP_CTX *Ctx)
{
    UINT32 Id;
    UINT16 Vid, Did;

    Ctx->Lpc = DpFindFn(0, 0x1F, 0);
    if (Ctx->Lpc == NULL) {
        return FALSE;
    }

    Id = 0;
    Ctx->Lpc->Pci.Read(Ctx->Lpc, EfiPciIoWidthUint32, 0, 1, &Id);
    Vid = (UINT16)(Id & 0xFFFF);
    Did = (UINT16)(Id >> 16);
    return (Vid == PCI_VENDOR_INTEL) && (Did >= LPC_DID_LYNXPOINT_LP_MIN) &&
           (Did <= LPC_DID_LYNXPOINT_LP_MAX);
}

static EFI_STATUS LpssRecon(DP_CTX *Ctx)
{
    EFI_PCI_IO_PROTOCOL *Lpc = Ctx->Lpc;
    EFI_PCI_IO_PROTOCOL *P;
    UINT32 Abase, AcpiCntl, GpioBase, Rcba, BiosCntl, Fd, Fd2, Vid;
    UINTN i;

    Abase = AcpiCntl = GpioBase = Rcba = BiosCntl = 0;
    Lpc->Pci.Read(Lpc, EfiPciIoWidthUint32, LPC_ABASE, 1, &Abase);
    Lpc->Pci.Read(Lpc, EfiPciIoWidthUint32, LPC_ACPI_CNTL, 1, &AcpiCntl);
    Lpc->Pci.Read(Lpc, EfiPciIoWidthUint32, LPC_GPIOBASE, 1, &GpioBase);
    Lpc->Pci.Read(Lpc, EfiPciIoWidthUint32, LPC_RCBA, 1, &Rcba);
    Lpc->Pci.Read(Lpc, EfiPciIoWidthUint32, LPC_BIOS_CNTL, 1, &BiosCntl);

    DpEmit(Ctx, "[DP] LPC ABASE=%08x ACPI_CNTL=%08x GPIOBASE=%08x RCBA=%08x BIOS_CNTL=%08x\n",
           Abase, AcpiCntl, GpioBase, Rcba, BiosCntl);

    if (Rcba & RCBA_EN) {
        Ctx->RcbaBase = (UINTN)(Rcba & RCBA_BASE_MASK);
        Fd = MmioRead32(Ctx->RcbaBase + RCBA_FD);
        Fd2 = MmioRead32(Ctx->RcbaBase + RCBA_FD2);
        DpEmit(Ctx, "[DP] RCBA base=%lx  FD(+3418)=%08x  FD2(+3428)=%08x\n", (UINT64)Ctx->RcbaBase,
               Fd, Fd2);

        for (i = 0; i < ARRAY_SIZE(SioFd); i++) {
            BOOLEAN Ok;
            UINT32 Val = DpIobpRead(Ctx->RcbaBase, SioFd[i].FuncDis, &Ok);
            DpEmit(Ctx, "[DP] IOBP %08x %-12a = %08x  %a\n", SioFd[i].FuncDis, SioFd[i].Name, Val,
                   !Ok                       ? "(IOBP FAIL)"
                   : (Val & SIO_FUNCDIS_DIS) ? "DISABLED (DIS bit set)"
                                             : "ENABLED");
        }
    }
    else {
        Ctx->RcbaBase = 0;
        DpEmit(Ctx, "[DP] RCBA enable bit clear -- cannot read FD/IOBP registers\n");
    }

    for (i = 0; i < ARRAY_SIZE(DarkList); i++) {
        P = DpFindFn(DarkList[i].Bus, DarkList[i].Dev, DarkList[i].Fn);
        if (P == NULL) {
            DpEmit(Ctx, "[DP] %02x:%02x.%x %-11a : no PciIo handle\n", DarkList[i].Bus,
                   DarkList[i].Dev, DarkList[i].Fn, DarkList[i].Name);
            continue;
        }
        Vid = 0;
        P->Pci.Read(P, EfiPciIoWidthUint32, 0, 1, &Vid);
        DpEmit(Ctx, "[DP] %02x:%02x.%x %-11a : cfg00=%08x %a\n", DarkList[i].Bus, DarkList[i].Dev,
               DarkList[i].Fn, DarkList[i].Name, Vid, (Vid == 0xFFFFFFFF) ? "(dark)" : "(LIVE!)");
    }

    return EFI_SUCCESS;
}

#if DP_ALLOW_REENABLE
static EFI_STATUS LpssActivate(DP_CTX *Ctx)
{
    UINTN i;
    UINT32 Top, Bar0, Bar1;

    if (Ctx->RcbaBase == 0) {
        DpEmit(Ctx, "[DP] activate: no RCBA base -- skipped\n");
        return EFI_NOT_READY;
    }

    DpEmit(Ctx, "[DP] *** RE-ENABLE: clearing FUNCDIS.DIS + PORTCTRL.PCI_CONF_DIS ***\n");
    for (i = 0; i < ARRAY_SIZE(SioFd); i++) {
        DpIobpClearBit(Ctx->RcbaBase, SioFd[i].FuncDis, SIO_FUNCDIS_DIS);
        DpIobpClearBit(Ctx->RcbaBase, SioFd[i].PortCtrl, SioFd[i].ConfDis);
    }
    gBS->Stall(10000);

    Top = DpPciMem32Top();
    Bar0 = Top ? ((Top - 0x4000) & ~0xFFFu) : 0xb0a27000;
    Bar1 = Top ? ((Top - 0x5000) & ~0xFFFu) : 0xb0a25000;
    DpEmit(Ctx, "[DP] PCI Mem32 top=%08x -> I2C0 BAR=%08x I2C1 BAR=%08x\n", Top, Bar0, Bar1);

    DpDwI2cProbe(Ctx, DpFindFn(0, 0x15, 1), Bar0, "I2C0 D21:F1");
    DpDwI2cProbe(Ctx, DpFindFn(0, 0x15, 2), Bar1, "I2C1 D21:F2");
    return EFI_SUCCESS;
}
#endif

const DARK_PASSENGER gDpLpssLynxPoint = {
    .Name = "lpss-lynxpoint",
    .Desc = "Wake Apple-disabled Haswell-ULT / Lynx Point-LP LPSS (I2C/UART/SPI/SDIO)",
    .Applies = LpssApplies,
    .Recon = LpssRecon,
#if DP_ALLOW_REENABLE
    .Activate = LpssActivate,
#else
    .Activate = NULL,
#endif
};
