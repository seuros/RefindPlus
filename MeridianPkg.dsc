# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

[Defines]
    PLATFORM_NAME           = Meridian
    PLATFORM_GUID           = CF424B08-2E9D-4E77-A6DD-71E0C8dE60F3
    PLATFORM_VERSION        = 4.5.0
    DSC_SPECIFICATION       = 0x00010006
    SUPPORTED_ARCHITECTURES = X64|AARCH64
    BUILD_TARGETS           = RELEASE|DEBUG|NOOPT
    SKUID_IDENTIFIER        = DEFAULT

[LibraryClasses]
##
# Entry point
##
    UefiApplicationEntryPoint|MdePkg/Library/UefiApplicationEntryPoint/UefiApplicationEntryPoint.inf
    UefiDriverEntryPoint|MdePkg/Library/UefiDriverEntryPoint/UefiDriverEntryPoint.inf
    BaseLib|MdePkg/Library/BaseLib/BaseLib.inf
    BaseMemoryLib|MdePkg/Library/BaseMemoryLib/BaseMemoryLib.inf
    SynchronizationLib|MdePkg/Library/BaseSynchronizationLib/BaseSynchronizationLib.inf
    PrintLib|MdePkg/Library/BasePrintLib/BasePrintLib.inf
# IoLib: BaseIoLibIntrinsic ships AArch64 sources (MMIO-based), so it stays common.
# PCI CF8 config-port libs are x86-only and move to [LibraryClasses.X64] below.
    IoLib|MdePkg/Library/BaseIoLibIntrinsic/BaseIoLibIntrinsic.inf
    CacheMaintenanceLib|MdePkg/Library/BaseCacheMaintenanceLib/BaseCacheMaintenanceLib.inf
    PeCoffLib|MdePkg/Library/BasePeCoffLib/BasePeCoffLib.inf
    PeCoffGetEntryPointLib|MdePkg/Library/BasePeCoffGetEntryPointLib/BasePeCoffGetEntryPointLib.inf


##
# UEFI & PI
##
    UefiBootServicesTableLib|MdePkg/Library/UefiBootServicesTableLib/UefiBootServicesTableLib.inf
    UefiRuntimeServicesTableLib|MdePkg/Library/UefiRuntimeServicesTableLib/UefiRuntimeServicesTableLib.inf
    UefiRuntimeLib|MdePkg/Library/UefiRuntimeLib/UefiRuntimeLib.inf
    UefiLib|MdePkg/Library/UefiLib/UefiLib.inf
    UefiHiiServicesLib|MdeModulePkg/Library/UefiHiiServicesLib/UefiHiiServicesLib.inf
    HiiLib|MdeModulePkg/Library/UefiHiiLib/UefiHiiLib.inf
    DevicePathLib|MdePkg/Library/UefiDevicePathLib/UefiDevicePathLib.inf
    UefiDecompressLib|MdePkg/Library/BaseUefiDecompressLib/BaseUefiDecompressLib.inf
    PeiServicesTablePointerLib|MdePkg/Library/PeiServicesTablePointerLib/PeiServicesTablePointerLib.inf
    PeiServicesLib|MdePkg/Library/PeiServicesLib/PeiServicesLib.inf
    DxeServicesLib|MdePkg/Library/DxeServicesLib/DxeServicesLib.inf
    DxeServicesTableLib|MdePkg/Library/DxeServicesTableLib/DxeServicesTableLib.inf


