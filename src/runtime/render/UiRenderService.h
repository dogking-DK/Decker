#pragma once

#include <memory>
#include <vector>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "Vulkan/Buffer.h"
#include "Vulkan/Context.h"

namespace dk::vkcore {
struct UploadContext;
}

namespace dk::render {

class UiRenderService
{
public:
    struct LineVertex
    {
        glm::vec4 position{};
    };

    struct LineBatch
    {
        glm::vec4 color{};
        std::vector<LineVertex> vertices;
        std::unique_ptr<vkcore::BufferResource> buffer;
        uint32_t line_count{0};
        size_t   buffer_size{0};
    };

    UiRenderService(vkcore::VulkanContext& ctx, vkcore::UploadContext& upload_ctx);

    void beginFrame();
    void finalize();

    void drawLine(const glm::vec3& start, const glm::vec3& end, const glm::vec4& color);
    void drawAxis(const glm::vec3& origin, const glm::vec3& dir, float length, const glm::vec4& color);
    void drawPlane(const glm::vec3& origin,
                   const glm::vec3& axisU,
                   const glm::vec3& axisV,
                   float minExtent,
                   float maxExtent,
                   const glm::vec4& color);
    void drawPoint(const glm::vec3& position, float size, const glm::vec4& color);

    const std::vector<LineBatch>& lineBatches() const { return _line_batches; }

private:
    LineBatch& getOrCreateBatch(const glm::vec4& color);
    void ensureLineBuffer(LineBatch& batch, size_t bytes);

    vkcore::VulkanContext& _context;
    vkcore::UploadContext& _upload_ctx;
    std::vector<LineBatch> _line_batches;
};

} // namespace dk::render
