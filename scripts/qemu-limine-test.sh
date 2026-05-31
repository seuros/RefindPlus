#!/usr/bin/env bash
# Limine boot-protocol conformance harness.
#
# Builds the upstream protocol test kernel (.tmp/limine/test/limine.c -- 780
# lines that print every response they receive over port 0xe9), stages it on an
# ESP next to Meridian with `scan_limine true`, boots it headless under
# OVMF/QEMU, and dumps the debugcon transcript.
#
# The transcript is the whole point: boot the same test.elf with stock Limine
# and diff the two, and any missing or wrong response field shows up as a text
# diff instead of a triple fault.
#
# Reference trees under .tmp/ (none are vendored, all are clone-on-demand):
#   limine/            the bootloader -- behavioural oracle + the test kernel
#   limine-protocol/   the 0BSD spec and header
#   flanterm/          github.com/Mintsuki/Flanterm
#   freestanding-c-hdrs/  github.com/osdev0/freestanding-c-hdrs
#
# flanterm and freestanding-c-hdrs must sit at the commits .tmp/limine/bootstrap
# pins -- the test kernel calls flanterm_fb_init(), whose arity changes between
# revisions, so a plain HEAD clone fails to compile.
#
# Usage: scripts/qemu-limine-test.sh [seconds]   (default boot window: 30s)
#        LIMINE_REV=N to build the test kernel against base revision N (default 4)
#        KEEP=1 scripts/qemu-limine-test.sh   to keep the staging directory
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
EFI="$ROOT/edk2/Build/Meridian/DEBUG_CLANGDWARF/X64/Meridian.efi"
TMP="$ROOT/.tmp"
TEST="$TMP/limine/test"
OVMF_CODE="/usr/share/edk2/x64/OVMF_CODE.4m.fd"
OVMF_VARS="/usr/share/edk2/x64/OVMF_VARS.4m.fd"
WINDOW="${1:-30}"

[ -f "$EFI" ] || { echo "Meridian.efi not found - run 'mise run build' first"; exit 1; }
[ -d "$TEST" ] || { echo "missing $TEST - clone limine into .tmp/ first"; exit 1; }
[ -d "$TMP/limine-protocol/include" ] || { echo "missing .tmp/limine-protocol"; exit 1; }
[ -d "$TMP/flanterm/src" ] || {
    echo "missing .tmp/flanterm - see the pins in .tmp/limine/bootstrap"
    exit 1
}
# Layout moved between revisions: older trees put the headers at include/,
# newer ones split them per-arch.
if [ -d "$TMP/freestanding-c-hdrs/x86_64/include" ]; then
    FREESTD="$TMP/freestanding-c-hdrs/x86_64/include"
elif [ -d "$TMP/freestanding-c-hdrs/include" ]; then
    FREESTD="$TMP/freestanding-c-hdrs/include"
else
    echo "missing .tmp/freestanding-c-hdrs - see the pins in .tmp/limine/bootstrap"
    exit 1
fi

WORK="$(mktemp -d /tmp/meridian-limine.XXXXXX)"
# KEEP=1 leaves the staging directory behind: the disk image, the built kernel
# and the raw logs, for when the transcript alone is not enough.
if [ "${KEEP:-0}" = "1" ]; then
    trap 'echo "(staging kept at $WORK)"' EXIT
else
    trap 'rm -rf "$WORK"' EXIT
fi

# --- Build the conformance kernel ---
# test.mk expects limine's own configure/bootstrap to have run; these are the
# same flags, spelled out, so the harness works against a bare clone.
CFLAGS=(
    -std=c11 -O2 -g -Wall
    -nostdinc -ffreestanding -fno-stack-protector -fno-stack-check -fno-lto -fPIE
    -I"$TEST" -I"$TMP/limine-protocol/include" -I"$TMP/flanterm/src"
    -isystem "$FREESTD"
    -D_LIMINE_PROTO
    -target x86_64-unknown-none-elf
    -m64 -march=x86-64 -mabi=sysv -mgeneral-regs-only -mno-red-zone
)

# The upstream test kernel asks for base revision 6 and halts immediately if it
# does not get it. Meridian's ceiling is 4, so the harness rewrites the request
# to the revision under test -- everything else about the kernel is untouched.
# Set LIMINE_REV=6 to watch the refusal path instead.
REV="${LIMINE_REV:-4}"
sed "s/LIMINE_BASE_REVISION(6)/LIMINE_BASE_REVISION($REV)/" "$TEST/limine.c" > "$WORK/limine.c"

echo "=== building conformance kernel (base revision $REV) ==="
clang "${CFLAGS[@]}" -c "$WORK/limine.c" -o "$WORK/limine.o"
for f in e9print memory; do
    clang "${CFLAGS[@]}" -c "$TEST/$f.c" -o "$WORK/$f.o"
