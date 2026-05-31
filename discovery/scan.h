// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2021-2025 Dayo Akanji
// SPDX-FileCopyrightText: 2012-2020 Roderick W. Smith
// SPDX-FileCopyrightText: 2006-2010 Christoph Pfisterer

#ifndef __SCAN_H_
#define __SCAN_H_

#define LABEL_BOOTORDER L"Manage BootOrder"
#define LABEL_CSR_ROTATE L"Rotate CSR"
#define LABEL_FIRMWARE L"Firmware Reboot"
#define LABEL_SHUTDOWN L"System Shutdown"
#define LABEL_REBOOT L"System Restart"
#define LABEL_MOK L"MOK Protocol"
#define LABEL_EXIT L"Exit Meridian"
#define LABEL_SHELL L"uEFI Shell"
#define LABEL_MEMTEST L"MemTest Tool"
#define LABEL_INSTALL L"Install Meridian"
#define LABEL_NETBOOT L"Net Boot"
#define LABEL_GDISK L"GDisk Tool"
#define LABEL_GPTSYNC L"GPTsync Tool"
#define LABEL_FWUPDATE L"Firmware Update"
#define LABEL_CLEAN_NVRAM L"Clean nvRAM"
#define LABEL_RECOVERY_MAC L"Recovery (Mac)"
#define LABEL_RECOVERY_WIN L"Recovery (Win)"
#define LABEL_CYDIA L"Diagnostics (Cydia)"

#if defined(EFIX64)
#define MEMTEST_FILES                                                                              \
    L"bootx64.efi,memtest.efi,memtest86p.efi,memtest86.efi,\
memtestx64.efi,memtest86px64.efi,memtest86x64.efi,\
memtest_x64.efi,memtest86p_x64.efi,memtest86_x64.efi,\
x64_memtest.efi,x64_memtest86p.efi,x64_memtest86.efi"
#define SKIPNAME_PATTERNS L"*ia32*.efi,*aa64*.efi,*mips*.efi"
#define FALLBACK_FULLNAME L"EFI\\BOOT\\bootx64.efi"
#define FALLBACK_BASENAME L"BOOTx64.efi"
#define NETBOOT_FILES L"ipxe.efi,ipxe_x64.efi,ipxex64.efi,x64_ipxe.efi"
#define GPTSYNC_FILES L"gptsync.efi,gptsync_x64.efi,gptsyncx64.efi,x64_gptsync.efi"
#define GDISK_FILES L"gdisk.efi,gdisk_x64.efi,gdiskx64.efi,x64_gdisk.efi"
#define CYDIA_FILES L"cydia.efi,cydia_x64.efi,cydiax64.efi,x64_cydia.efi"
#define SHELL_FILES L"shell.efi,shell_x64.efi,shellx64.efi,x64_shell.efi"
#define NVRAMCLEAN_FILES L"CleanNvram.efi,CleanNvramx64.efi,CleanNvram_x64.efi,x64_CleanNvram.efi"
#elif defined(EFIAARCH64)
#define MEMTEST_FILES                                                                              \
    L"bootaa64.efi,memtest.efi,memtest86p.efi,memtest86.efi,\
memtestaa64.efi,memtest86paa64.efi,memtest86aa64.efi,\
memtest_aa64.efi,memtest86p_aa64.efi,memtest86_aa64.efi,\
aa64_memtest.efi,aa64_memtest86p.efi,aa64_memtest86.efi"
#define SKIPNAME_PATTERNS L"*x64*.efi,*ia32*.efi,*mips*.efi"
#define FALLBACK_FULLNAME L"EFI\\BOOT\\bootaa64.efi"
#define FALLBACK_BASENAME L"BOOTaa64.efi"
#define NETBOOT_FILES L"ipxe.efi,ipxe_aa64.efi,ipxeaa64.efi,aa64_ipxe.efi"
#define GPTSYNC_FILES L"gptsync.efi,gptsync_aa64.efi,gptsyncaa64.efi,aa64_gptsync.efi"
#define GDISK_FILES L"gdisk.efi,gdisk_aa64.efi,gdiskaa64.efi,aa64_gdisk.efi"
#define CYDIA_FILES L"cydia.efi,cydia_aa64.efi,cydiaaa64.efi,aa64_cydia.efi"
#define SHELL_FILES L"shell.efi,shell_aa64.efi,shellaa64.efi,aa64_shell.efi"
#define NVRAMCLEAN_FILES                                                                           \
    L"CleanNvram.efi,CleanNvramaa64.efi,CleanNvram_aa64.efi,aa64_CleanNvram.efi"
