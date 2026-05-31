// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2020-2026 Dayo Akanji
// SPDX-FileCopyrightText: 2019-2021 Acidanthera (OpenCore OcApfsLib, BSD-3-Clause)

#ifndef MERIDIAN_APFS_LIB_H
#define MERIDIAN_APFS_LIB_H

EFI_STATUS MeridianApfsConnectParentDevice (
  VOID
  );

EFI_STATUS MeridianApfsConnectHandle (
  IN EFI_HANDLE  Handle
  );

EFI_STATUS MeridianApfsConnectDevices (
  VOID
  );

#endif
