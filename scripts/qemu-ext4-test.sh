#!/usr/bin/env bash
# Local reproduction harness for the .31 ext4-mount failure.
#
# Builds a GPT disk with an ESP (Meridian + fsw ext4 driver) and a second
# ext4 partition whose feature set mirrors a modern Arch mkfs.ext4 (.31:
# incompat=0x22C2 metadata_csum_seed, ro_compat=0x46B metadata_csum). Boots it
# under OVMF/QEMU headless, then extracts Meridian's ESP log so we can read the
# [EXTDBG] line (RootDir / SFS-on-handle / superblock) without touching .31.
#
# Usage: scripts/qemu-ext4-test.sh [seconds]   (default boot window: 35s)
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
EFI="$ROOT/edk2/Build/Meridian/DEBUG_CLANGDWARF/X64/Meridian.efi"
DRV="$ROOT/edk2/Build/Meridian/DEBUG_CLANGDWARF/X64/Meridian.efi"  # placeholder; real driver resolved below
OVMF_CODE="/usr/share/edk2/x64/OVMF_CODE.4m.fd"
OVMF_VARS="/usr/share/edk2/x64/OVMF_VARS.4m.fd"
WINDOW="${1:-35}"

WORK="$(mktemp -d /tmp/meridian-qemu.XXXXXX)"
trap 'rm -rf "$WORK"' EXIT

# Locate the built fsw ext4 driver (.efi)
EXT4DRV="$(find "$ROOT/edk2/Build/Meridian/DEBUG_CLANGDWARF/X64" -iname 'ext4*.efi' | head -1)"
[ -z "$EXT4DRV" ] && { echo "ext4 driver .efi not found - build first"; exit 1; }
echo "Meridian : $EFI"
echo "ext4 drv : $EXT4DRV"

# --- Stage ESP contents ---
mkdir -p "$WORK/esp/EFI/BOOT" "$WORK/esp/EFI/Meridian/fs"
cp "$EFI"     "$WORK/esp/EFI/BOOT/bootx64.efi"
cp "$EFI"     "$WORK/esp/EFI/Meridian/Meridian.efi"
cp "$EXT4DRV" "$WORK/esp/EFI/Meridian/fs/ext4.efi"

# --- Stage ext4 contents (mimic Arch root) ---
mkdir -p "$WORK/ext4/boot" "$WORK/ext4/etc"
printf 'DUMMY KERNEL' > "$WORK/ext4/boot/vmlinuz-linux"
printf 'DUMMY INITRD' > "$WORK/ext4/boot/initramfs-linux.img"
printf 'UUID=00000000-0000-0000-0000-000000000001 / ext4 rw,relatime 0 1\n' > "$WORK/ext4/etc/fstab"

# --- Build ESP FAT image ---
truncate -s 80M "$WORK/esp.img"
mkfs.vfat -F 32 -n MERIDIANESP "$WORK/esp.img" >/dev/null
mcopy -s -i "$WORK/esp.img" "$WORK/esp/EFI" ::

# --- Build ext4 image matching .31's feature flags ---
truncate -s 200M "$WORK/ext4.img"
mke2fs -q -F -t ext4 \
  -O metadata_csum_seed \
  -U 00000000-0000-0000-0000-000000000001 \
  -L Root \
  -d "$WORK/ext4" "$WORK/ext4.img"
echo "=== ext4 feature flags (local repro) ==="
dumpe2fs -h "$WORK/ext4.img" 2>/dev/null | grep -iE "Filesystem features|magic" || true

# --- Assemble GPT disk ---
truncate -s 320M "$WORK/disk.img"
sgdisk -Z "$WORK/disk.img" >/dev/null 2>&1 || true
sgdisk -o "$WORK/disk.img" >/dev/null
sgdisk -n 1:2048:+80M  -t 1:EF00 -c 1:ESP  "$WORK/disk.img" >/dev/null
sgdisk -n 2:0:+200M    -t 2:8300 -c 2:Root "$WORK/disk.img" >/dev/null
ESP_START=$(sgdisk -i 1 "$WORK/disk.img" | awk -F': ' '/First sector/{print $2}' | awk '{print $1}')
EXT4_START=$(sgdisk -i 2 "$WORK/disk.img" | awk -F': ' '/First sector/{print $2}' | awk '{print $1}')
echo "ESP @sector $ESP_START   ext4 @sector $EXT4_START"
dd if="$WORK/esp.img"  of="$WORK/disk.img" bs=512 seek="$ESP_START"  conv=notrunc status=none
dd if="$WORK/ext4.img" of="$WORK/disk.img" bs=512 seek="$EXT4_START" conv=notrunc status=none

# --- Boot under OVMF/QEMU headless ---
cp "$OVMF_VARS" "$WORK/vars.fd"
echo "=== booting QEMU for ${WINDOW}s ==="
qemu-system-x86_64 \
  -machine q35 -m 512 -nographic \
  -drive if=pflash,format=raw,readonly=on,file="$OVMF_CODE" \
  -drive if=pflash,format=raw,file="$WORK/vars.fd" \
  -drive format=raw,file="$WORK/disk.img" \
  -serial file:"$WORK/serial.log" -display none -no-reboot \
  >/dev/null 2>&1 &
QPID=$!
sleep "$WINDOW"
kill "$QPID" 2>/dev/null || true
wait "$QPID" 2>/dev/null || true

# --- Extract Meridian log from ESP ---
echo "=== ESP root listing ==="
mdir -i "$WORK/disk.img@@$((ESP_START*512))" :: 2>/dev/null || true
LOGNAME=$(mdir -i "$WORK/disk.img@@$((ESP_START*512))" :: 2>/dev/null | awk '/LOG/{print $1"."$2}' | head -1)
echo "=== [EXTDBG] / scan result ==="
for f in $(mdir -b -i "$WORK/disk.img@@$((ESP_START*512))" :: 2>/dev/null | grep -i '\.log$'); do
  mcopy -n -i "$WORK/disk.img@@$((ESP_START*512))" "$f" "$WORK/out.log" 2>/dev/null || true
  echo "--- $f ---"
  grep -nE "EXTDBG|RootDir|Scan Internal|Found .* on|vmlinuz" "$WORK/out.log" 2>/dev/null | head -40
done
cp -f "$WORK/out.log" "$ROOT/.qemu-last.log" 2>/dev/null || true
echo "(full log copied to .qemu-last.log)"
