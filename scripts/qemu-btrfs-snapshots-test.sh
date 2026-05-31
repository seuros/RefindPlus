#!/usr/bin/env bash
# QEMU harness for btrfs snapshot boot entries (scan_btrfs_snapshots).
#
# Builds a GPT disk shaped like a real snapper node: an ESP carrying Meridian,
# the btrfs driver and a Linux entry, plus a btrfs partition holding "@" and a
# "@.snapshots" tree. Boots it headless under OVMF and greps Meridian's own ESP
# log for the generated snapshot sub-entries.
#
# Menu generation only walks directories and reads info.xml, so the fixture does
# not need real subvolumes -- mkfs.btrfs --rootdir writes plain directories and
# that is enough to exercise enumeration, labelling, the newest-10 cap, the sort
# order and the rootflags= rewrite. Booting a snapshot for real is a hardware
# test (magellan), not this one.
#
# The fixture is deliberately hostile: it contains a non-numeric directory, a
# numbered directory with no "snapshot" child, an empty info.xml, one info.xml
# with a description but no date, and 40 valid snapshots. Expected outcome:
# exactly 10 entries, ids 40 down to 31, no fault.
#
# Usage: scripts/qemu-btrfs-snapshots-test.sh [seconds]   (default window: 35s)
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$ROOT/edk2/Build/Meridian/DEBUG_CLANGDWARF/X64"
EFI="$BUILD/Meridian.efi"
BTRFSDRV="$BUILD/btrfs.efi"
OVMF_CODE="/usr/share/edk2/x64/OVMF_CODE.4m.fd"
OVMF_VARS="/usr/share/edk2/x64/OVMF_VARS.4m.fd"
WINDOW="${1:-35}"

for f in "$EFI" "$BTRFSDRV"; do
  [ -f "$f" ] || { echo "missing $f - run 'mise run build' first"; exit 1; }
done

WORK="$(mktemp -d /tmp/meridian-btrfs-snap.XXXXXX)"
trap 'rm -rf "$WORK"' EXIT

echo "Meridian  : $EFI"
echo "btrfs drv : $BTRFSDRV"

# --- Stage ESP contents ---
mkdir -p "$WORK/esp/EFI/BOOT" "$WORK/esp/EFI/Meridian/fs" "$WORK/esp/EFI/arch"
cp "$EFI"      "$WORK/esp/EFI/BOOT/bootx64.efi"
cp "$EFI"      "$WORK/esp/EFI/Meridian/Meridian.efi"
cp "$BTRFSDRV" "$WORK/esp/EFI/Meridian/fs/btrfs.efi"

# A Linux entry for the snapshots to hang off. Meridian only builds an entry for
# a file that passes its PE validity check, so the stand-in kernel is a copy of
# Meridian's own binary -- a valid x64 EFI image under a vmlinuz name. We never
# launch it; we only need the entry and its submenu.
cp "$EFI" "$WORK/esp/EFI/arch/vmlinuz-linux"
printf 'DUMMY INITRD' > "$WORK/esp/EFI/arch/initramfs-linux.img"

# Base options the snapshot entries inherit and rewrite.
cat > "$WORK/esp/EFI/arch/refind_linux.conf" <<'EOF'
"Boot with standard options"  "ro root=UUID=00000000-0000-0000-0000-0000000000b7 rootflags=subvol=@ quiet"
"Boot to terminal"            "ro root=UUID=00000000-0000-0000-0000-0000000000b7 rootflags=subvol=@ systemd.unit=multi-user.target"
EOF

# TOKEN_OFF=1 runs the same fixture with the feature disabled: the expectation
# then is zero snapshot entries and an otherwise unchanged menu.
TOKEN="${TOKEN_OFF:+false}"
TOKEN="${TOKEN:-true}"
# GUI=1 opens a real QEMU window and hands the menu to the operator instead of
# grepping a log: arrow to the Linux entry, press Insert/F2, see the snapshots.
if [ -n "${GUI:-}" ]; then
  MENU_TIMEOUT=0   # never auto-boot; the fixture kernel is not a real kernel
  TEXTONLY=false
else
  MENU_TIMEOUT=5
  TEXTONLY=true
fi
cat > "$WORK/esp/EFI/Meridian/config.conf" <<EOF
timeout $MENU_TIMEOUT
textonly $TEXTONLY
scan_all_linux_kernels true
scan_btrfs_snapshots $TOKEN
scanfor internal,external,optical,manual
EOF
echo "scan_btrfs_snapshots: $TOKEN"

