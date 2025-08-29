#include "PointCloudRender.h"
#include <stdexcept>

// 构造函数和 updatePoints 方法与之前的版本完全相同
PointCloudRenderer::PointCloudRenderer(dk::vkcore::VulkanContext* context)
    : _context(context)
{
}

void PointCloudRenderer::updatePoints(const std::vector<PointData>& new_point_data)
{
    if (new_point_data.size() > _max_point_count)
    {
        throw std::runtime_error("Exceeded maximum point cloud capacity.");
    }
    _point_count = static_cast<uint32_t>(new_point_data.size());

    if (_point_count > 0)
    {
        _point_cloud_ssbo->update(new_point_data.data(), sizeof(PointData) * _point_count);
    }
}


// --- 初始化和绘制逻辑的核心变化在这里 ---

void PointCloudRenderer::init(vk::Format color_format, vk::Format depth_format)
{
    createBuffers();
    createDescriptors();
    // **核心变化**: createPipeline 现在将负责加载和手动布局定义
    createPipeline(color_format, depth_format);
}

void PointCloudRenderer::createBuffers()
{
    // 这个函数与之前的版本完全相同 (创建主机可见的 SSBO 和 UBO)
    _max_point_count       = 500000;
    _point_count           = 0;
    VkDeviceSize ssbo_size = sizeof(PointData) * _max_point_count;

    _point_cloud_ssbo = std::make_unique<dk::vkcore::BufferResource>(_context);
    _point_cloud_ssbo->create(ssbo_size, vk::BufferUsageFlagBits::eStorageBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU);

    _camera_ubo = std::make_unique<dk::vkcore::BufferResource>(_context);
    _camera_ubo->create(sizeof(CameraData), vk::BufferUsageFlagBits::eUniformBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU);
}

void PointCloudRenderer::createDescriptors()
{
    // 这个函数与之前的版本完全相同
    std::vector<vk::DescriptorPoolSize> pool_sizes = {
        {vk::DescriptorType::eStorageBuffer, 10},
        {vk::DescriptorType::eUniformBuffer, 10}
    };
    _frame_allocator = std::make_unique<dk::vkcore::GrowableDescriptorAllocator>(_context, 10, pool_sizes);
}

void PointCloudRenderer::createPipeline(vk::Format color_format, vk::Format depth_format)
{
    // =================================================================
    // 1. 手动创建布局 (不再有反射)
    // =================================================================
    {
        // 1a. 创建 DescriptorSetLayout
        // **必须**与 GLSL 中的 `layout(set = 0, binding = ...)` 声明匹配
        dk::vkcore::DescriptorSetLayout::Builder layout_builder(_context);
        layout_builder.
            addBinding(0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eMeshEXT); // binding = 0
        layout_builder.
            addBinding(1, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eMeshEXT); // binding = 1
        _layout = layout_builder.build();

        // 1b. 创建 PushConstantRange
        // **必须**与 GLSL 中的 `layout(push_constant)` 结构体匹配
        vk::PushConstantRange push_constant_range;
        push_constant_range.stageFlags = vk::ShaderStageFlagBits::eMeshEXT;
        push_constant_range.offset     = 0;
        push_constant_range.size       = sizeof(PushConstantData);

        // 1c. 创建 PipelineLayout
        _pipeline_layout = std::make_unique<dk::vkcore::PipelineLayout>(
            _context,
            std::vector<dk::vkcore::DescriptorSetLayout*>{_layout.get()},
            std::vector<vk::PushConstantRange>{push_constant_range}
        );
    }

    // =================================================================
    // 2. 从文件加载预编译的 SPIR-V 并创建 ShaderModule
    // =================================================================
    std::unique_ptr<dk::vkcore::ShaderModule> mesh_module;
    std::unique_ptr<dk::vkcore::ShaderModule> frag_module;
    try
    {
        auto mesh_spirv = dk::vkcore::loadSpirvFromFile("shaders/bin/pointcloud.mesh.spv");
        auto frag_spirv = dk::vkcore::loadSpirvFromFile("shaders/bin/pointcloud.frag.spv");

        mesh_module = std::make_unique<dk::vkcore::ShaderModule>(_context, mesh_spirv);
        frag_module = std::make_unique<dk::vkcore::ShaderModule>(_context, frag_spirv);
    }
    catch (const std::runtime_error& e)
    {
        // 提供更明确的错误信息
        throw std::runtime_error(
            "Failed to load precompiled shaders. Did you run the compile script? Error: " + std::string(e.what()));
    }

    // =================================================================
    // 3. 使用 PipelineBuilder 创建管线
    // =================================================================
    dk::vkcore::PipelineBuilder builder(_context);
    _pipeline = builder.setLayout(_pipeline_layout.get()) // 使用我们手动创建的布局
                       .setShaders(mesh_module->getHandle(), frag_module->getHandle())
                       .setRenderingInfo({color_format}, depth_format)
                       .setPolygonMode(vk::PolygonMode::ePoint)
                       .setCullMode(vk::CullModeFlagBits::eNone)
                       .enableDepthTest(true, vk::CompareOp::eLessOrEqual)
                       .disableColorBlending()
                       .build();

    // ShaderModule 的 unique_ptr 在此离开作用域，自动销毁 vk::ShaderModule
}

void PointCloudRenderer::draw(dk::vkcore::CommandBuffer& cmd, const CameraData& camera_data)
{
    // 这个函数与之前的版本完全相同，因为底层的绑定逻辑没有改变
    if (_point_count == 0) return;

    _camera_ubo->update(&camera_data, sizeof(CameraData));
    _frame_allocator->reset();

    vk::DescriptorSet            frame_set = _frame_allocator->allocate(*_layout);
    dk::vkcore::DescriptorWriter writer;
    writer.bindBuffer(0, _camera_ubo->getDescriptorInfo(), vk::DescriptorType::eUniformBuffer)
          .bindBuffer(1, _point_cloud_ssbo->getDescriptorInfo(), vk::DescriptorType::eStorageBuffer);
    writer.update(_context->getDevice(), frame_set);

    cmd.getHandle().bindPipeline(vk::PipelineBindPoint::eGraphics, _pipeline->getHandle());
    cmd.getHandle().bindDescriptorSets(vk::PipelineBindPoint::eGraphics, _pipeline_layout->getHandle(), 0, {frame_set},
                                       {});

    PushConstantData push_data;
    push_data.modelMatrix = glm::mat4(1.0f);
    push_data.pointSize   = 3.0f;
    cmd.getHandle().pushConstants(
        _pipeline_layout->getHandle(),
        vk::ShaderStageFlagBits::eMeshEXT,
        0, sizeof(PushConstantData), &push_data
    );

    constexpr uint32_t workgroup_size = 64;
    uint32_t           group_count    = (_point_count + workgroup_size - 1) / workgroup_size;
    cmd.getHandle().drawMeshTasksEXT(group_count, 1, 1);
}

void PointCloudRenderer::cleanup()
{
    // 这个函数与之前的版本完全相同
    _pipeline.reset();
    _pipeline_layout.reset();
    _layout.reset();
    _frame_allocator.reset();
    _camera_ubo.reset();
    _point_cloud_ssbo.reset();
}
