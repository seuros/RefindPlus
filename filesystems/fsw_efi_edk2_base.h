// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2026 Dayo Akanji
// SPDX-FileCopyrightText: 2006-2010 Christoph Pfisterer

#ifndef _FSW_EFI_EDK2_BASE_H_
#define _FSW_EFI_EDK2_BASE_H_

# include <Base.h>
# include <Uefi.h>
# include <Library/DebugLib.h>
# include <Library/BaseLib.h>
# include <Protocol/DriverBinding.h>
# include <Library/BaseMemoryLib.h>
# include <Library/UefiRuntimeServicesTableLib.h>
# include <Library/UefiDriverEntryPoint.h>
# include <Library/UefiBootServicesTableLib.h>
# include <Library/MemoryAllocationLib.h>
# include <Library/DevicePathLib.h>
# include <Protocol/DevicePathFromText.h>
# include <Protocol/DevicePathToText.h>
# include <Protocol/DebugPort.h>
# include <Protocol/DebugSupport.h>
# include <Library/PrintLib.h>
# include <Library/UefiLib.h>
# include <Protocol/SimpleFileSystem.h>
# include <Protocol/BlockIo.h>
# include <Protocol/DiskIo.h>
# include <Guid/FileSystemInfo.h>
# include <Guid/FileInfo.h>
# include <Guid/FileSystemVolumeLabelInfo.h>
# include <Protocol/ComponentName.h>

# define BS gBS

# define EFI_FILE_HANDLE_REVISION EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_REVISION
# define SIZE_OF_EFI_FILE_SYSTEM_VOLUME_LABEL_INFO  SIZE_OF_EFI_FILE_SYSTEM_VOLUME_LABEL
# define EFI_FILE_SYSTEM_VOLUME_LABEL_INFO EFI_FILE_SYSTEM_VOLUME_LABEL
# define EFI_SIGNATURE_32(a, b, c, d) SIGNATURE_32(a, b, c, d)
# define DivU64x32(x,y,z) DivU64x32((x),(y))

#endif
