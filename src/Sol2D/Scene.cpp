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

#include <Sol2D/Scene.h>
#include <Sol2D/Tiles/Tmx.h>
#include <Sol2D/Utils/Observable.h>
#include <Sol2D/SDL/SDL.h>
#include <Sol2D/AStar.h>
#include <box2d/box2d.h>

using namespace Sol2D;
using namespace Sol2D::SDL;
using namespace Sol2D::Tiles;
using namespace Sol2D::Utils;

namespace {

class BodyShape final
{
    S2_DISABLE_COPY_AND_MOVE(BodyShape)

public:
    BodyShape(const std::string & _key, std::optional<uint32_t> _tile_map_object_id);
    ~BodyShape();
    const std::string & getKey() const;
    const std::optional<uint32_t> getTileMapObjectId() const;
    void addGraphic(const std::string & _key, const BodyShapeGraphics & _graphic);
    bool setCurrentGraphic(const std::string & _key);
    BodyShapeGraphics * getCurrentGraphics();
    bool flipGraphic(const std::string & _key, bool _flip_horizontally, bool _flip_vertically);

private:
    const std::string m_key;
    const std::optional<uint32_t> m_tile_map_object_id;
    std::unordered_map<std::string, BodyShapeGraphics *> m_graphics;
    BodyShapeGraphics * mp_current_graphic;
};

inline BodyShape::BodyShape(const std::string & _key, std::optional<uint32_t> _tile_map_object_id) :
    m_key(_key),
    m_tile_map_object_id(_tile_map_object_id),
    mp_current_graphic(nullptr)
{
}

inline BodyShape::~BodyShape()
{
    for(auto & pair : m_graphics)
        delete pair.second;
}

inline const std::string & BodyShape::getKey() const
{
    return m_key;
}

inline const std::optional<uint32_t> BodyShape::getTileMapObjectId() const
{
    return m_tile_map_object_id;
}

inline void BodyShape::addGraphic(const std::string & _key, const BodyShapeGraphics & _graphic)
{
    auto it = m_graphics.find(_key);
    if(it != m_graphics.end())
        delete it->second;
    m_graphics[_key] = new BodyShapeGraphics(_graphic);
}

inline bool BodyShape::setCurrentGraphic(const std::string & _key)
{
    auto it = m_graphics.find(_key);
    if(it == m_graphics.end())
        return false;
    mp_current_graphic = it->second;
    return true;
}

inline BodyShapeGraphics * BodyShape::getCurrentGraphics()
{
    return mp_current_graphic;
}

inline bool BodyShape::flipGraphic(const std::string & _key, bool _flip_horizontally, bool _flip_vertically)
{
    auto it = m_graphics.find(_key);
    if(it == m_graphics.end())
        return false;
    it->second->options.is_flipped_horizontally = _flip_horizontally;
    it->second->options.is_flipped_vertically = _flip_vertically;
    return true;
}

class Body final
{
    S2_DISABLE_COPY_AND_MOVE(Body)

public:
    Body();
    ~Body();
    uint64_t getId() const;
    void setLayer(const std::string & _layer);
    const std::optional<std::string> & getLayer() const;
    BodyShape & createShape(
        const std::string & _key,
        std::optional<uint32_t> _tile_map_object_id = std::nullopt);
    BodyShape * findShape(const std::string & _key);

private:
    static uint64_t s_next_id;
    uint64_t m_id;
    std::optional<std::string> m_layer;
    std::unordered_multimap<std::string, BodyShape *> m_shapes;
};

uint64_t Body::s_next_id = 1;

inline Body::Body() :
    m_id(s_next_id++)
{
}

inline Body::~Body()
{
    for(auto & shape : m_shapes)
        delete shape.second;
}

inline uint64_t Body::getId() const
{
    return m_id;
}

inline void Body::setLayer(const std::string & _layer)
{
    m_layer = _layer;
}

inline const std::optional<std::string> & Body::getLayer() const
{
    return m_layer;
}

inline BodyShape & Body::createShape(
    const std::string & _key,
    std::optional<uint32_t> _tile_map_object_id /*= std::nullopt*/)
{
    BodyShape * shape = new BodyShape(_key, _tile_map_object_id);
    m_shapes.insert(std::make_pair(_key, shape));
    return *shape;
}

inline BodyShape * Body::findShape(const std::string & _key)
{
    auto it = m_shapes.find(_key);
    return it == m_shapes.end() ? nullptr : it->second;
}

inline Body * getUserData(b2BodyId _body_id)
{
    return static_cast<Body *>(b2Body_GetUserData(_body_id));
}

inline BodyShape * getUserData(b2ShapeId _shape_id)
{
    return static_cast<BodyShape *>(b2Shape_GetUserData(_shape_id));
}

} // namespace

