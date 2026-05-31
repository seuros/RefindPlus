#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
# Local QEMU test for config-layer features (macros / conditionals / URI) without
# risking a real machine. Builds an ESP with Meridian + a config.conf carrying a
# test block, boots under OVMF, and extracts the Meridian log so we can confirm:
#   - no early-init hang (boot reaches the menu / tool-options phase)
#   - ${ARCH}/${FW_TYPE}/${user} macros expand
#   - if_arch matches/skips correctly
# Usage: scripts/qemu-config-test.sh [seconds]   (default 30)
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
EFI="$ROOT/edk2/Build/Meridian/DEBUG_CLANGDWARF/X64/Meridian.efi"
OVMF_CODE="/usr/share/edk2/x64/OVMF_CODE.4m.fd"
OVMF_VARS="/usr/share/edk2/x64/OVMF_VARS.4m.fd"
WINDOW="${1:-30}"

WORK="$(mktemp -d /tmp/meridian-cfgtest.XXXXXX)"
trap 'rm -rf "$WORK"' EXIT
[ -f "$EFI" ] || { echo "build first: $EFI missing"; exit 1; }

mkdir -p "$WORK/esp/EFI/BOOT" "$WORK/esp/EFI/Meridian"
cp "$EFI" "$WORK/esp/EFI/BOOT/bootx64.efi"
cp "$EFI" "$WORK/esp/EFI/Meridian/Meridian.efi"

cat > "$WORK/esp/EFI/Meridian/config.conf" <<'CONF'
timeout 2
serial on
measured_boot on
default_selection "SelfBoot"
${TESTOS}=Beastie

menuentry "SelfBoot" {
    loader \EFI\BOOT\bootx64.efi
}

menuentry "ArchTest ${TESTOS}" {
    if_arch x86_64
    loader \EFI\nope\none.efi
}

menuentry "ArmTest shouldskip" {
    if_arch aarch64
    loader \EFI\nope\none.efi
}

menuentry "MacroTitle ${ARCH}-${FW_TYPE}" {
    loader \EFI\nope\none.efi
}
CONF

# ESP FAT image
truncate -s 64M "$WORK/esp.img"
mkfs.vfat -F 32 -n MRDESP "$WORK/esp.img" >/dev/null
mcopy -s -i "$WORK/esp.img" "$WORK/esp/EFI" ::

# GPT disk with the ESP
truncate -s 80M "$WORK/disk.img"
sgdisk -o "$WORK/disk.img" >/dev/null
sgdisk -n 1:2048:+64M -t 1:EF00 -c 1:ESP "$WORK/disk.img" >/dev/null
ESP_START=$(sgdisk -i 1 "$WORK/disk.img" | awk -F': ' '/First sector/{print $2}' | awk '{print $1}')
dd if="$WORK/esp.img" of="$WORK/disk.img" bs=512 seek="$ESP_START" conv=notrunc status=none

cp "$OVMF_VARS" "$WORK/vars.fd"
echo "=== booting QEMU ${WINDOW}s ==="
# Inject an SMBIOS Type 11 OEM string to exercise SMBIOS-embedded config: a
# timeout override (9) that should win over config.conf's timeout 3.
qemu-system-x86_64 -machine q35 -m 512 -nographic \
  -smbios type=11,value=meridian:config:timeout=9 \
  -drive if=pflash,format=raw,readonly=on,file="$OVMF_CODE" \
  -drive if=pflash,format=raw,file="$WORK/vars.fd" \
  -drive format=raw,file="$WORK/disk.img" \
  -serial file:"$WORK/serial.log" -display none -no-reboot >/dev/null 2>&1 &
QPID=$!
sleep "$WINDOW"
kill "$QPID" 2>/dev/null || true
wait "$QPID" 2>/dev/null || true

: > "$WORK/all.log"
for f in $(mdir -b -i "$WORK/disk.img@@$((ESP_START*512))" :: 2>/dev/null | grep -i '\.log$'); do
  mcopy -n -i "$WORK/disk.img@@$((ESP_START*512))" "$f" "$WORK/out.log" 2>/dev/null || true
  cat "$WORK/out.log" >> "$WORK/all.log" 2>/dev/null || true
done
echo "=== measured boot (#6 graceful path) ==="
grep -aE "Measured Boot" "$WORK/all.log" 2>/dev/null | sort -u | head || echo "(no measured-boot line)"
cp -f "$WORK/out.log" "$ROOT/.qemu-cfg-last.log" 2>/dev/null || true
cp -f "$WORK/serial.log" "$ROOT/.qemu-serial-last.log" 2>/dev/null || true
echo "=== serial mirror (Meridian log lines that should ONLY be on serial via the mirror) ==="
grep -aE "Scan Internal|Found Manual Stanza|SEEK INSTANCE|Highlighted Screen Option" "$WORK/serial.log" 2>/dev/null | head -4 \
  || echo "(no Meridian log lines found on serial)"
echo "=== results ==="
if [ -f "$WORK/out.log" ]; then
  echo "[boot reached]:"
  grep -aoE "H A N D L E   T O O L|Set Shortcuts|SEEK INSTANCE" "$WORK/out.log" | head -1 || true
  echo "[macros/conditionals]:"
  grep -anE "ArchTest|ArmTest|MacroTitle|Stanza Skipped|Found Manual" "$WORK/out.log" | head || true
else
  echo "NO LOG EXTRACTED (likely hung before writing log)"
fi