#else
#define MEMTEST_FILES L"boot.efi,memtest.efi,memtest86p.efi,memtest86.efi"
#define SKIPNAME_PATTERNS L"*x64*.efi,*ia32*.efi,*aa64*.efi,*mips*.efi"
#define FALLBACK_FULLNAME L"EFI\\BOOT\\boot.efi"
#define FALLBACK_BASENAME L"BOOT.efi"
#define NETBOOT_FILES L"ipxe.efi"
#define GPTSYNC_FILES L"gptsync.efi"
#define GDISK_FILES L"gdisk.efi"
#define CYDIA_FILES L"cydia.efi"
#define SHELL_FILES L"shell.efi"
#define NVRAMCLEAN_FILES L"CleanNvram.efi"
#endif

#define BSD_AUX_FILES                                                                              \
    L"loader_simp.efi,loader_4th.efi,loader.help.efi,gptboot.efi,gptzfsboot.efi,boot1.efi,\
zfsloader.efi"

#define SELF_LOADER_PATTERNS L"meridian*.efi"

// Scanned front to back and matched as substrings: a distro whose name
// contains another must come first, or "Omarchy" reports as "Arch".
#define BASE_LINUX_DISTROS                                                                         \
    L"Omarchy,Arch,Artful,Bionic,CachyOS,Centos,Chakra,Crunchbang,Debian,Deepin,Devuan,\
Elementary,EndeavourOS,Fedora,Frugalware,Gentoo,LinuxMint,\
Mageia,Mandriva,Manjaro,OpenSUSE,Redhat,Slackware,SUSE,\
Kubuntu,Lubuntu,Xubuntu,Ubuntu,Void,Zorin"

#define MAIN_LINUX_DISTROS                                                                         \
    L"Omarchy,Arch,CachyOS,Debian,Deepin,Elementary,EndeavourOS,Fedora,Gentoo,\
LinuxMint,Manjaro,OpenSUSE,Redhat,Slackware,SUSE,Ubuntu,Zorin"

#define RECOVERY_NAME_HFS L"HFS+ Instance"
#define RECOVERY_NAME_APFS L"APFS Instance"

LOADER_ENTRY *InitializeLoaderEntry(IN LOADER_ENTRY *Entry);
LOADER_ENTRY *CopyLoaderEntry(IN LOADER_ENTRY *Entry);

MERIDIAN_MENU_ENTRY *CopyMenuEntry(MERIDIAN_MENU_ENTRY *Entry);

MERIDIAN_MENU_SCREEN *CopyMenuScreen(MERIDIAN_MENU_SCREEN *Entry);
MERIDIAN_MENU_SCREEN *InitializeSubScreen(IN LOADER_ENTRY *Entry);

extern BOOLEAN DisplayLoader;
extern BOOLEAN HasMacOS;
extern BOOLEAN HasOpenCore;

BOOLEAN MeridianLaunchedByOpenCore(VOID);

BOOLEAN ScanMacOsLoader(MERIDIAN_VOLUME *Volume, CHAR16 *FullFileName);

BOOLEAN DuplicatesFallback(IN MERIDIAN_VOLUME *Volume, IN CHAR16 *FileName);