##
# Generic Modules
##
    UefiUsbLib|MdePkg/Library/UefiUsbLib/UefiUsbLib.inf
    UefiScsiLib|MdePkg/Library/UefiScsiLib/UefiScsiLib.inf
    SafeIntLib|MdePkg/Library/BaseSafeIntLib/BaseSafeIntLib.inf
    UefiBootManagerLib|MdeModulePkg/Library/UefiBootManagerLib/UefiBootManagerLib.inf
    VariablePolicyHelperLib|MdeModulePkg/Library/VariablePolicyHelperLib/VariablePolicyHelperLib.inf
    RegisterFilterLib|MdePkg/Library/RegisterFilterLibNull/RegisterFilterLibNull.inf
    StackCheckLib|MdePkg/Library/StackCheckLibNull/StackCheckLibNull.inf
    StackCheckFailureHookLib|MdePkg/Library/StackCheckFailureHookLibNull/StackCheckFailureHookLibNull.inf
    NetLib|NetworkPkg/Library/DxeNetLib/DxeNetLib.inf
    IpIoLib|NetworkPkg/Library/DxeIpIoLib/DxeIpIoLib.inf
    UdpIoLib|NetworkPkg/Library/DxeUdpIoLib/DxeUdpIoLib.inf
    TcpIoLib|NetworkPkg/Library/DxeTcpIoLib/DxeTcpIoLib.inf
    DpcLib|NetworkPkg/Library/DxeDpcLib/DxeDpcLib.inf
    SecurityManagementLib|MdeModulePkg/Library/DxeSecurityManagementLib/DxeSecurityManagementLib.inf
    TimerLib|MdePkg/Library/BaseTimerLibNullTemplate/BaseTimerLibNullTemplate.inf
    SerialPortLib|MdePkg/Library/BaseSerialPortLibNull/BaseSerialPortLibNull.inf
    CapsuleLib|MdeModulePkg/Library/DxeCapsuleLibNull/DxeCapsuleLibNull.inf
    PcdLib|MdePkg/Library/BasePcdLibNull/BasePcdLibNull.inf


##
# Misc
##
    DebugLib|MdePkg/Library/BaseDebugLibNull/BaseDebugLibNull.inf
    DebugPrintErrorLevelLib|MdePkg/Library/BaseDebugPrintErrorLevelLib/BaseDebugPrintErrorLevelLib.inf
    ReportStatusCodeLib|MdePkg/Library/BaseReportStatusCodeLibNull/BaseReportStatusCodeLibNull.inf
    PeCoffExtraActionLib|MdePkg/Library/BasePeCoffExtraActionLibNull/BasePeCoffExtraActionLibNull.inf
    PerformanceLib|MdePkg/Library/BasePerformanceLibNull/BasePerformanceLibNull.inf
    DebugAgentLib|MdeModulePkg/Library/DebugAgentLibNull/DebugAgentLibNull.inf
    PlatformHookLib|MdeModulePkg/Library/BasePlatformHookLibNull/BasePlatformHookLibNull.inf
    ResetSystemLib|MdeModulePkg/Library/BaseResetSystemLibNull/BaseResetSystemLibNull.inf
    SmbusLib|MdePkg/Library/DxeSmbusLib/DxeSmbusLib.inf
    S3BootScriptLib|MdeModulePkg/Library/PiDxeS3BootScriptLib/DxeS3BootScriptLib.inf
    CpuExceptionHandlerLib|MdeModulePkg/Library/CpuExceptionHandlerLibNull/CpuExceptionHandlerLibNull.inf
    MemoryAllocationLib|MdePkg/Library/UefiMemoryAllocationLib/UefiMemoryAllocationLib.inf
    HobLib|MdePkg/Library/DxeHobLib/DxeHobLib.inf
    BaseStackCheckLib|MdePkg/Library/StackCheckLibNull/StackCheckLibNull.inf
# For Debug logging ... Initially from Clover by Jief_Machak (sf.net/u/jief7/profile)
    MemLogLib|MeridianPkg/Library/MemLogLib/MemLogLib.inf
    FrameBufferBltLib|MdeModulePkg/Library/FrameBufferBltLib/FrameBufferBltLib.inf
# MtrrLib (memory-type range registers) is an x86 concept; scoped to X64 below.
    CpuLib|MdePkg/Library/BaseCpuLib/BaseCpuLib.inf
# For SupplyAPFS
    MeridianApfsLib|MeridianPkg/Library/MeridianApfsLib/MeridianApfsLib.inf
