#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT_DIR}"

MODE="files"
if [[ "${1:-}" == "--staged" ]]; then
    MODE="staged"
    shift
fi

TIDY_BIN="${CLANG_TIDY:-clang-tidy}"
if ! command -v "${TIDY_BIN}" >/dev/null 2>&1; then
    printf '[WARN] clang-tidy is not installed; skipping clang-tidy check\n' >&2
    exit 0
fi

TIDY_DIFF="${CLANG_TIDY_DIFF:-}"
if [[ -z "${TIDY_DIFF}" ]]; then
    for candidate in \
        /usr/share/clang/clang-tidy-diff.py \
        /usr/lib/llvm-*/share/clang/clang-tidy-diff.py
    do
        if [[ -f "${candidate}" ]]; then
            TIDY_DIFF="${candidate}"
            break
        fi
    done
fi

common_args=(
    -std=c11
    -fshort-wchar
    -fno-builtin
    -fno-strict-aliasing
    -target x86_64-pc-linux-gnu
    -Wno-unknown-pragmas
    -Wno-unknown-warning-option
    -Wno-varargs
    -Wno-incompatible-library-redeclaration
    -Wno-null-dereference
    -DEFIX64
    -DMERIDIAN_DEBUG=1
    -DFSW_DEBUG_LEVEL=1
    '-DEFIAPI=__attribute__((ms_abi))'
    "-I${ROOT_DIR}"
    "-I${ROOT_DIR}/include"
    "-I${ROOT_DIR}/boot"
    "-I${ROOT_DIR}/config"
    "-I${ROOT_DIR}/discovery"
    "-I${ROOT_DIR}/drivers"
    "-I${ROOT_DIR}/install"
    "-I${ROOT_DIR}/kernel"
    "-I${ROOT_DIR}/lib"
    "-I${ROOT_DIR}/loaders"
    "-I${ROOT_DIR}/loaders/limine"
    "-I${ROOT_DIR}/platform"
    "-I${ROOT_DIR}/policy"
    "-I${ROOT_DIR}/remote"
    "-I${ROOT_DIR}/storage"
    "-I${ROOT_DIR}/tools"
    "-I${ROOT_DIR}/ui"
    "-I${ROOT_DIR}/EfiLib"
    "-I${ROOT_DIR}/filesystems"
    "-I${ROOT_DIR}/Library/MemLogLib"
    "-I${ROOT_DIR}/Library/MeridianApfsLib"
    "-I${ROOT_DIR}/Library/NvmExpressLib"
    "-I${ROOT_DIR}/Library/StateMachineLib"
    "-I${ROOT_DIR}/Library/LuaLib"
    "-I${ROOT_DIR}/mok"
    "-I${ROOT_DIR}/conn/core"
    "-I${ROOT_DIR}/conn/efi"
    "-I${ROOT_DIR}/conn/host"
    "-I${ROOT_DIR}/libstatemachines/include"
    "-I${ROOT_DIR}/edk2/MdePkg/Include"
    "-I${ROOT_DIR}/edk2/MdePkg/Include/X64"
    "-I${ROOT_DIR}/edk2/MdeModulePkg/Include"
    "-I${ROOT_DIR}/edk2/ShellPkg/Include"
    "-I${ROOT_DIR}/edk2/UefiCpuPkg/Include"
)

is_tidy_source() {
    case "$1" in
        Application/*.c|EfiLib/*.c|Library/*.c|boot/*.c|config/*.c|discovery/*.c|drivers/*.c|install/*.c|kernel/*.c|lib/*.c|loaders/*.c|loaders/limine/*.c|platform/*.c|policy/*.c|remote/*.c|storage/*.c|tools/*.c|ui/*.c|conn/*.c|conn/core/*.c|conn/efi/*.c|conn/host/*.c|mok/*.c|net/*.c|net/snp/*.c|recon/*.c)
            return 0
            ;;
        *)
            return 1
            ;;
    esac
}

if [[ "${MODE}" == "staged" ]]; then
    if [[ -z "${TIDY_DIFF}" ]]; then
        printf '[WARN] clang-tidy-diff.py was not found; skipping clang-tidy staged-diff check\n' >&2
        exit 0
    fi

    staged_diff="$(git diff --cached -U0 --no-color -- '*.c' '*.h')"
    if [[ -z "${staged_diff}" ]]; then
        printf '[INFO] clang-tidy staged C check skipped; no staged C/H changes\n'
        exit 0
    fi

    tidy_extra=()
    for arg in "${common_args[@]}"; do
        tidy_extra+=("-extra-arg=${arg}")
    done

    printf '%s\n' "${staged_diff}" | python3 "${TIDY_DIFF}" \
        -clang-tidy-binary "${TIDY_BIN}" \
        -p1 \
        -quiet \
        -config-file "${ROOT_DIR}/.clang-tidy" \
        -regex '^(Application|EfiLib|Library|boot|config|conn|discovery|drivers|install|kernel|lib|loaders|mok|net|platform|policy|recon|remote|storage|tools|ui)/.*\.c$' \
        -j "${CLANG_TIDY_JOBS:-1}" \
        -warnings-as-errors "${CLANG_TIDY_WARNINGS_AS_ERRORS:-*}" \
        "${tidy_extra[@]}"
    exit $?
fi

files=("$@")
if [[ "${#files[@]}" -eq 0 ]]; then
    mapfile -t files < <(git ls-files '*.c' | while IFS= read -r file; do
        if is_tidy_source "${file}"; then
            printf '%s\n' "${file}"
        fi
    done)
fi

if [[ "${#files[@]}" -eq 0 ]]; then
    printf '[INFO] clang-tidy check skipped; no C sources selected\n'
    exit 0
fi

status=0
for file in "${files[@]}"; do
    if [[ ! -f "${file}" ]] || ! is_tidy_source "${file}"; then
        continue
    fi

    printf '[INFO] clang-tidy %s\n' "${file}"
    if ! "${TIDY_BIN}" --quiet --config-file="${ROOT_DIR}/.clang-tidy" "${file}" -- "${common_args[@]}"; then
        status=1
    fi
done

exit "${status}"
