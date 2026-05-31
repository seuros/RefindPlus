// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#ifndef LUA_MERIDIAN_H
#define LUA_MERIDIAN_H

#include "upstream/lua.h"

lua_State *MeridianLuaNewState(void);

void MeridianLuaRegister(lua_State *L);

int MeridianLuaDoString(lua_State *L, const char *chunk, const char *chunkname);

#endif
