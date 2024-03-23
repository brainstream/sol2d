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
#include <Sol2D/Lua/LuaForward.h>

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

void pushBodyPrototypeApi(lua_State * _lua, BodyPrototype & _body_prototype);

LuaBodyPrototype & getBodyPrototype(lua_State * _lua, int _idx);

} // namespace Sol2D::Lua
