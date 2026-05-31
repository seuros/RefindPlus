// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#include "cydia.h"
#include <Library/UefiApplicationEntryPoint.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/LoadedImage.h>

static UINT16 BitFor(IN CONST CHAR8 *Name)
{
    if (AsciiStrCmp(Name, "Battery") == 0) {
        return CY_EXP_BATTERY;
    }
    if (AsciiStrCmp(Name, "Display") == 0) {
        return CY_EXP_DISPLAY;
    }
    if (AsciiStrCmp(Name, "Network") == 0) {
        return CY_EXP_NETWORK;
    }
    if (AsciiStrCmp(Name, "Storage") == 0) {
        return CY_EXP_STORAGE;
    }
    if (AsciiStrCmp(Name, "TPM") == 0) {
        return CY_EXP_TPM;
    }
    return 0;
}

static CONST CHAR8 *Verdict(IN CY_COMPONENT *C)
{
    switch (C->State) {
    case CY_PRESENT:
        return "OK";
    case CY_DEGRADED:
        return "DEGRADED";
    case CY_UNKNOWN:
        return "UNKNOWN";
    case CY_ABSENT:
    default:
        return C->Expected ? "MISSING" : "absent";
    }
}

static VOID FormatLine(IN CY_COMPONENT *C, OUT CHAR8 *Line, IN UINTN Cap)
{
    CHAR8 Dots[20];
    UINTN NameLen = AsciiStrLen(C->Name);
    UINTN i, D = (NameLen < 18) ? (18 - NameLen) : 1;
    for (i = 0; i < D && i < sizeof(Dots) - 1; i++) {
        Dots[i] = '.';
    }
    Dots[i] = '\0';
    AsciiSPrint(Line, Cap, "%a %a %-8a%a  %a", C->Name, Dots, Verdict(C),
                (C->State == CY_ABSENT && C->Expected) ? " (expected)" : "", C->Detail);
}

static UINTN BuildReport(IN CY_REPORT *R, OUT CHAR8 *Buf, IN UINTN Cap, OUT UINTN *Faults)
{
    UINTN Off = 0, i;
    CHAR8 Line[192];
    *Faults = 0;

    Off += AsciiSPrint(Buf + Off, Cap - Off,
                       "================ CYDIA REPAIR REPORT ================\n"
                       "Model    : %a\n"
                       "Serial   : %a\n"
                       "Firmware : %a %a\n"
                       "CPU      : %a\n"
                       "Vendor   : %a\n"
                       "-----------------------------------------------------\n",
                       R->Model[0] ? R->Model : "unknown", R->Serial[0] ? R->Serial : "n/a",
                       R->FwVendor[0] ? R->FwVendor : "?", R->FwVersion[0] ? R->FwVersion : "?",
                       R->Cpu[0] ? R->Cpu : "?", R->IsApple ? "Apple" : "generic");

    UINTN Degraded = 0;
    for (i = 0; i < R->Count; i++) {
        FormatLine(&R->Comp[i], Line, sizeof(Line));
        Off += AsciiSPrint(Buf + Off, Cap - Off, "%a\n", Line);
        if (R->Comp[i].State == CY_ABSENT && R->Comp[i].Expected) {
            (*Faults)++;
        }
        else if (R->Comp[i].State == CY_DEGRADED) {
            Degraded++;
        }
    }

    Off += AsciiSPrint(Buf + Off, Cap - Off,
                       "-----------------------------------------------------\n"
                       "VERDICT  : %a  (%u missing, %u degraded)\n"
                       "=====================================================\n",
                       (*Faults || Degraded) ? "DEFECTIVE -- see MISSING/DEGRADED lines"
                                             : "healthy -- no faults detected",
                       (UINT32)*Faults, (UINT32)Degraded);
    return Off;
}

static EFI_STATUS WriteReport(IN EFI_HANDLE Image, IN CHAR16 *Name, IN CHAR8 *Data, IN UINTN Size)
{
    EFI_LOADED_IMAGE_PROTOCOL *Li = NULL;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *Sfs = NULL;
    EFI_FILE_PROTOCOL *Root = NULL, *File = NULL;
    EFI_STATUS Status;
    UINTN Written = Size;

    if (EFI_ERROR(gBS->HandleProtocol(Image, &gEfiLoadedImageProtocolGuid, (VOID **)&Li)) ||
        Li->DeviceHandle == NULL ||
        EFI_ERROR(gBS->HandleProtocol(Li->DeviceHandle, &gEfiSimpleFileSystemProtocolGuid,
                                      (VOID **)&Sfs))) {
        return EFI_NOT_FOUND;
    }
    Status = Sfs->OpenVolume(Sfs, &Root);
    if (EFI_ERROR(Status)) {
        return Status;
    }
    Status = Root->Open(Root, &File, Name,
                        EFI_FILE_MODE_CREATE | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_READ, 0);
    if (!EFI_ERROR(Status)) {
        Status = File->Write(File, &Written, Data);
        File->Close(File);
    }
    Root->Close(Root);
    return Status;
}

EFI_STATUS EFIAPI UefiMain(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable)
{
    CY_REPORT *R;
    CHAR8 *Buf;
    UINTN Size, Faults = 0, i;
    UINT16 Expect;
    UINT8 DimmSlots;
    EFI_STATUS Status;

    R = AllocateZeroPool(sizeof(CY_REPORT));
    Buf = AllocateZeroPool(8192);
    if (R == NULL || Buf == NULL) {
        Print(L"Cydia: out of memory\n");
        return EFI_OUT_OF_RESOURCES;
    }

    Print(L"\nCydia -- scanning hardware, please wait...\n");

    CyScanSystem(R);
    CyScanMemory(R);
    CyScanStorage(R);
    CyScanDisplay(R);
    CyScanBattery(R);
    CyScanNetwork(R);
    CyScanThermal(R);
    CyScanShutdown(R);
    CyScanTpm(R);
    CyScanUsb(R);

    Expect = CyExpectFor(R->Model, R->IsApple, &DimmSlots);
    for (i = 0; i < R->Count; i++) {
        UINT16 Bit = BitFor(R->Comp[i].Name);
        if (Bit != 0 && (Expect & Bit)) {
            R->Comp[i].Expected = TRUE;
        }
    }

    Size = BuildReport(R, Buf, 8192, &Faults);

    AsciiPrint("%a", Buf);

    Status = WriteReport(ImageHandle, L"cydia-report.txt", Buf, Size);
    if (EFI_ERROR(Status)) {
        Print(L"\nCydia: could not write cydia-report.txt (%r)\n", Status);
    }
    else {
        Print(L"\nCydia: report written to cydia-report.txt on the boot volume.\n");
    }
    Print(L"Cydia: %u expected part(s) missing.\n", (UINT32)Faults);

    FreePool(Buf);
    FreePool(R);
    return EFI_SUCCESS;
}
