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

#include <Sol2D/Lua/LuaSceneApi.h>
#include <Sol2D/Lua/LuaPointApi.h>
#include <Sol2D/Lua/LuaBodyPrototypeApi.h>
#include <Sol2D/Lua/LuaBodyOptionsApi.h>
#include <Sol2D/Lua/LuaBodyShapeOptionsApi.h>
#include <Sol2D/Lua/LuaContactApi.h>
#include <Sol2D/Lua/LuaTileMapObjectApi.h>
#include <Sol2D/Lua/Aux/LuaUserData.h>
#include <Sol2D/Lua/Aux/LuaCallbackStorage.h>
#include <Sol2D/Lua/Aux/LuaWeakRegistryStorage.h>
#include <Sol2D/Lua/Aux/LuaTable.h>
#include <Sol2D/Lua/Aux/LuaScript.h>

using namespace Sol2D;
using namespace Sol2D::Lua;
using namespace Sol2D::Lua::Aux;

namespace {

const char gc_metatable_scene[] = "sol.Scene";
const char gc_message_body_id_expected[] = "a body ID expected";
const char gc_message_shape_key_expected[] = "a shape key expected";
const char gc_message_graphic_key_expected[] = "a graphic key expected";
const char gc_message_subscription_id_expected[] = "a subscriptin ID expected";

struct Self : LuaUserData<Self, gc_metatable_scene>
{
    Scene * scene;
    const Workspace * workspace;
    ContactObserver * contact_observer;
    uint32_t callback_set_id_begin_contact;
    uint32_t callback_set_id_end_contact;
};

class LuaContactObserver : public ContactObserver
{
public:
    LuaContactObserver(
        lua_State * _lua,
        const Workspace & _workspace,
        uint32_t _callback_set_id_begin_contact,
        uint32_t _callback_set_id_end_contact);
    void beginContact(Contact & _contact);
    void endContact(Contact & _contact);

private:
    lua_State * mp_lua;
    const Workspace & mr_workspace;
    uint32_t m_callback_set_id_begin_contact;
    uint32_t m_callback_set_id_end_contact;
};

LuaContactObserver::LuaContactObserver(
    lua_State * _lua,
    const Workspace & _workspace,
    uint32_t _callback_set_id_begin_contact,
    uint32_t _callback_set_id_end_contact
) :
    mp_lua(_lua),
    mr_workspace(_workspace),
    m_callback_set_id_begin_contact(_callback_set_id_begin_contact),
    m_callback_set_id_end_contact(_callback_set_id_end_contact)
{
}

void LuaContactObserver::beginContact(Contact & _contact)
{
    pushContact(mp_lua, _contact);
    LuaCallbackStorage(mp_lua).callSet(mr_workspace, m_callback_set_id_begin_contact, 1);
}

void LuaContactObserver::endContact(Contact & _contact)
{
    pushContact(mp_lua, _contact);
    LuaCallbackStorage(mp_lua).callSet(mr_workspace, m_callback_set_id_end_contact, 1);
}

// 1 self
int luaApi_GC(lua_State * _lua)
{
    Self * self = Self::getUserData(_lua, 1);
    LuaCallbackStorage storage(_lua);
    storage.destroyCallbackSet(self->callback_set_id_begin_contact);
    storage.destroyCallbackSet(self->callback_set_id_end_contact);
    self->scene->removeContactObserver(*self->contact_observer);
    delete self->contact_observer;
    return 0;
}

// 1 self
// 2 file path
int luaApi_LoadTileMap(lua_State * _lua)
{
    Self * self = Self::getUserData(_lua, 1);
    const char * path = lua_tostring(_lua, 2);
    luaL_argcheck(_lua, path != nullptr, 2, "a file path expected");
    bool result = self->scene->loadTileMap(self->workspace->toAbsolutePath(path));
    lua_pushboolean(_lua, result);
    return 1;
}

// 1 self
// 2 object id
int luaApi_GetTileMapObjectById(lua_State * _lua)
{
    Self * self = Self::getUserData(_lua, 1);
    luaL_argcheck(_lua, lua_isinteger(_lua, 2), 2, "object id expected");
    const Tiles::TileMapObject * object = self->scene->getTileMapObjectById(static_cast<uint32_t>(lua_tointeger(_lua, 2)));
    if(object)
        pushTileMapObject(_lua, *object);
    else
        lua_pushnil(_lua);
    return 1;
}

// 1 self
// 2 object name
int luaApi_GetTileMapObjectByName(lua_State * _lua)
{
    Self * self = Self::getUserData(_lua, 1);
    const char * name = lua_tostring(_lua, 2);
    luaL_argcheck(_lua, name, 2, "object name expected");
    const Tiles::TileMapObject * object = self->scene->getTileMapObjectByName(name);
    if(object)
        pushTileMapObject(_lua, *object);
    else
        lua_pushnil(_lua);
    return 1;
}

// 1 self
// 2 position or nil
// 3 body prototype
// 4 script argument (optional)
int luaApi_CreateBody(lua_State * _lua)
{
    bool has_script_argument = lua_gettop(_lua) >= 4;
    Self * self = Self::getUserData(_lua, 1);
    SDL_FPoint position = { .0f, .0f };
    if(!lua_isnil(_lua, 2))
        luaL_argcheck(_lua, tryGetPoint(_lua, 2, position), 2, "body position expected");
    LuaBodyPrototype & lua_proto = getBodyPrototype(_lua, 3);
    uint64_t body_id = self->scene->createBody(position, lua_proto.proto);
    lua_pushinteger(_lua, body_id);

    if(lua_proto.script_path.has_value())
    {
        LuaTable table = LuaTable::pushNew(_lua);
        table.setIntegerValue("bodyId", body_id);
        lua_pushvalue(_lua, 1);
        table.setValueFromTop("scene");
        if(has_script_argument) {
            lua_pushvalue(_lua, 4);
            table.setValueFromTop("arg");
        }
        executeScriptWithContext(_lua, *self->workspace, lua_proto.script_path.value());
    }

    return 1;
}

// 1 self
// 2 class
// 3 body options (optional)
// 4 shape options (optional)
int luaApi_CreateBodiesFromMapObjects(lua_State * _lua)
{
    Self * self = Self::getUserData(_lua, 1);
    const char * class_name = lua_tostring(_lua, 2);
    luaL_argcheck(_lua, class_name != nullptr, 2, "a class name expected");

    int arg_count = lua_gettop(_lua);
    BodyOptions body_options;
    BodyShapeOptions body_shape_options;
    if(arg_count >= 3)
    {
        tryGetBodyOptions(_lua, 3, body_options);
        if(arg_count >= 4)
            tryGetBodyShapeOptions(_lua, 4, body_shape_options);
    }
    self->scene->createBodiesFromMapObjects(class_name, body_options, body_shape_options);
    return 0;
}

// 1 self
// 2 body id
// 3 force vector (point)
int luaApi_ApplyForce(lua_State * _lua)
{
    Self * self = Self::getUserData(_lua, 1);
    luaL_argcheck(_lua, lua_isinteger(_lua, 2), 2, gc_message_body_id_expected);
    uint64_t body_id = static_cast<uint64_t>(lua_tointeger(_lua, 2));
    SDL_FPoint force;
    luaL_argcheck(_lua, tryGetPoint(_lua, 3, force), 3, "a force vector expected");
    self->scene->applyForce(body_id, force);
    return 0;
}

// 1 self
// 2 body id
// 3 position
int luaApi_SetBodyPosition(lua_State * _lua)
{
    Self * self = Self::getUserData(_lua, 1);
    luaL_argcheck(_lua, lua_isinteger(_lua, 2), 2, gc_message_body_id_expected);
    uint64_t body_id = static_cast<uint64_t>(lua_tointeger(_lua, 2));
    SDL_FPoint position;
    luaL_argcheck(_lua, tryGetPoint(_lua, 3, position), 3, "a position expected");
    self->scene->setBodyPosition(body_id, position);
    return 0;
}

// 1 self
// 2 body id
int luaApi_GetBodyPosition(lua_State * _lua)
{
    Self * self = Self::getUserData(_lua, 1);
    luaL_argcheck(_lua, lua_isinteger(_lua, 2), 2, gc_message_body_id_expected);
    uint64_t body_id = static_cast<uint64_t>(lua_tointeger(_lua, 2));
    std::optional<SDL_FPoint> position = self->scene->getBodyPosition(body_id);
    if(position.has_value())
        pushPoint(_lua, position.value().x, position.value().y);
    else
        lua_pushnil(_lua);
    return 1;
}

// 1 self
// 2 body id
int luaApi_SetFollowedBody(lua_State * _lua)
{
    Self * self = Self::getUserData(_lua, 1);
    luaL_argcheck(_lua, lua_isinteger(_lua, 2), 2, gc_message_body_id_expected);
    uint64_t body_id = static_cast<uint64_t>(lua_tointeger(_lua, 2));
    lua_pushboolean(_lua, self->scene->setFollowedBody(body_id));
    return 1;
}

// 1 self
int luaApi_ResetFollowedBody(lua_State * _lua)
{
    Self * self = Self::getUserData(_lua, 1);
    self->scene->resetFollowedBody();
    return 0;
}

// 1 self
// 2 body id
// 3 layer
int luaApi_SetBodyLayer(lua_State * _lua)
{
    Self * self = Self::getUserData(_lua, 1);
    luaL_argcheck(_lua, lua_isinteger(_lua, 2), 2, gc_message_body_id_expected);
    uint64_t body_id = static_cast<uint64_t>(lua_tointeger(_lua, 2));
    const char * layer = lua_tostring(_lua, 3);
    luaL_argcheck(_lua, layer != nullptr, 3, "a layer name expected");
    lua_pushboolean(_lua, self->scene->setBodyLayer(body_id, layer));
    return 1;
}

// 1 self
// 2 body id
// 3 shape key
// 4 graphic key
int luaApi_SetBodyShapeCurrentGraphic(lua_State * _lua)
{
    Self * self = Self::getUserData(_lua, 1);
    luaL_argcheck(_lua, lua_isinteger(_lua, 2), 2, gc_message_body_id_expected);
    uint64_t body_id = static_cast<uint64_t>(lua_tointeger(_lua, 2));
    const char * shape_id = lua_tostring(_lua, 3);
    luaL_argcheck(_lua, shape_id != nullptr, 3, gc_message_shape_key_expected);
    const char * graphic_id = lua_tostring(_lua, 4);
    luaL_argcheck(_lua, graphic_id != nullptr, 4, gc_message_graphic_key_expected);
    lua_pushboolean(_lua, self->scene->setBodyShapeCurrentGraphic(body_id, shape_id, graphic_id));
    return 1;
}

// 1 self
// 2 body id
// 3 shape key
// 4 graphic key
// 5 flip horizontally
// 6 flip vertically
int luaApi_FlipBodyShapeGraphic(lua_State * _lua)
{
    Self * self = Self::getUserData(_lua, 1);
    luaL_argcheck(_lua, lua_isinteger(_lua, 2), 2, gc_message_body_id_expected);
    uint64_t body_id = static_cast<uint64_t>(lua_tointeger(_lua, 2));
    const char * shape_id = lua_tostring(_lua, 3);
    luaL_argcheck(_lua, shape_id != nullptr, 3, gc_message_shape_key_expected);
    const char * graphic_id = lua_tostring(_lua, 4);
    luaL_argcheck(_lua, graphic_id != nullptr, 4, gc_message_graphic_key_expected);
    luaL_argexpected(_lua, lua_isboolean(_lua, 5), 5, "boolean");
    luaL_argexpected(_lua, lua_isboolean(_lua, 6), 6, "boolean");
    bool result = self->scene->flipBodyShapeGraphic(
        body_id,
        shape_id,
        graphic_id,
        lua_toboolean(_lua, 5),
        lua_toboolean(_lua, 6));
    lua_pushboolean(_lua, result);
    return 1;
}

// 1 self
// 2 callback
int luaApi_SubscribeToBeginContact(lua_State * _lua)
{
    Self * self = Self::getUserData(_lua, 1);
    LuaCallbackStorage storage(_lua);
    uint32_t id = storage.addCallback(self->callback_set_id_begin_contact, 2);
    lua_pushinteger(_lua, static_cast<lua_Integer>(id));
    return 1;
}

// 1 self
// 2 subscription ID
int luaApi_UnsubscribeFromBeginContact(lua_State * _lua)
{
    Self * self = Self::getUserData(_lua, 1);
    luaL_argcheck(_lua, lua_isinteger(_lua, 2), 2, gc_message_subscription_id_expected);
    uint32_t subscription_id = static_cast<uint32_t>(lua_tointeger(_lua, 2));
    LuaCallbackStorage storage(_lua);
    storage.removeCallback(self->callback_set_id_begin_contact, subscription_id);
    return 0;
}

// 1 self
// 2 callback
int luaApi_SubscribeToEndContact(lua_State * _lua)
{
    Self * self = Self::getUserData(_lua, 1);
    LuaCallbackStorage storage(_lua);
    uint32_t id = storage.addCallback(self->callback_set_id_end_contact, 2);
    lua_pushinteger(_lua, static_cast<lua_Integer>(id));
    return 1;
}

// 1 self
// 2 subscription ID
int luaApi_UnsubscribeFromEndContact(lua_State * _lua)
{
    Self * self = Self::getUserData(_lua, 1);
    luaL_argcheck(_lua, lua_isinteger(_lua, 2), 2, gc_message_subscription_id_expected);
    uint32_t subscription_id = static_cast<uint32_t>(lua_tointeger(_lua, 2));
    LuaCallbackStorage storage(_lua);
    storage.removeCallback(self->callback_set_id_end_contact, subscription_id);
    return 0;
}

// 1 self
// 2 body id
// 3 destination
// TODO: options
int luaApi_FindPath(lua_State * _lua)
{
    Self * self = Self::getUserData(_lua, 1);
    luaL_argcheck(_lua, lua_isinteger(_lua, 2), 2, gc_message_body_id_expected);
    uint64_t body_id = static_cast<uint64_t>(lua_tointeger(_lua, 2));
    SDL_FPoint destination;
    luaL_argcheck(_lua, tryGetPoint(_lua, 3, destination), 3, "a destination point expected");
    auto result = self->scene->findPath(body_id, destination, false, false);
    if(result.has_value())
    {
        lua_newtable(_lua);
        for(size_t i = 0; i < result.value().size(); ++i)
        {
            pushPoint(_lua, result.value()[i].x, result.value()[i].y);
            lua_rawseti(_lua, -2, i + 1);
        }
    }
    else
    {
        lua_pushnil(_lua);
    }
    return 1;
}

} // namespace

