#include "MacGridPointRender.h"
#include <algorithm>
#include <filesystem>
#include <stdexcept>

#include "Vulkan/ShaderModule.h"

namespace dk {

    void MacGridPointRenderer::init(vk::Format color_format, vk::Format depth_format)
    {
        createBuffers();
        createDescriptors();
        createPipeline(color_format, depth_format);
    }

    void MacGridPointRenderer::createBuffers()
    {
        _capacity = 4'000'000; // 足够大，可按需调整

        auto bb = vkcore::BufferResource::Builder();
        _points_ssbo = bb.setSize(sizeof(GPUPoint) * _capacity)
            .setUsage(vk::BufferUsageFlagBits::eStorageBuffer)
            .withVmaRequiredFlags(vk::MemoryPropertyFlagBits::eHostVisible)
            .withVmaUsage(VMA_MEMORY_USAGE_CPU_TO_GPU)
            .buildUnique(*_ctx);

        _camera_ubo = vkcore::BufferResource::Builder()
            .setSize(sizeof(CameraData))
            .setUsage(vk::BufferUsageFlagBits::eUniformBuffer)
            .withVmaRequiredFlags(vk::MemoryPropertyFlagBits::eHostVisible)
            .withVmaUsage(VMA_MEMORY_USAGE_CPU_TO_GPU)
            .buildUnique(*_ctx);

        _params_ubo = vkcore::BufferResource::Builder()
            .setSize(sizeof(DrawParamsPoints))
            .setUsage(vk::BufferUsageFlagBits::eUniformBuffer)
            .withVmaRequiredFlags(vk::MemoryPropertyFlagBits::eHostVisible)
            .withVmaUsage(VMA_MEMORY_USAGE_CPU_TO_GPU)
            .buildUnique(*_ctx);
    }

    void MacGridPointRenderer::createDescriptors()
    {
        std::vector<vk::DescriptorPoolSize> pool_sizes = {
            {vk::DescriptorType::eStorageBuffer, 10},
            {vk::DescriptorType::eUniformBuffer, 10}
        };
        _frame_alloc = std::make_unique<vkcore::GrowableDescriptorAllocator>(_ctx, 10, pool_sizes);

        // set=0: 0=Camera, 1=Points SSBO, 2=Params
        vkcore::DescriptorSetLayout::Builder b(_ctx);
        b.addBinding(0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eMeshEXT)
            .addBinding(1, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eMeshEXT)
            .addBinding(2, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eMeshEXT);
        _layout = b.build();
    }

    void MacGridPointRenderer::createPipeline(vk::Format color_format, vk::Format depth_format)
    {
        // push 常量沿用你的结构（modelMatrix + size）
        vk::PushConstantRange pcr{};
        pcr.stageFlags = vk::ShaderStageFlagBits::eMeshEXT;
        pcr.offset = 0;
        pcr.size = sizeof(PushConstantData);

        _pipeline_layout = std::make_unique<vkcore::PipelineLayout>(
            _ctx,
            std::vector<vkcore::DescriptorSetLayout*>{_layout.get()},
            std::vector<vk::PushConstantRange>{pcr}
        );

        namespace fs = std::filesystem;
        fs::path cwd = fs::current_path();
        auto mesh_spv = vkcore::loadSpirvFromFile(fs::absolute(cwd / "../../assets/shaders/spv/fluid/macgrid_points.mesh.spv"));
        auto frag_spv = vkcore::loadSpirvFromFile(fs::absolute(cwd / "../../assets/shaders/spv/fluid/macgrid_points.frag.spv"));
        auto mesh_mod = std::make_unique<vkcore::ShaderModule>(_ctx, mesh_spv);
        auto frag_mod = std::make_unique<vkcore::ShaderModule>(_ctx, frag_spv);

        vkcore::PipelineBuilder pb(_ctx);
        _pipeline = pb.setLayout(_pipeline_layout.get())
            .setShaders(mesh_mod->getHandle(), frag_mod->getHandle())
            .setRenderingInfo({ color_format }, depth_format)
            .setPolygonMode(vk::PolygonMode::eFill)
            .setCullMode(vk::CullModeFlagBits::eNone)
            .enableDepthTest(true, vk::CompareOp::eLessOrEqual)
            .disableColorBlending()
            .build();
    }