Scene::Scene(const SceneOptions & _options, const Workspace & _workspace, SDL_Renderer & _renderer) :
    mr_workspace(_workspace),
    mr_renderer(_renderer),
    m_world_offset{.0f, .0f},
    m_meters_per_pixel(_options.meters_per_pixel),
    m_followed_body_id(b2_nullBodyId),
    mp_box2d_debug_draw(nullptr)
{
    if(m_meters_per_pixel <= .0f)
        m_meters_per_pixel = SceneOptions::default_meters_per_pixel;
    b2WorldDef world_def = b2DefaultWorldDef();
    world_def.gravity = _options.gravity;
    m_b2_world_id = b2CreateWorld(&world_def);
    b2World_SetPreSolveCallback(m_b2_world_id, &Scene::box2dPreSolveContact, this);
    if(_workspace.isDebugRenderingEnabled())
    {
        mp_box2d_debug_draw = new Box2dDebugDraw(
            mr_renderer,
            m_b2_world_id,
            [this](float __x, float __y) {
                return toAbsoluteCoords(physicalToGraphical(__x), physicalToGraphical(__y));
            },
            [this](float __len) {
                return physicalToGraphical(__len);
            }
        );
    }
}

Scene::~Scene()
{
    deinitializeTileMap();
    b2DestroyWorld(m_b2_world_id);
    delete mp_box2d_debug_draw;
}

void Scene::deinitializeTileMap()
{
    for(auto & pair : m_bodies)
        destroyBody(pair.second);
    m_bodies.clear();
    m_tile_heap_ptr.reset();
    m_object_heap_ptr.reset();
    m_tile_map_ptr.reset();
    m_followed_body_id = b2_nullBodyId;
}

void Scene::setGravity(const Point & _vector)
{
    m_defers.push_front([this, _vector]() {
        b2World_SetGravity(m_b2_world_id, *_vector.toBox2DPtr()); // TODO: scale factor?
    });
}

