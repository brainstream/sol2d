// Sol2D Game Engine
// Copyright (C) 2023-2025 Sergey Smolyannikov aka brainstream
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

#include <Sol2D/Workspace.h>
#include <Sol2D/StepState.h>
#include <Sol2D/Window.h>
#include <Sol2D/MediaLayer/MediaLayer.h>
#include <Sol2D/Lua/LuaLibrary.h>

using namespace Sol2D;
using namespace Sol2D::Lua;

namespace {

class SDLAssertionHandler final
{
    S2_DISABLE_COPY_AND_MOVE(SDLAssertionHandler)

public:
    explicit SDLAssertionHandler(const Workspace & _workspace) :
        mr_workspace(_workspace)
    {
    }

    void handle(const SDL_AssertData * _data) const
    {
        mr_workspace.getMainLogger().critical(
            "SDL assertion failed: {} at {}:{}",
            _data->condition,
            _data->filename,
            _data->linenum);
    }

private:
    const Workspace & mr_workspace;
};

class Application final
{
    S2_DISABLE_COPY_AND_MOVE(Application)

public:
    explicit Application(const Workspace & _workspace);
    ~Application();
    void exec();

private:
    bool handleEvent(const SDL_Event & _event);
    void onWindowResized(const SDL_WindowEvent & _event);
    void onMouseButtonDown(const SDL_MouseButtonEvent & _event);
    void onMouseButtonUp(const SDL_MouseButtonEvent & _event);
    void step();

private:
    const Workspace & mr_workspace;
    SDLAssertionHandler m_sdl_assertion_handler;
    StepState m_step_state;
    SDL_Window * mp_sdl_window;
    SDL_GPUDevice * mp_device;
    Window * mp_window;
};

SDL_AssertState SDLCALL sdlAssertionHandler(const SDL_AssertData * _data, void * _userdata)
{
    static_cast<const SDLAssertionHandler *>(_userdata)->handle(_data);
    return SDL_ASSERTION_BREAK;
}

} // namespace

Application::Application(const Workspace & _workspace) :
    mr_workspace(_workspace),
    m_sdl_assertion_handler(_workspace),
    m_step_state{},
    mp_sdl_window(nullptr),
    mp_device(nullptr),
    mp_window(new Window)
{
    if(!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
        throw SDLException("Unable to initialize SDL.");
    if(!TTF_Init())
        throw SDLException("SDL_TTF initialization failed.");
    if(!Mix_OpenAudio(0, nullptr)) // TODO: allow user to select a device
        throw SDLException("SDL_Mixer initialization failed.");
    SDL_SetAssertionHandler(sdlAssertionHandler, &m_sdl_assertion_handler);
    mp_sdl_window = SDL_CreateWindow(
        mr_workspace.getApplicationName().c_str(),
        1024,
        768,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN);
    if(!mp_sdl_window)
        throw SDLException("Unable to create window");
    mp_device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, true, nullptr);
    if(!mp_device)
        throw SDLException("Unable to create GPU device.");
    if(!SDL_ClaimWindowForGPUDevice(mp_device, mp_sdl_window))
        throw SDLException("Unable to claim window for GPU device.");
    if(!SDL_ShowWindow(mp_sdl_window))
        throw SDLException("Unable to show window.");
}

Application::~Application()
{
    delete mp_window;
    if(mp_device) SDL_DestroyGPUDevice(mp_device);
    if(mp_sdl_window) SDL_DestroyWindow(mp_sdl_window);
    TTF_Quit();
    Mix_Quit();
    SDL_Quit();
}

void Application::exec()
{
    ResourceManager resource_manager; // TODO: create in place
    Renderer renderer(resource_manager, mp_sdl_window, mp_device);
    StoreManager store_manager;
    std::unique_ptr<LuaLibrary> lua = std::make_unique<LuaLibrary>(mr_workspace, store_manager, *mp_window, renderer);
    lua->executeMainScript();
    const uint32_t render_frame_delay = floor(1000 / mr_workspace.getFrameRate());
    uint32_t last_rendering_ticks = SDL_GetTicks();
    SDL_Event event;
    for(;;)
    {
        while(SDL_PollEvent(&event))
            if(handleEvent(event)) return;
        const uint32_t now_ticks = SDL_GetTicks();
        const uint32_t passed_ticks = now_ticks - last_rendering_ticks;
        if(passed_ticks >= render_frame_delay)
        {
            last_rendering_ticks = now_ticks;
            m_step_state.delta_time = std::chrono::milliseconds(passed_ticks);
            m_step_state.mouse_state.buttons = SDL_GetMouseState(
                &m_step_state.mouse_state.position.x,
                &m_step_state.mouse_state.position.y);
            renderer.beginStep();
            step();
            renderer.submitStep();
            if(m_step_state.mouse_state.lb_click.state == MouseClickState::Finished)
                m_step_state.mouse_state.lb_click.state = MouseClickState::None;
            if(m_step_state.mouse_state.rb_click.state == MouseClickState::Finished)
                m_step_state.mouse_state.rb_click.state = MouseClickState::None;
            if(m_step_state.mouse_state.mb_click.state == MouseClickState::Finished)
                m_step_state.mouse_state.mb_click.state = MouseClickState::None;
        }
        int32_t delay = render_frame_delay - passed_ticks;
        if(delay > 5) SDL_Delay(delay);
    }
}