    void MacGridPointRenderer::uploadGPU()
    {
        _count = static_cast<uint32_t>(_cpu_points.size());
        if (_count == 0) return;
        if (_count > _capacity) throw std::runtime_error("MacGridPointRenderer: point count exceeds capacity");
        _points_ssbo->update(_cpu_points.data(), sizeof(GPUPoint) * _count);
    }

    void MacGridPointRenderer::updateFromGridUniform(
        const MacGrid& g,
        glm::ivec3 stride,
        PointScalarMode mode,
        float clampMin, float clampMax)
    {
        _cpu_points.clear();
        _cpu_points.reserve((g.nx() / stride.x + 1) * (g.ny() / stride.y + 1) * (g.nz() / stride.z + 1));

        auto sxi = std::max(1, stride.x);
        auto syi = std::max(1, stride.y);
        auto szi = std::max(1, stride.z);

        float vmax = 0.0f;

        for (int k = 0; k < g.nz(); k += szi)
            for (int j = 0; j < g.ny(); j += syi)
                for (int i = 0; i < g.nx(); i += sxi)
                {
                    glm::vec3 c = g.cellCenter(i, j, k);

                    float s = 0.0f;
                    switch (mode) {
                    case PointScalarMode::SpeedMagnitude: s = glm::length(g.sampleVelocity(c)); break;
                    case PointScalarMode::Dye:            s = g.Dye(i, j, k); break;
                    case PointScalarMode::Pressure:       s = g.P(i, j, k);   break;
                    case PointScalarMode::Divergence:     s = localDivergenceAt(g, i, j, k); break;
                    }
                    if (clampMax > 0.0f) s = std::min(s, clampMax);
                    if (s < clampMin) continue;

                    vmax = std::max(vmax, std::abs(s));
                    _cpu_points.push_back({ glm::vec4(c,1.f), glm::vec4(s,0,0,0) });
                }

        _cached_vmax = std::max(vmax, 1e-6f);
        uploadGPU();
    }

    void MacGridPointRenderer::draw(vkcore::CommandBuffer& cmd,
        const CameraData& camera,
        VkRenderingInfo& render_info,
        float pointPixelSize,
        float vmin, float vmax)
    {
        if (_count == 0) return;

        _camera_ubo->update(&camera, sizeof(CameraData));

        DrawParamsPoints params{};
        params.count = _count;
        params.pointPixelSize = pointPixelSize;                  // 像素大小
        params.vmin = vmin;
        params.vmax = (vmax > 0.0f ? vmax : _cached_vmax);
        _params_ubo->update(&params, sizeof(DrawParamsPoints));

        _frame_alloc->reset();
        vk::DescriptorSet set = _frame_alloc->allocate(*_layout);

        vkcore::DescriptorSetWriter w;
        w.writeBuffer(0, vk::DescriptorType::eUniformBuffer, _camera_ubo->getDescriptorInfo());
        w.writeBuffer(1, vk::DescriptorType::eStorageBuffer, _points_ssbo->getDescriptorInfo());
        w.writeBuffer(2, vk::DescriptorType::eUniformBuffer, _params_ubo->getDescriptorInfo());
        w.update(*_ctx, set);

        cmd.beginRendering(render_info);
        cmd.setViewportAndScissor(render_info.renderArea.extent.width, render_info.renderArea.extent.height);
        cmd.getHandle().bindPipeline(vk::PipelineBindPoint::eGraphics, _pipeline->getHandle());
        cmd.getHandle().bindDescriptorSets(vk::PipelineBindPoint::eGraphics, _pipeline_layout->getHandle(), 0, { set }, {});

        PushConstantData pc{};
        pc.modelMatrix = glm::mat4(1.f);
        pc.size = pointPixelSize; // 冗余
        cmd.getHandle().pushConstants(_pipeline_layout->getHandle(),
            vk::ShaderStageFlagBits::eMeshEXT,
            0, sizeof(PushConstantData), &pc);

        // 每组 128 个点
        constexpr uint32_t WG = 128;
        uint32_t groups = (_count + WG - 1) / WG;
        cmd.getHandle().drawMeshTasksEXT(groups, 1, 1);

        cmd.endRendering();
    }

    void MacGridPointRenderer::cleanup()
    {
        _pipeline.reset();
        _pipeline_layout.reset();
        _layout.reset();
        _frame_alloc.reset();
        _params_ubo.reset();
        _camera_ubo.reset();
        _points_ssbo.reset();
    }

} // namespace dk
