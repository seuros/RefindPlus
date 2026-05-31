// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2024 Dayo Akanji
// SPDX-FileCopyrightText: 2012-2020 Roderick W. Smith
// SPDX-FileCopyrightText: 2006-2010 Christoph Pfisterer

#ifndef __MERIDIAN_LINUX_H_
#define __MERIDIAN_LINUX_H_

BOOLEAN HasSignedCounterpart(IN MERIDIAN_VOLUME *Volume, IN CHAR16 *FullName);

CHAR16 *AddInitrdToOptions(CHAR16 *Options, CHAR16 *InitrdPath);
CHAR16 *FindInitrd(IN CHAR16 *LoaderPath, IN MERIDIAN_VOLUME *Volume);
CHAR16 *GetMainLinuxOptions(IN CHAR16 *LoaderPath, IN MERIDIAN_VOLUME *Volume);

VOID AddKernelToSubmenu(LOADER_ENTRY *TargetLoader, CHAR16 *FileName, MERIDIAN_VOLUME *Volume);
VOID GuessLinuxDistribution(CHAR16 **OSSplashHint, MERIDIAN_VOLUME *Volume, CHAR16 *LoaderPath,
                            BOOLEAN FirstOnly);

#endif
