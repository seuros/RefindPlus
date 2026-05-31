// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: Intel Corporation

#ifndef _GENERIC_ICH_H_
#define _GENERIC_ICH_H_

#define PCI_BUS_NUMBER_ICH                0x00
#define PCI_DEVICE_NUMBER_ICH_LPC           31
#define PCI_FUNCTION_NUMBER_ICH_LPC          0

#define R_ICH_LPC_ACPI_BASE                   0x40
#define B_ICH_LPC_ACPI_BASE_BAR               0x0000FF80
#define R_ICH_LPC_ACPI_CNT                    0x44
#define B_ICH_LPC_ACPI_CNT_ACPI_EN            0x80

#define R_ACPI_PM1_TMR                        0x08
#define V_ACPI_TMR_FREQUENCY                  3579545
#define V_ACPI_PM1_TMR_MAX_VAL                0x1000000

#define PCI_ICH_LPC_ADDRESS(Register) \
  ((UINTN)(PCI_LIB_ADDRESS (PCI_BUS_NUMBER_ICH, PCI_DEVICE_NUMBER_ICH_LPC, PCI_FUNCTION_NUMBER_ICH_LPC, Register)))

#endif
