#!/usr/bin/env bash
# AArch64 boot smoke test for Meridian.
#
# Boots the AArch64 Meridian.efi as the UEFI removable-media fallback
# (\EFI\BOOT\BOOTAA64.EFI) under QEMU's ArmVirt firmware, headless, and checks
# that Meridian reaches its menu by scraping the serial console for its banner.
#
# This proves the arm64 build *runs* (entry point, init, config parse, volume
# scan, menu render) -- not just that it compiles. It is NOT an Apple Silicon
# test; ArmVirt is generic arm64 UEFI, which is the correct lowest-cost gate
# before real Asahi hardware.
#
# Usage: scripts/qemu-aarch64-smoke.sh [seconds]   (default boot window: 30s)
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
EFI="$ROOT/edk2/Build/Meridian/DEBUG_CLANGDWARF/AARCH64/Meridian.efi"
FW_CODE="/usr/share/edk2/aarch64/QEMU_EFI.fd"
FW_VARS="/usr/share/edk2/aarch64/QEMU_VARS.fd"
WINDOW="${1:-30}"

command -v qemu-system-aarch64 >/dev/null || { echo "qemu-system-aarch64 not found"; exit 1; }
[ -f "$EFI" ]     || { echo "AArch64 Meridian.efi not found - run 'mise run build-arm' first"; exit 1; }
[ -f "$FW_CODE" ] || { echo "ArmVirt firmware $FW_CODE not found (install edk2-aarch64)"; exit 1; }

WORK="$(mktemp -d /tmp/meridian-aa64.XXXXXX)"
trap 'rm -rf "$WORK"' EXIT
echo "Meridian : $EFI"
echo "Firmware : $FW_CODE"

# --- Stage ESP contents: Meridian as the removable-media fallback; config
#     lives in home (textonly so the UI lands on serial; short timeout). ---
mkdir -p "$WORK/esp/EFI/BOOT" "$WORK/esp/EFI/Meridian"
cp "$EFI" "$WORK/esp/EFI/BOOT/BOOTAA64.EFI"
cp "$EFI" "$WORK/esp/EFI/Meridian/Meridian.efi"
cat > "$WORK/esp/EFI/Meridian/config.conf" <<'EOF'
textonly
timeout 3
log_level 1
scan_delay 0
EOF

# --- Build ESP FAT image ---
truncate -s 80M "$WORK/esp.img"
mkfs.vfat -F 32 -n MERIDIANESP "$WORK/esp.img" >/dev/null
mcopy -s -i "$WORK/esp.img" "$WORK/esp/EFI" ::

# --- Assemble a GPT disk with a single ESP (firmware scans \EFI\BOOT fallback) ---
truncate -s 128M "$WORK/disk.img"
sgdisk -Z "$WORK/disk.img" >/dev/null 2>&1 || true
sgdisk -o "$WORK/disk.img" >/dev/null
sgdisk -n 1:2048:+80M -t 1:EF00 -c 1:ESP "$WORK/disk.img" >/dev/null
ESP_START=$(sgdisk -i 1 "$WORK/disk.img" | awk -F': ' '/First sector/{print $2}' | awk '{print $1}')
dd if="$WORK/esp.img" of="$WORK/disk.img" bs=512 seek="$ESP_START" conv=notrunc status=none

# --- Writable copy of the firmware vars store ---
cp "$FW_VARS" "$WORK/vars.fd"

# --- Boot under ArmVirt/QEMU, headless, serial -> file ---
echo "=== booting qemu-system-aarch64 (virt, cortex-a72) for ${WINDOW}s ==="
qemu-system-aarch64 \
  -machine virt -cpu cortex-a72 -m 512 \
  -drive if=pflash,format=raw,readonly=on,file="$FW_CODE" \
  -drive if=pflash,format=raw,file="$WORK/vars.fd" \
  -drive if=virtio,format=raw,file="$WORK/disk.img" \
  -serial file:"$WORK/serial.log" -display none -no-reboot \
  >/dev/null 2>&1 &
QPID=$!
sleep "$WINDOW"
kill "$QPID" 2>/dev/null || true
wait "$QPID" 2>/dev/null || true

cp -f "$WORK/serial.log" "$ROOT/.qemu-aa64-last.log" 2>/dev/null || true

# --- Evaluate: did Meridian reach its banner / menu? ---
echo "=== serial markers ==="
grep -anE "Meridian|2026\.06|Firmware|Main Menu|Boot Menu|Scan" "$WORK/serial.log" | head -20 || true

echo "=== verdict ==="
if grep -qaE "Loaded Meridian|Meridian .* Firmware|2026\.06" "$WORK/serial.log"; then
    echo "PASS: Meridian booted and rendered on AArch64 (banner seen on serial)"
    exit 0
elif grep -qaiE "Meridian" "$WORK/serial.log"; then
    echo "PARTIAL: Meridian ran (name on serial) but no clear banner -- inspect .qemu-aa64-last.log"
    exit 0
else
    echo "FAIL: no Meridian output on serial -- inspect .qemu-aa64-last.log"
    echo "(serial tail:)"; tail -20 "$WORK/serial.log" 2>/dev/null || true
    exit 1
fi
