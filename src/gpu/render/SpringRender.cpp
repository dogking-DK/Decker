// SpringRenderer.cpp
#include "SpringRender.h"
#include <stdexcept>
#include <filesystem>

namespace dk {
SpringRenderer::SpringRenderer(vkcore::VulkanContext* context)
    : _context(context)
{
}

void SpringRenderer::init(vk::Format color_format, vk::Format depth_format)
{
    createBuffers();
    createDescriptors();
    createPipeline(color_format, depth_format);
}

void SpringRenderer::updateSprings(const ParticleData& particle_data, const Spring& topology)
{
    _particle_count = static_cast<uint32_t>(particle_data.size());
    _spring_count   = static_cast<uint32_t>(topology.size());

    if (_particle_count > _max_particle_count)
    {
        throw std::runtime_error("Exceeded maximum particle capacity for spring renderer.");
    }
    if (_spring_count > _max_spring_count)
    {
        throw std::runtime_error("Exceeded maximum spring capacity for spring renderer.");
    }

    // 1. 更新粒子数据 SSBO
    if (_particle_count > 0)
    {
        // 从物理系统的 SoA (vec3) 格式转换为渲染器需要的 AoS (vec4) 格式
        _cpu_particle_buffer.resize(_particle_count);
        for (uint32_t i = 0; i < _particle_count; ++i)
        {
            _cpu_particle_buffer[i] = {
                glm::vec4(particle_data.position[i], 1.0f),
                particle_data.color[i]
            };
        }
        _particle_data_ssbo->update(_cpu_particle_buffer.data(), sizeof(GPUParticle) * _particle_count);
    }

    // 2. 更新弹簧索引 SSBO
    // 注意: 在很多情况下，拓扑结构是不变的，可以优化为只上传一次
    if (_spring_count > 0)
    {
        _cpu_index_buffer.resize(_spring_count);
        for (uint32_t i = 0; i < _spring_count; ++i)
        {
            _cpu_index_buffer[i] = {
                glm::uvec2(topology.index_a[i], topology.index_b[i])
            };
        }
        _spring_index_ssbo->update(_cpu_index_buffer.data(), sizeof(SpringIndex) * _spring_count);
    }
}

void SpringRenderer::createBuffers()
{
    _max_particle_count = 2000000;
    _max_spring_count   = 10000000;

    auto ssbo_builder = vkcore::BufferBuilder();

    // 为粒子数据创建 SSBO
    _particle_data_ssbo = ssbo_builder.setSize(sizeof(GPUParticle) * _max_particle_count)
                                      .setUsage(vk::BufferUsageFlagBits::eStorageBuffer)
                                      .withVmaRequiredFlags(vk::MemoryPropertyFlagBits::eHostVisible)
                                      .withVmaUsage(VMA_MEMORY_USAGE_CPU_TO_GPU)
                                      .buildUnique(*_context);

    // 为弹簧索引创建 SSBO
    _spring_index_ssbo = ssbo_builder.setSize(sizeof(SpringIndex) * _max_spring_count)
                                     .setUsage(vk::BufferUsageFlagBits::eStorageBuffer)
                                     .withVmaRequiredFlags(vk::MemoryPropertyFlagBits::eHostVisible)
                                     .withVmaUsage(VMA_MEMORY_USAGE_CPU_TO_GPU)
                                     .buildUnique(*_context);

    // 创建相机 UBO
    auto ubo_builder = vkcore::BufferBuilder();
    _camera_ubo      = ubo_builder.setSize(sizeof(CameraData))
                                  .setUsage(vk::BufferUsageFlagBits::eUniformBuffer)
                                  .withVmaRequiredFlags(vk::MemoryPropertyFlagBits::eHostVisible)
                                  .withVmaUsage(VMA_MEMORY_USAGE_CPU_TO_GPU)
                                  .buildUnique(*_context);
}

void SpringRenderer::createDescriptors()
{
    // 与 PointCloudRenderer 相同
    std::vector<vk::DescriptorPoolSize> pool_sizes = {
        {vk::DescriptorType::eStorageBuffer, 10},
        {vk::DescriptorType::eUniformBuffer, 10}
    };
    _frame_allocator = std::make_unique<vkcore::GrowableDescriptorAllocator>(_context, 10, pool_sizes);
}

void SpringRenderer::createPipeline(vk::Format color_format, vk::Format depth_format)
{
    // 1. 创建布局
    {
        // **核心变化**: 我们现在需要3个绑定
        vkcore::DescriptorSetLayout::Builder layout_builder(_context);
        layout_builder
           .addBinding(0, vk::DescriptorType::eUniformBuffer,
                       vk::ShaderStageFlagBits::eMeshEXT) // binding=0: Camera UBO
           .addBinding(1, vk::DescriptorType::eStorageBuffer,
                       vk::ShaderStageFlagBits::eMeshEXT) // binding=1: Particle Data SSBO
           .addBinding(2, vk::DescriptorType::eStorageBuffer,
                       vk::ShaderStageFlagBits::eMeshEXT); // binding=2: Spring Index SSBO
        _layout = layout_builder.build();

        vk::PushConstantRange push_constant_range;
        push_constant_range.stageFlags = vk::ShaderStageFlagBits::eMeshEXT;
        push_constant_range.offset     = 0;
        push_constant_range.size       = sizeof(PushConstantData);

        _pipeline_layout = std::make_unique<vkcore::PipelineLayout>(
            _context,
            std::vector<vkcore::DescriptorSetLayout*>{_layout.get()},
            std::vector<vk::PushConstantRange>{push_constant_range}
        );
    }

    // 2. 加载着色器
    std::unique_ptr<vkcore::ShaderModule> mesh_module;
    std::unique_ptr<vkcore::ShaderModule> frag_module;
    try
    {
        namespace fs = std::filesystem;
        fs::path current_dir = fs::current_path();
        // **你需要创建这两个新的着色器文件**
        fs::path target_file_mesh = absolute(current_dir / "../../assets/shaders/spv/fluid/spring.mesh.spv");
        fs::path target_file_frag = absolute(current_dir / "../../assets/shaders/spv/fluid/spring.frag.spv");

        mesh_module = std::make_unique<vkcore::ShaderModule>(_context, vkcore::loadSpirvFromFile(target_file_mesh));
        frag_module = std::make_unique<vkcore::ShaderModule>(_context, vkcore::loadSpirvFromFile(target_file_frag));
    }
    catch (const std::runtime_error& e)
    {
        throw std::runtime_error("Failed to load spring shaders. Error: " + std::string(e.what()));
    }

    // 3. 创建管线
    vkcore::PipelineBuilder builder(_context);
    _pipeline = builder.setLayout(_pipeline_layout.get())
                       .setShaders(mesh_module->getHandle(), frag_module->getHandle())
                       .setRenderingInfo({color_format}, depth_format)
                        // **核心变化**: 设置为线框模式
                       .setPolygonMode(vk::PolygonMode::eLine)
                       .setCullMode(vk::CullModeFlagBits::eNone)
                       .enableDepthTest(true, vk::CompareOp::eLessOrEqual)
                       .disableColorBlending()
                       .build();
}

void SpringRenderer::draw(vkcore::CommandBuffer& cmd, const CameraData& camera_data, VkRenderingInfo& render_info)
{
    if (_spring_count == 0) return;

    _camera_ubo->update(&camera_data, sizeof(CameraData));
    _frame_allocator->reset();

    vk::DescriptorSet frame_set = _frame_allocator->allocate(*_layout);

    // **核心变化**: 绑定3个缓冲区到描述符集
    vkcore::DescriptorSetWriter writer;
    writer.writeBuffer(0, vk::DescriptorType::eUniformBuffer, _camera_ubo->getDescriptorInfo());
    writer.writeBuffer(1, vk::DescriptorType::eStorageBuffer, _particle_data_ssbo->getDescriptorInfo());
    writer.writeBuffer(2, vk::DescriptorType::eStorageBuffer, _spring_index_ssbo->getDescriptorInfo());
    writer.update(*_context, frame_set);

    cmd.beginRendering(render_info);
    cmd.setViewportAndScissor(render_info.renderArea.extent.width, render_info.renderArea.extent.height);
    cmd.getHandle().bindPipeline(vk::PipelineBindPoint::eGraphics, _pipeline->getHandle());
    cmd.getHandle().bindDescriptorSets(vk::PipelineBindPoint::eGraphics, _pipeline_layout->getHandle(), 0, {frame_set},
                                       {});

    PushConstantData push_data;
    push_data.modelMatrix = glm::mat4(1.0f);
    push_data.size   = 1.0f; // 设置线宽
    cmd.getHandle().pushConstants(
        _pipeline_layout->getHandle(),
        vk::ShaderStageFlagBits::eMeshEXT,
        0, sizeof(PushConstantData), &push_data
    );

    // 根据弹簧数量计算工作组数量
    constexpr uint32_t workgroup_size = 64;
    uint32_t           group_count    = (_spring_count + workgroup_size - 1) / workgroup_size;
    cmd.getHandle().drawMeshTasksEXT(group_count, 1, 1);

    cmd.endRendering();
}

void SpringRenderer::cleanup()
{
    _pipeline.reset();
    _pipeline_layout.reset();
    _layout.reset();
    _frame_allocator.reset();
    _camera_ubo.reset();
    _particle_data_ssbo.reset();
    _spring_index_ssbo.reset();
}
}
