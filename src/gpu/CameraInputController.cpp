#include "CameraInputController.h"

#include <SDL3/SDL.h>

namespace dk {

CameraInputController::CameraInputController(Camera& camera)
    : _camera(camera)
{
}

bool CameraInputController::handleInput(const input::InputState& state, const input::InputContext& ctx)
{
    const auto& mouse = state.mouse();

    if (!ctx.imguiWantsMouse)
    {
        if (mouse.leftPressed)
        {
            if (ctx.window)
            {
                SDL_SetWindowRelativeMouseMode(ctx.window, true);
            }
            SDL_HideCursor();
            _camera.mouse_left_down = true;
        }

        if (mouse.leftReleased)
        {
            if (ctx.window)
            {
                SDL_SetWindowRelativeMouseMode(ctx.window, false);
            }
            SDL_ShowCursor();
            _camera.mouse_left_down = false;
        }

        if (_camera.mouse_left_down)
        {
            _camera.yaw += mouse.delta.x / 200.f;
            _camera.pitch -= mouse.delta.y / 200.f;
        }
    }

    if (!ctx.imguiWantsKeyboard)
    {
        const auto& w = state.key(SDLK_W);
        const auto& s = state.key(SDLK_S);
        const auto& a = state.key(SDLK_A);
        const auto& d = state.key(SDLK_D);
        const auto& ctrl = state.key(SDLK_LCTRL);
        const auto& space = state.key(SDLK_SPACE);

        _camera.velocity.z = (w.down ? -1.0f : 0.0f) + (s.down ? 1.0f : 0.0f);
        _camera.velocity.x = (a.down ? -1.0f : 0.0f) + (d.down ? 1.0f : 0.0f);
        _camera.velocity.y = (ctrl.down ? -1.0f : 0.0f) + (space.down ? 1.0f : 0.0f);
    }

    return _camera.mouse_left_down;
}

} // namespace dk