uint64_t Scene::createBody(const Point & _position, const BodyPrototype & _prototype)
{
    Body * body = new Body;
    b2BodyDef b2_body_def = b2DefaultBodyDef();
    b2_body_def.type = mapBodyType(_prototype.getType());
    b2_body_def.position = { .x = _position.x, .y = _position.y };
    b2_body_def.linearDamping = 100; // TODO: for top-down
    b2_body_def.angularDamping = 100; // TODO: must be controlled by user (prevent infinite rotation)
    b2_body_def.fixedRotation = true; // TODO: must be controlled by user
    b2BodyId b2_body_id = b2CreateBody(m_b2_world_id, &b2_body_def);
    b2Body_SetUserData(b2_body_id, body);
    m_bodies.insert(std::make_pair(body->getId(), b2_body_id));

    _prototype.forEachShape([this, body, b2_body_id, &_prototype](
        const std::string & __key,
        const BodyShapePrototype & __shape_proto)
    {
        BodyShape * body_shape = nullptr;

        b2ShapeDef b2_shape_def = b2DefaultShapeDef();
        if(_prototype.getType() == BodyType::Dynamic) // TODO: duplicated
        {
            b2_shape_def.density = .002f; // TODO: real value from user
        }
        b2_shape_def.isSensor = __shape_proto.isSensor();
        b2_shape_def.enablePreSolveEvents = __shape_proto.isPreSolveEnabled();

        switch(__shape_proto.getType())
        {
        case BodyShapeType::Polygon:
        {
            const BodyPolygonShapePrototype * polygon_proto =
                dynamic_cast<const BodyPolygonShapePrototype *>(&__shape_proto);
            if(!polygon_proto)
                break;
            const std::vector<Point> & points = polygon_proto->getPoints();
            if(points.size() < 3 || points.size() > b2_maxPolygonVertices)
                break;
            std::vector<b2Vec2> shape_points(points.size());
            for(size_t i = 0; i < points.size(); ++i)
            {
                shape_points[i].x = graphicalToPhysical(points[i].x);
                shape_points[i].y = graphicalToPhysical(points[i].y);
            }
            b2Hull b2_hull = b2ComputeHull(shape_points.data(), shape_points.size());
            b2Polygon b2_polygon = b2MakePolygon(&b2_hull, .0f);
            b2ShapeId b2_shape_id = b2CreatePolygonShape(b2_body_id, &b2_shape_def, &b2_polygon);
            body_shape = &body->createShape(__key);
            b2Shape_SetUserData(b2_shape_id, body_shape);
        }
        break;
        case BodyShapeType::Circle:
        {
            const BodyCircleShapePrototype * circle_proto =
                dynamic_cast<const BodyCircleShapePrototype *>(&__shape_proto);
            if(!circle_proto)
                break;
            Point position = circle_proto->getCenter();
            b2Circle b2_circle
            {
                .center = { .x = graphicalToPhysical(position.x), .y = graphicalToPhysical(position.y) },
                .radius = graphicalToPhysical(circle_proto->getRadius())
            };
            if(b2_circle.radius <= .0f)
                break;
            b2ShapeId b2_shape_id = b2CreateCircleShape(b2_body_id, &b2_shape_def, &b2_circle);
            body_shape = &body->createShape(__key);
            b2Shape_SetUserData(b2_shape_id, body_shape);
        }
        break;
        default: return;
        }
        __shape_proto.forEachGraphic(
            [body_shape](const std::string & __key, const BodyShapeGraphics & __graphic) {
                body_shape->addGraphic(__key, __graphic);
            });
    });

    return body->getId();
}

