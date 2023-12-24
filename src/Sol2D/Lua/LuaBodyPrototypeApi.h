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

#include <Sol2D/BodyPrototype.h>
#include <Sol2D/Workspace.h>
#include <lua.hpp>
#include <filesystem>
#include <optional>

namespace Sol2D::Lua {

struct LuaBodyPrototype
{
    LuaBodyPrototype(BodyPrototype & _proto) :
        proto(_proto)
    {
    }

    BodyPrototype & proto;
    std::optional<std::filesystem::path> script_path;
};

void luaPushBodyPrototypeApiOntoStack(lua_State * _lua, const Sol2D::Workspace & _workspace, Sol2D::BodyPrototype & _proto);

bool luaTryGetBodyPrototype(lua_State * _lua, int _idx, LuaBodyPrototype ** _lproto);

} // namespace Sol2D::Lua
