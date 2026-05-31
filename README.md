# Meridian

Meridian is a UEFI boot manager for x86_64 systems. It is the current project
identity for a codebase descended from RefindPlus and rEFInd, with local work
focused on EDK2 builds, Apple firmware support, generic UEFI PCs, OpenCore and
Clover chain-loading, Secure Boot/MOK tooling, and filesystem driver support.

The public name of this repository is Meridian. New package metadata, user
documentation, generated assets, workflow names, and user-facing text should use
Meridian. Older RefindPlus and rEFInd names remain only where they describe
lineage, imported code, or platform-standard runtime paths such as the UEFI
fallback loader and Secure Boot shim/MOK tooling.

## Project Status

- Supported build target: X64 UEFI application.
- Supported build surface: EDK2 package metadata in `Meridian.inf`,
  `MeridianPkg.dsc`, and `MeridianPkg.dec`.
- Removed build surface: inherited GNU-EFI and old Tiano subdirectory
  makefiles.
- Current workspace dependency: an EDK2-compatible tree with the packages listed
  in `MeridianPkg.dsc`; in practice this is still the inherited
  RefindPlusUDK-style workspace until those package dependencies are fully
  renamed or vendored.
- Repository checks cover REUSE metadata, EDK2 manifest entries, active workflow
  naming, and accidental return of removed makefiles.

## Features

Meridian inherits the rEFInd-style boot manager model and extends it with local
behavior exposed through the auto-generated `config.conf` and companion notes:

- graphical boot menu, loader scanning, manual boot stanzas, and EFI tool
  launching
- Apple firmware handling, APFS discovery/synchronisation, Apple recovery and
  hardware-test entries, CSR/SIP controls, and macOS boot argument helpers
- generic UEFI PC support, OpenCore/Clover detection, and UEFI driver loading
- Secure Boot validation support and MOK manager integration
- filesystem drivers for common non-FAT filesystems used during boot discovery
- optional NVMe, APFS, UEFI-revision, console, graphics, and NVRAM support
  paths controlled through configuration tokens

Meridian is **not** a drop-in configuration replacement for rEFInd or
RefindPlus. Start from no config, let Meridian auto-detect the machine and write
`config.conf` from effective defaults, then edit that file if needed. If an
inherited config is present, clean it manually or delete it.

## Repository Layout

| Path | Purpose |
| --- | --- |
| `boot/` | UEFI application entry, early globals, and ESP-layout constants only. |
| `config/` | Configuration parser, token handlers, stanzas, and config synchronisation. |
| `discovery/` | Boot-entry discovery and scanner strategies. |
| `drivers/` | UEFI/DXE driver loading support. |
| `install/` | Installer and ESP deployment helpers. |
| `kernel/` | Core runtime services used by the boot-manager loop. |
| `lib/` | Meridian-local utility library and string helpers. |
| `loaders/` | OS / EFI target launchers and loader classification. |
| `platform/` | Firmware/platform quirks and hardware/system information. |
| `policy/` | NVRAM, trust, Secure Boot, CSR, and boot-policy glue. |
| `remote/` | Optional serial/web/netboot remote access and remote boot channels. |
| `storage/` | Volumes, GPT, file I/O, and storage-facing helpers. |
| `tools/` | Local operator tools and their menus. |
| `ui/` | Display, menus, screen management, and Conn bridge. |
| `conn/` | Conn renderer, reducers, widgets, and standalone test apps. |
| `EfiLib/` | UEFI helper code inherited from the upstream family. |
| `filesystems/` | EFI filesystem drivers and imported filesystem support code. |
| `Library/` | EDK2 libraries for APFS, NVMe, logging, and related support. |
| `mok/` | Secure Boot and Machine Owner Key support. |
| `net/` | Network-related inherited support code. |
| `scripts/` | Local maintenance and build entrypoints. |


## Build

Meridian builds as an EDK2 package. Expose this checkout inside an EDK2
workspace as `MeridianPkg`:

```sh
ln -s /path/to/meridian /path/to/edk2/MeridianPkg
```

Initialize EDK2, build BaseTools, then run the wrapper from this repository:

```sh
cd /path/to/edk2
source edksetup.sh
make -C BaseTools

cd /path/to/meridian
./scripts/build.sh --workspace /path/to/edk2 --arch X64 --target DEBUG
```

The wrapper is intentionally strict: it validates that the EDK2 workspace exposes
this checkout as `MeridianPkg` before invoking `build`.

## Install

Installing a built Meridian is essentially copying `Meridian.efi` to the
firmware fallback path and keeping Meridian's home under `\EFI\Meridian\`:
`config.conf`, boot-time `fs/` and `drivers/`, optional local `tools/`, and
optional remote-access `remote/`.

See `INSTALL.md` for the full ESP layout, copy commands, and per-platform boot
registration (fallback `BOOTX64.EFI`, Linux `efibootmgr`, macOS `bless`).

**Coexisting with OpenCore (hackintosh):** Meridian is the primary loader and
OpenCore is a chainload target beneath it -- install Meridian at the fallback
`\EFI\BOOT\BOOTX64.EFI` and keep OpenCore at `\EFI\OC\OpenCore.efi`. Meridian
then auto-presents a single **macOS (via OpenCore)** entry that chainloads
OpenCore (which supplies the kext/SMBIOS injection macOS needs). Do not run the
reverse (OpenCore → Meridian): a re-launch of OpenCore returns
`EFI_ALREADY_STARTED`, so when Meridian detects it was chainloaded *by* OpenCore
it suppresses that entry. See `INSTALL.md` §3b.

## Checks

Run the repository health check before changing firmware behavior:

```sh
./scripts/check.sh
```

The check validates REUSE metadata when `reuse` is installed, verifies that
sources listed in `Meridian.inf` exist, verifies package components listed in
`MeridianPkg.dsc`, confirms active workflow metadata uses the Meridian identity,
and fails if removed legacy makefiles are reintroduced.

For build metadata changes, also validate the build command resolution:

```sh
./scripts/build.sh --workspace /path/to/edk2 --dry-run
```

## Provenance And Configuration Policy

Meridian uses `config.conf` as its configuration file and generates one from
effective defaults when it is missing. A missing or deleted config is a
supported, first-class path: Meridian auto-detects entries, writes a starter
config, and keeps booting with in-memory defaults if the ESP is read-only.

Meridian does not preserve stale rEFInd/RefindPlus config-token compatibility.
Clean inherited configs against the generated Meridian file, or delete them and
let auto-detection take over.

## Contributing

Read `CONTRIBUTING.md` before larger changes. Keep changes small, keep imported
code separate from local Meridian behavior, run `./scripts/check.sh`, and avoid
mass-reformatting unrelated files.