# --- Stage btrfs contents ---
BT="$WORK/btrfs"
mkdir -p "$BT/@/etc" "$BT/@/boot" "$BT/@home"
printf 'UUID=00000000-0000-0000-0000-0000000000b7 / btrfs rw,subvol=@ 0 0\n' > "$BT/@/etc/fstab"

mk_snapshot() { # id, date, description
  mkdir -p "$BT/@.snapshots/$1/snapshot/etc"
  printf 'snapshot %s\n' "$1" > "$BT/@.snapshots/$1/snapshot/etc/hostname"
  cat > "$BT/@.snapshots/$1/info.xml" <<EOF
<?xml version="1.0"?>
<snapshot>
  <type>single</type>
  <num>$1</num>
  <date>$2</date>
  <description>$3</description>
</snapshot>
EOF
}

for n in $(seq 1 40); do
  mk_snapshot "$n" "2026-08-$(printf '%02d' $(( (n % 28) + 1 ))) 0$((n % 9)):15:00" "pacman -Syu #$n"
done

# --- Junk the scanner must survive ---
# 1. non-numeric directory
mkdir -p "$BT/@.snapshots/grub-btrfs-leftovers/snapshot"
# 2. numbered directory with no snapshot subvolume (interrupted snapper run)
mkdir -p "$BT/@.snapshots/99/whatever"
cat > "$BT/@.snapshots/99/info.xml" <<'EOF'
<snapshot><date>2026-01-01 00:00:00</date><description>never finished</description></snapshot>
EOF
# 3. empty info.xml
mkdir -p "$BT/@.snapshots/38/snapshot"
: > "$BT/@.snapshots/38/info.xml"
# 4. description but no date
mkdir -p "$BT/@.snapshots/37/snapshot"
printf '<snapshot><description>desc only</description></snapshot>\n' > "$BT/@.snapshots/37/info.xml"
# 5. a plain file where a snapshot dir would be
printf 'not a directory\n' > "$BT/@.snapshots/README"

# --- Build images ---
truncate -s 96M "$WORK/esp.img"
mkfs.vfat -F 32 -n MERIDIANESP "$WORK/esp.img" >/dev/null
mcopy -s -i "$WORK/esp.img" "$WORK/esp/EFI" ::

truncate -s 512M "$WORK/btrfs.img"
mkfs.btrfs -q -f -L SnapRoot \
  -U 00000000-0000-0000-0000-0000000000b7 \
  --rootdir "$BT" "$WORK/btrfs.img"

# --- Assemble GPT disk ---
truncate -s 640M "$WORK/disk.img"
sgdisk -o "$WORK/disk.img" >/dev/null
sgdisk -n 1:2048:+96M -t 1:EF00 -c 1:ESP     "$WORK/disk.img" >/dev/null
sgdisk -n 2:0:+512M   -t 2:8300 -c 2:SnapRoot "$WORK/disk.img" >/dev/null
ESP_START=$(sgdisk -i 1 "$WORK/disk.img" | awk -F': ' '/First sector/{print $2}' | awk '{print $1}')
BT_START=$(sgdisk -i 2 "$WORK/disk.img" | awk -F': ' '/First sector/{print $2}' | awk '{print $1}')
echo "ESP @sector $ESP_START   btrfs @sector $BT_START"
dd if="$WORK/esp.img"   of="$WORK/disk.img" bs=512 seek="$ESP_START" conv=notrunc status=none
dd if="$WORK/btrfs.img" of="$WORK/disk.img" bs=512 seek="$BT_START"  conv=notrunc status=none

# --- Boot ---
cp "$OVMF_VARS" "$WORK/vars.fd"

if [ -n "${GUI:-}" ]; then
  echo "=== booting QEMU in a window ==="
  echo "    arrow to the 'vmlinuz-linux' entry, press Insert or F2 for its submenu"
  echo "    (the fixture kernel is a copy of Meridian, so do not try to boot it)"
  trap - EXIT   # keep the disk around; the window owns it until the user quits
  qemu-system-x86_64 \
    -machine q35 -m 1024 \
    -drive if=pflash,format=raw,readonly=on,file="$OVMF_CODE" \
    -drive if=pflash,format=raw,file="$WORK/vars.fd" \
    -drive format=raw,file="$WORK/disk.img" \
    -display gtk -vga std -no-reboot
  rm -rf "$WORK"
  exit 0
