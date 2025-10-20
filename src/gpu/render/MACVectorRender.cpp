// MacGridVectorRender.cpp
#include "MACVectorRender.h"
#include <filesystem>
#include <stdexcept>
#include <algorithm>

namespace dk {

    void MacGridVectorRenderer::init(vk::Format color_format, vk::Format depth_format)
    {
        createBuffers();
        createDescriptors();
        createPipeline(color_format, depth_format);
    }

    void MacGridVectorRenderer::createBuffers()
    {
        _max_vectors = 8'000'000; // 预留上限，按需调整

        auto ssbo_builder = vkcore::BufferBuilder();
        _vec_ssbo = ssbo_builder.setSize(sizeof(GPUVector) * _max_vectors)
            .setUsage(vk::BufferUsageFlagBits::eStorageBuffer)
            .withVmaRequiredFlags(vk::MemoryPropertyFlagBits::eHostVisible)
            .withVmaUsage(VMA_MEMORY_USAGE_CPU_TO_GPU)
            .buildUnique(*_ctx);

        auto ubo_builder = vkcore::BufferBuilder();
        _camera_ubo = ubo_builder.setSize(sizeof(CameraData))
            .setUsage(vk::BufferUsageFlagBits::eUniformBuffer)
            .withVmaRequiredFlags(vk::MemoryPropertyFlagBits::eHostVisible)
            .withVmaUsage(VMA_MEMORY_USAGE_CPU_TO_GPU)
            .buildUnique(*_ctx);

        _params_ubo = ubo_builder.setSize(sizeof(DrawParams))
            .setUsage(vk::BufferUsageFlagBits::eUniformBuffer)
            .withVmaRequiredFlags(vk::MemoryPropertyFlagBits::eHostVisible)
            .withVmaUsage(VMA_MEMORY_USAGE_CPU_TO_GPU)
            .buildUnique(*_ctx);
    }

    void MacGridVectorRenderer::createDescriptors()
    {
        std::vector<vk::DescriptorPoolSize> pool_sizes = {
            {vk::DescriptorType::eStorageBuffer, 10},
            {vk::DescriptorType::eUniformBuffer, 10}
        };
        _frame_alloc = std::make_unique<vkcore::GrowableDescriptorAllocator>(_ctx, 10, pool_sizes);

        // layout(set=0): 0=Camera UBO, 1=Vectors SSBO, 2=DrawParams UBO
        vkcore::DescriptorSetLayout::Builder b(_ctx);
        b.addBinding(0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eMeshEXT)   // Camera
            .addBinding(1, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eMeshEXT)   // Vector SSBO
            .addBinding(2, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eMeshEXT);  // DrawParams
        _layout = b.build();
    }

    void MacGridVectorRenderer::createPipeline(vk::Format color_format, vk::Format depth_format)
    {
        // push constants：与工程里通用 PushConstantData 对齐（modelMatrix + size）
        vk::PushConstantRange pcr{};
        pcr.stageFlags = vk::ShaderStageFlagBits::eMeshEXT;
        pcr.offset = 0;
        pcr.size = sizeof(PushConstantData);

        _pipeline_layout = std::make_unique<vkcore::PipelineLayout>(
            _ctx,
            std::vector<vkcore::DescriptorSetLayout*>{_layout.get()},
            std::vector<vk::PushConstantRange>{pcr}
        );

        // 加载 mesh + frag
        namespace fs = std::filesystem;
        fs::path cwd = fs::current_path();
        auto mesh_spv = vkcore::loadSpirvFromFile(fs::absolute(cwd / "../../assets/shaders/spv/fluid/mac_vector.mesh.spv"));
        auto frag_spv = vkcore::loadSpirvFromFile(fs::absolute(cwd / "../../assets/shaders/spv/fluid/mac_vector.frag.spv"));
        auto mesh_mod = std::make_unique<vkcore::ShaderModule>(_ctx, mesh_spv);
        auto frag_mod = std::make_unique<vkcore::ShaderModule>(_ctx, frag_spv);

        vkcore::PipelineBuilder pb(_ctx);
        _pipeline = pb.setLayout(_pipeline_layout.get())
            .setShaders(mesh_mod->getHandle(), frag_mod->getHandle())
            .setRenderingInfo({ color_format }, depth_format)
            .setPolygonMode(vk::PolygonMode::eFill)               // 产出三角面箭头
            .setCullMode(vk::CullModeFlagBits::eNone)
            .enableDepthTest(true, vk::CompareOp::eLessOrEqual)
            .disableColorBlending()
            .build();
    }

