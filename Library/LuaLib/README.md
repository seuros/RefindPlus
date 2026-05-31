<!--
SPDX-License-Identifier: GPL-3.0-or-later
SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
-->

# LuaLib -- embedded Lua 5.4 for Meridian

A freestanding build of the [Lua 5.4](https://www.lua.org/) interpreter that runs
inside the Meridian UEFI image, for boot-time scripting (config logic, dynamic
menus, driver-bringup policy). It compiles under Meridian's default
**CLANGDWARF** toolchain with **no `StdLib`/edk2-libc dependency** -- the Lua core
runs on a small libc shim mapped to edk2 `BaseLib`.

## Why not edk2-libc's Lua?

edk2-libc ships a Lua 5.2.3 port, but it (a) depends on the full `StdLib`, which
only links under `GCC5` (clang dies on StdLib's `ms_abi` `va_start`), (b) is
shaped as a UEFI Shell application, not an embeddable library. This library is
Lua **5.4.7**, embeddable, and builds on the default clang toolchain.

## Layout

```
LuaLib/
  lua_meridian.{c,h}   Public API + the `meridian` host table
  LuaLib.inf           Library module (LIBRARY_CLASS = LuaLib)
  upstream/            Vendored Lua 5.4.7 core + the libc shim headers
  shim/                libc shim implementations (-> edk2 BaseLib / console)
  test/                LuaTest.efi -- standalone smoke test
```

The shim headers live in `upstream/` (next to the Lua sources) because edk2
adds each source file's directory to the include path; under `-nostdinc` that
is how `<string.h>`, `<math.h>`, etc. resolve to the shim instead of a host libc.

## What the shim provides

| Area | Backed by |
|------|-----------|
| `mem*`, alloc | edk2 `BaseMemoryLib` + size-prefixed `AllocatePool` pool |
| `setjmp`/`longjmp` | `BaseLib` `SetJump`/`LongJump` (`returns_twice`/`noreturn`) |
| `printf`/`snprintf` (incl. `%f`/`%e`/`%g`) | `shim/mlua_printf.c` (edk2 print has no float) |
| `strtod`, `strto*` | `shim/mlua_strtod.c` (decimal; hex floats use Lua's own parser) |
| `floor`/`pow`/`sin`/`log`/… | `shim/mlua_math.c` (hand-rolled; no libm) |
| console (`stdout`/`stderr`) | `gST->ConOut` |

## Standard libraries

Enabled (sandbox-safe): **base, table, string, coroutine, math**.
Excluded by design: **io, os, package/loadlib, debug, utf8** -- a boot-time
`.lua` cannot reach the host beyond the vetted `meridian` table.

## Host API (`meridian.*`)

| Function | Description |
|----------|-------------|
| `meridian.pci_present(vendor, device)` | scan `EFI_PCI_IO_PROTOCOL` handles for a VID/DID |
| `meridian.log(msg)` | write a line to the firmware console |
| `meridian.firmware_vendor()` | `EFI_SYSTEM_TABLE.FirmwareVendor` string |

## Embedding

```c
#include <Library/LuaLib/lua_meridian.h>

lua_State *L = MeridianLuaNewState();              // safe libs + meridian table
MeridianLuaDoString(L, "if meridian.pci_present(0x168c,0x0042) then "
                       "  meridian.log('Atheros present') end", "=policy");
lua_close(L);
```

## Building / testing

```sh
# library only
build -p MeridianPkg/MeridianPkg.dsc -m MeridianPkg/Library/LuaLib/LuaLib.inf -a X64 -t CLANGDWARF
# smoke test app (run under OVMF as \EFI\BOOT\BOOTX64.EFI)
build -p MeridianPkg/MeridianPkg.dsc -m MeridianPkg/Library/LuaLib/test/LuaTest.inf -a X64 -t CLANGDWARF
```

## Known limitations

- **Math is hand-rolled, not libm-grade.** `sin`/`cos`/`exp`/`log`/`pow` use
  range-reduced series -- fine for boot logic, not for numerically sensitive
  work. Drop in a vetted libm if that changes.
- **No filesystem.** `luaL_loadfile`/`dofile` are stubbed and link-stripped;
  load scripts via `MeridianLuaDoString` (host reads the ESP and passes the
  buffer).
- The shim is intentionally minimal -- it covers what the Lua 5.4 core + the
  enabled libraries reference, not all of C99.
