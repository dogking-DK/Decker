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

#include "data/MacGrid.h"

namespace dk {

    struct GPUPoint {
        glm::vec4 pos;  // world xyz, w=1
        glm::vec4 val;  // x=scalar，用于着色，y/z/w 预留
    };

    struct DrawParamsPoints {
        uint32_t count;
        float    pointPixelSize; // 像素尺寸（gl_PointSize）
        float    vmin;
        float    vmax;
    };

    enum class PointScalarMode : uint32_t {
        SpeedMagnitude = 0,
        Dye = 1,
        Pressure = 2,
        Divergence = 3
    };

    class MacGridPointRenderer {
    public:
        explicit MacGridPointRenderer(vkcore::VulkanContext* ctx) : _ctx(ctx) {}

        void init(vk::Format color_format, vk::Format depth_format);
        void cleanup();

        // 从 MacGrid 均匀采样（stride={sx,sy,sz}）
        void updateFromGridUniform(const MacGrid& g,
            glm::ivec3 stride = { 2,2,2 },
            PointScalarMode mode = PointScalarMode::SpeedMagnitude,
            float clampMin = 0.0f, float clampMax = -1.0f /*<=0自动*/);

        // 绘制（pointPixelSize 为像素单位）
        void draw(vkcore::CommandBuffer& cmd,
            const CameraData& camera,
            VkRenderingInfo& render_info,
            float pointPixelSize = 3.0f,
            float vmin = 0.0f, float vmax = -1.0f /*<=0用数据最大值*/);

        // 自定义点集
        std::vector<GPUPoint>& cpuPoints() { return _cpu_points; }

    private:
        void createBuffers();
        void createDescriptors();
        void createPipeline(vk::Format color_format, vk::Format depth_format);
        void uploadGPU();

        static float localDivergenceAt(const MacGrid& g, int i, int j, int k) {
            const float invh = 1.0f / g.h();
            float du = g.U(i + 1, j, k) - g.U(i, j, k);
            float dv = g.V(i, j + 1, k) - g.V(i, j, k);
            float dw = g.W(i, j, k + 1) - g.W(i, j, k);
            return (du + dv + dw) * invh;
        }

    private:
        vkcore::VulkanContext* _ctx = nullptr;

        std::vector<GPUPoint> _cpu_points;
        uint32_t _count = 0;
        uint32_t _capacity = 0;
        float    _cached_vmax = 1.0f;

        // GPU
        std::unique_ptr<vkcore::BufferResource> _points_ssbo;  // set=0,binding=1
        std::unique_ptr<vkcore::BufferResource> _camera_ubo;   // set=0,binding=0
        std::unique_ptr<vkcore::BufferResource> _params_ubo;   // set=0,binding=2

        // 描述符/管线
        std::unique_ptr<vkcore::DescriptorSetLayout>         _layout;
        std::unique_ptr<vkcore::GrowableDescriptorAllocator> _frame_alloc;
        std::unique_ptr<vkcore::PipelineLayout>              _pipeline_layout;
        std::unique_ptr<vkcore::Pipeline>                    _pipeline;
    };

} // namespace dk
