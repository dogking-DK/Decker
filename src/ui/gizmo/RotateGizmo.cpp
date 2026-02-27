#include "ui/gizmo/RotateGizmo.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
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
        world *= to_matrix((*it)->local_transform);
    }
    return world;
}

float computeGizmoScale(const glm::vec3& origin, const dk::Camera& camera)
{
    const float distance = glm::length(origin - camera.position);
    return std::max(0.1f, distance * 0.15f);
}

glm::vec3 computeGizmoOrigin(const dk::SceneNode* node, const glm::mat4& world)
{
    if (!node)
    {
        return glm::vec3(world[3]);
    }

    if (auto* bounds_component = node->getComponent<BoundsComponent>())
    {
        if (bounds_component->local_bounds.valid())
        {
            const AABB world_bounds = bounds_component->local_bounds.transform(world);
            if (world_bounds.valid())
            {
                return world_bounds.center();
            }
        }
    }

    return glm::vec3(world[3]);
}

glm::vec3 safeNormalize(const glm::vec3& v, const glm::vec3& fallback)
{
    const float len = glm::length(v);
    if (len < 1e-5f)
    {
        return fallback;
    }
    return v / len;
}

bool intersectRayPlane(const tool::Ray& ray,
                       const glm::vec3& planeOrigin,
                       const glm::vec3& planeNormal,
                       glm::vec3& outPoint,
                       float& outT)
{
    const float denom = glm::dot(ray.direction, planeNormal);
    if (std::abs(denom) < 1e-5f)
    {
        return false;
    }

    const float t = glm::dot(planeOrigin - ray.origin, planeNormal) / denom;
    if (t < 0.0f)
    {
        return false;
    }

    outPoint = ray.origin + t * ray.direction;
    outT = t;
    return true;
}

float signedAngleOnPlane(const glm::vec3& start,
                         const glm::vec3& current,
                         const glm::vec3& planeNormal)
{
    const glm::vec3 s = safeNormalize(start - planeNormal * glm::dot(start, planeNormal), start);
    const glm::vec3 c = safeNormalize(current - planeNormal * glm::dot(current, planeNormal), current);

    const float dotValue = glm::clamp(glm::dot(s, c), -1.0f, 1.0f);
    const float angle = std::acos(dotValue);
    const float sign = glm::dot(glm::cross(s, c), planeNormal) >= 0.0f ? 1.0f : -1.0f;
    return angle * sign;
}

void buildPlaneBasis(const glm::vec3& normal, glm::vec3& outU, glm::vec3& outV)
{
    const glm::vec3 tangent = std::abs(normal.y) < 0.95f ? glm::cross(normal, glm::vec3(0.0f, 1.0f, 0.0f))
                                                          : glm::cross(normal, glm::vec3(1.0f, 0.0f, 0.0f));
    outU = safeNormalize(tangent, glm::vec3(1.0f, 0.0f, 0.0f));
    outV = safeNormalize(glm::cross(normal, outU), glm::vec3(0.0f, 0.0f, 1.0f));
}

void drawRing(dk::render::UiRenderService& ui,
              const glm::vec3& origin,
              const glm::vec3& normal,
              float radius,
              const glm::vec4& color)
{
    glm::vec3 axisU{};
    glm::vec3 axisV{};
    buildPlaneBasis(normal, axisU, axisV);

    constexpr int kSegments = 64;
    for (int i = 0; i < kSegments; ++i)
    {
        const float t0 = (glm::two_pi<float>() * static_cast<float>(i)) / static_cast<float>(kSegments);
        const float t1 = (glm::two_pi<float>() * static_cast<float>(i + 1)) / static_cast<float>(kSegments);
        const glm::vec3 p0 = origin + (std::cos(t0) * axisU + std::sin(t0) * axisV) * radius;
        const glm::vec3 p1 = origin + (std::cos(t1) * axisU + std::sin(t1) * axisV) * radius;
        ui.drawLine(p0, p1, color);
    }
}

struct RingHit
{
    GizmoAxis axis{GizmoAxis::None};
    glm::vec3 normal{};
    glm::vec3 point{};
    float score{std::numeric_limits<float>::max()};
};

void testRing(const tool::Ray& ray,
              const glm::vec3& origin,
              const glm::vec3& ringNormal,
              GizmoAxis axis,
              float radius,
              float thickness,
              RingHit& ioBest)
{
    glm::vec3 hitPoint{};
    float hitT = 0.0f;
    if (!intersectRayPlane(ray, origin, ringNormal, hitPoint, hitT))
    {
        return;
    }

    const glm::vec3 fromCenter = hitPoint - origin;
    const float ringDistance = glm::length(fromCenter);
    if (ringDistance < 1e-4f)
    {
        return;
    }

    const float distanceToRing = std::abs(ringDistance - radius);
    if (distanceToRing > thickness)
    {
        return;
    }

    const float score = distanceToRing + hitT * 0.001f;
    if (score < ioBest.score)
    {
        ioBest.axis = axis;
        ioBest.normal = ringNormal;
        ioBest.point = hitPoint;
        ioBest.score = score;
    }
}

