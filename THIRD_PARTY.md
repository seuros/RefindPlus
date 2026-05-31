# Third-Party And Inherited Code

This repository contains local Meridian code, inherited RefindPlus/rEFInd code,
and imported third-party components. Keep those categories visible when making
changes.

## Inherited Project Lineage

| Component | Origin | Notes |
| --- | --- | --- |
| Boot manager core | rEFInd, RefindPlus | Local Meridian changes live under the subsystem tree (`boot/`, `config/`, `discovery/`, `drivers/`, `install/`, `kernel/`, `lib/`, `loaders/`, `platform/`, `policy/`, `remote/`, `storage/`, `tools/`, `ui/`), package metadata, and assets. |
| Secure Boot/MOK support | rEFInd/RefindPlus lineage | Located under `mok/`. |
| EFI helper code | rEFInd/RefindPlus lineage plus EDK2-derived interfaces | Located under `EfiLib/` and packaged subsystem/library headers. |

## Imported Libraries

| Path | Origin | License Tracking |
| --- | --- | --- |
| `filesystems/minilzo.*`, `filesystems/lzoconf.h`, `filesystems/lzodefs.h` | minilzo/LZO | Annotated in `REUSE.toml` as GPL-2.0-or-later. |
| `filesystems/zstd/**` | zstd | Annotated in `REUSE.toml` as BSD-2-Clause OR GPL-2.0-or-later. |
| `Library/NvmExpressLib/**` | EDK2/OpenCore-adjacent NVMe support lineage | Preserve upstream notices and verify package compatibility before changes. |
| `Library/MeridianApfsLib/**` | OpenCore APFS support lineage | Preserve upstream notices and verify package compatibility before changes. |
| `Library/MemLogLib/**` | Clover-derived logging support | Preserve upstream notices. |

## Assets

| Path | Notes |
| --- | --- |
| `conn/art/**` | Per-OS splash SVGs baked into the Conn UI atlas. |

## Update Rule

When importing or updating third-party code:

1. Record origin, version or commit, and local patches in this file.
2. Preserve upstream copyright notices.
3. Update `REUSE.toml` and run `./scripts/check.sh`.
4. Keep mechanical vendor updates separate from local Meridian behavior changes.
