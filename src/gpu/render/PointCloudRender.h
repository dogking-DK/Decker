#pragma once

#include <vector>
#include <memory>

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

namespace dk {
// --- GLSL/C++ 共享数据结构 ---
struct PointData
{
    glm::vec4 position;
    glm::vec4 color;
};

struct CameraData
{
    glm::mat4 viewProj;
};

struct PushConstantData
{
    glm::mat4 modelMatrix;
    float     pointSize;
};

class PointCloudRenderer
{
public:
    PointCloudRenderer(vkcore::VulkanContext* context) : _context(context), _point_cloud_ssbo(), _camera_ubo()
    {
    }

    void init(const std::vector<PointData>& initial_point_data, vk::Format color_format, vk::Format depth_format,
              FrameData&                    frame_data)
    {
        // 1. 创建资源缓冲区 (SSBO 和 UBO)
        createBuffers(initial_point_data, frame_data);

        // 2. 创建描述符分配器
        createDescriptorAllocator();

        // 3. 创建管线布局
        createPipelineLayout();

        // 4. 创建管线
        createPipeline(color_format, depth_format);
    }

    void draw(vkcore::CommandBuffer& cmd, const CameraData& camera_data)
    {
        // 1. 更新 UBO
        _camera_ubo.write(&camera_data, sizeof(CameraData));

        // 2. 重置描述符分配器 (通常在每帧开始时调用)
        _frame_allocator->reset();

        // 3. 分配并更新描述符集
        vk::DescriptorSet           frame_set = _frame_allocator->allocate(*_layout);
        vkcore::DescriptorSetWriter writer;
        writer.writeBuffer(0, vk::DescriptorType::eUniformBuffer, _camera_ubo.getDescriptorInfo())
              .writeBuffer(1, vk::DescriptorType::eStorageBuffer, _point_cloud_ssbo.getDescriptorInfo());

        writer.update(*_context, frame_set);

        // 4. 绑定管线
        cmd.getHandle().bindPipeline(vk::PipelineBindPoint::eGraphics, _pipeline->getHandle());

        // 5. 绑定描述符集
        cmd.getHandle().bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics,
            _pipeline_layout->getHandle(),
            0, {frame_set}, {}
        );

        // 6. 设置 Push Constants
        PushConstantData push_data;
        push_data.modelMatrix = glm::mat4(1.0f); // 假设单位模型矩阵
        push_data.pointSize   = 3.0f;
        cmd.getHandle().pushConstants(
            _pipeline_layout->getHandle(),
            vk::ShaderStageFlagBits::eMeshEXT,
            0, sizeof(PushConstantData), &push_data
        );

        // 7. 发出绘制命令
        constexpr uint32_t workgroup_size = 64;
        uint32_t           group_count    = (_point_count + workgroup_size - 1) / workgroup_size;
        cmd.getHandle().drawMeshTasksEXT(group_count, 1, 1);
    }

