#include "input/InputState.h"

namespace dk::input {

void InputState::beginFrame()
{
    for (auto& [keycode, state] : _keys)
    {
        state.pressed  = false;
        state.released = false;
    }

    _mouse.delta         = {};
    _mouse.leftPressed   = false;
    _mouse.rightPressed  = false;
    _mouse.middlePressed = false;
    _mouse.leftReleased  = false;
    _mouse.rightReleased = false;
    _mouse.middleReleased = false;
    _mouse.wheelDelta    = 0.0f;
}

const KeyState& InputState::key(SDL_Keycode keycode) const
{
    static KeyState empty{};
    auto            iter = _keys.find(keycode);
    if (iter == _keys.end())
    {
        return empty;
    }
    return iter->second;
}

KeyState& InputState::key(SDL_Keycode keycode)
{
    return _keys[keycode];
}

} // namespace dk::input
