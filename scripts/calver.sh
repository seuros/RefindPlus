#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT_DIR}"

DO_COMMIT=0

usage() {
    cat <<'USAGE'
Usage: scripts/calver.sh [--commit]

Prints the next CalVer version, YYYY.M.MICRO with no zero padding. MICRO
restarts at 0 in a new month and increments within the current one.

release-please has no CalVer strategy, and the config-file mode we need for
extra-files ignores the action's release-as input. A Release-As commit footer
is the one channel that reaches it, and it applies exactly once.

Options:
  --commit    Create an empty commit carrying the Release-As footer
  -h, --help  Show this help
USAGE
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --commit)  DO_COMMIT=1; shift ;;
        -h|--help) usage; exit 0 ;;
        *)
            printf 'Unknown option: %s\n\n' "$1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

current="$(tr -d '[:space:]' < version.txt)"
year="$(date +%Y)"
month="$(date +%-m)"

micro=0
if [[ "${current}" =~ ^([0-9]+)\.([0-9]+)\.([0-9]+)$ ]]; then
    if [[ "${BASH_REMATCH[1]}" == "${year}" && "${BASH_REMATCH[2]}" == "${month}" ]]; then
        micro=$((BASH_REMATCH[3] + 1))
    fi
else
    printf 'version.txt does not hold a bare YYYY.M.MICRO version: %s\n' "${current}" >&2
    exit 1
fi

next="${year}.${month}.${micro}"

if [[ "${DO_COMMIT}" -eq 0 ]]; then
    printf '%s\n' "${next}"
    exit 0
fi

if ! git diff --quiet || ! git diff --cached --quiet; then
    printf 'Working tree is dirty. Commit or stash before pinning a release.\n' >&2
    exit 1
fi

git commit --allow-empty -m "chore: release ${next}" -m "Release-As: ${next}"
printf '[OK] pinned %s. Push to master to open the release PR.\n' "${next}"
