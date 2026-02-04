#pragma once

#include <cmath>

#include <glm/geometric.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <SDL3/SDL_video.h>

#include "core/input/InputContext.h"

namespace dk::tool {

struct Ray
{
    glm::vec3 origin{};
    glm::vec3 direction{};
};

inline bool computeMouseRay(const dk::input::InputContext& ctx, const glm::vec2& mouse, Ray& outRay)
{
    if (!ctx.window || !ctx.camera)
    {
        return false;
    }

    int width = 0;
    int height = 0;
    SDL_GetWindowSize(ctx.window, &width, &height);
    if (width <= 0 || height <= 0)
    {
        return false;
    }

    const float ndc_x = (2.0f * mouse.x) / static_cast<float>(width) - 1.0f;
    const float ndc_y = 1.0f - (2.0f * mouse.y) / static_cast<float>(height);

    const glm::mat4 view = ctx.camera->getViewMatrix();
    const glm::mat4 viewproj = ctx.camera->projection * view;
    const glm::mat4 inv_viewproj = glm::inverse(viewproj);

    const glm::vec4 near_clip = inv_viewproj * glm::vec4(ndc_x, ndc_y, 0.0f, 1.0f);
    const glm::vec4 far_clip = inv_viewproj * glm::vec4(ndc_x, ndc_y, 1.0f, 1.0f);
    const glm::vec3 near_world = glm::vec3(near_clip) / near_clip.w;
    const glm::vec3 far_world = glm::vec3(far_clip) / far_clip.w;

    outRay.origin = near_world;
    outRay.direction = glm::normalize(far_world - near_world);
    return true;
}

inline bool intersectAxis(const Ray& ray,
                          const glm::vec3& axisOrigin,
                          const glm::vec3& axisDir,
                          float axisLength,
                          float threshold,
                          float& outAxisT,
                          glm::vec3& outPoint,
                          float& outScore)
{
    const glm::vec3 w0 = ray.origin - axisOrigin;
    const float a = glm::dot(ray.direction, ray.direction);
    const float b = glm::dot(ray.direction, axisDir);
    const float c = glm::dot(axisDir, axisDir);
    const float d = glm::dot(ray.direction, w0);
    const float e = glm::dot(axisDir, w0);
    const float denom = a * c - b * b;
    if (std::abs(denom) < 1e-5f)
    {
        return false;
    }

    const float s = (b * e - c * d) / denom;
    const float t = (a * e - b * d) / denom;
    if (s < 0.0f || t < 0.0f || t > axisLength)
    {
        return false;
    }

    const glm::vec3 p_ray = ray.origin + s * ray.direction;
    const glm::vec3 p_axis = axisOrigin + t * axisDir;
    const float dist = glm::length(p_ray - p_axis);
    if (dist > threshold)
    {
        return false;
    }

    outAxisT = t;
    outPoint = p_axis;
    outScore = dist;
    return true;
}

inline bool closestAxisParam(const Ray& ray,
                             const glm::vec3& axisOrigin,
                             const glm::vec3& axisDir,
                             float& outAxisT)
{
    const glm::vec3 w0 = ray.origin - axisOrigin;
    const float a = glm::dot(ray.direction, ray.direction);
    const float b = glm::dot(ray.direction, axisDir);
    const float c = glm::dot(axisDir, axisDir);
    const float d = glm::dot(ray.direction, w0);
    const float e = glm::dot(axisDir, w0);
    const float denom = a * c - b * b;
    if (std::abs(denom) < 1e-5f)
    {
        return false;
    }

    outAxisT = (a * e - b * d) / denom;
    return true;
}

inline bool intersectPlane(const Ray& ray,
                           const glm::vec3& planeOrigin,
                           const glm::vec3& planeNormal,
                           const glm::vec3& axisU,
                           const glm::vec3& axisV,
                           float minExtent,
                           float maxExtent,
                           glm::vec3& outPoint,
                           float& outScore)
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

    const glm::vec3 hit = ray.origin + t * ray.direction;
    const glm::vec3 local = hit - planeOrigin;
    const float u = glm::dot(local, axisU);
    const float v = glm::dot(local, axisV);
    if (u < minExtent || v < minExtent || u > maxExtent || v > maxExtent)
    {
        return false;
    }

    outPoint = hit;
    outScore = glm::length(local);
    return true;
}

} // namespace dk::tool
