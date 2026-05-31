// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2020-2026 Dayo Akanji

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/PrintLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include "MemLogLib.h"
#include <Library/DebugLib.h>

#include <Library/IoLib.h>
#include <Library/PciLib.h>
#include "GenericIch.h"
#include "meridian_funcs.h"

#if defined (EFIAARCH64)
static inline UINT64 MemLogReadTsc (VOID) {
    UINT64 Val;
    __asm__ volatile ("mrs %0, cntpct_el0" : "=r" (Val));
    return Val;
}
static inline UINT64 MemLogReadTscFreq (VOID) {
    UINT64 Val;
    __asm__ volatile ("mrs %0, cntfrq_el0" : "=r" (Val));
    return Val;
}
#else
static inline UINT64 MemLogReadTsc (VOID) {
    return AsmReadTsc();
}
#endif

typedef struct {
    CHAR8             *Buffer;
    CHAR8             *Cursor;
    UINTN              BufferSize;
    MEM_LOG_CALLBACK   Callback;

    UINT64             TscStart;

    UINT64             TscLast;

    UINT64             TscFreqSec;
} MEM_LOG;

EFI_GUID  mMemLogProtocolGuid = { 0x74B91DA4, 0x2B4C, 0x11E2, \
    { 0x99, 0x03, 0x22, 0xF0, 0x61, 0x88, 0x70, 0x9B } };

MEM_LOG   *mMemLog = NULL;

CHAR8     mTimingTxt[32];

BOOLEAN   mTimerPrev = FALSE;

UINT64 GetCurrentMS (VOID) {
	UINT64    CurrentMS;
	UINT64    CurrentTsc;

	if (!mMemLog || mMemLog->TscFreqSec == 0) {
        CurrentMS = 0;
    }
    else {
		CurrentTsc = MemLogReadTsc();

		CurrentMS = DivU64x64Remainder (
            MultU64x32(CurrentTsc - mMemLog->TscStart, 1000),
            mMemLog->TscFreqSec,
            NULL
        );
	}

	return CurrentMS;
}

CHAR8 * GetTiming (VOID) {
    UINT64    dTStartSec;
    UINT64    dTStartMs;
    UINT64    dTLastSec;
	UINT64    dTLastMs;
    UINT64    CurrentTsc;
    UINT64    dTStartSecLog;
    UINT64    dTLastSecLog;

	mTimingTxt[0] = '\0';

	if (mMemLog != NULL && mMemLog->TscFreqSec != 0) {
		CurrentTsc = MemLogReadTsc();

		dTStartMs = DivU64x64Remainder (
            MultU64x32(CurrentTsc - mMemLog->TscStart, 10000),
            mMemLog->TscFreqSec, NULL
        );
		dTLastMs = DivU64x64Remainder (
            MultU64x32(CurrentTsc - mMemLog->TscLast, 10000),
            mMemLog->TscFreqSec, NULL
        );

        dTStartSec = DivU64x64Remainder (dTStartMs, 10000, &dTStartMs);
        dTLastSec  = DivU64x64Remainder (dTLastMs,  10000, &dTLastMs);

        dTStartSecLog = (dTStartSec < 1000) ? dTStartSec : 999;
        dTLastSecLog  = (dTLastSec  < 1000) ? dTLastSec  : 999;

		AsciiSPrint (
            mTimingTxt,
            sizeof (mTimingTxt),
            "%3ld:%04ld %3ld:%04ld",
            dTStartSecLog,
            dTStartMs,
            dTLastSecLog,
            dTLastMs
        );

        mMemLog->TscLast = CurrentTsc;
	}

	return mTimingTxt;
}

