// SpringRenderer.h
#pragma once

#include <vector>
#include <memory>
#include "glm/glm.hpp"

// 假设这些头文件路径是正确的
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

// 引入我们物理模拟的数据结构
#include "ParticleData.h" // 包含 ParticleData
#include "SpringTopology.h" // 包含 SpringTopology

namespace dk {

    // --- GLSL/C++ 共享数据结构 ---

    // Camera UBO (与 PointCloudRenderer 相同)
    struct CameraData
    {
        glm::mat4 viewProj;
    };

    // Push Constants (将 pointSize 改为 lineWidth)
    struct PushConstantData
    {
        glm::mat4 modelMatrix;
        float     lineWidth; // 在支持的硬件上可以控制线宽
    };

    // 用于 SSBO 的粒子数据结构 (与着色器对齐)
    struct GPUParticle
    {
        glm::vec4 position;
        glm::vec4 color;
    };

    // 用于 SSBO 的弹簧索引结构
    struct SpringIndex
    {
        glm::uvec2 indices; // 存储构成弹簧的两个粒子的索引
    };


    class SpringRenderer
    {
    public:
        SpringRenderer(vkcore::VulkanContext* context);
        ~SpringRenderer() = default;

        // 初始化渲染器
        void init(vk::Format color_format, vk::Format depth_format);

        /**
         * @brief 每帧从物理系统中更新弹簧和粒子数据.
         * @param particle_data 包含所有粒子位置和颜色的数据.
         * @param topology 包含定义弹簧连接的索引对.
         */
        void updateSprings(const ParticleData& particle_data, const SpringTopology& topology);

        // 录制绘制命令
        void draw(vkcore::CommandBuffer& cmd, const CameraData& camera_data, VkRenderingInfo& render_info);

        // 清理资源
        void cleanup();

    private:
        void createBuffers();
        void createDescriptors();
        void createPipeline(vk::Format color_format, vk::Format depth_format);

        // --- 成员变量 ---
        vkcore::VulkanContext* _context;
        uint32_t _spring_count = 0;
        uint32_t _particle_count = 0;

        // 缓冲区的最大容量
        uint32_t _max_particle_count = 0;
        uint32_t _max_spring_count = 0;

        // CPU 端的临时转换缓冲区
        std::vector<GPUParticle> _cpu_particle_buffer;
        std::vector<SpringIndex> _cpu_index_buffer;

        // GPU 资源
        std::unique_ptr<vkcore::BufferResource> _particle_data_ssbo; // 存储所有粒子的位置和颜色
        std::unique_ptr<vkcore::BufferResource> _spring_index_ssbo;  // 存储弹簧的索引对
        std::unique_ptr<vkcore::BufferResource> _camera_ubo;

        // 描述符相关
        std::unique_ptr<vkcore::DescriptorSetLayout>         _layout;
        std::unique_ptr<vkcore::GrowableDescriptorAllocator> _frame_allocator;

        // 管线相关
        std::unique_ptr<vkcore::PipelineLayout> _pipeline_layout;
        std::unique_ptr<vkcore::Pipeline>       _pipeline;
    };
}