# For AcquireGOP
    HandleParsingLib|ShellPkg/Library/UefiHandleParsingLib/UefiHandleParsingLib.inf
    FileHandleLib|MdePkg/Library/UefiFileHandleLib/UefiFileHandleLib.inf
    SortLib|MdeModulePkg/Library/UefiSortLib/UefiSortLib.inf
# For SupplyNVME
    NvmExpressLib|MeridianPkg/Library/NvmExpressLib/NvmExpressLib.inf
# Embedded Lua 5.4 interpreter (freestanding, BaseLib shim)
    LuaLib|MeridianPkg/Library/LuaLib/LuaLib.inf
# libstatemachines runtime (freestanding profile, zero heap)
    StateMachineLib|MeridianPkg/Library/StateMachineLib/StateMachineLib.inf
# Shared rEFInd-derived "fsw" filesystem framework (core + EFI glue + utils),
# linked once into every filesystems/*.inf driver.
    FswFrameworkLib|MeridianPkg/filesystems/FswFrameworkLib.inf


##
# x86-only library instances. CF8/CFC port-mapped PCI config and MTRRs do not
# exist on AArch64, so these are scoped to X64 instead of living in the common
# [LibraryClasses] block.
##
[LibraryClasses.X64]
    PciLib|MdePkg/Library/BasePciLibCf8/BasePciLibCf8.inf
    PciCf8Lib|MdePkg/Library/BasePciCf8Lib/BasePciCf8Lib.inf
    MtrrLib|UefiCpuPkg/Library/MtrrLib/MtrrLib.inf


##
# AArch64 (Apple Silicon under Asahi m1n1 -> U-Boot UEFI, and generic arm64 UEFI).
# PCI config on arm64 is memory-mapped (ECAM), so PciLib maps to the PCI Express
# (MMIO) instance. It only matters if a pulled-in module actually walks PCI; a
# boot manager normally does not, but the class must resolve for the dep graph.
# PcdPciExpressBaseAddress must point at the ECAM window if PCI is ever exercised.
##
[LibraryClasses.AARCH64]
    CompilerIntrinsicsLib|MdePkg/Library/CompilerIntrinsicsLib/CompilerIntrinsicsLib.inf
    PciLib|MdePkg/Library/BasePciLibPciExpress/BasePciLibPciExpress.inf
    PciExpressLib|MdePkg/Library/BasePciExpressLib/BasePciExpressLib.inf


[Components]
    MeridianPkg/Meridian.inf
    MeridianPkg/Library/LuaLib/LuaLib.inf
    MeridianPkg/Library/LuaLib/test/LuaTest.inf
    MeridianPkg/Library/StateMachineLib/StateMachineLib.inf
    MeridianPkg/Application/SmSelfTest/SmSelfTest.inf
    MeridianPkg/Application/DarkPassenger/DarkPassenger.inf
    MeridianPkg/Application/Cydia/Cydia.inf
    MeridianPkg/net/snp/MeridianSnpDiscover.inf
    MeridianPkg/filesystems/FswFrameworkLib.inf
    MeridianPkg/filesystems/bcachefs.inf
    MeridianPkg/filesystems/btrfs.inf
    MeridianPkg/filesystems/ext2.inf
    MeridianPkg/filesystems/ext4.inf
    MeridianPkg/filesystems/hfs.inf
    MeridianPkg/filesystems/iso9660.inf
    MeridianPkg/filesystems/ntfs.inf
    MeridianPkg/filesystems/ufs.inf


[PcdsFixedAtBuild]
    gEfiMdePkgTokenSpaceGuid.PcdDebugPropertyMask|0x2F
    gEfiMdePkgTokenSpaceGuid.PcdDebugClearMemoryValue|0x00
    gEfiMdePkgTokenSpaceGuid.PcdDebugPrintErrorLevel|0x00000000


