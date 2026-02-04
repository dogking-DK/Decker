#include "ui/gizmo/TranslateGizmo.h"

#include <algorithm>
#include <limits>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "SceneTypes.h"
#include "render/UiRenderService.h"
#include "tool/ray.h"

namespace dk::ui {
namespace {
constexpr float kHighlightBoost = 1.35f;

glm::mat4 to_matrix(const Transform& t)
{
    const glm::mat4 translation = glm::translate(glm::mat4(1.0f), t.translation);
    const glm::mat4 rotation    = glm::mat4_cast(t.rotation);
    const glm::mat4 scale       = glm::scale(glm::mat4(1.0f), t.scale);
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
        world *= to_matrix((*it)->local_transform);
    }
    return world;
}

float computeGizmoScale(const glm::vec3& origin, const dk::Camera& camera)
{
    const float distance = glm::length(origin - camera.position);
    return std::max(0.1f, distance * 0.15f);
}

struct HitResult
{
    GizmoAxis axis{GizmoAxis::None};
    float     score{std::numeric_limits<float>::max()};
    glm::vec3 point{};
    glm::vec3 axisDir{};
    glm::vec3 planeNormal{};
    float     axisT{0.0f};
};

HitResult pickAxis(const tool::Ray& ray,
                   const glm::vec3& origin,
                   const glm::vec3& axisX,
                   const glm::vec3& axisY,
                   const glm::vec3& axisZ,
                   float axisLength)
{
    HitResult result;
    const float axisThreshold = axisLength * 0.08f;
    const float planeMin = axisLength * 0.2f;
    const float planeMax = axisLength * 0.45f;

    float axisT = 0.0f;
    glm::vec3 hitPoint{};
    float score = 0.0f;

    if (tool::intersectAxis(ray, origin, axisX, axisLength, axisThreshold, axisT, hitPoint, score))
    {
        result = {GizmoAxis::X, score, hitPoint, axisX, axisZ, axisT};
    }

    if (tool::intersectAxis(ray, origin, axisY, axisLength, axisThreshold, axisT, hitPoint, score)
        && score < result.score)
    {
        result = {GizmoAxis::Y, score, hitPoint, axisY, axisX, axisT};
    }

    if (tool::intersectAxis(ray, origin, axisZ, axisLength, axisThreshold, axisT, hitPoint, score)
        && score < result.score)
    {
        result = {GizmoAxis::Z, score, hitPoint, axisZ, axisY, axisT};
    }

    if (tool::intersectPlane(ray, origin, axisZ, axisX, axisY, planeMin, planeMax, hitPoint, score)
        && score < result.score)
    {
        result = {GizmoAxis::XY, score, hitPoint, axisX, axisZ, 0.0f};
        result.planeNormal = axisZ;
    }

    if (tool::intersectPlane(ray, origin, axisX, axisY, axisZ, planeMin, planeMax, hitPoint, score)
        && score < result.score)
    {
        result = {GizmoAxis::YZ, score, hitPoint, axisY, axisX, 0.0f};
        result.planeNormal = axisX;
    }

    if (tool::intersectPlane(ray, origin, axisY, axisZ, axisX, planeMin, planeMax, hitPoint, score)
        && score < result.score)
    {
        result = {GizmoAxis::ZX, score, hitPoint, axisZ, axisY, 0.0f};
        result.planeNormal = axisY;
    }

    return result;
}
} // namespace

