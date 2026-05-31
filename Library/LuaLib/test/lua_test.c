// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#include <Uefi.h>
#include <Library/UefiLib.h>

#include "lua_meridian.h"

EFI_STATUS EFIAPI UefiMain(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
  (void)ImageHandle; (void)SystemTable;

  AsciiPrint("MERIDIAN_LUA_TEST_BEGIN\n");

  lua_State *L = MeridianLuaNewState();
  if (!L) { AsciiPrint("newstate FAILED\n"); return EFI_OUT_OF_RESOURCES; }

  static const char *script =
    "print('hello from embedded Lua '.._VERSION)\n"
    "print('6*7 = '..(6*7))\n"
    "print('2^10 = '..(2^10))\n"
    "print('10 // 3 = '..(10 // 3)..', 10 % 3 = '..(10 % 3))\n"
    "local t = {} for i=1,5 do t[i] = i*i end\n"
    "print('squares = '..table.concat(t, ','))\n"
    "print('upper = '..string.upper('meridian'))\n"
    "print('format = '..string.format('%d/%0.3f/%x', 42, 3.14159, 255))\n"
    "print('math.floor(pi*100) = '..math.floor(math.pi*100))\n"
    "print('math.sqrt(2) = '..string.format('%0.5f', math.sqrt(2)))\n"
    "print('math.sin(pi/6) = '..string.format('%0.5f', math.sin(math.pi/6)))\n"
    "print('math.max = '..math.max(3, 9, 2, 7))\n"
    "meridian.log('host binding: meridian.log() works')\n"
    "print('firmware = '..meridian.firmware_vendor())\n"
    "print('pci 8086:1234 present = '..tostring(meridian.pci_present(0x8086, 0x1234)))\n";

  int rc = MeridianLuaDoString(L, script, "=luatest");
  lua_close(L);

  AsciiPrint("MERIDIAN_LUA_TEST_END rc=%d\n", rc);
  return EFI_SUCCESS;
}
