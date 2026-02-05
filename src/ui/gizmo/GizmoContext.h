#pragma once

namespace dk {
class Camera;
struct SceneNode;
}

namespace dk::ui {

struct GizmoContext
{
    dk::Camera*    camera{nullptr};
    dk::SceneNode* selectedNode{nullptr};
    float          translateSensitivity{0.01f};
    bool           translateEnabled{true};
};

} // namespace dk::ui