bool TranslateGizmo::handleInput(const dk::input::InputState& state, const dk::input::InputContext& ctx)
{
    if (!_state)
    {
        return false;
    }

    if (!ctx.selectedNode || !isEnabled())
    {
        _state->reset();
        return false;
    }

    if (ctx.imguiWantsMouse)
    {
        return false;
    }

    const auto& mouse = state.mouse();

    if (mouse.leftPressed)
    {
        tool::Ray ray{};
        if (!tool::computeMouseRay(ctx, mouse.position, ray))
        {
            return false;
        }

        const glm::mat4 world = worldTransform(ctx.selectedNode);
        const glm::vec3 origin = glm::vec3(world[3]);
        const glm::vec3 axisX = glm::normalize(glm::vec3(world[0]));
        const glm::vec3 axisY = glm::normalize(glm::vec3(world[1]));
        const glm::vec3 axisZ = glm::normalize(glm::vec3(world[2]));
        const float axisLength = computeGizmoScale(origin, *ctx.camera);
        HitResult hit = pickAxis(ray, origin, axisX, axisY, axisZ, axisLength);
        if (hit.axis == GizmoAxis::None)
        {
            return false;
        }

        _state->beginDrag(mouse.position, GizmoOperation::Translate, hit.axis);
        _dragOrigin = origin;
        _dragStartWorld = hit.point;
        _dragAxisDir = hit.axisDir;
        _dragPlaneNormal = hit.planeNormal;
        _dragStartAxisT = hit.axisT;
        return true;
    }

    if (_state->dragging && mouse.leftDown)
    {
        glm::vec2 delta = mouse.position - _state->dragStart;
        tool::Ray ray{};
        glm::vec3 worldDelta(0.0f);

        if (tool::computeMouseRay(ctx, mouse.position, ray))
        {
            if (_state->axis == GizmoAxis::X || _state->axis == GizmoAxis::Y || _state->axis == GizmoAxis::Z)
            {
                float axisT = 0.0f;
                if (tool::closestAxisParam(ray, _dragOrigin, _dragAxisDir, axisT))
                {
                    worldDelta = (axisT - _dragStartAxisT) * _dragAxisDir;
                }
            }
            else
            {
                glm::vec3 hitPoint{};
                float score = 0.0f;
                if (tool::intersectPlane(ray, _dragOrigin, _dragPlaneNormal, _dragAxisDir,
                                         glm::cross(_dragPlaneNormal, _dragAxisDir), -1e6f, 1e6f, hitPoint, score))
                {
                    worldDelta = hitPoint - _dragStartWorld;
                }
            }
        }

        _state->updateDeltas(delta, worldDelta);
        return true;
    }

    if (_state->dragging && mouse.leftReleased)
    {
        glm::vec2 delta = mouse.position - _state->dragStart;
        tool::Ray ray{};
        glm::vec3 worldDelta(0.0f);

        if (tool::computeMouseRay(ctx, mouse.position, ray))
        {
            if (_state->axis == GizmoAxis::X || _state->axis == GizmoAxis::Y || _state->axis == GizmoAxis::Z)
            {
                float axisT = 0.0f;
                if (tool::closestAxisParam(ray, _dragOrigin, _dragAxisDir, axisT))
                {
                    worldDelta = (axisT - _dragStartAxisT) * _dragAxisDir;
                }
            }
            else
            {
                glm::vec3 hitPoint{};
                float score = 0.0f;
                if (tool::intersectPlane(ray, _dragOrigin, _dragPlaneNormal, _dragAxisDir,
                                         glm::cross(_dragPlaneNormal, _dragAxisDir), -1e6f, 1e6f, hitPoint, score))
                {
                    worldDelta = hitPoint - _dragStartWorld;
                }
            }
        }

        _state->updateDeltas(delta, worldDelta);
        _state->endDrag();
        return true;
    }

    return false;
}

void TranslateGizmo::render(const GizmoContext& ctx)
{
    if (!ctx.camera || !ctx.selectedNode || !ctx.uiRenderService)
    {
        return;
    }

    const glm::mat4 world = worldTransform(ctx.selectedNode);
    const glm::vec3 origin = glm::vec3(world[3]);
    const glm::vec3 axisX = glm::normalize(glm::vec3(world[0]));
    const glm::vec3 axisY = glm::normalize(glm::vec3(world[1]));
    const glm::vec3 axisZ = glm::normalize(glm::vec3(world[2]));
    const float axisLength = computeGizmoScale(origin, *ctx.camera);
    const float planeMin = axisLength * 0.2f;
    const float planeMax = axisLength * 0.45f;

    auto axisColor = [&](const glm::vec4& base, bool highlight)
    {
        return highlight ? glm::vec4(base.r * kHighlightBoost,
                                     base.g * kHighlightBoost,
                                     base.b * kHighlightBoost,
                                     base.a)
                         : base;
    };

    const bool highlightX = _state && _state->axis == GizmoAxis::X;
    const bool highlightY = _state && _state->axis == GizmoAxis::Y;
    const bool highlightZ = _state && _state->axis == GizmoAxis::Z;
    const bool highlightXY = _state && _state->axis == GizmoAxis::XY;
    const bool highlightYZ = _state && _state->axis == GizmoAxis::YZ;
    const bool highlightZX = _state && _state->axis == GizmoAxis::ZX;

    ctx.uiRenderService->drawAxis(origin, axisX, axisLength, axisColor({0.85f, 0.2f, 0.2f, 1.0f}, highlightX));
    ctx.uiRenderService->drawAxis(origin, axisY, axisLength, axisColor({0.2f, 0.85f, 0.2f, 1.0f}, highlightY));
    ctx.uiRenderService->drawAxis(origin, axisZ, axisLength, axisColor({0.2f, 0.5f, 0.9f, 1.0f}, highlightZ));

    ctx.uiRenderService->drawPlane(origin, axisX, axisY, planeMin, planeMax,
                                   axisColor({0.85f, 0.85f, 0.2f, 0.8f}, highlightXY));
    ctx.uiRenderService->drawPlane(origin, axisY, axisZ, planeMin, planeMax,
                                   axisColor({0.2f, 0.85f, 0.85f, 0.8f}, highlightYZ));
    ctx.uiRenderService->drawPlane(origin, axisZ, axisX, planeMin, planeMax,
                                   axisColor({0.85f, 0.2f, 0.85f, 0.8f}, highlightZX));
}

} // namespace dk::ui
