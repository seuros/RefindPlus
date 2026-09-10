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

check_release_version() {
    local header_line header_version manifest_version file_version

    info "Checking release version agreement"

    header_line="$(grep -n 'VERSION_STRING_ASCII' include/version.h | head -1 || true)"
    if [[ "${header_line}" != *"x-release-please-version"* ]]; then
        fail "include/version.h lost its x-release-please-version marker; release-please would stop bumping it silently"
        return
    fi

    header_version="$(sed -n 's/.*VERSION_STRING_ASCII[[:space:]]*"\([^"]*\)".*/\1/p' include/version.h | head -1)"
    file_version="$(tr -d '[:space:]' < version.txt)"
    manifest_version="$(sed -n 's/.*"\."[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' .release-please-manifest.json | head -1)"

    # Unpadded YYYY.M.MICRO only. release-please parses the version as semver and
    # strips leading zeros, so a padded month would stop matching the marker.
    if [[ ! "${header_version}" =~ ^[0-9]{4}\.([1-9]|1[0-2])\.[0-9]+$ ]]; then
        fail "Version is not unpadded CalVer (YYYY.M.MICRO): ${header_version}"
    fi

    if [[ "${header_version}" != "${file_version}" ]] || [[ "${header_version}" != "${manifest_version}" ]]; then
        fail "Version disagreement: include/version.h=${header_version} version.txt=${file_version} manifest=${manifest_version}"
    else
        info "Version ${header_version} agrees across header, version.txt and manifest"
    fi
}

check_release_drivers() {
    local driver missing=0

    info "Checking packaged filesystem drivers against MeridianPkg.dsc"
    while IFS= read -r driver; do
        if ! grep -q "MeridianPkg/filesystems/${driver}.inf" MeridianPkg.dsc; then
            fail "package-release.sh ships ${driver}.efi but MeridianPkg.dsc does not build it"
            missing=$((missing + 1))
        fi
    done < <(sed -n 's/^FS_DRIVERS=(\(.*\))$/\1/p' scripts/package-release.sh | tr ' ' '\n')

    while IFS= read -r driver; do
        if ! grep -q "^FS_DRIVERS=(.*\b${driver}\b.*)" scripts/package-release.sh; then
            fail "MeridianPkg.dsc builds ${driver}.efi but package-release.sh does not ship it"
            missing=$((missing + 1))
        fi
    done < <(sed -n 's|^[[:space:]]*MeridianPkg/filesystems/\([a-z0-9]*\)\.inf$|\1|p' MeridianPkg.dsc)

    if [[ "${missing}" -eq 0 ]]; then
        info "Packaged filesystem drivers match the package definition"
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
check_release_version
check_release_drivers
check_active_workflows
check_no_legacy_makefiles

if [[ "${failures}" -ne 0 ]]; then
    printf '[FAIL] %d failure(s), %d warning(s)\n' "${failures}" "${warnings}" >&2
    exit 1
fi

printf '[OK] repository checks passed with %d warning(s)\n' "${warnings}"
