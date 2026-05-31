// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2020-2026 Dayo Akanji
// SPDX-FileCopyrightText: 2012-2023 Roderick W. Smith

#ifndef __MERIDIAN_POLICY_H_
#define __MERIDIAN_POLICY_H_

#include "tiano_includes.h"
#include "loader_types.h"

#ifndef EFI_OS_INDICATIONS_BOOT_TO_FW_UI
#define EFI_OS_INDICATIONS_BOOT_TO_FW_UI 0x0000000000000001ULL
#endif

#define ACCESS_FLAGS_FULL                                                                          \
    EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS;
#define ACCESS_FLAGS_BOOT EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS;

#define SYNC_TRUST_HALT (0)
#define SYNC_TRUST_EXIT (1)
#define SYNC_TRUST_SKIP (2)
#define SYNC_TRUST_BOOT (3)

#define ENFORCE_TRUST_NONE (0)
#define ENFORCE_TRUST_MACOS (1)
#define ENFORCE_TRUST_LINUX (2)
#define ENFORCE_TRUST_WINDOWS (4)
#define ENFORCE_TRUST_OPENCORE (8)
#define ENFORCE_TRUST_CLOVER (16)
#define ENFORCE_TRUST_OTHERS (32)
#define REQUIRE_TRUST_VERIFY (64)
#define ENFORCE_TRUST_EVERY (127)

EFI_STATUS SetHardwareNvramVariable(IN CHAR16 *VariableName, IN EFI_GUID *VendorGuid,
                                    IN UINT32 Attributes, IN UINTN VariableSize,
                                    IN VOID *VariableData OPTIONAL);
EFI_STATUS GetHardwareNvramVariable(IN CHAR16 *VariableName, IN EFI_GUID *VendorGuid,
                                    OUT VOID **VariableData, OUT UINTN *VariableSize OPTIONAL);

UINTN RunTrustSync(LOADER_ENTRY *Entry);
VOID RunNVramSync(CHAR16 *SelectionName, BOOLEAN IsMacOS);

#endif
