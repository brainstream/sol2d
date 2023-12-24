// Sol2D Game Engine
// Copyright (C) 2023-2024 Sergey Smolyannikov aka brainstream
//
// This program is free software: you can redistribute it and/or modify it under
// the terms of the GNU Lesser General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option) any
// later version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU General Lesser Public License for more
// details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include <Sol2D/Scene.h>
#include <Sol2D/Workspace.h>
#include <lua.hpp>
#include <memory>

namespace Sol2D::Lua {

class LuaCallObject final
{
public:
    LuaCallObject(lua_State * _lua, const std::string & _name);
    ~LuaCallObject();

private:
    lua_State * mp_lua;
    std::string m_key;
};

void luaRegisterLibrary(lua_State * _lua, const Sol2D::Workspace & _workspace, Sol2D::Scene & _scene);

// [-1, +0]
std::unique_ptr<LuaCallObject> luaUseCallObject(lua_State * _lua, const std::string & _key);

} // namespace Sol2D::Lua
