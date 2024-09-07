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

#include <Sol2D/GraphicsPack.h>
#include <Sol2D/Utils/Math.h>

using namespace Sol2D;
using namespace Sol2D::Utils;

GraphicsPack::GraphicsPack(SDL_Renderer & _renderer) :
    mp_renderer(&_renderer),
    m_current_frame_index(0),
    m_current_frame_duration(std::chrono::milliseconds::zero()),
    m_total_duration(std::chrono::milliseconds::zero())
{
}

GraphicsPack::~GraphicsPack()
{
    for(Frame * frame : m_frames)
        delete frame;
}

size_t GraphicsPack::addFrame(const GraphicsPackFrameOptions & _options)
{
    Frame * frame = new Frame(_options);
    m_frames.push_back(frame);
    if(frame->is_visible)
        m_total_duration += frame->duration;
    return m_frames.size() - 1;
}

size_t GraphicsPack::insertFrame(size_t _index, const GraphicsPackFrameOptions & _options)
{
    Frame * frame = new Frame(_options);
    m_frames.insert(m_frames.begin() + _index, frame);
    if(frame->is_visible)
        m_total_duration += frame->duration;
    return _index;
}

bool GraphicsPack::removeFrame(size_t _index)
{
    auto it = m_frames.begin() + _index;
    if(it == m_frames.end())
        return false;
    Frame * frame = *it;
    if(frame->is_visible)
        m_total_duration -= frame->duration;
    m_frames.erase(it);
    if(m_current_frame_index >= m_frames.size())
    {
        m_current_frame_index = 0;
        m_current_frame_duration = std::chrono::milliseconds::zero();
    }
    return true;
}

bool GraphicsPack::setFrameVisibility(size_t _index, bool _is_visible)
{
    if(_index < m_frames.size())
    {
        Frame * frame = m_frames[_index];
        if(frame->is_visible && !_is_visible)
        {
            m_total_duration -= frame->duration;
            frame->is_visible = false;
        }
        else if(!frame->is_visible && _is_visible)
        {
            m_total_duration += frame->duration;
            frame->is_visible = true;
        }
        return true;
    }
    return false;
}

std::optional<bool> GraphicsPack::isFrameVisible(size_t _index) const
{
    if(_index < m_frames.size())
    {
        return m_frames[_index]->is_visible;
    }
    return std::optional<bool>();
}

bool GraphicsPack::setFrameDuration(size_t _index, std::chrono::milliseconds _duration)
{
    if(_index < m_frames.size())
    {
        Frame * frame = m_frames[_index];
        if(frame->is_visible)
            m_total_duration = m_total_duration - frame->duration + _duration;
        frame->duration = _duration;
        return true;
    }
    return false;
}

std::optional<std::chrono::milliseconds> GraphicsPack::getFrameDuration(size_t _index) const
{
    if(_index < m_frames.size())
    {
        return m_frames[_index]->duration;
    }
    return std::optional<std::chrono::milliseconds>();
}

std::pair<bool, size_t> GraphicsPack::addSprite(
    size_t _frame,
    const Sprite & _sprite,
    const GraphicsPackSpriteOptions & _options)
{
    if(_frame >= m_frames.size())
        return std::make_pair(false, 0);
    std::vector<Graphics> & graphics = m_frames[_frame]->graphics;
    graphics.push_back(Graphics(_sprite, _options));
    return std::make_pair(true, graphics.size() - 1);
}

std::pair<bool, size_t> GraphicsPack::addSprite(
    size_t _frame,
    const SpriteSheet & _sprite_sheet,
    size_t _sprite_index,
    const GraphicsPackSpriteOptions & _options)
{
    if(_frame >= m_frames.size() || !_sprite_sheet.isValid())
        return std::make_pair(false, 0);
    const std::vector<Rect> & sheet_rects = _sprite_sheet.getRects();
    if(_sprite_index >= sheet_rects.size())
        return std::make_pair(false, 0);
    std::vector<Graphics> & graphics = m_frames[_frame]->graphics;
    graphics.push_back(Graphics(
        _sprite_sheet.getTexture(),
        sheet_rects[_sprite_index],
        sheet_rects[_sprite_index].getSize(),
        _options
    ));
    return std::make_pair(true, graphics.size() - 1);
}

