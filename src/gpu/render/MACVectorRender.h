// MacGridVectorRender.h
#pragma once
#include <vector>
#include <memory>
#include "glm/glm.hpp"

#include "vk_engine.h"
#include "Vulkan/Context.h"
#include "Vulkan/Buffer.h"
#include "Vulkan/BufferBuilder.h"
#include "Vulkan/CommandBuffer.h"
#include "Vulkan/DescriptorSetLayout.h"
#include "Vulkan/DescriptorSet.h"
#include "Vulkan/DescriptorSetPool.h"
#include "Vulkan/DescriptorWriter.h"
#include "Vulkan/Pipeline.h"
#include "Vulkan/PipelineLayout.h"
#include "Vulkan/ShaderModule.h"

#include "data/MacGrid.h" // 取速度、中心点

namespace dk {

    /// 与 shader 对齐的矢量条目
    struct GPUVector
    {
        glm::vec4 pos; // world position (xyz), w 未用
        glm::vec4 vel; // velocity (xyz), w 可放 magnitude/备用
    };

    /// 绘制参数 UBO（与 shader 对齐）
    /// 注意：CameraData 由你现有系统维护；这里单独放 count/缩放与阈值
    struct DrawParams
    {
        uint32_t count;
        float    arrowScale;
        float    vmin;
        float    vmax;
    };

    class MacGridVectorRenderer
    {
    public:
        explicit MacGridVectorRenderer(vkcore::VulkanContext* ctx) : _ctx(ctx) {}

        // 初始化/销毁
        void init(vk::Format color_format, vk::Format depth_format);
        void cleanup();

        // 从 MACGrid 采样生成矢量（默认在 cell center，支持 stride 下采样）
        // stride = {2,2,2} 表示每隔 2 个 cell 取一个箭头，避免太密
        void updateFromGrid(const MacGrid& g, glm::ivec3 stride = { 2,2,2 },
            float minMagnitude = 0.0f, float maxMagnitude = 1e9f);

        // 绘制
        void draw(vkcore::CommandBuffer& cmd,
            const CameraData& camera,           // 你已有 CameraData
            VkRenderingInfo& render_info,
            float arrowScale = 0.5f,            // 箭头长度缩放
            float vmin = 0.0f, float vmax = -1  // 颜色映射阈值（vmax<0则自动用数据最大值）
        );

        // 可选：直接设置 vectors（若你想自定义采样）
        std::vector<GPUVector>& cpuVectors() { return _cpu_vectors; }

    private:
        void createBuffers();
        void createDescriptors();
        void createPipeline(vk::Format color_format, vk::Format depth_format);
        void uploadGPU();

    private:
        vkcore::VulkanContext* _ctx = nullptr;

        // CPU 端缓存
        std::vector<GPUVector> _cpu_vectors;
        uint32_t               _vector_count = 0;
        uint32_t               _max_vectors = 0;

        float _cached_vmax = 1.0f;

        // GPU 资源
        std::unique_ptr<vkcore::BufferResource> _vec_ssbo;    // binding=1
        std::unique_ptr<vkcore::BufferResource> _camera_ubo;  // binding=0 (沿用你的 CameraData)
        std::unique_ptr<vkcore::BufferResource> _params_ubo;  // binding=2 (DrawParams)

        // 描述符/管线
        std::unique_ptr<vkcore::DescriptorSetLayout>         _layout;
        std::unique_ptr<vkcore::GrowableDescriptorAllocator> _frame_alloc;
        std::unique_ptr<vkcore::PipelineLayout>              _pipeline_layout;
        std::unique_ptr<vkcore::Pipeline>                    _pipeline;
    };

} // namespace dk
