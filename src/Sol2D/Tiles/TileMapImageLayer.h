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

#include <Sol2D/Tiles/TileMapLayer.h>

namespace Sol2D::Tiles {

class TileMapImageLayer : public TileMapLayer
{
public:
    TileMapImageLayer(const TileMapLayer * _parent, uint32_t _id, const std::string & _name) :
        TileMapLayer(_parent, _id, _name, TileMapLayerType::Image)
    {
    }

    void setImage(std::shared_ptr<SDL_Texture> _image) { m_image = _image; }
    const std::shared_ptr<SDL_Texture> getImage() const { return m_image; }

private:
    std::shared_ptr<SDL_Texture> m_image;
};

} // namespace Tiles::Sol2D