std::pair<bool, size_t> GraphicsPack::addSprites(
    size_t _frame,
    const SpriteSheet & _sprite_sheet,
    std::vector<size_t> _sprite_indices,
    const GraphicsPackSpriteOptions & _options)
{
    if(_frame >= m_frames.size() || !_sprite_sheet.isValid())
        return std::make_pair(false, 0);
    const std::vector<Rect> & sheet_rects = _sprite_sheet.getRects();
    for(size_t idx : _sprite_indices)
    {
        if(idx >= sheet_rects.size())
            return std::make_pair(false, 0);
    }
    std::vector<Graphics> & graphics = m_frames[_frame]->graphics;
    graphics.reserve(graphics.size() + _sprite_indices.size());
    for(size_t i = 0; i < _sprite_indices.size(); ++i)
    {
        graphics.push_back(Graphics(
            _sprite_sheet.getTexture(),
            sheet_rects[_sprite_indices[i]],
            sheet_rects[_sprite_indices[i]].getSize(),
            _options
        ));
    }
    return std::make_pair(true, graphics.size() - 1);
}

bool GraphicsPack::removeSprite(size_t _frame, size_t _sprite)
{
    if(_frame >= m_frames.size())
        return false;
    std::vector<Graphics> & graphics = m_frames[_frame]->graphics;
    if(_sprite >= graphics.size())
        return false;
    graphics.erase(graphics.begin() + _sprite);
    return true;
}

void GraphicsPack::render(const Point & _point, std::chrono::milliseconds _time_passed)
{
    if(m_total_duration == std::chrono::milliseconds::zero())
    {
        performRender(_point);
    }
    m_current_frame_duration += _time_passed;
    Frame * frame = m_frames[m_current_frame_index];
    if(!frame->is_visible)
    {
        frame = switchToNextVisibleFrame();
        if(frame)
        {
            m_current_frame_duration = _time_passed;
        }
        else
        {
            m_current_frame_duration = std::chrono::milliseconds::zero();
            return;
        }
    }
    for(;;)
    {
        if(frame->duration < m_current_frame_duration)
        {
            m_current_frame_duration -= frame->duration;
            frame = switchToNextVisibleFrame();
        }
        else
        {
            break;
        }
    }
    performRender(_point);
}

GraphicsPack::Frame * GraphicsPack::switchToNextVisibleFrame()
{
    for(size_t i = m_current_frame_index + 1; i < m_frames.size(); ++i)
    {
        if(m_frames[i]->is_visible)
        {
            m_current_frame_index = i;
            return m_frames[i];
        }
    }
    if(m_current_frame_index > 0)
    {
        for(size_t i = 0; i < m_current_frame_index; ++i)
        {
            if(m_frames[i]->is_visible)
            {
                m_current_frame_index = i;
                return m_frames[i];
            }
        }
    }
    return m_frames[m_current_frame_index]->is_visible ? m_frames[m_current_frame_index] : nullptr;
}

void GraphicsPack::performRender(const Point & _point)
{
    if(m_frames.empty())
        return;
    Frame * frame = m_frames[m_current_frame_index];
    if(!frame->is_visible)
        return;
    for(const Graphics & graphics : frame->graphics)
    {
        SDL_FRect dest_rect
        {
            .x = _point.x - graphics.position.x,
            .y = _point.y - graphics.position.y,
            .w = graphics.dest_size.w,
            .h = graphics.dest_size.h
        };
        union { int as_int; SDL_FlipMode as_flip_mode; } flip;
        flip.as_int = SDL_FLIP_NONE;
        if(graphics.is_flipped_horizontally)
            flip.as_int |= SDL_FLIP_HORIZONTAL;
        if(graphics.is_flipped_vertically)
            flip.as_int |= SDL_FLIP_VERTICAL;
        SDL_RenderTextureRotated(
            mp_renderer,
            graphics.texture.get(),
            graphics.src_rect.toSdlPtr(),
            &dest_rect,
            radiansToDegrees(graphics.angle_rad),
            graphics.flip_center.has_value() ? graphics.flip_center->toSdlPtr() : nullptr,
            flip.as_flip_mode
        );
    }
}
