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

#include <Sol2D/GraphicsPack.h>
#include <Sol2D/BodyType.h>
#include <Sol2D/BodyShapeType.h>
#include <Sol2D/GraphicsPackDefinition.h>
#include <Sol2D/BodyShapePhysicsDefinition.h>
#include <Sol2D/BodyPhysicsDefinition.h>
#include <Sol2D/Rect.h>
#include <SDL3/SDL_surface.h>
#include <filesystem>
#include <variant>
#include <vector>

namespace Sol2D {

struct PolygonShapeDefinition
{
    std::vector<Point> points;
};

using RectShapeDefinition = Rect;

struct CircleShapeDefinition
{
    CircleShapeDefinition() :
        center{ .x = .0f, .y = .0f },
        radius(1.0f)
    {
    }

    Point center;
    float radius;
};

template<BodyShapeType shape_type>
struct BodyBasicShapeDefinition
{
    BodyShapePhysicsDefinition physics;
    std::vector<std::pair<std::string, GraphicsPackDefinition>> graphics;
    const BodyShapeType type = shape_type;

};

struct BodyPolygonDefinition : BodyBasicShapeDefinition<BodyShapeType::Polygon>, PolygonShapeDefinition { };

struct BodyRectDefinition : BodyBasicShapeDefinition<BodyShapeType::Polygon>, RectShapeDefinition { };

struct BodyCircleDefinition : BodyBasicShapeDefinition<BodyShapeType::Circle>, CircleShapeDefinition { };

using BodyVariantShapeDefinition = std::variant<BodyPolygonDefinition, BodyRectDefinition, BodyCircleDefinition>;

struct BodyDefinition
{
    BodyDefinition() :
        type(BodyType::Static)
    {
    }

    BodyType type;
    std::optional<std::filesystem::path> script;
    BodyPhysicsDefinition physics;
    std::vector<std::pair<std::string, BodyVariantShapeDefinition>> shapes;
};

} // namespace Sol2D
