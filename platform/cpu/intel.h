// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#ifndef MERIDIAN_CPU_INTEL_H
#define MERIDIAN_CPU_INTEL_H

#include <Uefi.h>

MeridianUarch MeridianIntelUarch(UINT32 Family, UINT32 Model, UINT32 Stepping);

#endif
