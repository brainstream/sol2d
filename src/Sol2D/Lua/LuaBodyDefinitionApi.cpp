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

#include <Sol2D/Lua/LuaBodyDefinitionApi.h>
#include <Sol2D/Lua/LuaRectApi.h>
#include <Sol2D/Lua/LuaPointApi.h>
#include <Sol2D/Lua/LuaGraphicsPackApi.h>
#include <Sol2D/Lua/Aux/LuaTable.h>

using namespace Sol2D;
using namespace Sol2D::Lua;
using namespace Sol2D::Lua::Aux;

namespace {


template<BodyShapeType shape_type>
void readBasicShape(LuaTable & _table, BodyBasicShapeDefinition<shape_type> & _shape);

bool tryGetPoints(lua_State * _lua, int _idx, std::vector<Point> & _points);

void addPolygon(
    LuaTable & _table,
    const std::string & _key,
    std::unordered_map<std::string, BodyVariantShapeDefinition> & _shapes);

void addCircle(
    LuaTable & _table,
    const std::string & _key,
    std::unordered_map<std::string, BodyVariantShapeDefinition> & _shapes);

void addShape(
    lua_State * _lua,
    int _idx,
    const std::string & _key,
    std::unordered_map<std::string, BodyVariantShapeDefinition> & _shapes);

void getShapes(lua_State * _lua, int _idx, std::unordered_map<std::string, BodyVariantShapeDefinition> & _shapes);

void addGraphics(
    lua_State * _lua,
    int _idx,
    const std::string & _key,
    std::unordered_map<std::string, BodyShapeGraphics> & _graphics);


void getShapes(lua_State * _lua, int _idx, std::unordered_map<std::string, BodyVariantShapeDefinition> & _shapes)
{
    int dictionary_index = lua_absindex(_lua, _idx);
    if(!lua_istable(_lua, dictionary_index))
        return;
    lua_pushnil(_lua);
    while(lua_next(_lua, dictionary_index))
    {
        if(lua_isstring(_lua, -2))
            addShape(_lua, -1, lua_tostring(_lua, -2), _shapes);
        lua_pop(_lua, 1);
    }
}

void addShape(
    lua_State * _lua,
    int _idx,
    const std::string & _key,
    std::unordered_map<std::string, BodyVariantShapeDefinition> & _shapes)
{
    if(!lua_istable(_lua, _idx))
        return;
    LuaTable table(_lua, _idx);
    lua_Integer value;
    if(!table.tryGetInteger("type", &value))
        return;
    switch(value)
    {
    case static_cast<lua_Integer>(BodyShapeType::Polygon):
        addPolygon(table, _key, _shapes);
        break;
    case static_cast<lua_Integer>(BodyShapeType::Circle):
        addCircle(table, _key, _shapes);
        break;
    default:
        break;
    }
}

void addPolygon(
    LuaTable & _table,
    const std::string & _key,
    std::unordered_map<std::string, BodyVariantShapeDefinition> & _shapes)
{
    if(!_table.tryGetValue("points"))
        return;
    do {
        {
            BodyRectDefinition def;
            if(tryGetRect(_table.getLua(), -1, def))
            {
                readBasicShape(_table, def);
                _shapes.insert(std::make_pair(_key, def));
                break;
            }
        }
        {
            BodyPolygonDefinition def;
            if(tryGetPoints(_table.getLua(), -1, def.points))
            {
                readBasicShape(_table, def);
                _shapes.insert(std::make_pair(_key, def));
                break;
            }
        }
    } while(false);
    lua_pop(_table.getLua(), 1);
}

template<BodyShapeType shape_type>
void readBasicShape(LuaTable & _table, BodyBasicShapeDefinition<shape_type> & _shape)
{
    _table.tryGetBoolean("isSensor", &_shape.is_sensor);
    _table.tryGetBoolean("isPreSolveEnabled", &_shape.is_pre_solve_enabled);
    if(_table.tryGetValue("graphics"))
    {
        int graphics_table_idx = lua_gettop(_table.getLua());
        lua_pushnil(_table.getLua());
        while(lua_next(_table.getLua(), graphics_table_idx))
        {
            if(lua_isstring(_table.getLua(), -2))
                addGraphics(_table.getLua(), -1, lua_tostring(_table.getLua(), -2), _shape.graphics);
            lua_pop(_table.getLua(), 1);
        }
        lua_pop(_table.getLua(), 1);
    }
}

void addGraphics(
    lua_State * _lua,
    int _idx,
    const std::string & _key,
    std::unordered_map<std::string, BodyShapeGraphics> & _graphics)
{
    if(!lua_istable(_lua, _idx))
        return;
    LuaTable table(_lua, _idx);
    BodyShapeGraphics graphics;
    if(table.tryGetValue("graphics"))
    {
        graphics.graphics = tryGetGraphicsPack(_lua, -1);
        if(!graphics.graphics)
        {
            lua_pop(_lua, 1);
            return;
        }
        if(table.tryGetValue("position"))
        {
            tryGetPoint(_lua, -1, graphics.position);
            lua_pop(_lua, 1);
        }
        {
            bool flipped;
            if(table.tryGetBoolean("isFlippedHorizontally", &flipped))
                graphics.setFilippedHorizontally(flipped);
            if(table.tryGetBoolean("isFlippedVertically", &flipped))
                graphics.setFilippedVertically(flipped);
        }
        lua_pop(_lua, 1);
        _graphics.insert(std::make_pair(_key, graphics));
    }
}

bool tryGetPoints(lua_State * _lua, int _idx, std::vector<Point> & _points)
{
    if(!lua_istable(_lua, _idx))
        return false;
    Point point;
    lua_Unsigned len = lua_rawlen(_lua, _idx);
    for(lua_Unsigned i = 1; i <= len; ++i)
    {
        if(lua_rawgeti(_lua, _idx, i) == LUA_TTABLE)
        {
            if(Lua::tryGetPoint(_lua, -1, point))
                _points.push_back(point);
        }
        lua_pop(_lua, 1);
    }
    return true;
}

void addCircle(
    LuaTable & _table,
    const std::string & _key,
    std::unordered_map<std::string, BodyVariantShapeDefinition> & _shapes)
{
    BodyCircleDefinition def;
    {
        lua_Number radius;
        if(!_table.tryGetNumber("radius", &radius))
            return;
        def.radius = static_cast<float>(radius);
    }
    {
        if(!_table.tryGetValue("center"))
            return;
        bool success = tryGetPoint(_table.getLua(), -1, def.center);
        lua_pop(_table.getLua(), 1);
        if(!success)
            return;
    }
    readBasicShape(_table, def);
    _shapes.insert(std::make_pair(_key, def));
}


} // namespace

std::unique_ptr<BodyDefinition> Sol2D::Lua::tryGetBodyDefinition(lua_State * _lua, int _idx)
{
    if(!lua_istable(_lua, _idx))
        return nullptr;
    std::unique_ptr<BodyDefinition> def = std::make_unique<BodyDefinition>();
    LuaTable table(_lua, _idx);
    {
        lua_Integer value;
        if(!table.tryGetInteger("type", &value))
            return nullptr;
        switch(value)
        {
        case static_cast<lua_Integer>(BodyType::Static):
            def->type = BodyType::Static;
            break;
        case static_cast<lua_Integer>(BodyType::Dynamic):
            def->type = BodyType::Dynamic;
            break;
        case static_cast<lua_Integer>(BodyType::Kinematic):
            def->type = BodyType::Kinematic;
            break;
        default:
            return nullptr;
        }
    }
    {
        if(table.tryGetValue("shapes"))
        {
            getShapes(_lua, -1, def->shapes);
            lua_pop(_lua, 1);
        }
    }
    {
        std::string script;
        if(table.tryGetString("script", script))
            def->script = script;
    }
    return def;
}
