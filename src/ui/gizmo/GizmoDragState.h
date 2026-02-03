#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace dk::ui {

enum class GizmoOperation
{
    None,
    Translate,
    Rotate,
    Scale
};

enum class GizmoAxis
{
    None,
    X,
    Y,
    Z,
    XY,
    YZ,
    ZX,
    Screen
};

struct GizmoDragState
{
    bool dragging{false};
    bool dragStarted{false};
    bool dragEnded{false};
    GizmoOperation operation{GizmoOperation::None};
    GizmoAxis axis{GizmoAxis::None};
    glm::vec2 dragStart{};
    glm::vec2 screenDelta{};
    glm::vec3 worldDelta{};
    float rotationDelta{0.0f};
    float scaleDelta{0.0f};

    void beginDrag(const glm::vec2& start, GizmoOperation newOperation, GizmoAxis newAxis)
    {
        dragging = true;
        dragStarted = true;
        dragEnded = false;
        operation = newOperation;
        axis = newAxis;
        dragStart = start;
        screenDelta = glm::vec2(0.0f);
        worldDelta = glm::vec3(0.0f);
        rotationDelta = 0.0f;
        scaleDelta = 0.0f;
    }

    void updateDeltas(const glm::vec2& newScreenDelta, const glm::vec3& newWorldDelta)
    {
        screenDelta = newScreenDelta;
        worldDelta = newWorldDelta;
    }

    void updateRotationDelta(float delta)
    {
        rotationDelta = delta;
    }

    void updateScaleDelta(float delta)
    {
        scaleDelta = delta;
    }

    void endDrag()
    {
        dragging = false;
        dragEnded = true;
        operation = GizmoOperation::None;
        axis = GizmoAxis::None;
    }

    void reset()
    {
        dragging = false;
        dragStarted = false;
        dragEnded = false;
        operation = GizmoOperation::None;
        axis = GizmoAxis::None;
        dragStart = glm::vec2(0.0f);
        screenDelta = glm::vec2(0.0f);
        worldDelta = glm::vec3(0.0f);
        rotationDelta = 0.0f;
        scaleDelta = 0.0f;
    }

    void clearFrameFlags()
    {
        dragStarted = false;
        dragEnded = false;
    }
};

} // namespace dk::ui
