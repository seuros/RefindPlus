// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#ifndef __FW_STRATEGY_H_
#define __FW_STRATEGY_H_

typedef struct _FW_STRATEGY
{

    CONST CHAR16 *Name;

    BOOLEAN HasQueryVariableInfo;

    BOOLEAN TrustNvramSizes;
} FW_STRATEGY;

extern CONST FW_STRATEGY gFwStrategyUefi1;
extern CONST FW_STRATEGY gFwStrategyUefi2;
extern CONST FW_STRATEGY gFwStrategyApple;
extern CONST FW_STRATEGY gFwStrategyAppleLegacy;

extern CONST FW_STRATEGY *gFwStrategy;

VOID SelectFwStrategy(VOID);

#endif
