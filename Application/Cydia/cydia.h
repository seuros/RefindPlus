// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#ifndef __CYDIA_H_
#define __CYDIA_H_

#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>

typedef enum
{
    CY_ABSENT = 0,
    CY_UNKNOWN,
    CY_DEGRADED,
    CY_PRESENT
} CY_STATE;

typedef struct
{
    CHAR8 Name[24];
    CY_STATE State;
    BOOLEAN Expected;
    CHAR8 Detail[160];
} CY_COMPONENT;

#define CY_MAX_COMPONENTS 24

typedef struct
{
    CHAR8 Model[48];
    CHAR8 Serial[32];
    CHAR8 FwVendor[40];
    CHAR8 FwVersion[40];
    CHAR8 Cpu[64];
    BOOLEAN IsApple;
    BOOLEAN NonAppleDisk;

    CY_COMPONENT Comp[CY_MAX_COMPONENTS];
    UINTN Count;
} CY_REPORT;

#define CY_EXP_BATTERY BIT0
#define CY_EXP_DISPLAY BIT1
#define CY_EXP_NETWORK BIT2
#define CY_EXP_STORAGE BIT3
#define CY_EXP_TPM BIT4

UINT16 CyExpectFor(IN CONST CHAR8 *Model, IN BOOLEAN IsApple, OUT UINT8 *DimmSlots);

VOID CyScanSystem(IN OUT CY_REPORT *R);
VOID CyScanMemory(IN OUT CY_REPORT *R);
VOID CyScanStorage(IN OUT CY_REPORT *R);
VOID CyScanDisplay(IN OUT CY_REPORT *R);
VOID CyScanBattery(IN OUT CY_REPORT *R);
VOID CyScanNetwork(IN OUT CY_REPORT *R);
VOID CyScanThermal(IN OUT CY_REPORT *R);
VOID CyScanShutdown(IN OUT CY_REPORT *R);
VOID CyScanTpm(IN OUT CY_REPORT *R);
VOID CyScanUsb(IN OUT CY_REPORT *R);

CY_COMPONENT *CyAdd(IN OUT CY_REPORT *R, IN CONST CHAR8 *Name, IN CY_STATE State);

BOOLEAN CySmcRead(IN CONST CHAR8 Key[4], OUT UINT8 *Buf, IN UINT8 Len);

#endif
