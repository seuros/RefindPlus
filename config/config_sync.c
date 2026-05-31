// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2020-2026 Dayo Akanji
// SPDX-FileCopyrightText: 2021 Joe van Tunen
// SPDX-FileCopyrightText: 2012-2023 Roderick W. Smith
// SPDX-FileCopyrightText: 2006 Christoph Pfisterer

#include "global.h"
#include "lib.h"
#include "menu.h"
#include "scan.h"
#include "apple.h"
#include "config.h"
#include "screenmgt.h"
#include "mystrings.h"
#include "mok.h"

static VOID SetLinuxMatchPatterns(CHAR16 *Prefixes)
{
    UINTN i;
    CHAR16 *Pattern;
    CHAR16 *PatternSet;

    i = 0;
    PatternSet = NULL;
    while (1) {
        Pattern = FindCommaDelimited(Prefixes, i++);
        if (Pattern == NULL)
            break;

        MergeStrings(&Pattern, L"*", 0);
        MergeStrings(&PatternSet, Pattern, L',');
        MRD_FREE_POOL(Pattern);
    }

    MRD_FREE_POOL(GlobalConfig.LinuxMatchPatterns);
    GlobalConfig.LinuxMatchPatterns = PatternSet;
}

VOID SyncLinuxPrefixes(VOID)
{
    if (GlobalConfig.LinuxPrefixes == NULL) {
        GlobalConfig.LinuxPrefixes = StrDuplicate(LINUX_PREFIXES);
    }
    else {
        MergeUniqueItems(&GlobalConfig.LinuxPrefixes, LINUX_PREFIXES, L',');
    }

    SetLinuxMatchPatterns(GlobalConfig.LinuxPrefixes);
}

VOID SyncToolPaths(VOID)
{
    MRD_FREE_POOL(GlobalConfig.ToolLocations);

    GlobalConfig.ToolLocations = StrDuplicate(SelfToolPath);

    MergeUniqueItems(&GlobalConfig.ToolLocations, TOOL_LOCATIONS, L',');
}

VOID SyncAlsoScan(VOID)
{
    if (GlobalConfig.AlsoScan == NULL) {
        GlobalConfig.AlsoScan = StrDuplicate(ALSO_SCAN_DIRS);
    }
    else {
        MergeUniqueItems(&GlobalConfig.AlsoScan, ALSO_SCAN_DIRS, L',');
    }
}

VOID SyncDontScanDirs(VOID)
{
    CHAR16 *GuidString;

    if (SelfVolume == NULL || SelfDirPath == NULL) {
        return;
    }

    if (GuidsAreEqual(&(SelfVolume->PartGuid), &GuidNull)) {
        return;
    }

    GuidString = GuidAsString(&(SelfVolume->PartGuid));
    if (GuidString == NULL) {
        return;
    }

    if (GlobalConfig.DontScanDirs == NULL) {
        GlobalConfig.DontScanDirs = StrDuplicate(GuidString);
    }
    else {
        MergeStrings(&GlobalConfig.DontScanDirs, GuidString, L',');
    }
    MRD_FREE_POOL(GuidString);

    MergeStrings(&GlobalConfig.DontScanDirs, SelfDirPath, L':');
}

VOID SyncDontScanFiles(VOID)
{
    if (GlobalConfig.DontScanFiles == NULL) {
        GlobalConfig.DontScanFiles = StrDuplicate(DONT_SCAN_FILES);
    }
    else {
        MergeUniqueItems(&GlobalConfig.DontScanFiles, DONT_SCAN_FILES, L',');
    }

    MergeUniqueItems(&GlobalConfig.DontScanFiles, SHELL_FILES, L',');
    MergeUniqueItems(&GlobalConfig.DontScanFiles, GDISK_FILES, L',');
    MergeUniqueItems(&GlobalConfig.DontScanFiles, GPTSYNC_FILES, L',');
    MergeUniqueItems(&GlobalConfig.DontScanFiles, NETBOOT_FILES, L',');
    MergeUniqueItems(&GlobalConfig.DontScanFiles, FWUPDATE_FILES, L',');
    MergeUniqueItems(&GlobalConfig.DontScanFiles, MOK_FILES, L',');
    MergeUniqueItems(&GlobalConfig.DontScanFiles, NVRAMCLEAN_FILES, L',');
    MergeUniqueItems(&GlobalConfig.DontScanFiles, GlobalConfig.WindowsRecoveryFiles, L',');
    MergeUniqueItems(&GlobalConfig.DontScanFiles, GlobalConfig.MacOSRecoveryFiles, L',');
}

VOID SyncShowTools(VOID)
{
    extern BOOLEAN SetShowTools;

    if (SetShowTools) {
        return;
    }

    SetShowTools = TRUE;
    GlobalConfig.ShowTools[0] = TAG_CSR_ROTATE;
    GlobalConfig.ShowTools[1] = TAG_BOOTORDER;
    GlobalConfig.ShowTools[2] = TAG_REBOOT;
    GlobalConfig.ShowTools[3] = TAG_SHUTDOWN;
}
