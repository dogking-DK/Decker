#pragma once

#include <unordered_map>

#include <glm/vec2.hpp>
#include <SDL3/SDL_events.h>

namespace dk::input {

struct KeyState
{
    bool down{false};
    bool pressed{false};
    bool released{false};
};

struct MouseState
{
    glm::vec2 position{};
    glm::vec2 delta{};
    bool      leftDown{false};
    bool      rightDown{false};
    bool      middleDown{false};
    bool      leftPressed{false};
    bool      rightPressed{false};
    bool      middlePressed{false};
    bool      leftReleased{false};
    bool      rightReleased{false};
    bool      middleReleased{false};
    float     wheelDelta{0.0f};
};

class InputState
{
public:
    void beginFrame();

    const KeyState& key(SDL_Keycode keycode) const;
    KeyState&       key(SDL_Keycode keycode);

    MouseState&       mouse() { return _mouse; }
    const MouseState& mouse() const { return _mouse; }

private:
    std::unordered_map<SDL_Keycode, KeyState> _keys;
    MouseState                                _mouse;
};

} // namespace dk::input
