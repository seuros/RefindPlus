#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
#
# build-rescue-stick.sh -- reproducible headless Meridian rescue USB.
#
# Dogfoods Meridian as the bootloader for an Arch live system that:
#   * autologins root on tty1 + ttyS0 (headless / serial friendly)
#   * brings up the network (dhcpcd) and sshd (key-only, root via key)
#   * trusts your SSH pubkey out of the box
#   * carries the firmware recon toolkit: flashrom + the coreboot utils
#     (cbmem, cbfstool, ifdtool, inteltool, intelmetool, intelvbttool,
#      superiotool, nvramtool, msrtool, ectool, amdfwtool, pmh7tool,
#      acpidump-all, autoport, me_cleaner)
#
# The coreboot utils are AUR-only and pacstrap can only install from configured
# repos, so build_aur_repo() builds them into a local file:// repo first.
#
# Layout written to the USB (single GPT FAT32 ESP, label RESCUE):
#   /EFI/BOOT/bootx64.efi          <- Meridian firmware fallback binary
#   /EFI/Meridian/Meridian.efi     <- Meridian home binary
#   /EFI/Meridian/config.conf      <- menuentries: live system + memtest
#   /EFI/tools/memtest.efi         <- memtest86+ (standalone EFI app)
#   /arch/boot/x86_64/vmlinuz-linux + initramfs-linux.img
#   /arch/x86_64/airootfs.erofs (+ .sha512)
#
# memtest86+ is a bare EFI application, not a kernel: it is booted directly by
# Meridian and would be dead weight inside the live rootfs. So it is pulled
# from [extra] and unpacked straight onto the ESP rather than added to
# packages.x86_64 -- which also means adding it costs no image rebuild.
#
# Usage:
#   scripts/build-rescue-stick.sh --build-only          # build artifacts only
#   scripts/build-rescue-stick.sh --flash /dev/sdX      # WIPES disk, provisions from scratch
#   scripts/build-rescue-stick.sh --update /dev/sdX     # swap live components, keep the ESP
#   scripts/build-rescue-stick.sh --flash /dev/sdX --qemu-test
#
# Prefer --update on an already-provisioned stick: --flash regenerates
# config.conf and destroys anything the stick collected in the field.
#
# Env overrides:
#   PUBKEY     path to SSH pubkey to trust   (default: ~/.ssh/id_rsa.pub)
#   MERIDIAN   path to the Meridian .efi     (default: latest CLANGDWARF build output)
#   WORK       archiso work dir              (default: /tmp/rescue-work)
#   OUT        archiso output dir            (default: /tmp/rescue-out)
#   PROF       generated profile dir         (default: /tmp/rescue-prof)
#   AURREPO    local AUR package repo        (default: /tmp/rescue-aur)
#   AURSRC     AUR build scratch dir         (default: /tmp/rescue-aur-src)
#   TOOLSRC    ESP EFI-app staging dir       (default: /tmp/rescue-tools)
#   AUR_REBUILD=1  force a rebuild of the coreboot utils
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PUBKEY="${PUBKEY:-$HOME/.ssh/id_rsa.pub}"
WORK="${WORK:-/tmp/rescue-work}"
OUT="${OUT:-/tmp/rescue-out}"
PROF="${PROF:-/tmp/rescue-prof}"
AURREPO="${AURREPO:-/tmp/rescue-aur}"
AURSRC="${AURSRC:-/tmp/rescue-aur-src}"
TOOLSRC="${TOOLSRC:-/tmp/rescue-tools}"
AURDB="rescue-aur"
ISO_LABEL="RESCUE"

MODE="" ; DEV="" ; QEMU=0
while [ $# -gt 0 ]; do
  case "$1" in
    --build-only) MODE="build" ;;
    --flash)      MODE="flash";  DEV="${2:-}"; shift ;;
    --update)     MODE="update"; DEV="${2:-}"; shift ;;
    --qemu-test)  QEMU=1 ;;
    *) echo "unknown arg: $1" >&2; exit 2 ;;
  esac
  shift
done
[ -n "$MODE" ] || {
  echo "usage: $0 --build-only | --flash /dev/sdX | --update /dev/sdX [--qemu-test]" >&2
  exit 2
}

