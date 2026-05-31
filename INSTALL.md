# Installing Meridian

Installing Meridian is essentially: **drop the binary at the firmware fallback
path and put everything else under `\EFI\Meridian\`, then point the firmware at
it.** Meridian's home is **always `\EFI\Meridian\`** -- config, filesystem
drivers, tools, and its runtime state live there and nowhere else. This is a
decree, not a convention.

This document covers deployment of an already-built binary.

## The layout (the one and only home)

```text
<ESP>/
├── EFI/
│   ├── BOOT/
│   │   └── BOOTX64.EFI         Meridian binary at the firmware fallback path
│   └── Meridian/               Meridian's home -- ALWAYS here
│       ├── Meridian.efi        the binary (when booted via an NVRAM entry)
│       ├── config.conf         auto-generated on first boot if absent
│       ├── fs/                 filesystem drivers -- loaded at boot
│       ├── drivers/            device-driver blobs -- loaded at boot
│       ├── tools/              optional local utility .efi -- loaded on demand
│       └── remote/             optional remote reach/boot .efi -- on demand
```

Two things matter and are non-negotiable:

- **Home is fixed.** Wherever the firmware launches the binary from (the fallback
  `\EFI\BOOT\BOOTX64.EFI`, or an NVRAM entry pointing at
  `\EFI\Meridian\Meridian.efi`), Meridian anchors its home at `\EFI\Meridian\`
  unconditionally. There is no "install it wherever you like", no asset probing,
  and no override.
- **`\EFI\BOOT\` is binary-only.** Put *only* `BOOTX64.EFI` there. Meridian never
  reads config from nor writes anything back into `\EFI\BOOT\`; everything
  (config, menu cache, vars) is read and written under `\EFI\Meridian\`.

## What you need

From the build output directory
`edk2/Build/Meridian/<TARGET>_CLANGDWARF/X64/` (where `<TARGET>` is `RELEASE`,
`DEBUG`, or `NOOPT`):

- `Meridian.efi` -- the boot manager itself.
- Filesystem driver binaries (needed to read non-FAT boot/root partitions --
  e.g. `ext4.efi` to derive a Linux `root=` from `/etc/fstab`):
  `ext2.efi`, `ext4.efi`, `btrfs.efi`, `bcachefs.efi`, `ufs.efi`, `hfs.efi`,
  `ntfs.efi`, `iso9660.efi`. APFS and NVMe support are compiled **into**
  `Meridian.efi` -- do not drop them in `fs/`.

No icons, fonts, or banner folders are required -- the default banner is embedded
in the binary. `config.conf` is optional: Meridian auto-generates one in
`\EFI\Meridian\` on first boot.

If you have not built yet, the short version:

```sh
cd /path/to/meridian
mise run build                 # X64 DEBUG -> edk2/Build/Meridian/DEBUG_CLANGDWARF/X64/
# or, explicitly:
./scripts/build.sh --workspace /path/to/edk2 --arch X64 --target RELEASE
```

## 1. Mount the ESP

The ESP is the FAT32 partition flagged as EFI System.

**Linux:**

```sh
lsblk -o NAME,FSTYPE,PARTTYPENAME,SIZE   # find the EFI System partition, e.g. /dev/sda1
sudo mkdir -p /mnt/esp
sudo mount /dev/sda1 /mnt/esp
```

**macOS:**

```sh
diskutil list                            # find the EFI partition, e.g. disk0s1
sudo diskutil mount disk0s1              # mounts at /Volumes/EFI
```

**Windows (PowerShell, admin):**

```powershell
mountvol S: /S                           # assigns the ESP to drive S:
```

**FreeBSD:**

```sh
gpart show                               # find the "efi" partition, e.g. nda1p1
sudo mkdir -p /mnt/esp
sudo mount -t msdosfs /dev/nda1p1 /mnt/esp
# read-only first if you only want to inspect:  mount -t msdosfs -o ro ...
```

## 2. Copy the bundle

From the repository root (Linux example; adjust the ESP mountpoint and build
target as needed):

```sh
ESP=/mnt/esp
BUILD=edk2/Build/Meridian/RELEASE_CLANGDWARF/X64

# Meridian's home + capability folders
sudo mkdir -p "$ESP/EFI/Meridian/fs" "$ESP/EFI/Meridian/drivers"

# the binary at the firmware fallback path (binary-only directory!)
sudo mkdir -p "$ESP/EFI/BOOT"
sudo cp "$BUILD/Meridian.efi" "$ESP/EFI/BOOT/BOOTX64.EFI"

# (optional) also keep a copy in the home for an NVRAM boot entry
sudo cp "$BUILD/Meridian.efi" "$ESP/EFI/Meridian/Meridian.efi"

# filesystem drivers -> fs/  (arch-agnostic names, no _x64 suffix)
sudo cp "$BUILD"/{ext2,ext4,btrfs,bcachefs,ufs,hfs,ntfs,iso9660}.efi \
        "$ESP/EFI/Meridian/fs/"

# optional: drop any device-driver blobs (NIC/USB3/storage) in drivers/
# sudo cp broadcom.efi "$ESP/EFI/Meridian/drivers/"

# no config copy needed: missing config.conf is auto-generated on first boot
```

Do **not** copy `config.conf`, `fs/`, or any folders into `\EFI\BOOT\` -- that
directory holds the binary and nothing else.

## 3. Make the firmware boot Meridian

Pick one of the following. Either way, the home stays `\EFI\Meridian\`.

### a) Removable / fallback path -- simplest, no NVRAM changes

Most firmware automatically boots `\EFI\BOOT\BOOTX64.EFI` from the ESP. Step 2
already placed the binary there. Nothing else goes in that directory -- Meridian
finds its config and drivers under `\EFI\Meridian\` on its own.

### b) Linux -- register a boot entry with `efibootmgr`

```sh
sudo efibootmgr --create \
  --disk /dev/sda --part 1 \
  --label "Meridian" \
  --loader '\EFI\Meridian\Meridian.efi'