void Scene::createBodiesFromMapObjects(
    const std::string & _class,
    const BodyOptions & _body_options,
    const BodyShapeOptions & _shape_options)
{
    b2BodyType body_type = mapBodyType(_body_options.type);

    m_object_heap_ptr->forEachObject([&](const TileMapObject & __map_object) {
        if(__map_object.getClass() != _class) return;
        Body * body = new Body;
        b2BodyDef b2_body_def = b2DefaultBodyDef();
        b2_body_def.type = body_type;
        b2_body_def.position =
        {
            .x = graphicalToPhysical(__map_object.getX()),
            .y = graphicalToPhysical(__map_object.getY())
        };
        b2_body_def.linearDamping = _body_options.linear_damping;
        b2_body_def.angularDamping = _body_options.angular_damping;
        b2_body_def.fixedRotation = _body_options.fixed_rotation;
        b2_body_def.userData = body;
        b2BodyId b2_body_id = b2CreateBody(m_b2_world_id, &b2_body_def);
        m_bodies.insert(std::make_pair(body->getId(), b2_body_id));

        b2ShapeDef b2_shape_def = b2DefaultShapeDef();
        b2_shape_def.isSensor = _shape_options.is_sensor;
        b2_shape_def.enablePreSolveEvents = _shape_options.is_pre_solve_enalbed;
        if(_body_options.type == BodyType::Dynamic)
            b2_shape_def.density = _shape_options.density;

        switch(__map_object.getObjectType())
        {
        case TileMapObjectType::Polygon:
        {
            const TileMapPolygon * polygon = static_cast<const TileMapPolygon *>(&__map_object);
            const std::vector<Point> & points = polygon->getPoints();
            if(points.size() < 3 || points.size() > b2_maxPolygonVertices)
                break;
            std::vector<b2Vec2> shape_points(points.size());
            for(size_t i = 0; i < points.size(); ++i)
            {
                shape_points[i].x = graphicalToPhysical(points[i].x);
                shape_points[i].y = graphicalToPhysical(points[i].y);
            }
            b2Hull b2_hull = b2ComputeHull(shape_points.data(), shape_points.size());
            b2Polygon b2_polygon = b2MakePolygon(&b2_hull, .0f);
            b2ShapeId b2_shape_id = b2CreatePolygonShape(b2_body_id, &b2_shape_def, &b2_polygon);
            BodyShape * body_shape = &body->createShape(_class, polygon->getId());
            b2Shape_SetUserData(b2_shape_id, body_shape);
        }
        break;
        case TileMapObjectType::Circle:
        {
            const TileMapCircle * circle = static_cast<const TileMapCircle *>(&__map_object);
            const float radius = graphicalToPhysical(circle->getRadius());
            if(radius <= .0f)
                break;
            b2Circle b2_circle
            {
                .center = { .x = .0f, .y = .0f },
                .radius = radius
            };
            b2ShapeId b2_shape_id = b2CreateCircleShape(b2_body_id, &b2_shape_def, &b2_circle);
            BodyShape * body_shape = &body->createShape(_class, circle->getId());
            b2Shape_SetUserData(b2_shape_id, body_shape);
        }
        break;
        default: return;
        }
    });
}

bool Scene::destroyBody(uint64_t _body_id)
{
    b2BodyId b2_body_id = findBody(_body_id);
    if(B2_IS_NULL(b2_body_id))
        return false;
    if(B2_ID_EQUALS(m_followed_body_id, b2_body_id))
        m_followed_body_id = b2_nullBodyId;
    m_bodies.erase(_body_id);
    destroyBody(b2_body_id);
    return true;
}

void Scene::destroyBody(b2BodyId _body_id)
{
    Body * body = getUserData(_body_id);
    b2DestroyBody(_body_id);
    delete body;
}

b2BodyId Scene::findBody(uint64_t _body_id) const
{
    auto it = m_bodies.find(_body_id);
    return it == m_bodies.end() ? b2_nullBodyId : it->second;
}

b2BodyType Scene::mapBodyType(BodyType _type)
{
    switch(_type)
    {
    case BodyType::Dynamic:
        return b2_dynamicBody;
    case BodyType::Kinematic:
        return b2_kinematicBody;
    default:
        return b2_staticBody;
    }
}

bool Scene::setFollowedBody(uint64_t _body_id)
{
    m_followed_body_id = findBody(_body_id);
    return B2_IS_NON_NULL(m_followed_body_id);
}

void Scene::resetFollowedBody()
{
    m_followed_body_id = b2_nullBodyId;
}

bool Scene::setBodyLayer(uint64_t _body_id, const std::string & _layer)
{
    b2BodyId b2_body_id = findBody(_body_id);
    if(B2_IS_NULL(b2_body_id))
        return false;
    getUserData(b2_body_id)->setLayer(_layer);
    return true;
}

bool Scene::setBodyShapeCurrentGraphic(
    uint64_t _body_id,
    const std::string & _shape_key,
    const std::string & _graphic_key)
{
    b2BodyId b2_body_id = findBody(_body_id);
    if(B2_IS_NULL(b2_body_id))
        return false;
    BodyShape * shape = getUserData(b2_body_id)->findShape(_shape_key);
    if(shape == nullptr)
        return false;
    return shape->setCurrentGraphic(_graphic_key);
}