BOOLEAN IsInstallerMac(MERIDIAN_VOLUME *Volume);
UINTN DetectBsdKind(IN MERIDIAN_VOLUME *Volume);
CHAR16 *BsdEspLoaderTitle(IN CHAR16 *LoaderPath);
BOOLEAN IsBsdRootLoaderPath(IN CHAR16 *LoaderPath);

BOOLEAN IsDuplicateBsdEspDir(IN MERIDIAN_VOLUME *EspVolume, IN CHAR16 *DirName);
CHAR16 *DragonFlyCurrdevOption(VOID);
BOOLEAN ScanBsdRootLoader(IN MERIDIAN_VOLUME *Volume);

VOID ScanForTools(VOID);
VOID ScanForBootloaders(VOID);
VOID ScanNetboot(VOID);

BOOLEAN ScanLoaderDir(IN MERIDIAN_VOLUME *Volume, IN CHAR16 *Path, IN CHAR16 *Pattern);
VOID ScanBLSEntries(IN MERIDIAN_VOLUME *Volume);
VOID ClearBlsClaimedLoaders(VOID);
BOOLEAN IsBlsClaimedLinuxPath(IN MERIDIAN_VOLUME *Volume, IN CHAR16 *LoaderPath);

BOOLEAN IsBsdAuxLoader(IN MERIDIAN_VOLUME *Volume, IN CHAR16 *FileName);

BOOLEAN IsToolSet(UINTN ToolTag);
BOOLEAN HidePreboot(CHAR16 *Type);

VOID VetCSR(VOID);
LOADER_ENTRY *AddEfiLoaderEntry(IN EFI_DEVICE_PATH_PROTOCOL *EfiLoaderPath, IN CHAR16 *LoaderTitle,
                                IN UINT16 EfiBootNum, IN UINTN Row, IN UINTN TypeTag);

LOADER_ENTRY *AddLoaderEntry(IN OUT CHAR16 *LoaderPath, IN CHAR16 *LoaderTitle,
                             IN MERIDIAN_VOLUME *Volume, IN BOOLEAN SubScreenReturn,
                             IN BOOLEAN CheckLinux, IN CHAR16 *OverrideOptions OPTIONAL);
VOID SetLoaderDefaults(IN LOADER_ENTRY *Entry, IN CHAR16 *LoaderPath, IN MERIDIAN_VOLUME *Volume);
VOID GenerateSubScreen(IN OUT LOADER_ENTRY *Entry, IN MERIDIAN_VOLUME *Volume,
                       IN BOOLEAN GenerateReturn);
VOID ScanFirmwareDefined(IN UINTN Row, IN CHAR16 *MatchThis OPTIONAL, IN UINTN TypeTag);

CHAR16 *GetShowName(IN CHAR16 *LinuxName);
CHAR16 *SetVolJoin(IN CHAR16 *OurItem, IN BOOLEAN ForBoot);
CHAR16 *SetVolKind(IN CHAR16 *OurItem, IN CHAR16 *VolName, IN UINT32 VolFSType);
CHAR16 *SetVolFlag(IN CHAR16 *OurItem, IN CHAR16 *VolName);
CHAR16 *SetVolType(IN CHAR16 *OurItem OPTIONAL, IN CHAR16 *VolName, IN UINT32 VolFSType);
CHAR16 *BuildLoaderTitle(IN CHAR16 *Title, IN CHAR16 *VolName, IN UINT32 VolFSType);
CHAR16 *GetVolumeGroupName(IN CHAR16 *LoaderPath, IN MERIDIAN_VOLUME *Volume);

BOOLEAN ShouldScan(MERIDIAN_VOLUME *Volume, CHAR16 *Path);
BOOLEAN IsValidTool(MERIDIAN_VOLUME *BaseVolume, CHAR16 *PathName);
BOOLEAN FindTool(CHAR16 *Locations, CHAR16 *Names, CHAR16 *Description, BOOLEAN SelfVolOnly,
                 BOOLEAN ScanMultiple, UINTN TypeTag);

#endif
