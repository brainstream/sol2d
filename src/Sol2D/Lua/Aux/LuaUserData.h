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

#include <lua.hpp>
#include <Sol2D/Lua/Aux/LuaMetatable.h>
#include <cstring>
#include <new>
#include <concepts>

namespace Sol2D::Lua::Aux {

struct LuaSelfBase;

namespace __Private {

struct LuaUserDataBase
{
private:
    LuaUserDataBase() {} // Static struct must not be instantiated

protected:
    static void beforeDestroyLuaSelf(lua_State * _lua, LuaSelfBase & _lua_self);
};

} // namespace __Private

struct LuaSelfBase
{
    friend struct __Private::LuaUserDataBase;

public:
    virtual ~LuaSelfBase() { }

protected:
    virtual void beforeDestruction(lua_State * /*_lua*/) { }
};

inline void __Private::LuaUserDataBase::beforeDestroyLuaSelf(lua_State * _lua, LuaSelfBase & _lua_self)
{
    _lua_self.beforeDestruction(_lua);
}

template<typename T>
concept LuaSelfConcept = std::derived_from<T, LuaSelfBase>;

template<LuaSelfConcept LuaSelf, const char metatable[]>
struct LuaUserData : __Private::LuaUserDataBase
{
    template<typename ...CtorArgs>
    static LuaSelf * pushUserData(lua_State * _lua, CtorArgs ... _ctor_args)
    {
        void * data = lua_newuserdata(_lua, sizeof(LuaSelf));
        std::memset(data, 0, sizeof(LuaSelf));
        return new(data) LuaSelf(_ctor_args...);
    }

    static MetatablePushResult pushMetatable(lua_State * _lua)
    {
        return Sol2D::Lua::Aux::pushMetatable(_lua, metatable);
    }

    static LuaSelf * getUserData(lua_State * _lua, int _idx)
    {
        return static_cast<LuaSelf *>(luaL_checkudata(_lua, _idx, metatable));
    }

    static int luaGC(lua_State * _lua)
    {
        LuaSelf * udata = getUserData(_lua, 1);
        beforeDestroyLuaSelf(_lua, *udata);
        udata->~LuaSelf();
        return 0;
    }
};

} // namespace Sol2D::Lua::Aux
