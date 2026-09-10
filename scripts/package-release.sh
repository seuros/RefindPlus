#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORKSPACE_DIR="${EDK2_WORKSPACE:-${WORKSPACE:-${ROOT_DIR}/edk2}}"
ARCH="X64"
TARGET="RELEASE"
TOOLCHAIN="CLANGDWARF"
BUILD_DIR=""
VERSION=""
OUT_DIR="${ROOT_DIR}/dist"

# Filesystem drivers shipped in EFI\Meridian\fs. Kept in sync with the
# filesystems/*.inf entries in MeridianPkg.dsc; scripts/check.sh enforces that.
FS_DRIVERS=(bcachefs btrfs ext2 ext4 hfs iso9660 ntfs ufs)

# Standalone applications shipped as loose downloads, not inside the ESP tree.
# MeridianSnpDiscover, SmSelfTest and LuaTest are build-time/bench tools and are
# deliberately not released.
APPLICATIONS=(Cydia DarkPassenger)

usage() {
    cat <<'USAGE'
Usage: scripts/package-release.sh [options]

Stages the on-ESP layout from an existing build and zips it, alongside the
loose Meridian.efi, the standalone applications, and a SHA256SUMS file.

Options:
  --arch ARCH        X64 or AARCH64, default: X64
  --target TARGET    RELEASE, DEBUG, or NOOPT, default: RELEASE
  --toolchain TAG    EDK2 toolchain tag, default: CLANGDWARF
  --workspace PATH   EDK2 workspace holding Build/, default: $EDK2_WORKSPACE
  --build-dir PATH   Directory holding the built *.efi, overrides the above
  --version VERSION  Version string used in artifact names, required
  --out PATH         Output directory, default: dist/
  -h, --help         Show this help
USAGE
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --arch)       ARCH="${2:-}"; shift 2 ;;
        --target)     TARGET="${2:-}"; shift 2 ;;
        --toolchain)  TOOLCHAIN="${2:-}"; shift 2 ;;
        --workspace)  WORKSPACE_DIR="${2:-}"; shift 2 ;;
        --build-dir)  BUILD_DIR="${2:-}"; shift 2 ;;
        --version)    VERSION="${2:-}"; shift 2 ;;
        --out)        OUT_DIR="${2:-}"; shift 2 ;;
        -h|--help)    usage; exit 0 ;;
        *)
            printf 'Unknown option: %s\n\n' "$1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

ARCH="${ARCH^^}"
TARGET="${TARGET^^}"

case "${ARCH}" in
    X64)      FALLBACK_NAME="BOOTX64.EFI"; ARCH_SLUG="x64" ;;
    AARCH64)  FALLBACK_NAME="BOOTAA64.EFI"; ARCH_SLUG="aarch64" ;;
    *)
        printf 'Unsupported --arch value: %s (X64 or AARCH64)\n' "${ARCH}" >&2
        exit 2
        ;;
esac

if [[ -z "${VERSION}" ]]; then
    printf -- '--version is required\n\n' >&2
    usage >&2
    exit 2
fi

if [[ -z "${BUILD_DIR}" ]]; then
    BUILD_DIR="${WORKSPACE_DIR}/Build/Meridian/${TARGET}_${TOOLCHAIN}/${ARCH}"
fi

if [[ ! -d "${BUILD_DIR}" ]]; then
    printf 'No build output at %s -- build first.\n' "${BUILD_DIR}" >&2
    exit 1
fi

BUILD_DIR="$(cd "${BUILD_DIR}" && pwd)"

require() {
    if [[ ! -f "$1" ]]; then
        printf 'Missing expected build artifact: %s\n' "$1" >&2
        exit 1
    fi
}

require "${BUILD_DIR}/Meridian.efi"
for driver in "${FS_DRIVERS[@]}"; do
    require "${BUILD_DIR}/${driver}.efi"
done
for app in "${APPLICATIONS[@]}"; do
    require "${BUILD_DIR}/${app}.efi"
done

mkdir -p "${OUT_DIR}"
OUT_DIR="$(cd "${OUT_DIR}" && pwd)"

STAGE_DIR="$(mktemp -d)"
trap 'rm -rf "${STAGE_DIR}"' EXIT

# The layout below is the one boot/esp_layout.h declares and install/install.c
# creates. Keep all three in agreement.
mkdir -p \
    "${STAGE_DIR}/EFI/BOOT" \
    "${STAGE_DIR}/EFI/Meridian/fs" \
    "${STAGE_DIR}/EFI/Meridian/drivers"

cp "${BUILD_DIR}/Meridian.efi" "${STAGE_DIR}/EFI/BOOT/${FALLBACK_NAME}"
cp "${BUILD_DIR}/Meridian.efi" "${STAGE_DIR}/EFI/Meridian/Meridian.efi"
for driver in "${FS_DRIVERS[@]}"; do
    cp "${BUILD_DIR}/${driver}.efi" "${STAGE_DIR}/EFI/Meridian/fs/${driver}.efi"
done

# drivers/ ships empty on purpose: it is where the operator drops NIC/USB3/
# storage blobs. zip(1) drops empty directories, so leave a note behind that
# explains the folder rather than an unexplained placeholder.
cat > "${STAGE_DIR}/EFI/Meridian/drivers/README.txt" <<'DRIVERS'
Drop optional device drivers here (NIC, USB3, storage).
Filesystem drivers belong in ..\fs instead.
This directory ships empty on purpose.
DRIVERS

# No BOOT.CSV. The in-firmware installer writes one as UCS-2LE, and it only
# means anything next to shim/fbx64.efi, which this bundle does not ship.

ESP_ZIP="${OUT_DIR}/meridian-${VERSION}-${ARCH_SLUG}-esp.zip"
rm -f "${ESP_ZIP}"
printf '[INFO] arch      : %s\n' "${ARCH}"
printf '[INFO] target    : %s\n' "${TARGET}"
printf '[INFO] build dir : %s\n' "${BUILD_DIR}"
printf '[INFO] staging   : %s\n' "${STAGE_DIR}"

( cd "${STAGE_DIR}" && zip -q -r -X "${ESP_ZIP}" EFI )
printf '[INFO] packaged  : %s\n' "${ESP_ZIP}"

artifacts=("meridian-${VERSION}-${ARCH_SLUG}-esp.zip")

cp "${BUILD_DIR}/Meridian.efi" "${OUT_DIR}/Meridian-${VERSION}-${ARCH_SLUG}.efi"
artifacts+=("Meridian-${VERSION}-${ARCH_SLUG}.efi")

for app in "${APPLICATIONS[@]}"; do
    cp "${BUILD_DIR}/${app}.efi" "${OUT_DIR}/${app}-${VERSION}-${ARCH_SLUG}.efi"
    artifacts+=("${app}-${VERSION}-${ARCH_SLUG}.efi")
done

SUMS="SHA256SUMS.${ARCH_SLUG}"
( cd "${OUT_DIR}" && sha256sum "${artifacts[@]}" > "${SUMS}" )

printf '[INFO] checksums : %s\n' "${OUT_DIR}/${SUMS}"
printf '[OK] release artifacts for %s staged in %s\n' "${ARCH}" "${OUT_DIR}"
