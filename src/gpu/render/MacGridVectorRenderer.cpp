#include "MacGridVectorRenderer.h"

#include "data/MACGrid.h"

#include <stdexcept>
#include <filesystem>

namespace dk {
void MacGridVectorRenderer::init(vk::Format color_format, vk::Format depth_format)
{
    createBuffers();
    createDescriptors();
    createPipeline(color_format, depth_format);
}

void MacGridVectorRenderer::createBuffers()
{
    _max_vectors = 2'000'000; // 可按需调整

    // SSBO: vectors
    _vector_ssbo = vkcore::BufferBuilder()
                  .setSize(sizeof(GPUVector) * _max_vectors)
                  .setUsage(vk::BufferUsageFlagBits::eStorageBuffer)
                  .withVmaRequiredFlags(vk::MemoryPropertyFlagBits::eHostVisible)
                  .withVmaUsage(VMA_MEMORY_USAGE_CPU_TO_GPU)
                  .buildUnique(*_context);

    // UBO: camera
    _camera_ubo = vkcore::BufferBuilder()
                 .setSize(sizeof(CameraData))
                 .setUsage(vk::BufferUsageFlagBits::eUniformBuffer)
                 .withVmaRequiredFlags(vk::MemoryPropertyFlagBits::eHostVisible)
                 .withVmaUsage(VMA_MEMORY_USAGE_CPU_TO_GPU)
                 .buildUnique(*_context);
}

void MacGridVectorRenderer::createDescriptors()
{
    std::vector<vk::DescriptorPoolSize> pool_sizes = {
        {vk::DescriptorType::eStorageBuffer, 10},
        {vk::DescriptorType::eUniformBuffer, 10}
    };
    _frame_allocator = std::make_unique<vkcore::GrowableDescriptorAllocator>(_context, 10, pool_sizes);

    // set layout: 0=Camera(UBO), 1=Vectors(SSBO)
    vkcore::DescriptorSetLayout::Builder b(_context);
    b.addBinding(0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eMeshEXT)
     .addBinding(1, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eMeshEXT);
    _layout = b.build();
}

void MacGridVectorRenderer::createPipeline(vk::Format color_format, vk::Format depth_format)
{
    // push constants
    vk::PushConstantRange pc;
    pc.stageFlags = vk::ShaderStageFlagBits::eMeshEXT;
    pc.offset     = 0;
    pc.size       = sizeof(PushVector);

    _pipeline_layout = std::make_unique<vkcore::PipelineLayout>(
        _context,
        std::vector<vkcore::DescriptorSetLayout*>{_layout.get()},
        std::vector<vk::PushConstantRange>{pc}
    );

    // shaders
    std::unique_ptr<vkcore::ShaderModule> mesh_mod;
    std::unique_ptr<vkcore::ShaderModule> frag_mod;
    try
    {
        namespace fs = std::filesystem;
        fs::path cwd = fs::current_path();
        // 自行调整路径
        fs::path mesh_spv = absolute(cwd / "../../assets/shaders/spv/fluid/mac_vector.mesh.spv");
        fs::path frag_spv = absolute(cwd / "../../assets/shaders/spv/fluid/mac_vector.frag.spv");
        mesh_mod          = std::make_unique<vkcore::ShaderModule>(_context, vkcore::loadSpirvFromFile(mesh_spv));
        frag_mod          = std::make_unique<vkcore::ShaderModule>(_context, vkcore::loadSpirvFromFile(frag_spv));
    }
    catch (const std::runtime_error& e)
    {
        throw std::runtime_error(std::string("Failed to load vector shaders: ") + e.what());
    }

    // pipeline
    vkcore::PipelineBuilder pb(_context);
    _pipeline = pb.setLayout(_pipeline_layout.get())
                  .setShaders(mesh_mod->getHandle(), frag_mod->getHandle())
                  .setRenderingInfo({color_format}, depth_format)
                  .setPolygonMode(vk::PolygonMode::eFill)               // 填充
                  .setCullMode(vk::CullModeFlagBits::eNone)             // billboard 需要禁背面
                  .enableDepthTest(true, vk::CompareOp::eLessOrEqual)   // 可按需调整
                  .disableColorBlending()
                  .build();
}

void MacGridVectorRenderer::updateVectors(const std::vector<GPUVector>& cpu_vectors)
{
    _vector_count = static_cast<uint32_t>(cpu_vectors.size());
    if (_vector_count > _max_vectors)
        throw std::runtime_error("Exceeded maximum vector capacity.");

    if (_vector_count > 0)
    {
        _cpu_stage.assign(cpu_vectors.begin(), cpu_vectors.end());
        _vector_ssbo->update(_cpu_stage.data(), sizeof(GPUVector) * _vector_count);
    }
}

void MacGridVectorRenderer::draw(vkcore::CommandBuffer& cmd,
                          const CameraData&      camera,
                          VkRenderingInfo&       render_info,
                          const PushVector&      push)
{
    if (_vector_count == 0) return;

    _camera_ubo->update(&camera, sizeof(CameraData));
    _frame_allocator->reset();

    vk::DescriptorSet set = _frame_allocator->allocate(*_layout);
    vkcore::DescriptorSetWriter()
       .writeBuffer(0, vk::DescriptorType::eUniformBuffer, _camera_ubo->getDescriptorInfo())
       .writeBuffer(1, vk::DescriptorType::eStorageBuffer, _vector_ssbo->getDescriptorInfo())
       .update(*_context, set);

    cmd.beginRendering(render_info);
    cmd.setViewportAndScissor(render_info.renderArea.extent.width, render_info.renderArea.extent.height);

    auto& vkcmd = cmd.getHandle();
    vkcmd.bindPipeline(vk::PipelineBindPoint::eGraphics, _pipeline->getHandle());
    vkcmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, _pipeline_layout->getHandle(), 0, {set}, {});

