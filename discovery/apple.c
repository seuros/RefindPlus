// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2020-2026 Dayo Akanji
// SPDX-FileCopyrightText: 2012-2023 Roderick W. Smith

#include "global.h"
#include "config.h"
#include "lib.h"
#include "menu.h"
#include "scan.h"
#include "apple.h"
#include "mystrings.h"
#include "launch_efi.h"
#include "fw_strategy.h"

CONST FW_STRATEGY gFwStrategyApple = {
    .Name = L"Apple EFI", .HasQueryVariableInfo = TRUE, .TrustNvramSizes = FALSE};

CONST FW_STRATEGY gFwStrategyAppleLegacy = {
    .Name = L"Apple EFI 1.x", .HasQueryVariableInfo = FALSE, .TrustNvramSizes = FALSE};

BOOLEAN ScanMacOsLoader(MERIDIAN_VOLUME *Volume, CHAR16 *FullFileName)
{
    UINTN i;
    CHAR16 *NameOS;
    CHAR16 *VolName;
    CHAR16 *PathName;
    CHAR16 *FileName;
    BOOLEAN InstallerMac;
    BOOLEAN AddThisEntry;
    BOOLEAN CheckFallback;

    InstallerMac = FALSE;
    CheckFallback = AddThisEntry = TRUE;
    PathName = FileName = VolName = NULL;

    SplitPathName(FullFileName, &VolName, &PathName, &FileName);
    if (FileExists(Volume->RootDir, FullFileName) &&
        !FilenameIn(Volume, PathName, L"boot.efi", GlobalConfig.DontScanFiles)) {
        HasMacOS = TRUE;
        InstallerMac = IsInstallerMac(Volume);
        if (GlobalConfig.SyncAPFS) {
            for (i = 0; i < SystemVolumesCount; i++) {
                if (GuidsAreEqual(&(SystemVolumes[i]->VolUuid), &(Volume->VolUuid))) {
                    AddThisEntry = FALSE;
                    break;
                }
            }
        }

        if (AddThisEntry) {
            NameOS = (InstallerMac) ? L"Instance: Mac OS Installer" : L"Instance: Mac OS";

            DisplayLoader = TRUE;
            AddLoaderEntry(FullFileName, NameOS, Volume, TRUE, FALSE, NULL);
        }

        if (DuplicatesFallback(Volume, FullFileName)) {
            CheckFallback = FALSE;
        }
    }

    MRD_FREE_POOL(VolName);
    MRD_FREE_POOL(PathName);
    MRD_FREE_POOL(FileName);

    return CheckFallback;
}