    void cleanup()
    {
        //_camera_ubo.reset();
        //_point_cloud_ssbo.reset();
        _frame_allocator.reset();
        _layout.reset();
        _pipeline_layout.reset();
        _pipeline.reset();
    }

private:
    void createBuffers(const std::vector<PointData>& initial_point_data, FrameData& frame_data)
    {
        _point_count           = static_cast<uint32_t>(initial_point_data.size());
        VkDeviceSize ssbo_size = sizeof(PointData) * _point_count;

        vkcore::BufferBuilder staging;
        staging.setUsage(vk::BufferUsageFlagBits::eTransferSrc)
               .setSize(ssbo_size)
               .withVmaUsage(VMA_MEMORY_USAGE_AUTO) // 让 VMA 选 Host 内存
               .withVmaFlags(VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                             | VMA_ALLOCATION_CREATE_MAPPED_BIT);
        auto stagingBuf = staging.build(*_context);

        // 3) 目标 SSBO（设备本地 + 作为拷贝目标 & 着色器读）
        vkcore::BufferBuilder gpuSSBO;
        gpuSSBO.setSize(ssbo_size)
               .setUsage(vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst)
               .withVmaUsage(VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
        _point_cloud_ssbo = gpuSSBO.build(*_context);

        // 4) CPU → staging
        stagingBuf.write(initial_point_data.data(), ssbo_size, 0);  // ← 关键修正

        // 5) staging → device（可优先选择 transfer 队列）
        vk::BufferCopy2 region{0, 0, ssbo_size};
        copyBufferImmediate(_context,
                            frame_data._command_pool_transfer,
                            _context->getTransferQueue(),  // 伪代码：优先 transfer，没有就 graphics
                            stagingBuf,
                            _point_cloud_ssbo,
                            {region});

        // 创建 UBO (主机可见)
        vkcore::BufferBuilder _camera_uniform_builder;
        _camera_uniform_builder.setSize(sizeof(CameraData))
                               .setUsage(vk::BufferUsageFlagBits::eUniformBuffer)
                               .withVmaUsage(VMA_MEMORY_USAGE_AUTO); // 让 VMA 选 Host 可见内存
        _camera_ubo = _camera_uniform_builder.build(*_context);
    }

    void createDescriptorAllocator()
    {
        std::vector<vk::DescriptorPoolSize> pool_sizes = {
            {vk::DescriptorType::eStorageBuffer, 10}, // 每个池最多10个SSBO
            {vk::DescriptorType::eUniformBuffer, 10}  // 每个池最多10个UBO
        };
        _frame_allocator = std::make_unique<vkcore::GrowableDescriptorAllocator>(_context, 10, pool_sizes);
    }

    void createPipelineLayout()
    {
        // 1. 创建 DescriptorSetLayout
        vkcore::DescriptorSetLayout::Builder layout_builder(_context);
        layout_builder.addBinding(0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eMeshEXT)
                      .addBinding(1, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eMeshEXT);
        _layout = layout_builder.build();

        // 2. 创建 PushConstantRange
        vk::PushConstantRange push_constant_range;
        push_constant_range.stageFlags = vk::ShaderStageFlagBits::eMeshEXT;
        push_constant_range.offset     = 0;
        push_constant_range.size       = sizeof(PushConstantData);

        // 3. 创建 PipelineLayout
        _pipeline_layout = std::make_unique<vkcore::PipelineLayout>(
            _context,
            std::vector<vkcore::DescriptorSetLayout*>{_layout.get()},
            std::vector<vk::PushConstantRange>{push_constant_range}
        );
    }

    void createPipeline(vk::Format color_format, vk::Format depth_format)
    {
        std::unique_ptr<vkcore::ShaderModule> mesh_shader;
        std::unique_ptr<vkcore::ShaderModule> frag_shader;

        try
        {
            // 1. 使用工具函数加载预编译的 SPIR-V 文件
            std::vector<uint32_t> mesh_spirv = vkcore::loadSpirvFromFile("shaders/bin/pointcloud.mesh.spv");
            std::vector<uint32_t> frag_spirv = vkcore::loadSpirvFromFile("shaders/bin/pointcloud.frag.spv");

            // 2. 将加载的字节码传递给 ShaderModule 构造函数
            mesh_shader = std::make_unique<vkcore::ShaderModule>(_context, mesh_spirv);
            frag_shader = std::make_unique<vkcore::ShaderModule>(_context, frag_spirv);

            fmt::print("Successfully loaded precompiled shaders.");
        }
        catch (const std::runtime_error& e)
        {
            fmt::print("Error loading shaders: {}\n", e.what());
            return;
        }
        // 2. 使用 PipelineBuilder 创建管线
        vkcore::PipelineBuilder builder(_context);
        _pipeline = builder.setLayout(_pipeline_layout.get())
                           .setShaders(mesh_shader->getHandle(), frag_shader->getHandle())
                           .setRenderingInfo({color_format}, depth_format)
                           .setPolygonMode(vk::PolygonMode::ePoint) // 告诉管线我们要画点
                           .setCullMode(vk::CullModeFlagBits::eNone)
                           .enableDepthTest(true, vk::CompareOp::eLessOrEqual) // 开启深度测试和写入
                           .disableColorBlending() // 不透明点云不需要混合
                           .build();
    }

    // --- 成员变量 ---
    vkcore::VulkanContext* _context;
    uint32_t               _point_count = 0;

    // 资源
    vkcore::BufferResource _point_cloud_ssbo;
    vkcore::BufferResource _camera_ubo;

    // 描述符相关
    std::unique_ptr<vkcore::DescriptorSetLayout>         _layout;
    std::unique_ptr<vkcore::GrowableDescriptorAllocator> _frame_allocator;

    // 管线相关
    std::unique_ptr<vkcore::PipelineLayout> _pipeline_layout;
    std::unique_ptr<vkcore::Pipeline>       _pipeline;
};
}