EFI_STATUS EFIAPI MemLogInit (VOID) {
    EFI_STATUS      Status;
    CHAR8           InitError[50];
#if defined (EFIX64)
    UINT32          TimerAddr;
    UINT64          Tsc0;
    UINT64          Tsc1;
    UINT32          AcpiTick0;
    UINT32          AcpiTick1;
    UINT32          AcpiTicksDelta;
    UINT32          AcpiTicksTarget;
#endif

    static BOOLEAN  SkipLog = FALSE;

    if (SkipLog) {
        return EFI_NOT_READY;
    }

    Status = gBS->LocateProtocol(&mMemLogProtocolGuid, NULL, (VOID **) &mMemLog);
    if (!EFI_ERROR(Status) && mMemLog) {
        if (!mTimerPrev) {

            mTimerPrev        =         TRUE;
            mMemLog->TscStart = MemLogReadTsc();
            mMemLog->TscLast  = MemLogReadTsc();
        }

        return EFI_SUCCESS;
    }

    mMemLog = AllocateZeroPool ( sizeof (MEM_LOG) );
    if (mMemLog == NULL) {

        SkipLog = TRUE;

        return EFI_OUT_OF_RESOURCES;
    }

    mMemLog->Buffer = AllocateZeroPool (MEM_LOG_INITIAL_SIZE);
    if (mMemLog->Buffer == NULL) {
        MRD_FREE_POOL(mMemLog);

        SkipLog = TRUE;

        return EFI_OUT_OF_RESOURCES;
    }

    mMemLog->BufferSize = MEM_LOG_INITIAL_SIZE;
    mMemLog->Cursor     = mMemLog->Buffer;
    mMemLog->Callback   = NULL;

    InitError[0]='\0';

#if defined (EFIX64)

    if ((PciRead16( PCI_ICH_LPC_ADDRESS(0))) != 0x8086) {

        TimerAddr = 0;

        AsciiSPrint (
            InitError,
            sizeof (InitError),
            "Intel ICH Device *NOT* Found"
        );
    }
    else if (
        (
            PciRead8 (
                PCI_ICH_LPC_ADDRESS(R_ICH_LPC_ACPI_CNT)
            ) & B_ICH_LPC_ACPI_CNT_ACPI_EN
        ) == 0
    ) {
        TimerAddr = 0;

        AsciiSPrint (
            InitError,
            sizeof (InitError),
            "ACPI I/O Space *NOT* Enabled"
        );
    }
    else {
        TimerAddr = (
            (
                PciRead16 (
                    PCI_ICH_LPC_ADDRESS(R_ICH_LPC_ACPI_BASE)
                )
            ) & B_ICH_LPC_ACPI_BASE_BAR
        ) + R_ACPI_PM1_TMR;

        if (TimerAddr < 9) {
            TimerAddr = 0;

            AsciiSPrint (
                InitError,
                sizeof (InitError),
                "Timer Address *NOT* Obtained"
            );
        }
        else {

            AcpiTick0 = IoRead32 (TimerAddr);
            gBS->Stall(1000);
            AcpiTick1 = IoRead32 (TimerAddr);

            if (AcpiTick0 == AcpiTick1) {
                TimerAddr = 0;

                AsciiSPrint (
                    InitError,
                    sizeof (InitError),
                    "Timer *NOT* Advancing"
                );
            }
        }
    }

    if (TimerAddr == 0) {

        Tsc0 = MemLogReadTsc();

        MeridianStall (10);

        Tsc1 = MemLogReadTsc();

        mMemLog->TscFreqSec = MultU64x32((Tsc1 - Tsc0), 10);
    }
    else {

        AcpiTicksTarget = V_ACPI_TMR_FREQUENCY/10;
        AcpiTick0       = IoRead32 (TimerAddr);
        Tsc0            = MemLogReadTsc();

        do {
            CpuPause();

            AcpiTick1 = IoRead32 (TimerAddr);
            if (AcpiTick0 <= AcpiTick1) {

                AcpiTicksDelta = AcpiTick1 - AcpiTick0;
            }
            else if (AcpiTick0 - AcpiTick1 <= 0x00FFFFFF) {

                AcpiTicksDelta = (0x00FFFFFF - AcpiTick0) + AcpiTick1;
            }
            else {

                AcpiTicksDelta = (0xFFFFFFFF - AcpiTick0) + AcpiTick1;
            }
        } while (AcpiTicksDelta < AcpiTicksTarget);

        Tsc1 = MemLogReadTsc();

        mMemLog->TscFreqSec = DivU64x32(
            MultU64x32(
                (Tsc1 - Tsc0),
                V_ACPI_TMR_FREQUENCY
            ),
            AcpiTicksDelta
        );
    }
#else

    mMemLog->TscFreqSec = MemLogReadTscFreq();
#endif

    mTimerPrev        =         TRUE;
    mMemLog->TscStart = MemLogReadTsc();
    mMemLog->TscLast  = MemLogReadTsc();

    Status = gBS->InstallMultipleProtocolInterfaces(&gImageHandle, &mMemLogProtocolGuid, mMemLog, NULL);
    if (EFI_ERROR(Status)) {
        MRD_FREE_POOL(mMemLog->Buffer);
        MRD_FREE_POOL(mMemLog);

        SkipLog = TRUE;

        return Status;
    }

    if (InitError[0] != '\0') {
        MemLog (FALSE, 1,
            "** Could *NOT* Calibrate ACPI PM Timer ... %a **\n\n",
            InitError
        );
    }

    return EFI_SUCCESS;
}