die() { echo "ERROR: $*" >&2; exit 1; }
log() { echo -e "\n=== $* ==="; }

# --- locate the Meridian binary -------------------------------------------
if [ -z "${MERIDIAN:-}" ]; then
  MERIDIAN="$(ls -t "$REPO"/edk2/Build/Meridian/*/X64/Meridian.efi 2>/dev/null | head -1 || true)"
fi
[ -n "$MERIDIAN" ] && [ -f "$MERIDIAN" ] \
  || die "Meridian.efi not found; run 'mise run build' first (or set MERIDIAN=...)"
[ -f "$PUBKEY" ] || die "SSH pubkey not found at $PUBKEY (set PUBKEY=...)"

# ==========================================================================
# 1. Build the coreboot suite into a local pacman repo
#
# flashrom is in [extra], but the coreboot utilities are AUR-only -- and
# pacstrap/mkarchiso can only install from configured pacman repos. So build
# them here into a file:// repo that the archiso profile points at.
#
# All of these track the same coreboot release (26.06 at time of writing) and
# need only `go` to build.
# ==========================================================================
# AUR pkgbases to build. Note these are *pkgbases*, not package names: AUR git
# is per-pkgbase, so `cbmem.git` and friends are empty repos -- all 15 coreboot
# utilities come out of one `coreboot-utils` clone and one makepkg.
#
# hfsprogs is here because pathfinder gets it from chaotic-aur, which the
# archiso profile deliberately does not enable; building it ourselves keeps the
# image sourced from core/extra + PKGBUILDs we compiled, with no third-party
# binary repo in the trust path.
AUR_BASES=(coreboot-utils hfsprogs)

# coreboot release tarball signers (validpgpkeys in the PKGBUILD). Imported so
# the source signature is actually verified -- this is flashing tooling, we do
# not --skippgpcheck it.
AUR_KEYS=(C75AAA4E5C9DB017C1DC6EDBDB1B0EC29202D874   # Matt DeVillier
          574CE6F6855CFDEB7D368E9D19796C2B3E4F7DF7)  # Martin Roth

