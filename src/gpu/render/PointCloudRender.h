#pragma once

#include <vector>
#include <memory>
#include "glm/glm.hpp"

// --- 包含我们封装的头文件 ---
#include "vk_engine.h"
#include "Vulkan/Context.h"
#include "Vulkan/Buffer.h" // 假设有 BufferResource 类
#include "Vulkan/BufferBuilder.h" // 假设有 BufferResource 类
#include "Vulkan/Image.h"  // 假设有 ImageResource 类
#include "Vulkan/CommandBuffer.h"
#include "Vulkan/DescriptorSetLayout.h"
#include "Vulkan/DescriptorSet.h"
#include "Vulkan/DescriptorSetPool.h"
#include "Vulkan/DescriptorWriter.h"
#include "Vulkan/Pipeline.h"
#include "Vulkan/PipelineLayout.h"
#include "Vulkan/ShaderModule.h"

// --- GLSL/C++ 共享数据结构 (必须与 Slang 中的定义匹配) ---
struct PointData
{
    glm::vec4 position;
    glm::vec4 color;
};

// Camera UBO
struct CameraData
{
    glm::mat4 viewProj;
};

// Push Constants
struct PushConstantData
{
    glm::mat4 modelMatrix;
    float     pointSize;
};

class PointCloudRenderer
{
public:
    PointCloudRenderer(dk::vkcore::VulkanContext* context);
    ~PointCloudRenderer() = default;

    // 初始化渲染器所需的所有 Vulkan 对象
    void init(vk::Format color_format, vk::Format depth_format);

    // 每帧更新点云数据
    void updatePoints(const std::vector<PointData>& new_point_data);

    // 在命令缓冲区中录制绘制命令
    void draw(dk::vkcore::CommandBuffer& cmd, const CameraData& camera_data);

    // 清理所有资源
    void cleanup();

private:
    void createBuffers();
    void createDescriptors();
    void createPipeline(vk::Format color_format, vk::Format depth_format);

    // --- 成员变量 ---
    dk::vkcore::VulkanContext* _context;
    uint32_t                   _point_count     = 0;
    uint32_t                   _max_point_count = 0; // 缓冲区的最大容量

    // 资源
    std::unique_ptr<dk::vkcore::BufferResource> _point_cloud_ssbo;
    std::unique_ptr<dk::vkcore::BufferResource> _camera_ubo;

    // 描述符相关
    std::unique_ptr<dk::vkcore::DescriptorSetLayout>         _layout;
    std::unique_ptr<dk::vkcore::GrowableDescriptorAllocator> _frame_allocator;

    // 管线相关
    std::unique_ptr<dk::vkcore::PipelineLayout> _pipeline_layout;
    std::unique_ptr<dk::vkcore::Pipeline>       _pipeline;
};