```

Use `--disk`/`--part` matching the disk and ESP partition number. Check and
reorder entries with `efibootmgr -v` and `efibootmgr -o <bootnum>,...`.

### c) macOS -- set the boot file with `bless`

```sh
sudo bless --mount /Volumes/EFI --setBoot \
  --file /Volumes/EFI/EFI/Meridian/Meridian.efi
```

On modern Macs you may need to relax System Integrity Protection to bless a
custom loader: boot into Recovery and run `csrutil disable` (or
`csrutil enable --without nvram`), then re-`bless`. Re-enable SIP afterward if
you wish.

### d) FreeBSD -- use the fallback path

FreeBSD ships `efibootmgr`, but EFI runtime variables are frequently
unavailable (`efibootmgr` reports *"efi variables not supported on this system,
kldload efirt?"*, and `efirt` is not loadable on every board). Don't fight it:
use the **fallback path (3a)** -- drop the binary at `\EFI\BOOT\BOOTX64.EFI` and
let the firmware boot it. No NVRAM entry is needed, and the home stays
`\EFI\Meridian\` regardless. This is the supported route for FreeBSD-only nodes
and for firmware that ignores externally-written boot entries.

## 3b. Coexisting with OpenCore (hackintosh / macOS)

On a hackintosh, macOS can only boot through OpenCore -- it injects the kexts,
ACPI patches, and SMBIOS that the macOS kernel needs. Meridian is a boot
*manager*, not a kext injector, so it boots macOS by chainloading OpenCore.

**Meridian must be primary; OpenCore is a chainload target underneath it:**

```text
Firmware ─▶ Meridian (\EFI\BOOT\BOOTX64.EFI, fallback)
              └─▶ macOS entry ─▶ \EFI\OC\OpenCore.efi ─▶ macOS
```

- Put **Meridian** at the fallback `\EFI\BOOT\BOOTX64.EFI` (see 3a).
- Keep **OpenCore** at `\EFI\OC\OpenCore.efi`. Do **not** leave OpenCore's
  bootstrap at `\EFI\BOOT\BOOTX64.EFI` -- that makes OpenCore primary and inverts
  the topology.
- **Disable OpenCore self-blessing.** In `config.plist` set
  `Misc → Boot → LauncherOption = Disabled`. With the default `Full`/`Short`,
  OpenCore re-registers itself as the first NVRAM boot entry on *every* run and
  silently reclaims the boot order from Meridian -- so the firmware boots
  OpenCore, which then chainloads Meridian (the unsupported reverse topology).
  `Disabled` leaves the boot order to Meridian.

Meridian auto-detects this layout: on non-Apple firmware where it finds both a
macOS APFS volume and `\EFI\OC\OpenCore.efi`, it drops the direct APFS macOS
entry (which would kernel-panic without injection) and presents a single
**macOS (via OpenCore)** entry that chainloads OpenCore. No config needed.

**Do not run the reverse (OpenCore ─▶ Meridian).** If OpenCore is primary and
chainloads Meridian, a second launch of `OpenCore.efi` returns
`EFI_ALREADY_STARTED` (OpenCore refuses to re-initialise), and returning to
OpenCore's picker is unreliable. When Meridian detects it was itself launched by
OpenCore (its parent image is `OpenCore.efi`), it suppresses the
macOS-via-OpenCore entry rather than offer a route that cannot work -- boot macOS
from OpenCore's own picker in that case, or move Meridian to the fallback path.

## 4. Secure Boot / MOK (optional)

Meridian supports Secure Boot validation and Machine Owner Key (MOK) enrollment
(see `mok/`). For the first boot you can either:

- **Disable Secure Boot** in firmware setup, boot Meridian, confirm it works,
  then re-enable and enroll keys; or
- **Enroll Meridian's key** via your shim/MOK manager so the firmware trusts the
  loader.

Relevant behavior is controlled through `config.conf`. Boot once without a
config to let Meridian generate the current starter file, then edit that file if
needed.

## 5. First boot and configuration

Reboot and select Meridian (or let the fallback path boot it). On first run
Meridian auto-generates a `config.conf` under `\EFI\Meridian\` if one is absent.

Linux entries need no per-machine stanza: once the matching filesystem driver is
in `\EFI\Meridian\fs\`, Meridian reads the root volume's `/etc/fstab` and derives
`root=` automatically.

Do not carry a rEFInd/RefindPlus config over as a drop-in replacement. If you
already have one on the ESP, either clean it manually or delete it and let
Meridian auto-detect entries and regenerate a fresh `config.conf` from effective
defaults.

## Troubleshooting

- **Firmware skips Meridian / boots straight to the OS** -- either use the
  fallback `\EFI\BOOT\BOOTX64.EFI` path, or verify your NVRAM entry and boot
  order with `efibootmgr -v` (Linux) or re-run `bless` (macOS). Some firmware
  also has a one-time boot menu (often F12/F8) to pick the entry manually.
- **Linux/other partitions not detected, or a Linux entry panics at
  `switch_root`** -- the matching filesystem driver is missing from
  `\EFI\Meridian\fs\` (e.g. `ext4.efi` for an ext4 root). Without it Meridian
  cannot read `/etc/fstab`, so no `root=` is derived and the kernel boots with no
  root. Copy the needed driver into `fs/` and reboot.
- **Config/driver changes seem ignored** -- make sure you edited the files under
  `\EFI\Meridian\`, not `\EFI\BOOT\`. `\EFI\BOOT\` is binary-only and is never
  read for configuration.