build_aur_repo() {
  # Per-base stamps rather than one repo-wide guard, so adding a pkgbase to
  # AUR_BASES builds just that one instead of being silently skipped.
  local todo=()
  for base in "${AUR_BASES[@]}"; do
    { [ -f "$AURREPO/.built-$base" ] && [ "${AUR_REBUILD:-0}" != "1" ]; } || todo+=("$base")
  done
  if [ ${#todo[@]} -eq 0 ]; then
    log "AUR repo up to date at $AURREPO (AUR_REBUILD=1 to force)"; return
  fi

  # makepkg runs in its own env and will not see a mise shim, so make sure a
  # real go is installed system-wide.
  [ -x /usr/bin/go ] || {
    log "installing go (makedep for the coreboot utils)"
    sudo pacman -S --needed --noconfirm go
  }

  log "importing coreboot release signing keys"
  gpg --keyserver keyserver.ubuntu.com --recv-keys "${AUR_KEYS[@]}" \
    || die "could not import coreboot signing keys; import them manually and re-run"

  mkdir -p "$AURREPO" "$AURSRC"
  for base in "${todo[@]}"; do
    log "AUR build: $base (downloads upstream sources; slow)"
    rm -rf "$AURSRC/$base"
    git clone -q "https://aur.archlinux.org/${base}.git" "$AURSRC/$base" \
      || die "failed to clone AUR pkgbase $base"
    [ -f "$AURSRC/$base/PKGBUILD" ] \
      || die "$base: AUR clone has no PKGBUILD (wrong pkgbase?)"
    # Pin the system toolchain. coreboot's bundled vboot is -Werror clean under
    # gcc but not clang (-Wuninitialized-const-pointer fires in tlcl.c), and a
    # clang shim ahead of /usr/bin in PATH -- or this repo's own UEFI clang
    # setup -- would otherwise capture the build.
    ( cd "$AURSRC/$base" \
        && CC=/usr/bin/gcc CXX=/usr/bin/g++ HOSTCC=/usr/bin/gcc \
           makepkg -sf --noconfirm --nocheck ) \
      || die "makepkg failed for $base"
    cp "$AURSRC/$base"/*.pkg.tar.zst "$AURREPO/" \
      || die "$base produced no packages"
    touch "$AURREPO/.built-$base"
  done

  log "indexing local repo $AURDB at $AURREPO"
  rm -f "$AURREPO/$AURDB".{db,files}*
  repo-add -q "$AURREPO/$AURDB.db.tar.zst" "$AURREPO"/*.pkg.tar.zst \
    || die "repo-add failed"
  echo "coreboot utils packaged: $(ls "$AURREPO"/*.pkg.tar.zst | wc -l)"
}

# ==========================================================================
# 2. Build the archiso profile + image (idempotent: skip if erofs exists)
# ==========================================================================
build_image() {
  if [ -f "$WORK/iso/arch/x86_64/airootfs.erofs" ]; then
    log "image already built at $WORK (delete it to rebuild)"; return
  fi

  command -v mkarchiso >/dev/null 2>&1 || {
    log "installing archiso"; sudo pacman -S --needed --noconfirm archiso
  }

  log "generating profile at $PROF (from baseline)"
  rm -rf "$PROF"; cp -r /usr/share/archiso/configs/baseline "$PROF"

  # point the profile's pacman at the locally-built coreboot suite
  cat >> "$PROF/pacman.conf" <<EOF

[$AURDB]
SigLevel = Optional TrustAll
Server = file://$AURREPO
EOF

  # rescue toolkit (baseline is intentionally tiny)
  cat > "$PROF/packages.x86_64" <<'EOF'
# --- core / live system ---
base
linux
linux-firmware
mkinitcpio
mkinitcpio-archiso
util-linux
python

# --- network / remote access ---
openssh
dhcpcd
iwd
wpa_supplicant
nfs-utils
cifs-utils
rsync

# --- partitioning / filesystems ---
dosfstools
e2fsprogs
btrfs-progs
xfsprogs
f2fs-tools
jfsutils
exfatprogs
hfsprogs
ntfs-3g
udftools
fuse3
gptfdisk
parted
efibootmgr

# --- disk recovery / health ---
testdisk
ddrescue
smartmontools
hdparm

# --- firmware: flashing + coreboot suite (see build_aur_repo) ---
flashrom
cbmem
cbfstool
ifdtool
inteltool
intelmetool
intelvbttool
superiotool
nvramtool
msrtool
ectool
amdfwtool
pmh7tool
acpidump-all
autoport
me_cleaner

# --- firmware: inspection ---
dmidecode
acpica
iucode-tool
fwupd
i2c-tools
libdisplay-info
read-edid
pciutils
usbutils

# --- shell comforts ---
vim
less
tmux
EOF

  sed -i "s/^iso_label=.*/iso_label=\"$ISO_LABEL\"/" "$PROF/profiledef.sh"
  sed -i 's/^iso_name=.*/iso_name="meridian-rescue"/' "$PROF/profiledef.sh"
  # single ESP-only UEFI boot mode (systemd-boot lays down kernel/initramfs/erofs)
  python3 - "$PROF/profiledef.sh" <<'PY'
import re,sys
f=sys.argv[1]; s=open(f).read()
s=re.sub(r'bootmodes=\([^)]*\)', "bootmodes=('uefi.systemd-boot')", s, count=1, flags=re.S)
open(f,"w").write(s)
PY

  # --- airootfs overlay: key, hostname, sshd, services, autologin ---
  install -dm700 "$PROF/airootfs/root/.ssh"
  cp "$PUBKEY" "$PROF/airootfs/root/.ssh/authorized_keys"
  chmod 600 "$PROF/airootfs/root/.ssh/authorized_keys"
  echo "rescue" > "$PROF/airootfs/etc/hostname"

  install -dm755 "$PROF/airootfs/etc/ssh/sshd_config.d"
  cat > "$PROF/airootfs/etc/ssh/sshd_config.d/10-rescue.conf" <<'EOF'
PermitRootLogin prohibit-password
PasswordAuthentication no
EOF

  # msr    -> /dev/cpu/*/msr, needed by msrtool / inteltool / permafrost checks
  # i2c-dev-> /dev/i2c-*, needed by i2c-tools and EDID reads over DDC
  install -dm755 "$PROF/airootfs/etc/modules-load.d"
  printf 'msr\ni2c-dev\n' > "$PROF/airootfs/etc/modules-load.d/firmware-tools.conf"

  install -dm755 "$PROF/airootfs/etc/systemd/system/multi-user.target.wants"
  ln -sf /usr/lib/systemd/system/sshd.service   "$PROF/airootfs/etc/systemd/system/multi-user.target.wants/sshd.service"
  ln -sf /usr/lib/systemd/system/dhcpcd.service "$PROF/airootfs/etc/systemd/system/multi-user.target.wants/dhcpcd.service"

  for g in getty@tty1 serial-getty@ttyS0; do
    d="$PROF/airootfs/etc/systemd/system/${g}.service.d"; install -dm755 "$d"
    cat > "$d/autologin.conf" <<'EOF'
[Service]
ExecStart=
ExecStart=-/usr/bin/agetty --autologin root --noclear %I 38400 linux
EOF
  done

  # archiso enforces ownership/perms via this array
  python3 - "$PROF/profiledef.sh" <<'PY'
import re,sys
f=sys.argv[1]; s=open(f).read()
add='  ["/root/.ssh"]="0:0:0700"\n  ["/root/.ssh/authorized_keys"]="0:0:0600"\n'
s=re.sub(r'(file_permissions=\([^)]*?)(\n\))', r'\1\n'+add+r'\2', s, count=1, flags=re.S)
open(f,"w").write(s)
PY

  # Pre-flight: pacstrap only reports the FIRST unresolvable package and does so
  # ~10 min into the run. Check the whole list up front against exactly the
  # repos the image gets (core/extra + our local one) -- notably NOT any extra
  # repo this workstation happens to have enabled, like chaotic-aur.
  log "resolving package list"
  local missing=() repo
  while read -r p; do
    # `|| true`: pacman -Si exits nonzero for anything not in a synced repo,
    # which under `set -e` + pipefail would kill the script mid-loop.
    repo="$(pacman -Si "$p" 2>/dev/null | awk -F': ' '/^Repository/{print $2; exit}' || true)"
    case "$repo" in
      core|extra) continue ;;
    esac
    ls "$AURREPO/${p}-"*.pkg.tar.zst >/dev/null 2>&1 && continue
    missing+=("$p${repo:+ (only in $repo)}")
  done < <(sed '/^[[:blank:]]*#.*/d;s/#.*//;/^[[:blank:]]*$/d' "$PROF/packages.x86_64")
  if [ ${#missing[@]} -gt 0 ]; then
    printf 'unresolvable package: %s\n' "${missing[@]}" >&2
    die "${#missing[@]} package(s) not in core/extra or $AURDB; add the pkgbase to AUR_BASES"
  fi

  log "mkarchiso build (downloads packages + builds erofs; ~10-20 min)"
  # mkarchiso populates $WORK as root, so a plain rm cannot clear a previous run
  sudo rm -rf "$WORK"; mkdir -p "$WORK" "$OUT"
  sudo mkarchiso -v -w "$WORK" -o "$OUT" "$PROF"
  [ -f "$WORK/iso/arch/x86_64/airootfs.erofs" ] || die "build produced no airootfs.erofs"
}

# ==========================================================================
# 3. Standalone EFI applications for the ESP
#
# memtest86+ (GPLv2, [extra]) rather than PassMark MemTest86: it is a real
# distro package so the stick stays reproducible, and it is free software.
# The v6/v7 line is a rewrite off PCMemTest and is actively maintained -- the
# "memtest86+ is abandoned" reputation is stale and predates it.
# ==========================================================================
MEMTEST_PKG="memtest86+-efi"
MEMTEST_SRC="boot/memtest86+/memtest.efi"   # path inside the package

fetch_esp_tools() {
  local out="$TOOLSRC/memtest.efi"
  [ -s "$out" ] && { log "memtest.efi cached at $out"; return; }

  mkdir -p "$TOOLSRC"
  log "downloading $MEMTEST_PKG"
  # -Sw into a private cachedir: fetch the package without installing it on the
  # workstation, since all we want is one file out of it.
  sudo pacman -Sw --noconfirm --cachedir "$TOOLSRC" "$MEMTEST_PKG" >/dev/null \
    || die "could not download $MEMTEST_PKG"

  local pkg
  pkg="$(ls -t "$TOOLSRC/$MEMTEST_PKG"-*.pkg.tar.zst 2>/dev/null | head -1 || true)"
  [ -n "$pkg" ] || die "$MEMTEST_PKG downloaded but no package file found in $TOOLSRC"

  bsdtar -xOf "$pkg" "$MEMTEST_SRC" > "$out" \
    || die "$MEMTEST_SRC not found in $pkg (package layout changed?)"
  [ -s "$out" ] || die "extracted memtest.efi is empty"
  # Verify it is really an x86_64 EFI application, not an error page or a
  # BIOS-only build. memtest86+ 7.x ships as an EFI-stub bzImage, so it carries
  # a PE header behind the MZ stub -- which is exactly what LoadImage needs.
  python3 - "$out" <<'PY' || die "memtest.efi is not an x86_64 EFI application"
import struct, sys
d = open(sys.argv[1], 'rb').read()
assert d[:2] == b'MZ', 'no MZ header'
off = struct.unpack_from('<I', d, 0x3c)[0]
assert d[off:off+4] == b'PE\0\0', 'no PE signature'
assert struct.unpack_from('<H', d, off+4)[0] == 0x8664, 'not x86_64'
assert struct.unpack_from('<H', d, off+92)[0] == 10, 'not an EFI application'
PY
  echo "memtest.efi: $(stat -c%s "$out") bytes from $(basename "$pkg")"
}

copy_esp_tools() {
  local mnt="$1"
  sudo mkdir -p "$mnt/EFI/tools"
  sudo cp "$TOOLSRC/memtest.efi" "$mnt/EFI/tools/memtest.efi"
}

# ==========================================================================
# 4. Write to USB
#
#   --flash  wipes the disk and lays down everything (first-time provisioning)
#   --update swaps the live components and adds any new ESP tools/menuentries,
#            but never rewrites what is already there: Meridian binaries,
#            existing config lines, and any recon reports left on the stick by
#            a previous run all survive.
# ==========================================================================
require_usb() {
  [ -n "$DEV" ] || die "need a device, e.g. /dev/sdb"
  [ -b "$DEV" ] || die "$DEV is not a block device"
  local tran; tran="$(lsblk -dno TRAN "$DEV" 2>/dev/null || true)"
  [ "$tran" = "usb" ] || die "$DEV TRAN='$tran' (not usb). Refusing to touch a non-USB disk."
}

# echo the partition node for $DEV (sdX1 vs nvmeXn1p1 naming)
usb_part() {
  local p="${DEV}1"; [ -b "$p" ] || p="${DEV}p1"
  [ -b "$p" ] || die "no partition 1 on $DEV"
  echo "$p"
}

copy_live_components() {
  local mnt="$1"
  sudo mkdir -p "$mnt/arch/boot/x86_64" "$mnt/arch/x86_64"
  sudo cp "$WORK/iso/arch/boot/x86_64/vmlinuz-linux"       "$mnt/arch/boot/x86_64/"
  sudo cp "$WORK/iso/arch/boot/x86_64/initramfs-linux.img" "$mnt/arch/boot/x86_64/"
  sudo cp "$WORK/iso/arch/x86_64/airootfs.erofs"  "$mnt/arch/x86_64/"
  sudo cp "$WORK/iso/arch/x86_64/airootfs.sha512" "$mnt/arch/x86_64/"
}

update_usb() {
  require_usb
  local part; part="$(usb_part)"
  local label; label="$(lsblk -dno LABEL "$part" 2>/dev/null || true)"
  [ "$label" = "$ISO_LABEL" ] \
    || die "$part label='$label', expected '$ISO_LABEL'. Use --flash to provision a new stick."

  local mnt; mnt="$(mktemp -d)"
  sudo mount "$part" "$mnt"

  [ -d "$mnt/EFI/Meridian" ] || { sudo umount "$mnt"; rmdir "$mnt"
    die "no EFI/Meridian on $part -- this is not a provisioned rescue stick"; }

  # Drop the previous backup BEFORE copying: the new erofs is larger than the
  # old one and the stick has no room for three generations at once.
  local new_sz cur_sz avail
  new_sz=$(stat -c%s "$WORK/iso/arch/x86_64/airootfs.erofs")
  sudo rm -f "$mnt/arch/x86_64/airootfs.erofs.bak"
  if [ -f "$mnt/arch/x86_64/airootfs.erofs" ]; then
    cur_sz=$(stat -c%s "$mnt/arch/x86_64/airootfs.erofs")
    avail=$(( $(stat -f -c '%a' "$mnt") * $(stat -f -c '%S' "$mnt") ))
    if [ "$avail" -gt $(( new_sz + 64*1024*1024 )) ]; then
      log "rotating current erofs to .bak ($(numfmt --to=iec "$cur_sz"))"
      sudo mv "$mnt/arch/x86_64/airootfs.erofs" "$mnt/arch/x86_64/airootfs.erofs.bak"
    else
      log "not enough room to keep a .bak; replacing erofs in place"
      sudo rm -f "$mnt/arch/x86_64/airootfs.erofs"
    fi
  fi

  # Additive ESP work: drop in any new standalone EFI apps and register the
  # menuentries for them. Meridian binaries and existing config lines untouched.
  copy_esp_tools "$mnt"
  merge_menuentries "$mnt/EFI/Meridian/config.conf"

  log "updating live components on $part (erofs $(numfmt --to=iec "$new_sz"); slow)"
  copy_live_components "$mnt"
  sudo sync
  df -h "$mnt" | tail -1
  sudo umount "$mnt"; rmdir "$mnt"
  log "RESCUE STICK UPDATED on $DEV (ESP contents preserved)"
}

rescue_config() {
  cat <<'EOF'
timeout 5
default_selection "Rescue"

# A rescue stick must only ever offer its own entries: auto-detection would list
# the host's bootloaders too, which is both noise and a way to boot the very
# system you plugged the stick in to repair.
scanfor manual

# The menu cache is keyed on disk topology + build id, not on this file's
# contents, so a hand edit here would otherwise be ignored until the stick moved
# to a different machine. A rescue stick is exactly the thing you edit in the
# field and expect to take effect on the next boot; the saved scan is not worth
# that surprise.
enable_menu_cache false

menuentry "Rescue" {
    loader /arch/boot/x86_64/vmlinuz-linux
    initrd /arch/boot/x86_64/initramfs-linux.img
    options "console=tty0 console=ttyS0,115200 archisobasedir=arch archisolabel=RESCUE cow_spacesize=2G iomem=relaxed"
}

menuentry "Memtest86+" {
    loader /EFI/tools/memtest.efi
}
EOF
}

# Global tokens the stick must carry. Merged in by key, so a field-edited value
# (a longer timeout, say) is kept while a missing token is added.
REQUIRED_TOKENS=(
  "scanfor manual"
  "enable_menu_cache false"
)

# Append anything rescue_config() defines that the stick's config.conf does not
# already have -- required global tokens first, then menuentries. Existing content
# -- timeout, default_selection, and anything hand-edited in the field -- is never
# rewritten; this only ever adds.
merge_menuentries() {
  local cfg="$1" tmp title tok key added=0
  tmp="$(mktemp)"
  sudo cat "$cfg" > "$tmp"

  for tok in "${REQUIRED_TOKENS[@]}"; do
    key="${tok%% *}"
    # Match the token at the start of a line so a mention inside a comment or a
    # menuentry body does not count as "already set".
    grep -qE "^[[:space:]]*$key([[:space:]]|$)" "$tmp" && continue
    log "adding global token \"$tok\" to config.conf"
    # Global tokens must precede the first menuentry, so insert at the top rather
    # than appending: a token after a menuentry block is parsed as part of it.
    sed -i "1i $tok" "$tmp"
    added=1
  done

  while IFS= read -r title; do
    grep -qF "menuentry \"$title\"" "$tmp" && continue
    log "adding menuentry \"$title\" to config.conf"
    printf '\n' >> "$tmp"
    # Emit just this stanza: from its menuentry line to the closing brace, which
    # rescue_config() always puts in column 0.
    rescue_config \
      | awk -v t="menuentry \"$title\" {" '$0==t {f=1} f {print} f && /^\}/ {exit}' \
      >> "$tmp"
    added=1
  done < <(rescue_config | sed -n 's/^menuentry "\([^"]*\)".*/\1/p')

  if [ "$added" = 1 ]; then
    sudo cp "$tmp" "$cfg"
    # menu.cache is keyed on disk topology + build id ONLY (kernel/cache.h) --
    # config.conf is not part of the fingerprint. Same stick, same binary means
    # the next boot is a cache hit that rebuilds the menu from the stale cache
    # and the entry we just added never shows up. Drop it; it is rebuilt on the
    # next scan.
    log "invalidating menu.cache (config.conf changed)"
    sudo rm -f "$(dirname "$cfg")/menu.cache"
  else
    log "config.conf already defines every required token and menuentry"
  fi
  rm -f "$tmp"
}

flash_usb() {
  require_usb
  local sz; sz="$(lsblk -dno SIZE "$DEV")"
  echo "About to ERASE $DEV ($sz, TRAN=usb) and write the rescue stick."
  read -r -p "Type the device path again to confirm [$DEV]: " ok
  [ "$ok" = "$DEV" ] || die "confirmation mismatch; aborted"

  log "partition $DEV: GPT, single FAT32 ESP, label $ISO_LABEL"
  sudo umount "${DEV}"?* 2>/dev/null || true
  sudo sgdisk -Z "$DEV" >/dev/null 2>&1 || true
  sudo sgdisk -o -n 1:2048:0 -t 1:EF00 -c "1:$ISO_LABEL" "$DEV" >/dev/null
  sudo partprobe "$DEV"; sleep 2
  local part="${DEV}1"; [ -b "$part" ] || part="${DEV}p1"
  sudo mkfs.vfat -F 32 -n "$ISO_LABEL" "$part" >/dev/null

  local mnt; mnt="$(mktemp -d)"
  sudo mount "$part" "$mnt"
  sudo mkdir -p "$mnt/EFI/BOOT" "$mnt/EFI/Meridian" "$mnt/arch/boot/x86_64" "$mnt/arch/x86_64"

  log "deploy Meridian bootloader + rescue menuentry"
  sudo cp "$MERIDIAN" "$mnt/EFI/BOOT/bootx64.efi"
  sudo cp "$MERIDIAN" "$mnt/EFI/Meridian/Meridian.efi"
  rescue_config | sudo tee "$mnt/EFI/Meridian/config.conf" >/dev/null
  copy_esp_tools "$mnt"

  log "copy live components (erofs copy is slow)"
  copy_live_components "$mnt"
  sudo sync
  sudo umount "$mnt"; rmdir "$mnt"
  log "RESCUE STICK READY on $DEV"
}

# ==========================================================================
# 5. Optional: QEMU smoke test (OVMF -> Meridian -> kernel -> autologin)
# ==========================================================================
qemu_test() {
  [ -b "$DEV" ] || die "--qemu-test needs the --flash/--update device"
  local vars=/tmp/resc-vars.fd serial=/tmp/resc-serial.log
  cp /usr/share/edk2/x64/OVMF_VARS.4m.fd "$vars"
  log "QEMU-booting $DEV via OVMF (75s, serial capture)"
  sudo timeout 80 qemu-system-x86_64 -machine q35 -m 2048 -nographic \
    -drive if=pflash,format=raw,readonly=on,file=/usr/share/edk2/x64/OVMF_CODE.4m.fd \
    -drive if=pflash,format=raw,file="$vars" \
    -drive format=raw,file="$DEV" \
    -serial file:"$serial" -display none -no-reboot >/dev/null 2>&1 || true
  echo "--- serial highlights ---"
  grep -aiE "Meridian|Linux version|archiso|Reached target|rescue login|autologin" "$serial" | head -20
}

# --- run ------------------------------------------------------------------
echo "Meridian : $MERIDIAN"
echo "Pubkey   : $PUBKEY ($(ssh-keygen -lf "$PUBKEY" 2>/dev/null | awk '{print $2}'))"
echo "AUR repo : $AURREPO (pkgbases: ${AUR_BASES[*]})"
build_aur_repo
fetch_esp_tools
build_image
case "$MODE" in
  flash)  flash_usb  ; [ "$QEMU" = "1" ] && qemu_test ;;
  update) update_usb ; [ "$QEMU" = "1" ] && qemu_test ;;
esac
log "done"
