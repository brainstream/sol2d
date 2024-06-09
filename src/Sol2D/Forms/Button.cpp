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

#include <Sol2D/Forms/Button.h>

using namespace Sol2D;
using namespace Sol2D::Forms;

Button::Button(const Canvas & _parent, const std::string & _text, SDL_Renderer & _renderer) :
    Label(_parent, _text, _renderer)
{
}

void Button::render(const RenderState & _state)
{
    handleState(_state);
    Label::render(_state);
}

void Button::handleState(const RenderState & _state)
{
    SDL_FRect rect =
    {
        .x = m_x.getPixels(mr_parent.getWidth()),
        .y = m_y.getPixels(mr_parent.getHeight()),
        .w = m_width.getPixels(mr_parent.getWidth()),
        .h = m_height.getPixels(mr_parent.getHeight())
    };
    if(!isPointIn(_state.mouse_state.position, rect))
    {
        setState(WidgetState::Default);
        return;
    }
    switch(_state.mouse_state.lb_click.state)
    {
    case MouseClickState::None:
        setState(WidgetState::Focused);
        break;
    case MouseClickState::Started:
        if(isPointIn(_state.mouse_state.lb_click.start, rect))
            setState(WidgetState::Activated);
        else
            setState(WidgetState::Default);
        break;
    case MouseClickState::Finished:
        setState(WidgetState::Focused);
        if(isPointIn(_state.mouse_state.lb_click.start, rect))
            m_click_observable.callObservers(&ButtonClickObserver::onClick);
        break;
    }
}

bool Button::isPointIn(const SDL_FPoint & _point, const SDL_FRect & _rect) const
{
    SDL_FPoint point = mr_parent.getTranslatedPoint(_point.x, _point.y);
    return SDL_PointInRectFloat(&point, &_rect);
}