void Sol2D::Lua::pushSceneApi(lua_State * _lua, const Workspace & _workspace, Scene & _scene)
{
    LuaWeakRegistryStorage weak_registry(_lua);
    if(weak_registry.tryGet(&_scene, LUA_TUSERDATA))
        return;

    Self * self = Self::pushUserData(_lua);
    self->scene = &_scene;
    self->workspace = &_workspace;
    self->callback_set_id_begin_contact = LuaCallbackStorage::generateUniqueSetId();
    self->callback_set_id_end_contact = LuaCallbackStorage::generateUniqueSetId();
    self->contact_observer = new LuaContactObserver(
        _lua,
        _workspace,
        self->callback_set_id_begin_contact,
        self->callback_set_id_end_contact);
    self->scene->addContactObserver(*self->contact_observer);
    if(Self::pushMetatable(_lua) == MetatablePushResult::Created)
    {
        luaL_Reg funcs[] = {
            { "__gc", luaApi_GC, },
            { "loadTileMap", luaApi_LoadTileMap },
            { "getTileMapObjectById", luaApi_GetTileMapObjectById },
            { "getTileMapObjectByName", luaApi_GetTileMapObjectByName },
            { "createBody", luaApi_CreateBody },
            { "createBodiesFromMapObjects", luaApi_CreateBodiesFromMapObjects },
            { "applyForce", luaApi_ApplyForce },
            { "setBodyPosition", luaApi_SetBodyPosition },
            { "getBodyPosition", luaApi_GetBodyPosition },
            { "setFollowedBody", luaApi_SetFollowedBody },
            { "resetFollowedBody", luaApi_ResetFollowedBody },
            { "setBodyLayer", luaApi_SetBodyLayer },
            { "setBodyShapeCurrentGraphic", luaApi_SetBodyShapeCurrentGraphic },
            { "flipBodyShapeGraphic", luaApi_FlipBodyShapeGraphic },
            { "subscribeToBeginContact", luaApi_SubscribeToBeginContact },
            { "unsubscribeFromBeginContact", luaApi_UnsubscribeFromBeginContact },
            { "subscribeToEndContact", luaApi_SubscribeToEndContact },
            { "unsubscribeFromEndContact", luaApi_UnsubscribeFromEndContact },
            { "findPath", luaApi_FindPath },
            { nullptr, nullptr }
        };
        luaL_setfuncs(_lua, funcs, 0);
    }
    lua_setmetatable(_lua, -2);

    weak_registry.save(&_scene, -1);
}