bool Application::handleEvent(const SDL_Event & _event)
{
    switch (_event.type)
    {
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        onMouseButtonDown(_event.button);
        break;
    case SDL_EVENT_MOUSE_BUTTON_UP:
        onMouseButtonUp(_event.button);
        break;
    case SDL_EVENT_WINDOW_RESIZED:
        onWindowResized(_event.window);
        break;
    case SDL_EVENT_QUIT:
        return true;
    default:
        break;
    }
    return false;
}

inline void Application::onWindowResized(const SDL_WindowEvent & /*_event*/)
{
    mp_window->resize();
}

inline void Application::onMouseButtonDown(const SDL_MouseButtonEvent & _event)
{
    switch(_event.button)
    {
    case SDL_BUTTON_LEFT:
        m_step_state.mouse_state.lb_click.state = MouseClickState::Started;
        m_step_state.mouse_state.lb_click.start.x = _event.x;
        m_step_state.mouse_state.lb_click.start.y = _event.y;
        break;
    case SDL_BUTTON_RIGHT:
        m_step_state.mouse_state.rb_click.state = MouseClickState::Started;
        m_step_state.mouse_state.rb_click.start.x = _event.x;
        m_step_state.mouse_state.rb_click.start.y = _event.y;
        break;
    case SDL_BUTTON_MIDDLE:
        m_step_state.mouse_state.mb_click.state = MouseClickState::Started;
        m_step_state.mouse_state.mb_click.start.x = _event.x;
        m_step_state.mouse_state.mb_click.start.y = _event.y;
        break;
    }
}

inline void Application::onMouseButtonUp(const SDL_MouseButtonEvent & _event)
{
    switch(_event.button)
    {
    case SDL_BUTTON_LEFT:
        m_step_state.mouse_state.lb_click.state = MouseClickState::Finished;
        m_step_state.mouse_state.lb_click.finish.x = _event.x;
        m_step_state.mouse_state.lb_click.finish.y = _event.y;
        break;
    case SDL_BUTTON_RIGHT:
        m_step_state.mouse_state.rb_click.state = MouseClickState::Finished;
        m_step_state.mouse_state.rb_click.finish.x = _event.x;
        m_step_state.mouse_state.rb_click.finish.y = _event.y;
        break;
    case SDL_BUTTON_MIDDLE:
        m_step_state.mouse_state.mb_click.state = MouseClickState::Finished;
        m_step_state.mouse_state.mb_click.finish.x = _event.x;
        m_step_state.mouse_state.mb_click.finish.y = _event.y;
        break;
    }
}

void Application::step()
{
    mp_window->step(m_step_state);
}

int main(int _argc, const char ** _argv)
{
    std::unique_ptr<Workspace> workspace;
    {
        std::filesystem::path config(_argc > 1 ? _argv[1] : "game.xml");
        if(config.is_relative())
            config = std::filesystem::path(SDL_GetBasePath()) / config;
        workspace = Workspace::load(config);
        if(!workspace)
        {
            SDL_MessageBoxData data = {};
            data.flags = SDL_MESSAGEBOX_ERROR;
            data.message = "Unable to load manifest file";
            data.title = "Critical error";
            SDL_ShowMessageBox(&data, nullptr);
            return -1;
        }        
    }
    try
    {
        Application(*workspace).exec();
        return 0;
    }
    catch(const std::exception & error)
    {
        workspace->getMainLogger().critical(error.what());
        return -2;
    }
    catch(...)
    {
        workspace->getMainLogger().critical("An unknown critical error has occurred");
        return -3;
    }
}
