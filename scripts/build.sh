#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORKSPACE_DIR="${EDK2_WORKSPACE:-${WORKSPACE:-${ROOT_DIR}/edk2}}"
ARCH="X64"
TARGET="DEBUG"
TOOLCHAIN="CLANGDWARF"
DRY_RUN=0

usage() {
    cat <<'USAGE'
Usage: scripts/build.sh [options]

Options:
  --workspace PATH   EDK2 workspace containing MeridianPkg symlink/directory
  --arch ARCH        EDK2 architecture: X64 or AARCH64, default: X64
                     (no IA32 -- 32-bit-EFI Macs e.g. Mac Pro 1,1/2,1 are
                     unsupported; the binary is 64-bit only)
  --target TARGET    RELEASE, DEBUG, or NOOPT, default: DEBUG
  --toolchain TAG    EDK2 toolchain tag, default: CLANGDWARF
  --dry-run          Print the resolved build command without executing it
  -h, --help         Show this help
USAGE
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --workspace)
            WORKSPACE_DIR="${2:-}"
            shift 2
            ;;
        --arch)
            ARCH="${2:-}"
            shift 2
            ;;
        --target)
            TARGET="${2:-}"
            shift 2
            ;;
        --toolchain)
            TOOLCHAIN="${2:-}"
            shift 2
            ;;
        --dry-run)
            DRY_RUN=1
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            printf 'Unknown option: %s\n\n' "$1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

ARCH="${ARCH^^}"
TARGET="${TARGET^^}"

# IA32 is intentionally absent: MeridianPkg.dsc is X64|AARCH64 only, and
# 32-bit-EFI Macs (Mac Pro 1,1/2,1, early Core Solo/Duo) cannot load a 64-bit
# binary anyway. Reject it here with a clear message instead of failing deep
# inside the EDK2 build.
case "${ARCH}" in
    X64|AARCH64) ;;
    *)
        printf 'Unsupported --arch value: %s (Meridian is 64-bit only: X64 or AARCH64)\n' \
            "${ARCH}" >&2
        exit 2
        ;;
esac

case "${TARGET}" in
    RELEASE|DEBUG|NOOPT) ;;
    *)
        printf 'Unsupported --target value: %s\n' "${TARGET}" >&2
        exit 2
        ;;
esac

if [[ -z "${WORKSPACE_DIR}" ]]; then
    cat >&2 <<EOF
No EDK2 workspace provided.

Pass --workspace /path/to/edk2 or set EDK2_WORKSPACE/WORKSPACE.
The workspace must expose this checkout as:

  /path/to/edk2/MeridianPkg -> ${ROOT_DIR}
EOF
    exit 2
fi

WORKSPACE_DIR="$(cd "${WORKSPACE_DIR}" && pwd)"
PACKAGE_DIR="${WORKSPACE_DIR}/MeridianPkg"

if [[ ! -e "${PACKAGE_DIR}" ]]; then
    cat >&2 <<EOF
Missing EDK2 package path:

  ${PACKAGE_DIR}

Create it with:

  ln -s ${ROOT_DIR} ${PACKAGE_DIR}
EOF
    exit 2
fi

if [[ "$(cd "${PACKAGE_DIR}" && pwd -P)" != "$(cd "${ROOT_DIR}" && pwd -P)" ]]; then
    cat >&2 <<EOF
${PACKAGE_DIR} does not point at this checkout.

Expected:
  ${ROOT_DIR}

Found:
  $(cd "${PACKAGE_DIR}" && pwd -P)
EOF
    exit 2
fi

# kernel/cache.c folds its own __DATE__/__TIME__ into the menu.cache fingerprint
# so a rebuilt binary invalidates a stale fast-boot cache. Touch it every build
# to guarantee a fresh compile timestamp even on incremental builds where only
# other files changed -- otherwise a scan-logic change can be masked by a cache
# written by the previous binary.
touch "${ROOT_DIR}/kernel/cache.c" 2>/dev/null || true

cmd=(build -p MeridianPkg/MeridianPkg.dsc -a "${ARCH}" -b "${TARGET}" -t "${TOOLCHAIN}")

printf '[INFO] workspace : %s\n' "${WORKSPACE_DIR}"
printf '[INFO] package   : %s\n' "${PACKAGE_DIR}"
printf '[INFO] command   :'
printf ' %q' "${cmd[@]}"
printf '\n'

if [[ "${DRY_RUN}" -eq 1 ]]; then
    exit 0
fi

# Build BaseTools if not already built
if [[ ! -x "${WORKSPACE_DIR}/BaseTools/Source/C/bin/GenFw" ]]; then
    printf '[INFO] Building BaseTools...\n'
    make -C "${WORKSPACE_DIR}/BaseTools" --no-print-directory -j"$(nproc)"
fi

export WORKSPACE="${WORKSPACE_DIR}"
export EDK_TOOLS_PATH="${WORKSPACE_DIR}/BaseTools"
export CONF_PATH="${WORKSPACE_DIR}/Conf"
export PYTHON_COMMAND="${PYTHON_COMMAND:-python3}"

source "${WORKSPACE_DIR}/edksetup.sh" --reconfig > /dev/null 2>&1 || true

# CLANGDWARF emits DWARF debug files (*.debug), not MS-style PDBs. EDK2's
# generated GCC/CLANGDWARF dynamic-library rule still tries to copy *.pdb with
# an ignored shell error, which makes successful builds look noisy.
if [[ -f "${CONF_PATH}/build_rule.txt" ]]; then
    perl -ni -e '
        $in = 1 if /^\s*<Command\.GCC, Command\.CLANGDWARF>/;
        $in = 0 if /^\s*<Command\./ && !/^\s*<Command\.GCC, Command\.CLANGDWARF>/;
        print unless $in && /\$\(CP\).*\*\.pdb.*\$\(OUTPUT_DIR\)/;
    ' \
        "${CONF_PATH}/build_rule.txt"
fi

build_dir="${WORKSPACE_DIR}/Build/Meridian/${TARGET}_${TOOLCHAIN}"
if [[ "${TOOLCHAIN^^}" == "CLANGDWARF" && -d "${build_dir}" ]]; then
    find "${build_dir}" -name GNUmakefile -type f -exec \
        perl -ni -e 'print unless /^\t-\$\(CP\) \$\(DEBUG_DIR\)\/\*\.pdb \$\(OUTPUT_DIR\)\s*$/' {} +
fi

cd "${WORKSPACE_DIR}"
exec "${cmd[@]}"
