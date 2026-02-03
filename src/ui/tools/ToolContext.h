#pragma once

namespace dk {
class Camera;
struct SceneNode;
}

namespace dk::ui {

struct ToolContext
{
    dk::Camera*    camera{nullptr};
    dk::SceneNode* selectedNode{nullptr};
};

} // namespace dk::ui
