#include "render/UiRenderService.h"

#include <algorithm>

#include "Vulkan/CommandBuffer.h"

namespace dk::render {

UiRenderService::UiRenderService(vkcore::VulkanContext& ctx, vkcore::UploadContext& upload_ctx)
    : _context(ctx)
    , _upload_ctx(upload_ctx)
{
}

void UiRenderService::beginFrame()
{
    for (auto& batch : _line_batches)
    {
        batch.vertices.clear();
        batch.line_count = 0;
    }
}

void UiRenderService::finalize()
{
    for (auto& batch : _line_batches)
    {
        const uint32_t vertex_count = static_cast<uint32_t>(batch.vertices.size());
        if (vertex_count == 0)
        {
            batch.line_count = 0;
            continue;
        }

        const size_t bytes = sizeof(LineVertex) * batch.vertices.size();
        ensureLineBuffer(batch, bytes);
        vkcore::upload_buffer_data_immediate(_upload_ctx,
                                             batch.vertices.data(),
                                             static_cast<vk::DeviceSize>(bytes),
                                             *batch.buffer,
                                             0,
                                             vkcore::BufferUsage::StorageBuffer);
        batch.line_count = vertex_count / 2;
    }
}

void UiRenderService::drawLine(const glm::vec3& start, const glm::vec3& end, const glm::vec4& color)
{
    auto& batch = getOrCreateBatch(color);
    batch.vertices.push_back({glm::vec4(start, 1.0f)});
    batch.vertices.push_back({glm::vec4(end, 1.0f)});
}

void UiRenderService::drawAxis(const glm::vec3& origin, const glm::vec3& dir, float length, const glm::vec4& color)
{
    drawLine(origin, origin + dir * length, color);
}

void UiRenderService::drawPlane(const glm::vec3& origin,
                                const glm::vec3& axisU,
                                const glm::vec3& axisV,
                                float minExtent,
                                float maxExtent,
                                const glm::vec4& color)
{
    const glm::vec3 p0 = origin + axisU * minExtent + axisV * minExtent;
    const glm::vec3 p1 = origin + axisU * maxExtent + axisV * minExtent;
    const glm::vec3 p2 = origin + axisU * maxExtent + axisV * maxExtent;
    const glm::vec3 p3 = origin + axisU * minExtent + axisV * maxExtent;

    drawLine(p0, p1, color);
    drawLine(p1, p2, color);
    drawLine(p2, p3, color);
    drawLine(p3, p0, color);
}

void UiRenderService::drawPoint(const glm::vec3& position, float size, const glm::vec4& color)
{
    const float half = size * 0.5f;
    drawLine(position + glm::vec3(-half, 0.0f, 0.0f), position + glm::vec3(half, 0.0f, 0.0f), color);
    drawLine(position + glm::vec3(0.0f, -half, 0.0f), position + glm::vec3(0.0f, half, 0.0f), color);
    drawLine(position + glm::vec3(0.0f, 0.0f, -half), position + glm::vec3(0.0f, 0.0f, half), color);
}

UiRenderService::LineBatch& UiRenderService::getOrCreateBatch(const glm::vec4& color)
{
    auto iter = std::find_if(_line_batches.begin(), _line_batches.end(),
                             [&](const LineBatch& batch)
                             {
                                 return batch.color == color;
                             });
    if (iter != _line_batches.end())
    {
        return *iter;
    }

    LineBatch batch{};
    batch.color = color;
    _line_batches.push_back(std::move(batch));
    return _line_batches.back();
}

void UiRenderService::ensureLineBuffer(LineBatch& batch, size_t bytes)
{
    if (batch.buffer && batch.buffer_size >= bytes)
    {
        return;
    }

    vkcore::BufferResource::Builder builder;
    builder.setSize(bytes)
           .setUsage(vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst)
           .withVmaUsage(VMA_MEMORY_USAGE_GPU_ONLY);

    batch.buffer = builder.buildUnique(_context);
    batch.buffer_size = bytes;
}

} // namespace dk::render