bool Scene::flipBodyShapeGraphic(
    uint64_t _body_id,
    const std::string & _shape_key,
    const std::string & _graphic_key,
    bool _flip_horizontally,
    bool _flip_vertically)
{
    b2BodyId b2_body_id = findBody(_body_id);
    if(B2_IS_NULL(b2_body_id))
        return false;
    BodyShape * shape = getUserData(b2_body_id)->findShape(_shape_key);
    if(shape == nullptr)
        return false;
    return shape->flipGraphic(_graphic_key, _flip_horizontally, _flip_vertically);
}

bool Scene::loadTileMap(const std::filesystem::path & _file_path)
{
    deinitializeTileMap();
    m_tile_heap_ptr.reset();
    m_tile_map_ptr.reset();
    m_object_heap_ptr.reset();
    Tmx tmx = loadTmx(mr_renderer, mr_workspace, _file_path); // TODO: handle exceptions
    m_tile_heap_ptr = std::move(tmx.tile_heap);
    m_tile_map_ptr = std::move(tmx.tile_map);
    m_object_heap_ptr = std::move(tmx.object_heap);
    return m_tile_map_ptr != nullptr; // TODO: only exceptions
}

void Scene::render(const RenderState & _state)
{
    if(!m_tile_map_ptr)
    {
        return;
    }
    executeDefers();
    b2World_Step(m_b2_world_id, _state.time_passed.count() / 1000.0f, 4); // TODO: stable rate (1.0f / 60.0f), all from user settings
    handleBox2dContactEvents();
    syncWorldWithFollowedBody();
    const Color & bg_color = m_tile_map_ptr->getBackgroundColor();
    SDL_SetRenderDrawColor(&mr_renderer, bg_color.r, bg_color.g, bg_color.b, bg_color.a);
    SDL_RenderClear(&mr_renderer);

    std::unordered_set<uint64_t> bodies_to_render(m_bodies.size());
    for(const auto & pair : m_bodies)
        bodies_to_render.insert(pair.first);
    drawLayersAndBodies(*m_tile_map_ptr, bodies_to_render, _state.time_passed);
    for(const uint64_t body_id : bodies_to_render)
        drawBody(m_bodies[body_id], _state.time_passed);

    if(mp_box2d_debug_draw)
        mp_box2d_debug_draw->draw();
}

void Scene::executeDefers()
{
    if(!m_defers.empty())
    {
        for(auto & action : m_defers)
            action();
        m_defers.clear();
    }
}

bool Scene::box2dPreSolveContact(b2ShapeId _shape_id_a, b2ShapeId _shape_id_b, b2Manifold * _manifold, void * _context)
{
    Scene * self = static_cast<Scene *>(_context);
    bool result = true;
    PreSolveContact contact;
    if(!tryGetContactSide(_shape_id_a, contact.side_a) || !tryGetContactSide(_shape_id_b, contact.side_b))
        return true;
    contact.manifold = _manifold;
    self->forEachObserver([&result, &contact](ContactObserver & __observer) {
        if(!__observer.preSolveContact(contact))
            result = false;
        return result;
    });
    return result;
}

void Scene::handleBox2dContactEvents()
{
    {
        Contact contact;
        b2ContactEvents contact_events = b2World_GetContactEvents(m_b2_world_id);
        for(int i = 0; i < contact_events.beginCount; ++i)
        {
            b2ContactBeginTouchEvent & event = contact_events.beginEvents[i];
            if(tryGetContactSide(event.shapeIdA, contact.side_a) && tryGetContactSide(event.shapeIdB, contact.side_b))
                callObservers(&ContactObserver::beginContact, contact);
        }
        for(int i = 0; i < contact_events.endCount; ++i)
        {
            b2ContactEndTouchEvent & event = contact_events.endEvents[i];
            if(tryGetContactSide(event.shapeIdA, contact.side_a) && tryGetContactSide(event.shapeIdB, contact.side_b))
                callObservers(&ContactObserver::endContact, contact);
        }
    }

    {
        SensorContact contact;
        b2SensorEvents sensor_events = b2World_GetSensorEvents(m_b2_world_id);
        for(int i = 0; i < sensor_events.beginCount; ++i)
        {
            b2SensorBeginTouchEvent & event = sensor_events.beginEvents[i];
            if(tryGetContactSide(event.sensorShapeId, contact.sensor) &&
                tryGetContactSide(event.visitorShapeId, contact.visitor))
            {
                callObservers(&ContactObserver::beginSensorContact, contact);
            }
        }
        for(int i = 0; i < sensor_events.endCount; ++i)
        {
            b2SensorEndTouchEvent & event = sensor_events.endEvents[i];
            if(tryGetContactSide(event.sensorShapeId, contact.sensor) &&
                tryGetContactSide(event.visitorShapeId, contact.visitor))
            {
                callObservers(&ContactObserver::endSensorContact, contact);
            }
        }
    }
}

