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

#include <Sol2D/Workspace.h>
#include <Sol2D/BodyPrototype.h>
#include <Sol2D/Level.h>
#include <Sol2D/Def.h>
#include <SDL3/SDL.h>
#include <vector>


namespace Sol2D {

class Scene final
{
    DISABLE_COPY_AND_MOVE(Scene)

public:
    Scene(SDL_Renderer & _renderer, const Workspace & _workspace);
    ~Scene();
    bool loadLevelFromTmx(const std::filesystem::path & _tmx_file);
    Level * getLevel();
    BodyPrototype & createBodyPrototype();
    Uint8 * getKeyboardState() const;
    void render(const SDL_FRect & _viewport);

private:
    SDL_Renderer & mr_renderer;
    const Workspace & mr_workspace;
    std::vector<BodyPrototype *> m_body_prototypes;
    Level * mp_level;
};

} // namespace Sol2D
