#pragma once
#include <vector>
#include <memory>
#include "glm/glm.hpp"

#include "vk_engine.h"
#include "Vulkan/Context.h"
#include "Vulkan/Buffer.h"
#include "Vulkan/DescriptorSetLayout.h"
#include "Vulkan/DescriptorSet.h"
#include "Vulkan/DescriptorSetPool.h"
#include "Vulkan/DescriptorWriter.h"
#include "Vulkan/Pipeline.h"
#include "Vulkan/PipelineLayout.h"
#include "Vulkan/ShaderModule.h"

// 你的 CameraData 应该已在工程里（与 SpringRenderer 一致）
// struct CameraData { glm::mat4 view, proj, viewproj; glm::vec4 eye; ... };

namespace dk {
class MacGrid;

struct GPUVector
{
    glm::vec4 pos; // world position (xyz), w 未用
    glm::vec4 vel; // velocity (xyz), w 可放 magnitude/备用
};

struct PushVector
{
    glm::mat4 model;       // 一般为单位
    float     baseScale;   // 全局尺寸缩放（像素/世界单位转换放在这里做）
    float     headRatio;   // 箭头头部长度占比 [0.1, 0.8]，默认 0.35
    float     halfWidth;   // 尾部半宽（世界单位）
    float     magScale;    // 速度幅值→长度的比例（乘到 |v|）
};

class MacGridVectorRenderer
{
public:
    explicit MacGridVectorRenderer(vkcore::VulkanContext* ctx) : _context(ctx)
    {
    }

    ~MacGridVectorRenderer() = default;

    void init(vk::Format color_format, vk::Format depth_format);

    // 每帧更新：把 CPU 的向量写进 SSBO
    void updateVectors(const std::vector<GPUVector>& cpu_vectors);

    // 录制绘制命令
    void draw(vkcore::CommandBuffer& cmd,
              const CameraData&      camera,
              VkRenderingInfo&       render_info,
              const PushVector&      push);

    void cleanup();

    // 从 MACGrid 采样生成矢量（默认在 cell center，支持 stride 下采样）
// stride = {2,2,2} 表示每隔 2 个 cell 取一个箭头，避免太密
    void updateFromGrid(const MacGrid& g, glm::ivec3 stride = { 2,2,2 },
        float minMagnitude = 0.0f, float maxMagnitude = 1e9f);

private:
    void createBuffers();
    void createDescriptors();
    void createPipeline(vk::Format color_format, vk::Format depth_format);

    vkcore::VulkanContext* _context{};

    uint32_t _vector_count = 0;
    uint32_t _max_vectors  = 0;

    // GPU 资源
    std::unique_ptr<vkcore::BufferResource> _vector_ssbo;
    std::unique_ptr<vkcore::BufferResource> _camera_ubo;

    // 描述符
    std::unique_ptr<vkcore::DescriptorSetLayout>         _layout;
    std::unique_ptr<vkcore::GrowableDescriptorAllocator> _frame_allocator;

    // 管线
    std::unique_ptr<vkcore::PipelineLayout> _pipeline_layout;
    std::unique_ptr<vkcore::Pipeline>       _pipeline;

    // CPU 暂存
    std::vector<GPUVector> _cpu_stage;
};
} // namespace dk