bool Scene::tryGetContactSide(b2ShapeId _shape_id, ContactSide & _contact_side)
{
    b2BodyId b2_body_id = b2Shape_GetBody(_shape_id);
    BodyShape * shape = getUserData(_shape_id);
    Body * body = getUserData(b2_body_id);
    if(shape && body)
    {
        _contact_side.body_id = body->getId();
        _contact_side.shape_key = shape->getKey();
        _contact_side.tile_map_object_id = shape->getTileMapObjectId();
        return true;
    }
    return false;
}

void Scene::syncWorldWithFollowedBody()
{
    if(B2_IS_NULL(m_followed_body_id))
    {
        return;
    }
    int output_width, output_height;
    SDL_GetCurrentRenderOutputSize(&mr_renderer, &output_width, &output_height);
    b2Vec2 followed_body_position = b2Body_GetPosition(m_followed_body_id);
    m_world_offset.x = physicalToGraphical(followed_body_position.x) - static_cast<float>(output_width) / 2;
    m_world_offset.y = physicalToGraphical(followed_body_position.y) - static_cast<float>(output_height) / 2;
    const int32_t map_x = m_tile_map_ptr->getX() * m_tile_map_ptr->getTileWidth();
    const int32_t map_y = m_tile_map_ptr->getY() * m_tile_map_ptr->getTileHeight();
    if(m_world_offset.x < map_x)
    {
        m_world_offset.x = map_x;
    }
    else
    {
        int32_t max_offset_x = m_tile_map_ptr->getWidth() * m_tile_map_ptr->getTileWidth() - output_width;
        if(m_world_offset.x > max_offset_x)
            m_world_offset.x = max_offset_x;
    }
    if(m_world_offset.y < map_y)
    {
        m_world_offset.y = map_y;
    }
    else
    {
        int32_t max_offset_y = m_tile_map_ptr->getHeight() * m_tile_map_ptr->getTileHeight() - output_height;
        if(m_world_offset.y > max_offset_y)
            m_world_offset.y = max_offset_y;
    }
}

void Scene::drawBody(b2BodyId _body_id, std::chrono::milliseconds _time_passed)
{
    GraphicsRenderOptions options; // TODO: to FixtureUserData? How to rotate multiple shapes?
    Point body_position;
    {
        b2Vec2 pos = b2Body_GetPosition(_body_id);
        body_position = toAbsoluteCoords(physicalToGraphical(pos.x), physicalToGraphical(pos.y));
    }
    int shape_count = b2Body_GetShapeCount(_body_id);
    std::vector<b2ShapeId> shapes(shape_count);
    b2Body_GetShapes(_body_id, shapes.data(), shape_count);
    for(const b2ShapeId & shape_id : shapes)
    {
        BodyShape * shape = getUserData(shape_id);
        BodyShapeGraphics * shape_graphic = shape->getCurrentGraphics();
        if(shape_graphic)
        {
            options.flip = shape_graphic->options.getFlip();
            shape_graphic->graphics.render(body_position + shape_graphic->options.position, _time_passed, options);
        }
    }
}

