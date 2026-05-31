#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT_DIR}"

if ! command -v git-clang-format >/dev/null 2>&1 && ! git clang-format -h >/dev/null 2>&1; then
    printf '[WARN] git clang-format is not installed; skipping clang-format check\n' >&2
    exit 0
fi

diff_output="$(git clang-format --diff --staged --extensions c,h -- 2>/dev/null || true)"

if printf '%s\n' "${diff_output}" | rg -q '^diff --git '; then
    printf '%s\n' "${diff_output}" >&2
    printf '[FAIL] clang-format would change staged C/H lines. Run: git clang-format --staged --extensions c,h\n' >&2
    exit 1
fi

printf '[INFO] clang-format staged C/H check passed\n'
