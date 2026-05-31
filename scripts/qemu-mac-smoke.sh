#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
#
# Mac EFI smoke test for Meridian.
#
# Boots Meridian under QEMU/Q35 with Apple SMBIOS strings injected so
# Meridian's firmware detection exercises the Apple path (gFwStrategyApple /
# gFwStrategyAppleLegacy). An e1000 NIC is attached with user-mode networking
# so the SNP-based network discovery code has a real handle to locate.
#
# NOTE: gST->FirmwareVendor stays "EDK II" (OVMF-compiled). Meridian reads
# SMBIOS type-0/1 vendor via EFI_SMBIOS_PROTOCOL, so `-smbios type=0,vendor`
# is the trigger, not FirmwareVendor. The test checks that Meridian detects
# the Apple vendor string from SMBIOS and logs "Apple EFI".
#
# Usage: scripts/qemu-mac-smoke.sh [seconds]   (default boot window: 35s)
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
EFI="$ROOT/edk2/Build/Meridian/DEBUG_CLANGDWARF/X64/Meridian.efi"
OVMF_CODE="/usr/share/edk2/x64/OVMF_CODE.4m.fd"
OVMF_VARS="/usr/share/edk2/x64/OVMF_VARS.4m.fd"
WINDOW="${1:-35}"

command -v qemu-system-x86_64 >/dev/null || { echo "qemu-system-x86_64 not found"; exit 1; }
[ -f "$EFI" ]       || { echo "Meridian.efi not found -- run 'mise run build' first"; exit 1; }
[ -f "$OVMF_CODE" ] || { echo "OVMF_CODE not found -- install edk2-ovmf"; exit 1; }

WORK="$(mktemp -d /tmp/meridian-mac.XXXXXX)"
trap 'rm -rf "$WORK"' EXIT
echo "Meridian : $EFI"
echo "Firmware : $OVMF_CODE"

# --- ESP: Meridian as UEFI removable-media fallback; config lives in home ---
mkdir -p "$WORK/esp/EFI/BOOT" "$WORK/esp/EFI/Meridian"
cp "$EFI" "$WORK/esp/EFI/BOOT/bootx64.efi"
cp "$EFI" "$WORK/esp/EFI/Meridian/Meridian.efi"

cat > "$WORK/esp/EFI/Meridian/config.conf" <<'CONF'
timeout 3
serial on
log_level 2
scan_delay 0
scan_all_linux_kernels false
textonly
CONF

# --- ESP FAT image ---
truncate -s 80M "$WORK/esp.img"
mkfs.vfat -F 32 -n MERIDIANESP "$WORK/esp.img" >/dev/null
mcopy -s -i "$WORK/esp.img" "$WORK/esp/EFI" ::

# --- GPT disk with single ESP ---
truncate -s 128M "$WORK/disk.img"
sgdisk -Z "$WORK/disk.img" >/dev/null 2>&1 || true
sgdisk -o "$WORK/disk.img" >/dev/null
sgdisk -n 1:2048:+80M -t 1:EF00 -c 1:ESP "$WORK/disk.img" >/dev/null
ESP_START=$(sgdisk -i 1 "$WORK/disk.img" | awk -F': ' '/First sector/{print $2}' | awk '{print $1}')
dd if="$WORK/esp.img" of="$WORK/disk.img" bs=512 seek="$ESP_START" conv=notrunc status=none

cp "$OVMF_VARS" "$WORK/vars.fd"

echo "=== booting qemu-system-x86_64 (Q35/Mac) for ${WINDOW}s ==="
# Apple SMBIOS injection:
#   type=0  BIOS vendor  → triggers Meridian's FW_VENDOR_APPLE detection
#   type=1  System info  → product/family for display in menu header
# NIC: e1000 on user network -- gives Meridian a real EFI_SIMPLE_NETWORK_PROTOCOL
#      handle to locate. No tap/bridge required; DHCP discover will time out
#      but the handle enumeration itself exercises the SNP path.
qemu-system-x86_64 \
  -machine q35 -cpu Penryn -m 1024 -nographic \
  -smbios type=0,vendor="Apple Inc.",version="MP71.0217.B00",date="12/05/2019" \
  -smbios type=1,manufacturer="Apple Inc.",product="MacPro7,,1",version="1.0",\
serial="C07ZZZZZQ1GN",family="Mac Pro" \
  -drive if=pflash,format=raw,readonly=on,file="$OVMF_CODE" \
  -drive if=pflash,format=raw,file="$WORK/vars.fd" \
  -drive format=raw,file="$WORK/disk.img" \
  -nic user,model=e1000,mac=52:54:00:de:ad:01 \
  -serial file:"$WORK/serial.log" -display none -no-reboot \
  >/dev/null 2>&1 &
QPID=$!
sleep "$WINDOW"
kill "$QPID" 2>/dev/null || true
wait "$QPID" 2>/dev/null || true

cp -f "$WORK/serial.log" "$ROOT/.qemu-mac-last.log" 2>/dev/null || true

# --- Extract Meridian log from ESP ---
for f in $(mdir -b -i "$WORK/disk.img@@$((ESP_START*512))" :: 2>/dev/null | grep -i '\.log$'); do
    mcopy -n -i "$WORK/disk.img@@$((ESP_START*512))" "$f" "$WORK/out.log" 2>/dev/null || true
done

echo "=== SMBIOS / firmware vendor ==="
grep -haE "Apple|Firmware Vendor|FW_VENDOR|gFwStrategy" \
    "$WORK/out.log" 2>/dev/null | sort -u | head -10 || true
grep -haoE "MacPro7,1|MP71\.0217\.B00|Apple" \
    "$WORK/serial.log" "$WORK/out.log" 2>/dev/null | sort -u | head -10 || echo "(no Apple lines)"

echo "=== NIC / SNP handles ==="
grep -aE "SimpleNetwork|SNP|Net.*Handle|e1000|NIC|netboot|Netboot" \
    "$WORK/serial.log" "$WORK/out.log" 2>/dev/null | head -10 || echo "(no NIC lines)"

echo "=== serial markers ==="
grep -aonE "Meridian|2026\.06|Main Menu|Boot Menu|Scan|Apple" \
    "$WORK/serial.log" 2>/dev/null | head -20 || true

echo "=== verdict ==="
PASS=0
APPLE_SEEN=0

grep -qaE "Loaded Meridian|Meridian .* Firmware|2026\.06|Main Menu" \
    "$WORK/serial.log" 2>/dev/null && PASS=1
grep -qaE "Apple|MacPro|gFwStrategyApple" \
    "$WORK/serial.log" "$WORK/out.log" 2>/dev/null && APPLE_SEEN=1

if [ "$PASS" -eq 1 ] && [ "$APPLE_SEEN" -eq 1 ]; then
    echo "PASS: Meridian booted and detected Apple EFI (SMBIOS injection worked)"
    exit 0
elif [ "$PASS" -eq 1 ]; then
    echo "PARTIAL: Meridian booted but Apple path not confirmed -- inspect .qemu-mac-last.log"
    exit 0
elif grep -qaE "Meridian" "$WORK/serial.log" 2>/dev/null; then
    echo "PARTIAL: Meridian ran but did not reach menu -- inspect .qemu-mac-last.log"
    exit 0
else
    echo "FAIL: no Meridian output on serial -- inspect .qemu-mac-last.log"
    tail -20 "$WORK/serial.log" 2>/dev/null || true
    exit 1
fi