    // 推常量：大小、比例等
    vkcmd.pushConstants(_pipeline_layout->getHandle(),
                        vk::ShaderStageFlagBits::eMeshEXT,
                        0, sizeof(PushVector), &push);

    // 注意：mesh 局部工作组大小与着色器保持一致（见 .mesh.glsl）
    constexpr uint32_t kWG         = 24; // 每工作组 24 支箭头（9 顶点/箭头，<= 256 顶点限制）
    uint32_t           group_count = (_vector_count + kWG - 1) / kWG;

    vkcmd.drawMeshTasksEXT(group_count, 1, 1);
    cmd.endRendering();
}

void MacGridVectorRenderer::cleanup()
{
    _pipeline.reset();
    _pipeline_layout.reset();
    _layout.reset();
    _frame_allocator.reset();

    _camera_ubo.reset();
    _vector_ssbo.reset();
}

void MacGridVectorRenderer::updateFromGrid(const MacGrid& g, glm::ivec3 stride,
    float minMagnitude, float maxMagnitude)
{
    _cpu_stage.clear();
    _cpu_stage.reserve((g.nx() / stride.x + 1) * (g.ny() / stride.y + 1) * (g.nz() / stride.z + 1));

    float vmax = 0.f;

    for (int k = 0; k < g.nz(); k += std::max(1, stride.z))
        for (int j = 0; j < g.ny(); j += std::max(1, stride.y))
            for (int i = 0; i < g.nx(); i += std::max(1, stride.x))
            {
                glm::vec3 c = g.cellCenter(i, j, k);
                glm::vec3 v = g.sampleVelocity(c); // 自带三线性，取 face 值组合
                float m = glm::length(v);
                if (m < minMagnitude) continue;
                if (m > maxMagnitude) m = maxMagnitude;

                vmax = std::max(vmax, m);
                _cpu_stage.push_back({ glm::vec4(c, 1.f), glm::vec4(v, m) });
            }

    updateVectors(_cpu_stage);
}

} // namespace dk
