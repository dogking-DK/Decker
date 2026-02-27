#include "ui/tools/TranslateTool.h"

#include <cmath>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>

#include "SceneTypes.h"

namespace dk::ui {
namespace {

glm::mat4 toMatrix(const Transform& t)
{
    const glm::mat4 translation = glm::translate(glm::mat4(1.0f), t.translation);
    const glm::mat4 rotation = glm::mat4_cast(t.rotation);
    const glm::mat4 scale = glm::scale(glm::mat4(1.0f), t.scale);
    return translation * rotation * scale;
}

glm::mat4 worldTransform(const dk::SceneNode* node)
{
    if (!node)
    {
        return glm::mat4(1.0f);
    }

    std::vector<const dk::SceneNode*> chain;
    const dk::SceneNode* current = node;
    while (current)
    {
        chain.push_back(current);
        auto parent = current->parent.lock();
        current = parent ? parent.get() : nullptr;
    }

    glm::mat4 world(1.0f);
    for (auto it = chain.rbegin(); it != chain.rend(); ++it)
    {
        world *= toMatrix((*it)->local_transform);
    }
    return world;
}

} // namespace

bool TranslateTool::handleInput(const dk::input::InputState& /*state*/, const dk::input::InputContext& /*ctx*/)
{
    return false;
}

void TranslateTool::update(const ToolContext& ctx)
{
    if (!_state || !ctx.translateEnabled)
    {
        return;
    }

    if (!ctx.selectedNode)
    {
        _state->clearFrameFlags();
        return;
    }

    if (_state->operation != GizmoOperation::Translate)
    {
        _state->clearFrameFlags();
        return;
    }

    if (_state->dragStarted)
    {
        _startTranslation = ctx.selectedNode->local_transform.translation;
    }

    if (_state->dragging || _state->dragEnded)
    {
        glm::vec3 localDelta = _state->worldDelta;
        if (auto parent = ctx.selectedNode->parent.lock())
        {
            const glm::mat3 parentLinear = glm::mat3(worldTransform(parent.get()));
            const float det = glm::determinant(parentLinear);
            if (std::abs(det) > 1e-6f)
            {
                localDelta = glm::inverse(parentLinear) * _state->worldDelta;
            }
        }

        ctx.selectedNode->local_transform.translation = _startTranslation + localDelta;
    }

    _state->clearFrameFlags();
}

} // namespace dk::ui