void Scene::drawLayersAndBodies(
    const TileMapLayerContainer & _container,
    std::unordered_set<uint64_t> & _bodies_to_render,
    std::chrono::milliseconds _time_passed)
{
    _container.forEachLayer([&_bodies_to_render, _time_passed, this](const TileMapLayer & __layer) {
        if(!__layer.isVisible()) return;
        switch(__layer.getType())
        {
        case TileMapLayerType::Tile:
            drawTileLayer(dynamic_cast<const TileMapTileLayer &>(__layer));
            break;
        case TileMapLayerType::Object:
            if(mr_workspace.isDebugRenderingEnabled())
                drawObjectLayer(dynamic_cast<const TileMapObjectLayer &>(__layer)); // TODO: _viewport
            break;
        case TileMapLayerType::Image:
            drawImageLayer(dynamic_cast<const TileMapImageLayer &>(__layer)); // TODO: _viewport
            break;
        case TileMapLayerType::Group:
        {
            const TileMapGroupLayer & group = dynamic_cast<const TileMapGroupLayer &>(__layer);
            if(group.isVisible())
                drawLayersAndBodies(group, _bodies_to_render, _time_passed);
            break;
        }}
        for(const auto & pair : m_bodies)
        {
            b2BodyId box2d_body_id = pair.second;
            Body * body = getUserData(box2d_body_id);
            if(body->getLayer() == __layer.getName() && _bodies_to_render.erase(pair.first))
                drawBody(box2d_body_id, _time_passed);
        }
    });
}

void Scene::drawObjectLayer(const TileMapObjectLayer & _layer)
{
    SDL_SetRenderDrawColor(&mr_renderer, 10, 0, 200, 255);
    _layer.forEachObject([this](const TileMapObject & __object) {
        if(!__object.isVisible()) return;
        // TODO: if in viewport
        switch (__object.getObjectType())
        {
        case TileMapObjectType::Polygon:
            drawPolyXObject(dynamic_cast<const TileMapPolygon &>(__object), true);
            break;
        case TileMapObjectType::Polyline:
            drawPolyXObject(dynamic_cast<const TileMapPolyline &>(__object), false);
            break;
        case TileMapObjectType::Circle:
            drawCircle(dynamic_cast<const TileMapCircle &>(__object));
            break;
        default:
            // TODO: point
            break;
        }
    });
}

void Scene::drawPolyXObject(const TileMapPolyX & _poly, bool _close)
{
    const std::vector<Point> & poly_points = _poly.getPoints();
    size_t poly_points_count = poly_points.size();
    if(poly_points_count < 2) return;
    size_t total_points_count = _close ? poly_points_count + 1 : poly_points_count;
    Point base_point = toAbsoluteCoords(_poly.getX(), _poly.getY());
    std::vector<Point> points(total_points_count);
    for(size_t i = 0; i < poly_points_count; ++i)
    {
        points[i].x = base_point.x + poly_points[i].x;
        points[i].y = base_point.y + poly_points[i].y;
    }
    if(_close)
    {
        points[poly_points_count].x = points[0].x;
        points[poly_points_count].y = points[0].y;
    }
    SDL_RenderLines(&mr_renderer, points.data()->toSdlPtr(), total_points_count);
}

void Scene::drawCircle(const TileMapCircle & _circle)
{
    Point position = toAbsoluteCoords(_circle.getX(), _circle.getY());
    sdlRenderCircle(&mr_renderer, position, _circle.getRadius());
}