VOID EFIAPI MemLogVA (
    IN  const BOOLEAN  Timing,
    IN  const INTN     DebugMode,
    IN  const CHAR8   *Format,
    IN        VA_LIST  Marker
) {
    EFI_STATUS      Status;
    UINTN           Offset;
    UINTN           DataWritten;
    CHAR8           *LastMessage;

    if (Format == NULL) {
        return;
    }

    Status = MemLogInit();
    if (EFI_ERROR(Status)) {
        return;
    }

    if ((UINTN) (mMemLog->Cursor - mMemLog->Buffer) + MEM_LOG_MAX_LINE_SIZE > mMemLog->BufferSize) {

        if ((mMemLog->BufferSize + MEM_LOG_INITIAL_SIZE) > MEM_LOG_MAX_SIZE) {

            return;
        }

        Offset = mMemLog->Cursor - mMemLog->Buffer;
        mMemLog->Buffer = ReallocatePool (
            mMemLog->BufferSize,
            mMemLog->BufferSize + MEM_LOG_INITIAL_SIZE,
            mMemLog->Buffer
        );
        if (mMemLog->Buffer == NULL) {
            return;
        }

        mMemLog->BufferSize += MEM_LOG_INITIAL_SIZE;
        mMemLog->Cursor = mMemLog->Buffer + Offset;
    }

    LastMessage = mMemLog->Cursor;
    if (Timing) {

        if (mMemLog->Buffer[0]  == '\0' ||
            mMemLog->Cursor[-1] == '\n'
        ) {
            DataWritten = AsciiSPrint (
                mMemLog->Cursor,
                mMemLog->BufferSize - (mMemLog->Cursor - mMemLog->Buffer),
                "%a  ",
                GetTiming()
            );
            mMemLog->Cursor += DataWritten;
        }
    }

    DataWritten = AsciiVSPrint (
        mMemLog->Cursor,
        mMemLog->BufferSize - (mMemLog->Cursor - mMemLog->Buffer),
        Format,
        Marker
    );
    mMemLog->Cursor += DataWritten;

    if (mMemLog->Callback) {
        mMemLog->Callback(DebugMode, LastMessage);
    }

    DebugPrint (DEBUG_INFO, LastMessage);
}

VOID EFIAPI MemLog (
    IN  const BOOLEAN  Timing,
    IN  const INTN     DebugMode,
    IN  const CHAR8   *Format,
    ...
) {
    VA_LIST           Marker;

    if (!Format) {
        return;
    }

    VA_START(Marker, Format);
    MemLogVA (Timing, DebugMode, Format, Marker);
    VA_END(Marker);
}

CHAR8 * EFIAPI GetMemLogBuffer (VOID) {
    EFI_STATUS        Status;

    Status = MemLogInit();
    if (EFI_ERROR(Status)) {
        return NULL;
    }

    return (mMemLog != NULL) ? mMemLog->Buffer : NULL;
}

UINTN EFIAPI GetMemLogLen (VOID) {
    EFI_STATUS        Status;

    Status = MemLogInit();
    if (EFI_ERROR(Status)) {
        return 0;
    }

    return (mMemLog != NULL) ? mMemLog->Cursor - mMemLog->Buffer : 0;
}

VOID EFIAPI SetMemLogCallback (
    MEM_LOG_CALLBACK  Callback
) {
    EFI_STATUS        Status;

    Status = MemLogInit();
    if (EFI_ERROR(Status)) {
        return;
    }

    mMemLog->Callback = Callback;
}

UINT64 EFIAPI GetMemLogTscTicksPerSecond (VOID) {
    EFI_STATUS        Status;

    Status = MemLogInit();
    if (EFI_ERROR(Status)) {
        return 0;
    }

    return mMemLog->TscFreqSec;
}
