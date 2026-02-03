#include "input/InputBackend.h"

#include <SDL3/SDL.h>

namespace dk::input {

InputBackend::InputBackend(InputState& state)
    : _state(state)
{
}

void InputBackend::feedSDLEvent(const SDL_Event& e)
{
    switch (e.type)
    {
    case SDL_EVENT_MOUSE_MOTION: {
        auto& mouse = _state.mouse();
        mouse.position.x = static_cast<float>(e.motion.x);
        mouse.position.y = static_cast<float>(e.motion.y);
        mouse.delta.x += static_cast<float>(e.motion.xrel);
        mouse.delta.y += static_cast<float>(e.motion.yrel);
        break;
    }
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP: {
        auto& mouse   = _state.mouse();
        bool  pressed = (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN);
        if (e.button.button == SDL_BUTTON_LEFT)
        {
            mouse.leftDown = pressed;
            if (pressed)
            {
                mouse.leftPressed = true;
            }
            else
            {
                mouse.leftReleased = true;
            }
        }
        else if (e.button.button == SDL_BUTTON_RIGHT)
        {
            mouse.rightDown = pressed;
            if (pressed)
            {
                mouse.rightPressed = true;
            }
            else
            {
                mouse.rightReleased = true;
            }
        }
        else if (e.button.button == SDL_BUTTON_MIDDLE)
        {
            mouse.middleDown = pressed;
            if (pressed)
            {
                mouse.middlePressed = true;
            }
            else
            {
                mouse.middleReleased = true;
            }
        }
        break;
    }
    case SDL_EVENT_MOUSE_WHEEL: {
        auto& mouse = _state.mouse();
        mouse.wheelDelta += static_cast<float>(e.wheel.y);
        break;
    }
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP: {
        bool pressed = (e.type == SDL_EVENT_KEY_DOWN);
        auto& key    = _state.key(e.key.key);
        if (pressed && !key.down)
        {
            key.pressed = true;
        }
        if (!pressed && key.down)
        {
            key.released = true;
        }
        key.down = pressed;
        break;
    }
    default:
        break;
    }
}

} // namespace dk::input
