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

#include <Sol2D/StepState.h>
#include <Sol2D/Outlet.h>
#include <unordered_map>
#include <list>

namespace Sol2D {

class View final
{
    S2_DISABLE_COPY_AND_MOVE(View)

public:
    explicit View(SDL_Renderer & _renderer);
    uint16_t createFragment(const Fragment & _fragment);
    const Fragment * getFragment(uint16_t _id) const;
    bool updateFragment(uint16_t _id, const Fragment & _fragment);
    bool deleteFragment(uint16_t _id);
    bool bindFragment(uint16_t _fragment_id, std::shared_ptr<Canvas> _canvas);
    void resize();
    void step(const StepState & _state);

private:
    void emplaceOrderedOutlet(Outlet * _outlet);
    void eraseOrderedOutlet(Outlet * _outlet);

private:
    SDL_Renderer & mr_renderer;
    std::unordered_map<uint16_t, std::unique_ptr<Outlet>> m_outlets;
    uint16_t m_next_fragment_id;
    std::list<Outlet *> m_ordered_outlets;
};

inline View::View(SDL_Renderer & _renderer) :
    mr_renderer(_renderer),
    m_next_fragment_id(1)
{
}

} // namespace Sol2D
