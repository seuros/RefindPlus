// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#include "global.h"
#include "fw_strategy.h"

CONST FW_STRATEGY gFwStrategyUefi2 = {
    .Name = L"UEFI 2.x", .HasQueryVariableInfo = TRUE, .TrustNvramSizes = TRUE};
