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

#include <Sol2D/Lua/LuaFormApi.h>
#include <Sol2D/Lua/LuaColorApi.h>
#include <Sol2D/Lua/LuaFontApi.h>
#include <Sol2D/Lua/LuaWidgetlApi.h>
#include <Sol2D/Lua/LuaStrings.h>
#include <Sol2D/Lua/Aux/LuaUserData.h>

using namespace Sol2D;
using namespace Sol2D::SDL;
using namespace Sol2D::Forms;
using namespace Sol2D::Lua;
using namespace Sol2D::Lua::Aux;

namespace {

struct Self : LuaSelfBase
{
    Form * form;
    const Workspace * workspace;
};

using UserData = LuaUserData<Self, LuaTypeName::form>;

// 1 self
// 2 text
int luaApi_CreateLabel(lua_State * _lua)
{
    Self * self = UserData::getUserData(_lua, 1);
    const char * text = lua_tostring(_lua, 2);
    Label & label = self->form->createLabel(text ? std::string(text) : std::string());
    pushLabelApi(_lua, label);
    return 1;
}

// 1 self
// 2 text
int luaApi_CreateButton(lua_State * _lua)
{
    Self * self = UserData::getUserData(_lua, 1);
    const char * text = lua_tostring(_lua, 2);
    Button & button = self->form->createButton(text ? std::string(text) : std::string());
    pushButtonApi(_lua, button, *self->workspace);
    return 1;
}

} // namespace

void Sol2D::Lua::pushFormApi(lua_State * _lua, const Workspace & _workspace, Form & _form)
{
    Self * self = UserData::pushUserData(_lua);
    self->form = &_form;
    self->workspace = &_workspace;
    if(UserData::pushMetatable(_lua) == MetatablePushResult::Created)
    {
        luaL_Reg funcs[] = {
            { "__gc", UserData::luaGC },
            { "createLabel", luaApi_CreateLabel },
            { "createButton", luaApi_CreateButton },
            { nullptr, nullptr }
        };
        luaL_setfuncs(_lua, funcs, 0);
    }
    lua_setmetatable(_lua, -2);
}