RingHit pickRing(const tool::Ray& ray,
                 const glm::vec3& origin,
                 const glm::vec3& axisX,
                 const glm::vec3& axisY,
                 const glm::vec3& axisZ,
                 float radius,
                 float thickness)
{
    RingHit result{};
    testRing(ray, origin, axisX, GizmoAxis::X, radius, thickness, result);
    testRing(ray, origin, axisY, GizmoAxis::Y, radius, thickness, result);
    testRing(ray, origin, axisZ, GizmoAxis::Z, radius, thickness, result);
    return result;
}

} // namespace

bool RotateGizmo::handleInput(const dk::input::InputState& state, const dk::input::InputContext& ctx)
{
    if (!_state || !ctx.rotateEnabled)
    {
        return false;
    }

    if (!ctx.selectedNode || !isEnabled())
    {
        _state->reset();
        return false;
    }

    const auto& mouse = state.mouse();

    if (ctx.imguiWantsMouse)
    {
        if (_state->dragging && mouse.leftReleased)
        {
            _state->endDrag();
            return true;
        }
        return false;
    }

    const glm::mat4 world = worldTransform(ctx.selectedNode);
    const glm::vec3 origin = computeGizmoOrigin(ctx.selectedNode, world);
    const glm::vec3 axisX = safeNormalize(glm::vec3(world[0]), glm::vec3(1.0f, 0.0f, 0.0f));
    const glm::vec3 axisY = safeNormalize(glm::vec3(world[1]), glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::vec3 axisZ = safeNormalize(glm::vec3(world[2]), glm::vec3(0.0f, 0.0f, 1.0f));
    const float gizmoScale = computeGizmoScale(origin, *ctx.camera);
    const float ringRadius = gizmoScale * 0.75f;
    const float ringThickness = gizmoScale * 0.12f;

    auto updateDragAngle = [&](float& outAngle) -> bool
    {
        tool::Ray ray{};
        if (!tool::computeMouseRay(ctx, mouse.position, ray))
        {
            return false;
        }

        glm::vec3 hitPoint{};
        float hitT = 0.0f;
        if (!intersectRayPlane(ray, _dragOrigin, _dragAxisWorld, hitPoint, hitT))
        {
            return false;
        }

        const glm::vec3 fromCenter = hitPoint - _dragOrigin;
        if (glm::length(fromCenter) < 1e-5f)
        {
            return false;
        }

        const glm::vec3 currentVec = glm::normalize(fromCenter);
        outAngle = signedAngleOnPlane(_dragStartVectorWorld, currentVec, _dragAxisWorld);
        return true;
    };

    if (mouse.leftPressed)
    {
        tool::Ray ray{};
        if (!tool::computeMouseRay(ctx, mouse.position, ray))
        {
            return false;
        }

        const RingHit hit = pickRing(ray, origin, axisX, axisY, axisZ, ringRadius, ringThickness);
        if (hit.axis == GizmoAxis::None)
        {
            return false;
        }

        _state->beginDrag(mouse.position, GizmoOperation::Rotate, hit.axis);
        _dragOrigin = origin;
        _dragAxisWorld = hit.normal;
        _dragStartVectorWorld = safeNormalize(hit.point - origin, axisX);
        _state->updateRotationDelta(0.0f);
        return true;
    }

    if (_state->dragging && mouse.leftDown)
    {
        float angle = 0.0f;
        if (updateDragAngle(angle))
        {
            _state->updateRotationDelta(angle);
        }
        _state->updateDeltas(mouse.position - _state->dragStart, glm::vec3(0.0f));
        return true;
    }

    if (_state->dragging && mouse.leftReleased)
    {
        float angle = 0.0f;
        if (updateDragAngle(angle))
        {
            _state->updateRotationDelta(angle);
        }
        _state->updateDeltas(mouse.position - _state->dragStart, glm::vec3(0.0f));
        _state->endDrag();
        return true;
    }

    return false;
}

void RotateGizmo::render(const GizmoContext& ctx)
{
    if (!ctx.camera || !ctx.selectedNode || !ctx.uiRenderService || !ctx.rotateEnabled)
    {
        return;
    }

    const glm::mat4 world = worldTransform(ctx.selectedNode);
    const glm::vec3 origin = computeGizmoOrigin(ctx.selectedNode, world);
    const glm::vec3 axisX = safeNormalize(glm::vec3(world[0]), glm::vec3(1.0f, 0.0f, 0.0f));
    const glm::vec3 axisY = safeNormalize(glm::vec3(world[1]), glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::vec3 axisZ = safeNormalize(glm::vec3(world[2]), glm::vec3(0.0f, 0.0f, 1.0f));
    const float ringRadius = computeGizmoScale(origin, *ctx.camera) * 0.75f;

    auto ringColor = [&](const glm::vec4& base, bool highlight)
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

    drawRing(*ctx.uiRenderService, origin, axisX, ringRadius, ringColor({0.90f, 0.25f, 0.25f, 1.0f}, highlightX));
    drawRing(*ctx.uiRenderService, origin, axisY, ringRadius, ringColor({0.25f, 0.90f, 0.25f, 1.0f}, highlightY));
    drawRing(*ctx.uiRenderService, origin, axisZ, ringRadius, ringColor({0.25f, 0.55f, 0.95f, 1.0f}, highlightZ));
}

} // namespace dk::ui
