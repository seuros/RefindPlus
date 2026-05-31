// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/PciIo.h>

#include "upstream/lua.h"
#include "upstream/lauxlib.h"
#include "upstream/lualib.h"

LUALIB_API void luaL_openlibs(lua_State *L) {
  static const luaL_Reg libs[] = {
    { LUA_GNAME,      luaopen_base },
    { LUA_TABLIBNAME, luaopen_table },
    { LUA_STRLIBNAME, luaopen_string },
    { LUA_COLIBNAME,  luaopen_coroutine },
    { LUA_MATHLIBNAME, luaopen_math },
    { NULL, NULL }
  };
  for (const luaL_Reg *lib = libs; lib->func; lib++) {
    luaL_requiref(L, lib->name, lib->func, 1);
    lua_pop(L, 1);
  }
}

static int meridian_pci_present(lua_State *L) {
  UINT16 want_vid = (UINT16)luaL_checkinteger(L, 1);
  UINT16 want_did = (UINT16)luaL_checkinteger(L, 2);

  EFI_HANDLE *handles = NULL;
  UINTN count = 0;
  EFI_STATUS st = gBS->LocateHandleBuffer(ByProtocol, &gEfiPciIoProtocolGuid,
                                          NULL, &count, &handles);
  int found = 0;
  if (!EFI_ERROR(st)) {
    for (UINTN i = 0; i < count && !found; i++) {
      EFI_PCI_IO_PROTOCOL *pci = NULL;
      if (EFI_ERROR(gBS->HandleProtocol(handles[i], &gEfiPciIoProtocolGuid, (void **)&pci)))
        continue;
      UINT16 ids[2] = { 0, 0 };
      if (EFI_ERROR(pci->Pci.Read(pci, EfiPciIoWidthUint16, 0, 2, ids)))
        continue;
      if (ids[0] == want_vid && ids[1] == want_did) found = 1;
    }
    if (handles) gBS->FreePool(handles);
  }
  lua_pushboolean(L, found);
  return 1;
}

static int meridian_log(lua_State *L) {
  const char *msg = luaL_checkstring(L, 1);
  AsciiPrint("%a\n", msg);
  return 0;
}

static int meridian_firmware_vendor(lua_State *L) {
  CHAR16 *v = gST->FirmwareVendor;
  char buf[128]; UINTN i = 0;
  if (v) for (; v[i] && i < sizeof(buf) - 1; i++) buf[i] = (char)v[i];
  buf[i] = 0;
  lua_pushstring(L, buf);
  return 1;
}

static const luaL_Reg meridian_funcs[] = {
  { "pci_present",     meridian_pci_present },
  { "log",             meridian_log },
  { "firmware_vendor", meridian_firmware_vendor },
  { NULL, NULL }
};

void MeridianLuaRegister(lua_State *L) {
  luaL_newlib(L, meridian_funcs);
  lua_setglobal(L, "meridian");
}

lua_State *MeridianLuaNewState(void) {
  lua_State *L = luaL_newstate();
  if (!L) return NULL;
  luaL_openlibs(L);
  MeridianLuaRegister(L);
  return L;
}

int MeridianLuaDoString(lua_State *L, const char *chunk, const char *chunkname) {

  int rc = luaL_loadbuffer(L, chunk, AsciiStrLen(chunk), chunkname ? chunkname : "=chunk");
  if (rc == LUA_OK) rc = lua_pcall(L, 0, 0, 0);
  if (rc != LUA_OK) {
    const char *msg = lua_tostring(L, -1);
    if (msg) AsciiPrint("meridian-lua: %a\n", msg);
  }
  return rc;
}
