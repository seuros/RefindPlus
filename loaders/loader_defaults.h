// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2020-2026 Dayo Akanji
// SPDX-FileCopyrightText: 2012-2023 Roderick W. Smith

#ifndef __MERIDIAN_LOADER_DEFAULTS_H_
#define __MERIDIAN_LOADER_DEFAULTS_H_

#define LABEL_UNKNOWN L"Unknown"

#if defined(EFIX64)
#define WINDOWS_RECOVERY_FILES                                                                     \
    L"EFI\\Microsoft\\Boot\\LrsBootmgr.efi,Recovery:\\EFI\\BOOT\\bootx64.efi,"                     \
    L"Recovery:\\EFI\\BOOT\\boot.efi,EFI\\OEM\\Boot\\bootmgfw.efi"
#elif defined(EFIAARCH64)
#define WINDOWS_RECOVERY_FILES                                                                     \
    L"EFI\\Microsoft\\Boot\\LrsBootmgr.efi,Recovery:\\EFI\\BOOT\\bootaa64.efi,"                    \
    L"Recovery:\\EFI\\BOOT\\boot.efi,EFI\\OEM\\Boot\\bootmgfw.efi"
#else
#define WINDOWS_RECOVERY_FILES                                                                     \
    L"EFI\\Microsoft\\Boot\\LrsBootmgr.efi,Recovery:\\EFI\\BOOT\\boot.efi,"                        \
    L"EFI\\OEM\\Boot\\bootmgfw.efi"
#endif

#define VENTOY_NAMES L"VTOYEFI,Ventoy"

#define DATA_NAME_APFS L"Data,Daten,Datos,Donnees,Dados,Dati,Tiedot,Gegevens,Podaci"

#define MACOSX_LOADER_DIR L"System\\Library\\CoreServices"
#define MACOSX_LOADER_PATH (MACOSX_LOADER_DIR L"\\boot.efi")
#define MACOSX_DIAGNOSTICS (MACOSX_LOADER_DIR L"\\.diagnostics\\diags.efi")

#define MACOS_RECOVERY_BASE L"com.apple.recovery.boot"
#define MACOS_RECOVERY_FILES (MACOS_RECOVERY_BASE L"\\boot.efi")
#define MACOS_RECOVERY_VERSION_FILE (MACOS_RECOVERY_BASE L"\\SystemVersion.plist")

#define LOADER_MATCH_PATTERNS L"*.efi,*.EFI"

#if defined(EFIAARCH64)
#define LINUX_PREFIXES L"vmlinuz,kernel,Image"
#else
#define LINUX_PREFIXES L"vmlinuz,kernel,bzImage"
#endif

#endif