    void MacGridVectorRenderer::uploadGPU()
    {
        _vector_count = static_cast<uint32_t>(_cpu_vectors.size());
        if (_vector_count == 0) return;
        if (_vector_count > _max_vectors) throw std::runtime_error("Vector count exceeds capacity.");
        _vec_ssbo->update(_cpu_vectors.data(), sizeof(GPUVector) * _vector_count);
    }

    void MacGridVectorRenderer::updateFromGrid(const MacGrid& g, glm::ivec3 stride,
        float minMagnitude, float maxMagnitude)
    {
        _cpu_vectors.clear();
        _cpu_vectors.reserve((g.nx() / stride.x + 1) * (g.ny() / stride.y + 1) * (g.nz() / stride.z + 1));

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
                    _cpu_vectors.push_back({ glm::vec4(c, 1.f), glm::vec4(v, m) });
                }

        _cached_vmax = std::max(vmax, 1e-6f); // 防 0
        uploadGPU();
    }

    void MacGridVectorRenderer::draw(vkcore::CommandBuffer& cmd,
        const CameraData& camera,
        VkRenderingInfo& render_info,
        float arrowScale,
        float vmin, float vmax)
    {
        if (_vector_count == 0) return;

        // UBO
        _camera_ubo->update(&camera, sizeof(CameraData));

        DrawParams params{};
        params.count = _vector_count;
        params.arrowScale = arrowScale;
        params.vmin = vmin;
        params.vmax = (vmax > 0.0f ? vmax : _cached_vmax);
        _params_ubo->update(&params, sizeof(DrawParams));

        _frame_alloc->reset();
        vk::DescriptorSet set = _frame_alloc->allocate(*_layout);

        vkcore::DescriptorSetWriter w;
        w.writeBuffer(0, vk::DescriptorType::eUniformBuffer, _camera_ubo->getDescriptorInfo());
        w.writeBuffer(1, vk::DescriptorType::eStorageBuffer, _vec_ssbo->getDescriptorInfo());
        w.writeBuffer(2, vk::DescriptorType::eUniformBuffer, _params_ubo->getDescriptorInfo());
        w.update(*_ctx, set);

        cmd.beginRendering(render_info);
        cmd.setViewportAndScissor(render_info.renderArea.extent.width, render_info.renderArea.extent.height);
        cmd.getHandle().bindPipeline(vk::PipelineBindPoint::eGraphics, _pipeline->getHandle());
        cmd.getHandle().bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
            _pipeline_layout->getHandle(), 0, { set }, {});

        // push constants（沿用你工程里的结构：modelMatrix + size）
        PushConstantData pc{};
        pc.modelMatrix = glm::mat4(1.0f);
        pc.size = arrowScale; // 在 shader 中用于最终缩放
        cmd.getHandle().pushConstants(_pipeline_layout->getHandle(),
            vk::ShaderStageFlagBits::eMeshEXT,
            0, sizeof(PushConstantData), &pc);

        // 每 64 个矢量为一组
        constexpr uint32_t WG = 64;
        uint32_t group_count = (_vector_count + WG - 1) / WG;
        cmd.getHandle().drawMeshTasksEXT(group_count, 1, 1);

        cmd.endRendering();
    }

    void MacGridVectorRenderer::cleanup()
    {
        _pipeline.reset();
        _pipeline_layout.reset();
        _layout.reset();
        _frame_alloc.reset();
        _params_ubo.reset();
        _camera_ubo.reset();
        _vec_ssbo.reset();
    }

} // namespace dk
