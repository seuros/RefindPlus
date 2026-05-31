#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT_DIR}"

failures=0
warnings=0

info() {
    printf '[INFO] %s\n' "$*"
}

warn() {
    warnings=$((warnings + 1))
    printf '[WARN] %s\n' "$*" >&2
}

fail() {
    failures=$((failures + 1))
    printf '[FAIL] %s\n' "$*" >&2
}

section_entries() {
    local section="$1"
    local file="$2"

    awk -v wanted="${section}" '
        BEGIN { active = 0 }
        $0 ~ "^\\[" wanted "\\]" { active = 1; next }
        $0 ~ "^\\[" && active { active = 0 }
        active {
            line = $0
            sub(/[[:space:]]+#.*/, "", line)
            sub(/^[[:space:]]+/, "", line)
            sub(/[[:space:]]+$/, "", line)
            if (line ~ /^#/) {
                next
            }
            if (line != "") {
                print line
            }
        }
    ' "${file}"
}

check_reuse() {
    local reuse_cmd=()

    if command -v reuse >/dev/null 2>&1; then
        reuse_cmd=(reuse)
    elif command -v python3 >/dev/null 2>&1 && python3 -c 'import reuse' >/dev/null 2>&1; then
        reuse_cmd=(python3 -m reuse)
    else
        warn "reuse is not installed; skipping REUSE lint"
        return
    fi

    info "Running reuse lint"
    if ! "${reuse_cmd[@]}" lint; then
        fail "REUSE lint failed"
    fi
}

check_linter_configs() {
    info "Checking linter configuration"

    if command -v pre-commit >/dev/null 2>&1; then
        if ! pre-commit validate-config; then
            fail "pre-commit configuration is invalid"
        fi
    else
        warn "pre-commit is not installed; skipping pre-commit config validation"
    fi

    if command -v clang-tidy >/dev/null 2>&1; then
        if ! clang-tidy --verify-config --config-file=.clang-tidy >/dev/null; then
            fail "clang-tidy configuration is invalid"
        fi
    else
        warn "clang-tidy is not installed; skipping clang-tidy config validation"
    fi
}

check_inf_sources() {
    local missing=0
    local path

    info "Checking Meridian.inf source entries"
    while IFS= read -r path; do
        if [[ ! -f "${path}" ]]; then
            fail "Meridian.inf references missing source: ${path}"
            missing=$((missing + 1))
        fi
    done < <(section_entries "Sources" "Meridian.inf")

    if [[ "${missing}" -eq 0 ]]; then
        info "Meridian.inf source entries exist"
    fi
}

check_dsc_components() {
    local missing=0
    local path
    local local_path

    info "Checking MeridianPkg.dsc package components"
    while IFS= read -r path; do
        local_path="${path#MeridianPkg/}"
        if [[ "${local_path}" == "${path}" ]]; then
            warn "Skipping non-Meridian package component: ${path}"
            continue
        fi

        if [[ ! -f "${local_path}" ]]; then
            fail "MeridianPkg.dsc references missing component: ${path}"
            missing=$((missing + 1))
        fi
    done < <(section_entries "Components" "MeridianPkg.dsc")

    if [[ "${missing}" -eq 0 ]]; then
        info "MeridianPkg.dsc package components exist"
    fi
}

check_public_docs() {
    local stale_pattern='RefindPlusRepo/RefindPlus|GOPFix|BUILDING REFINDPLUS|^# RefindPlus$'

    info "Checking root public docs for stale upstream-only references"
    if rg -n "${stale_pattern}" \
        README.md \
        >/tmp/meridian-doc-check.$$ 2>/dev/null
    then
        cat /tmp/meridian-doc-check.$$ >&2
        rm -f /tmp/meridian-doc-check.$$
        fail "Root public docs still contain stale upstream-only references"
    else
        rm -f /tmp/meridian-doc-check.$$
        info "Root public docs use Meridian identity"
    fi
}

check_no_legacy_makefiles() {
    local found

    found="$(find boot EfiLib libeg mok filesystems \
        -maxdepth 1 \
        \( -name 'Makefile' -o -name 'Make.tiano' -o -name 'Make.gnuefi' \) \
        -type f 2>/dev/null || true)"

    found="${found}"$'\n'"$(find net \
        -maxdepth 2 \
        \( -name 'Makefile' -o -name 'Makefile.housekeeping' \) \
        -type f 2>/dev/null || true)"
    found="$(printf '%s\n' "${found}" | sed '/^$/d')"

    info "Checking for legacy makefiles"
    if [[ -n "${found}" ]]; then
        fail "Legacy makefiles are not part of the supported build surface:"
        printf '%s\n' "${found}" >&2
    else
        info "No legacy makefiles found"
    fi
}

check_active_workflows() {
    local stale_pattern='RefindPlusRepo/RefindPlus|GOPFix|x64_RefindPlus|RefindPlus-Artefacts'

    info "Checking active GitHub workflows"
    if [[ ! -d .github/workflows ]]; then
        warn "No active GitHub workflow directory found"
        return
    fi

    if rg -n "${stale_pattern}" .github/workflows >/tmp/meridian-workflow-check.$$ 2>/dev/null; then
        cat /tmp/meridian-workflow-check.$$ >&2
        rm -f /tmp/meridian-workflow-check.$$
        fail "Active GitHub workflows still contain stale upstream-only references"
    else
        rm -f /tmp/meridian-workflow-check.$$
        info "Active GitHub workflows use Meridian-safe checks"
    fi
}

check_reuse
check_linter_configs
check_inf_sources
check_dsc_components
check_public_docs
check_active_workflows
check_no_legacy_makefiles

if [[ "${failures}" -ne 0 ]]; then
    printf '[FAIL] %d failure(s), %d warning(s)\n' "${failures}" "${warnings}" >&2
    exit 1
fi

printf '[OK] repository checks passed with %d warning(s)\n' "${warnings}"
