# Community Help Wanted

Things where human hands and eyes are genuinely needed.
I can't do everything, and I'm honest about where I fall short.
Pull requests welcome -- no formality required, just open one.

---

## 1. Better splash / boot logo art

**File:** `conn/art/splash/*.svg`

The current SVGs are rough first-pass silhouettes.
Some I hand-drew myself and I am bad at SVG.
Some were machine-extracted by an AI that is blind and sincerely
believes every logo looks perfect.

If you have SVG or illustration skills, any of the logos could use love:

| File | Status | Notes |
|------|--------|-------|
| `macos.svg` | passable | Apple logo from Wikimedia -- shape ok, could be cleaner |
| `openbsd.svg` | ok | Puffy from Wikimedia, complex path |
| `netbsd.svg` | ok | Flag + pole extracted from official SVG |
| `freebsd.svg` | rough | Hand-drawn beastie -- really needs a proper source |
| `dragonfly.svg` | rough | Hand-drawn insect silhouette |
| `arch.svg` | rough | Hand-drawn A logo |
| `windows.svg` | rough | Four squares -- works but basic |
| `9front.svg` | rough | Hand-drawn rabbit |
| `linux.svg` | rough | Hand-drawn Tux |
| `meridian.svg` | rough | Our own logo -- should look the best, currently does not |

How to contribute: replace the SVG, run `make -C conn splash` to bake
it, open a PR. No code change needed.

---

## 2. Per-OS color splash (read colors from SVG)

**Files:** `conn/tools/bake_splash.py`, `conn/core/splash_atlas.h`,
`ui/conn_bridge.c` -- `ConnLaunchSplash()`

Right now the bake pipeline throws away all color information and stores
only a mono alpha/coverage plane. At runtime the logo is tinted with a
single hardcoded accent color per OS.

A better implementation would:
- Extract per-pixel RGB from the SVG at bake time (not just alpha)
- Store a compact color-indexed or full-RGB RLE plane in `splash_atlas.h`
- Have `ConnLaunchSplash()` composite the real colors instead of tinting

The pipeline is in `conn/tools/bake_splash.py` (Python + Pillow +
rsvg-convert). The runtime renderer is `ConnLaunchSplash()` in
`ui/conn_bridge.c`. Both ends need updating.

---

## 3. Broader OS testing

The OSes I have splash logos for have been booted and verified on my hardware.
What I have not tested at all, or only superficially:

- **ReactOS** -- EFI boot is relatively new there, happy to hear any result
- **Redox OS** -- UEFI support exists, nobody has tried it with Meridian
- **Windows on FAT32 ESP** -- most testing was on NTFS/standard layouts
- **Windows with non-standard partition schemes** -- e.g. no MSR, merged partitions
- **Exotic Linux setups** -- btrfs-native initrd, dracut with unusual modules, etc.
- **Any OS on ARM hardware** -- the AArch64 build exists but fleet coverage is thin

**Virtual machines / hypervisors are also very welcome.** Everything I
have tested was on real hardware -- months of crashes on bare metal. I
have not done systematic testing under QEMU, VirtualBox, VMware, Hyper-V,
or any other hypervisor. If you have a VM setup handy, firing it up there
costs you nothing and tells me a lot.

Open an issue with your hardware (or hypervisor), OS version, what you
tried, and what happened -- good or bad. Confirmed working configs are as
useful to me as bug reports.

---

*More items coming. Check back or watch the repo.*
