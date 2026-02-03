#pragma once

#include <SDL3/SDL_events.h>

#include "input/InputState.h"

namespace dk::input {

class InputBackend
{
public:
    explicit InputBackend(InputState& state);

    void feedSDLEvent(const SDL_Event& e);

private:
    InputState& _state;
};

} // namespace dk::input