fi

echo "=== booting QEMU for ${WINDOW}s ==="
qemu-system-x86_64 \
  -machine q35 -m 1024 -nographic \
  -drive if=pflash,format=raw,readonly=on,file="$OVMF_CODE" \
  -drive if=pflash,format=raw,file="$WORK/vars.fd" \
  -drive format=raw,file="$WORK/disk.img" \
  -serial file:"$WORK/serial.log" -display none -no-reboot \
  >/dev/null 2>&1 &
QPID=$!
sleep "$WINDOW"
kill "$QPID" 2>/dev/null || true
wait "$QPID" 2>/dev/null || true

# --- Extract Meridian's ESP log ---
IMG="$WORK/disk.img@@$((ESP_START*512))"
: > "$WORK/out.log"
for f in $(mdir -b -i "$IMG" :: 2>/dev/null | grep -i '\.log$'); do
  mcopy -n -i "$IMG" "$f" "$WORK/one.log" 2>/dev/null || true
  cat "$WORK/one.log" >> "$WORK/out.log" 2>/dev/null || true
done
cp -f "$WORK/out.log" "$ROOT/.qemu-last.log" 2>/dev/null || true

echo "=== snapshot scan ==="
grep -aE "btrfs Snapshot|Snapshot Entry|Boot Snapshot" "$WORK/out.log" | head -30 || true

# Meridian rotates its boot log, so out.log is several passes concatenated and
# the raw counts are per-pass multiples. Assert on the per-pass numbers instead:
# every pass must report exactly 10 snapshots, and every pass must have emitted
# the same 10 ids (40 down to 31 -- 99 has no snapshot child, and the
# non-numeric names are not snapshots).
if [ -n "${TOKEN_OFF:-}" ]; then
  OFFCOUNT=$(grep -ac "btrfs Snapshot" "$WORK/out.log" || true)
  LINUX=$(grep -ac "Instance: Linux via Stub" "$WORK/out.log" || true)
  echo "snapshot log lines : $OFFCOUNT (expected 0)"
  echo "linux entry lines  : $LINUX (expected > 0 -- the menu is otherwise unchanged)"
  if [ "$OFFCOUNT" = "0" ] && [ "$LINUX" -gt 0 ]; then
    echo "RESULT: PASS (token off)"
    exit 0
  fi
  echo "RESULT: FAIL - see .qemu-last.log"
  exit 1
fi

PASSES=$(grep -ac "btrfs Snapshot" "$WORK/out.log" || true)
BADPASS=$(grep -a "btrfs Snapshot" "$WORK/out.log" | grep -avc "Found 10 btrfs Snapshots" || true)
ADDED=$(grep -ac "Added Snapshot Entry" "$WORK/out.log" || true)
IDS=$(grep -ao "Boot Snapshot [0-9]*" "$WORK/out.log" | awk '{print $3}' | sort -un | tr '\n' ' ')

# The rewrite must repoint the subvolume and keep everything else: root=, the
# initrd, and the leading "ro" that the base options already carried.
REWRITES=$(grep -ac "rootflags=subvol=@\.snapshots/[0-9]*/snapshot" "$WORK/out.log" || true)
KEPT=$(grep -a "Added Snapshot Entry" "$WORK/out.log" |
       grep -ac "ro root=UUID=00000000-0000-0000-0000-0000000000b7 .*initrd=" || true)
echo
echo "=== a rewritten command line ==="
grep -ao "Options:- '[^']*'" "$WORK/out.log" | sort -u | head -3

echo
echo "scan passes        : $PASSES (each must report 10)"
echo "passes not at 10   : $BADPASS (expected 0)"
echo "entries added      : $ADDED (expected 10 x $PASSES)"
echo "distinct ids       : $IDS"
echo "expected ids       : 31 32 33 34 35 36 37 38 39 40"
echo "subvol rewrites    : $REWRITES (expected $ADDED)"
echo "root= + initrd kept: $KEPT (expected $ADDED)"

if [ "$PASSES" -gt 0 ] && [ "$BADPASS" = "0" ] && [ "$ADDED" = "$((10 * PASSES))" ] &&
   [ "$REWRITES" = "$ADDED" ] && [ "$KEPT" = "$ADDED" ] &&
   [ "$IDS" = "31 32 33 34 35 36 37 38 39 40 " ]; then
  echo "RESULT: PASS"
else
  echo "RESULT: FAIL - see .qemu-last.log"
  exit 1
fi