void Scene::drawTileLayer(const TileMapTileLayer & _layer)
{
    Rect camera;
    camera.x = m_world_offset.x;
    camera.y = m_world_offset.y;
    {
        int w, h;
        SDL_GetCurrentRenderOutputSize(&mr_renderer, &w, &h);
        camera.w = static_cast<float>(w);
        camera.h = static_cast<float>(h);
    }

    const float first_col = std::floor(camera.x / m_tile_map_ptr->getTileWidth());
    const float first_row = std::floor(camera.y / m_tile_map_ptr->getTileHeight());
    const float last_col = std::ceil((camera.x + camera.w) / m_tile_map_ptr->getTileWidth());
    const float last_row = std::ceil((camera.y + camera.h) / m_tile_map_ptr->getTileHeight());
    const float start_x = first_col * m_tile_map_ptr->getTileWidth() - camera.x;
    const float start_y = first_row * m_tile_map_ptr->getTileHeight() - camera.y;

    SDL_FRect src_rect;
    SDL_FRect dest_rect
    {
        .x = .0f,
        .y = .0f,
        .w = static_cast<float>(m_tile_map_ptr->getTileWidth()),
        .h = static_cast<float>(m_tile_map_ptr->getTileHeight())
    };

    for(int32_t row = static_cast<int32_t>(first_row), dest_row = 0; row <= static_cast<int32_t>(last_row); ++row, ++dest_row)
    {
        for(int32_t col = static_cast<int32_t>(first_col), dest_col = 0; col <= static_cast<int32_t>(last_col); ++col, ++dest_col)
        {
            const Tile * tile = _layer.getTile(col, row);
            if(!tile) continue;

            src_rect.x = tile->getSourceX();
            src_rect.y = tile->getSourceY();
            src_rect.w = tile->getWidth();
            src_rect.h = tile->getHeight();

            dest_rect.x = start_x + dest_col * m_tile_map_ptr->getTileWidth();
            dest_rect.y = start_y + dest_row * m_tile_map_ptr->getTileHeight();

            SDL_RenderTexture(&mr_renderer, &tile->getSource(), &src_rect, &dest_rect);
        }
    }
}

void Scene::drawImageLayer(const TileMapImageLayer & _layer)
{
    std::shared_ptr<SDL_Texture> image = _layer.getImage();
    float width, height;
    SDL_GetTextureSize(image.get(), &width, &height);
    SDL_FRect dim { .0f, .0f, width, height };
    SDL_RenderTexture(&mr_renderer, image.get(), nullptr, &dim);
}

void Scene::applyForce(uint64_t _body_id, const Point & _force)
{
    m_defers.push_front([this, _body_id, _force]() {
        b2BodyId b2_body_id = findBody(_body_id);
        if(B2_IS_NON_NULL(b2_body_id))
            b2Body_ApplyForceToCenter(b2_body_id, _force, true); // TODO: what is wake?
    });
}

void Scene::setBodyPosition(uint64_t _body_id, const Point & _position)
{
    m_defers.push_front([this, _body_id, _position]() {
        b2BodyId b2_body_id = findBody(_body_id);
        if(B2_IS_NON_NULL(b2_body_id))
            b2Body_SetTransform(b2_body_id, _position, b2Body_GetRotation(b2_body_id));
    });
}

std::optional<Point> Scene::getBodyPosition(uint64_t _body_id) const
{
    b2BodyId b2_body_id = findBody(_body_id);
    if(B2_IS_NON_NULL(b2_body_id))
        return asPoint(b2Body_GetPosition(b2_body_id));
    return std::nullopt;
}

std::optional<std::vector<Point>> Scene::findPath(
    uint64_t _body_id,
    const Point & _destination,
    bool _allow_diagonal_steps,
    bool _avoid_sensors) const
{
    const b2BodyId b2_body_id = findBody(_body_id);
    if(B2_IS_NULL(b2_body_id))
        return std::nullopt;
    AStarOptions options;
    options.allow_diagonal_steps = _allow_diagonal_steps;
    options.avoid_sensors = _avoid_sensors;
    auto b2_result = aStarFindPath(m_b2_world_id, b2_body_id, _destination, options);
    if(!b2_result.has_value())
        return std::nullopt;
    std::vector<Point> result;
    result.reserve(b2_result.value().size());
    for(size_t i = 0; i < b2_result.value().size(); ++i)
        result.push_back(makePoint(b2_result.value()[i].x, b2_result.value()[i].y));
    return result;
}
