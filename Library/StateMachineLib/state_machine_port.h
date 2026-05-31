/** @file
  state_machine_port.h - UEFI port for libstatemachines.

  Satisfies the libstatemachines port contract selected by
  -DSTATE_MACHINE_FREESTANDING, mapping it onto EDK2 base types. Placed ahead of
  the library's own ports/ reference header on the include path so the library
  core pulls THIS under EDK2's -nostdinc build instead of <stdint.h>.

  All UEFI knowledge lives here in MeridianPkg; the upstream library names no
  platform.

  SPDX-License-Identifier: GPL-3.0-or-later
  SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
**/

#ifndef STATE_MACHINE_PORT_H
#define STATE_MACHINE_PORT_H

#include <Uefi.h>

typedef UINT8 uint8_t;
typedef UINT16 uint16_t;
typedef UINT32 uint32_t;
typedef UINT64 uint64_t;

#ifndef bool
typedef BOOLEAN bool;
#define true TRUE
#define false FALSE
#endif

#ifndef UINT16_MAX
#define UINT16_MAX MAX_UINT16
#endif
#ifndef offsetof
#define offsetof(type, member) ((UINTN) & (((type *)0)->member))
#endif

#ifndef EINVAL
#define EINVAL 22
#endif
#ifndef EBUSY
#define EBUSY 16
#endif
#ifndef EPROTO
#define EPROTO 71
#endif
#ifndef EOPNOTSUPP
#define EOPNOTSUPP 95
#endif

#endif
