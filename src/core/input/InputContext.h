#pragma once

#include <SDL3/SDL_video.h>

namespace dk {
class Camera;
struct SceneNode;
}

namespace dk::input {

struct InputContext
{
    SDL_Window* window{nullptr};
    dk::Camera* camera{nullptr};
    dk::SceneNode* selectedNode{nullptr};
    float translateSensitivity{ 0.2 };
    bool imguiWantsMouse{false};
    bool imguiWantsKeyboard{false};
};

} // namespace dk::input