[BuildOptions.common]
    # FSW_DEBUG_LEVEL is consumed by the filesystems/ framework (FswFrameworkLib +
    # drivers); it used to live in each driver .inf. Mapped to the build target here
    # so the framework no longer needs per-driver [BuildOptions]. FSTYPE is gone
    # entirely (drivers now export fsw_active_fstype_table / _name instead).
    # FSW_DEBUG_LEVEL is deliberately decoupled from MERIDIAN_DEBUG. The fsw
    # filesystem drivers are separate EFI images: their FSW_MSG_* macros expand
    # to Print() -> ConOut (the screen), with no access to Meridian's in-memory
    # log buffer or on-ESP log file. At level >=1 they emit a per-volume,
    # per-op trace, so on a multiboot box (N drivers x M volumes) they bury the
    # console in a wall of output on every boot. The fleet runs DEBUG for
    # Meridian's *file* log (which already records the volume scan via INFO_LOG),
    # so the drivers stay quiet (level 0) there. Driver-internal tracing is a
    # bench activity: use the NOOPT/forensic build (level 2) when you need it.
    DEFINE RFT_BLD_REL      = -DMERIDIAN_DEBUG=0 -DMDEPKG_NDEBUG -DFSW_DEBUG_LEVEL=0
    DEFINE RFT_BLD_DBG      = -DMERIDIAN_DEBUG=1 -DFSW_DEBUG_LEVEL=0
    DEFINE RFT_BLD_NPT      = -DMERIDIAN_DEBUG=2 -DFSW_DEBUG_LEVEL=2

    *_*_*_CC_FLAGS = -nostdlibinc
    *_*_*_CC_FLAGS = -DSTATE_MACHINE_FREESTANDING -I$(WORKSPACE)/MeridianPkg/Library/StateMachineLib -I$(WORKSPACE)/MeridianPkg/libstatemachines/include

    XCODE:*_*_*_DLINK_FLAGS = -seg1addr 0x300



[BuildOptions.X64]
    CLANG38:RELEASE_*_*_CC_FLAGS    = -Os -DEFIX64 $(RFT_BLD_REL)
    CLANG38:DEBUG_*_*_CC_FLAGS      = -Os -DEFIX64 $(RFT_BLD_DBG)
    CLANG38:NOOPT_*_*_CC_FLAGS      = -Os -DEFIX64 $(RFT_BLD_NPT)
    XCODE:RELEASE_*_*_CC_FLAGS      = -Os -DEFIX64 $(RFT_BLD_REL)
    XCODE:DEBUG_*_*_CC_FLAGS        = -Os -DEFIX64 $(RFT_BLD_DBG)
    XCODE:NOOPT_*_*_CC_FLAGS        = -Os -DEFIX64 $(RFT_BLD_NPT)
    GCC:RELEASE_*_*_CC_FLAGS        = -Os -DEFIX64 $(RFT_BLD_REL)
    GCC:DEBUG_*_*_CC_FLAGS          = -Os -DEFIX64 $(RFT_BLD_DBG)
    GCC:NOOPT_*_*_CC_FLAGS          = -Os -DEFIX64 $(RFT_BLD_NPT)


[BuildOptions.AARCH64]
    CLANG38:RELEASE_*_*_CC_FLAGS    = -Os -DEFIAARCH64 $(RFT_BLD_REL)
    CLANG38:DEBUG_*_*_CC_FLAGS      = -Os -DEFIAARCH64 $(RFT_BLD_DBG)
    CLANG38:NOOPT_*_*_CC_FLAGS      = -Os -DEFIAARCH64 $(RFT_BLD_NPT)
    XCODE:RELEASE_*_*_CC_FLAGS      = -Os -DEFIAARCH64 $(RFT_BLD_REL)
    XCODE:DEBUG_*_*_CC_FLAGS        = -Os -DEFIAARCH64 $(RFT_BLD_DBG)
    XCODE:NOOPT_*_*_CC_FLAGS        = -Os -DEFIAARCH64 $(RFT_BLD_NPT)
    GCC:RELEASE_*_*_CC_FLAGS        = -Os -DEFIAARCH64 $(RFT_BLD_REL)
    GCC:DEBUG_*_*_CC_FLAGS          = -Os -DEFIAARCH64 $(RFT_BLD_DBG)
    GCC:NOOPT_*_*_CC_FLAGS          = -Os -DEFIAARCH64 $(RFT_BLD_NPT)