done
clang "${CFLAGS[@]}" -c "$TMP/flanterm/src/flanterm.c" -o "$WORK/flanterm.o"
clang "${CFLAGS[@]}" -c "$TMP/flanterm/src/flanterm_backends/fb.c" -o "$WORK/fb.o"

# -ztext because the kernel is a static PIE: every relocation has to be a
# R_X86_64_RELATIVE in .rela.dyn, which is the only kind loaders/limine/elf.c
# applies. A text relocation here would mean the harness is testing something
# Meridian deliberately rejects.
ld.lld -m elf_x86_64 -T "$TEST/linker.ld" -nostdlib -zmax-page-size=0x1000 -pie -ztext \
    "$WORK"/limine.o "$WORK"/e9print.o "$WORK"/memory.o "$WORK"/flanterm.o "$WORK"/fb.o \
    -o "$WORK/test.elf"

echo "kernel   : $(du -h "$WORK/test.elf" | cut -f1)  $(file -b "$WORK/test.elf" | cut -d, -f1-2)"
echo "Meridian : $EFI"

# --- Stage the ESP ---
mkdir -p "$WORK/esp/EFI/BOOT" "$WORK/esp/EFI/Meridian"
cp "$EFI" "$WORK/esp/EFI/BOOT/bootx64.efi"
cp "$EFI" "$WORK/esp/EFI/Meridian/Meridian.efi"
# ESP root, because that is one of the handful of directories the loader scan
# actually walks (root, boot, @/boot) -- a kernels/ subdirectory is invisible
# without an also_scan_dirs entry.
cp "$WORK/test.elf" "$WORK/esp/test.elf"

# scan_limine is off by default; without it the *.elf pattern is never merged
# into the loader search and the kernel is invisible. timeout 1 so the harness
# does not spend its whole window in the menu.
cat > "$WORK/esp/EFI/Meridian/config.conf" <<'CONF'
timeout 1
scan_limine true
scanfor internal,external,manual
default_selection test
CONF
cp "$WORK/esp/EFI/Meridian/config.conf" "$WORK/esp/EFI/BOOT/config.conf"

# --- Build the ESP image ---
truncate -s 64M "$WORK/esp.img"
mkfs.vfat -F 32 -n MERIDIANESP "$WORK/esp.img" >/dev/null
mcopy -s -i "$WORK/esp.img" "$WORK/esp/EFI" ::
mcopy -i "$WORK/esp.img" "$WORK/esp/test.elf" ::

truncate -s 96M "$WORK/disk.img"
sgdisk -Z "$WORK/disk.img" >/dev/null 2>&1 || true
sgdisk -o "$WORK/disk.img" >/dev/null
sgdisk -n 1:2048:+64M -t 1:EF00 -c 1:ESP "$WORK/disk.img" >/dev/null
ESP_START=$(sgdisk -i 1 "$WORK/disk.img" | awk -F': ' '/First sector/{print $2}' | awk '{print $1}')
dd if="$WORK/esp.img" of="$WORK/disk.img" bs=512 seek="$ESP_START" conv=notrunc status=none

# --- Boot ---
cp "$OVMF_VARS" "$WORK/vars.fd"
echo "=== booting QEMU for ${WINDOW}s ==="
qemu-system-x86_64 \
    -machine q35 -m 2048 -cpu qemu64,+nx,+pdpe1gb \
    -drive if=pflash,format=raw,readonly=on,file="$OVMF_CODE" \
    -drive if=pflash,format=raw,file="$WORK/vars.fd" \
    -drive format=raw,file="$WORK/disk.img" \
    -debugcon file:"$WORK/debugcon.log" -global isa-debugcon.iobase=0xe9 \
    -serial file:"$WORK/serial.log" -display none -no-reboot \
    >/dev/null 2>&1 &
QPID=$!
sleep "$WINDOW"
kill "$QPID" 2>/dev/null || true
wait "$QPID" 2>/dev/null || true

# --- Results ---
echo "=== conformance kernel output (port 0xe9) ==="
if [ -s "$WORK/debugcon.log" ]; then
    cat "$WORK/debugcon.log"
else
    echo "(nothing -- the kernel was never entered)"
fi

echo "=== Meridian ESP log (Limine lines) ==="
for f in $(mdir -b -i "$WORK/disk.img@@$((ESP_START * 512))" :: 2>/dev/null | grep -i '\.log$'); do
    mcopy -n -i "$WORK/disk.img@@$((ESP_START * 512))" "$f" "$WORK/meridian.log" 2>/dev/null || true
    grep -niE "limine" "$WORK/meridian.log" 2>/dev/null | head -40 || true
done

cp -f "$WORK/debugcon.log" "$ROOT/.qemu-limine-debugcon.log" 2>/dev/null || true
cp -f "$WORK/meridian.log" "$ROOT/.qemu-limine-meridian.log" 2>/dev/null || true
echo "(transcripts copied to .qemu-limine-debugcon.log / .qemu-limine-meridian.log)"
