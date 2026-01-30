#include "DebugRenderService.h"

#include <stack>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include "SceneTypes.h"
#include "Vulkan/CommandBuffer.h"

namespace dk::render {
namespace {
glm::mat4 to_matrix(const Transform& t)
{
    glm::mat4 translation = glm::translate(glm::mat4(1.0f), t.translation);
    glm::mat4 rotation    = glm::mat4_cast(t.rotation);
    glm::mat4 scale       = glm::scale(glm::mat4(1.0f), t.scale);
    return translation * rotation * scale;
}
} // namespace

DebugRenderService::DebugRenderService(vkcore::VulkanContext& ctx, vkcore::UploadContext& upload_ctx)
    : _context(ctx)
    , _upload_ctx(upload_ctx)
{
}

void DebugRenderService::setEnabled(bool enabled)
{
    _enabled = enabled;
    if (!_enabled)
    {
        _line_vertices.clear();
        _line_count = 0;
    }
}

void DebugRenderService::updateFromScene(const Scene& scene)
{
    if (!_enabled)
    {
        return;
    }

    _line_vertices.clear();

    auto root = scene.getRoot();
    if (!root)
    {
        _line_count = 0;
        return;
    }

    struct StackEntry
    {
        std::shared_ptr<SceneNode> node;
        glm::mat4                  parent_transform;
        bool                       parent_visible;
    };

    std::stack<StackEntry> stack;
    stack.push({root, glm::mat4(1.0f), true});

    while (!stack.empty())
    {
        auto entry = stack.top();
        stack.pop();

        auto& node = entry.node;
        const glm::mat4 world = entry.parent_transform * to_matrix(node->local_transform);
        const bool node_visible = entry.parent_visible && node->visible;

        if (node_visible)
        {
            if (auto* bounds_component = node->getComponent<BoundsComponent>())
            {
                if (bounds_component->local_bounds.valid())
                {
                    appendAabbLines(bounds_component->local_bounds.transform(world));
                }
            }
        }

        for (const auto& child : node->children)
        {
            if (child)
            {
                stack.push({child, world, node_visible});
            }
        }
    }

    const uint32_t vertex_count = static_cast<uint32_t>(_line_vertices.size());
    if (vertex_count == 0)
    {
        _line_count = 0;
        return;
    }

    const size_t bytes = sizeof(LineVertex) * _line_vertices.size();
    ensureLineBuffer(bytes);
    vkcore::upload_buffer_data_immediate(_upload_ctx,
                                         _line_vertices.data(),
                                         static_cast<vk::DeviceSize>(bytes),
                                         *_line_buffer,
                                         0,
                                         vkcore::BufferUsage::StorageBuffer);
    _line_count = vertex_count / 2;
}

void DebugRenderService::appendAabbLines(const AABB& bounds)
{
    const glm::vec3& min = bounds.min;
    const glm::vec3& max = bounds.max;

    const glm::vec3 corners[] = {
        {min.x, min.y, min.z},
        {min.x, min.y, max.z},
        {min.x, max.y, min.z},
        {min.x, max.y, max.z},
        {max.x, min.y, min.z},
        {max.x, min.y, max.z},
        {max.x, max.y, min.z},
        {max.x, max.y, max.z},
    };

    constexpr uint8_t edges[][2] = {
        {0, 1}, {1, 3}, {3, 2}, {2, 0},
        {4, 5}, {5, 7}, {7, 6}, {6, 4},
        {0, 4}, {1, 5}, {2, 6}, {3, 7},
    };

    for (const auto& edge : edges)
    {
        _line_vertices.push_back({glm::vec4(corners[edge[0]], 1.0f)});
        _line_vertices.push_back({glm::vec4(corners[edge[1]], 1.0f)});
    }
}

void DebugRenderService::ensureLineBuffer(size_t bytes)
{
    if (_line_buffer && _line_buffer_size >= bytes)
    {
        return;
    }

    vkcore::BufferResource::Builder builder;
    builder.setSize(bytes)
           .setUsage(vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst)
           .withVmaUsage(VMA_MEMORY_USAGE_GPU_ONLY);

    _line_buffer = builder.buildUnique(_context);
    _line_buffer_size = bytes;
}
} // namespace dk::render
