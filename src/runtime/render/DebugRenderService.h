#pragma once

#include <memory>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "BVH/AABB.hpp"
#include "Scene.h"
#include "Vulkan/Buffer.h"
#include "Vulkan/Context.h"

namespace dk::render {
class DebugRenderService
{
public:
    struct LineVertex
    {
        glm::vec4 position{};
    };

    DebugRenderService(vkcore::VulkanContext& ctx, vkcore::UploadContext& upload_ctx);

    void setEnabled(bool enabled);
    bool isEnabled() const { return _enabled; }

    void updateFromScene(const Scene& scene);

    const vkcore::BufferResource* lineBuffer() const { return _line_buffer.get(); }
    uint32_t                      lineCount() const { return _line_count; }

private:
    void appendAabbLines(const AABB& bounds);
    void ensureLineBuffer(size_t bytes);

    vkcore::VulkanContext& _context;
    vkcore::UploadContext& _upload_ctx;
    bool                   _enabled{false};
    std::vector<LineVertex> _line_vertices;
    std::unique_ptr<vkcore::BufferResource> _line_buffer;
    uint32_t _line_count{0};
    size_t   _line_buffer_size{0};
};
} // namespace dk::render
