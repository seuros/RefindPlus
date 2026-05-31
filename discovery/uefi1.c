// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#include "global.h"
#include "fw_strategy.h"

CONST FW_STRATEGY gFwStrategyUefi1 = {
    .Name = L"EFI 1.x", .HasQueryVariableInfo = FALSE, .TrustNvramSizes = FALSE};
