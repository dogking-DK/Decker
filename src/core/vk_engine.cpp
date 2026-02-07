#include "vk_engine.h"

#include "importer/GLTF_Importer.h"
#include <vk_types.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include "VkBootstrap.h"

#include "imgui_impl_sdl3.h"
#include "imgui_impl_vulkan.h"
#include "imgui_freetype.h"

#include <glm/gtx/transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <unordered_set>

#include "Generator.h"
#include "MassSpring.h"
#include "force/GravityForce.h"
#include "force/SpringForce.h"
#include "solver/EulerSolver.h"
#include "World.h"
#include "color/UniformColorizer.h"
#include "data/MACInit.h"
#include "fluid/FluidSystem.h"
#include "force/DampingForce.h"
#include "render graph/RenderGraph.h"
#include "render graph/Resource.h"
#include "render graph/ResourceTexture.h"
#include "render graph/renderpass/BlitPass.h"
#include "render/MacGridPointRender.h"
#include "render/MacGridVectorRenderer.h"
#include "solver/PBDSolver.h"
#include "solver/StableFliuidsSolver.h"
#include "solver/VerletSolver.h"

# include <vk_mem_alloc.h>

#include "loader.h"
#include "Scene.h"
#include "render graph/renderpass/GaussianBlurPass.h"
#include "render graph/renderpass/DistortionPass.h"
#include "resource/cpu/MeshLoader.h"
#include "render/RenderSystem.h"

// 定义全局默认分发器的存储（只在一个 TU 里写！）
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

#include "AssetDB.h"
#include "resource/cpu/ResourceCache.h"
#include "resource/cpu/ResourceLoader.h"
#include "vk_debug_util.h"
#include "Vulkan/CommandBuffer.h"
#include "Vulkan/Texture.h"
#include <Vulkan/DescriptorSetLayout.h>
#include <Vulkan/DescriptorSetPool.h>
#include <Vulkan/DescriptorWriter.h>
#include "render/PointCloudRender.h"
#include "render/SpringRender.h"
#include "ui/gizmo/TranslateGizmo.h"
#include "ui/tools/TranslateTool.h"
#include "ui/tools/ToolContext.h"

template <>
struct fmt::formatter<glm::vec3>
{
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

    template <typename Context>
    constexpr auto format(const glm::vec3& foo, Context& ctx) const
    {
        return format_to(ctx.out(), "[{:7.2f}, {:7.2f}, {:7.2f}]", foo.x, foo.y, foo.z);
    }
};

namespace dk {
// we want to immediately abort when there is an error. In normal engines this
// would give an error message to the user, or perform a dump of state.
using namespace std;
using namespace render;

VulkanEngine* loadedEngine = nullptr;

VulkanEngine& VulkanEngine::Get()
{
    return *loadedEngine;
}

void VulkanEngine::init()
{
    // only one engine initialization is allowed with the application.
    assert(loadedEngine == nullptr);
    loadedEngine = this;
    SDL_SetLogPriorities(SDL_LOG_PRIORITY_DEBUG);

    // We initialize SDL and create a window with it.
    SDL_Init(SDL_INIT_VIDEO);

    //_window = SDL_CreateWindow("Vulkan Engine", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, _windowExtent.width,
    //                           _windowExtent.height, window_flags);

    init_vulkan();

    init_swapchain();

    init_commands();

    init_sync_structures();

    init_descriptors();

    init_default_data();

    init_renderables();


    fmt::print("build world\n");
    physic_world = std::make_unique<World>(WorldSettings{0.01f, 1});
    physic_world->addSystem<SpringMassSystem>("spring", std::make_unique<PBDSolver>(3));
    FluidSystem::Config cfg;
    cfg.nx     = cfg.ny = cfg.nz = 20;  // 每边 100 个 cell
    cfg.h      = 1.0f;                   // 每个 cell 1 个世界单位 => 盒子长宽高各 100
    cfg.origin = {0, 0, 0};            // 放在世界原点（可改）

    auto fluid = physic_world->addSystem<FluidSystem>("fluid", cfg);

    auto& g = fluid->grid();

    // 1) 高处落水柱
    gridinit::Scene_FallingWaterColumn(
        fluid->grid(),
        /*columnRadiusW   */ 10.0f,
        /*topHeightRatio  */ 0.90f,
        /*initVy          */ -5.0f,
        /*dyeValue        */ 1.0f
    );

    auto            sm_sys = physic_world->getSystemAs<SpringMassSystem>("spring");
    ClothProperties clothProps;
    clothProps.width_segments  = 5;
    clothProps.height_segments = 5;
    clothProps.width           = 100.0f;
    clothProps.height          = 100.0f;
    clothProps.start_position  = vec3(-7.5f, 15.0f, 0.0f);
    create_cloth(*sm_sys, clothProps);
    fmt::print("build cloth\n");

    sm_sys->addForce(std::make_unique<GravityForce>(vec3(0.0f, -9.8f, 0.0f)));
    //sm_sys->addForce(std::make_unique<DampingForce>(0.005));
    //sm_sys->addForce(std::make_unique<SpringForce>(sm_sys->getTopology_mut()));

    sm_sys->setColorizer(std::make_unique<VelocityColorizer>());

    point_cloud_renderer = std::make_unique<PointCloudRenderer>(_context);
    point_cloud_renderer->init(vk::Format::eR16G16B16A16Sfloat, vk::Format::eD32Sfloat);
    fmt::print("build point cloud render\n");

    test_render_graph();


    m_spring_renderer = std::make_unique<SpringRenderer>(_context);
    m_spring_renderer->init(vk::Format::eR16G16B16A16Sfloat, vk::Format::eD32Sfloat);
    fmt::print("build spring render\n");

    m_vector_render = std::make_unique<MacGridVectorRenderer>(_context);
    m_vector_render->init(vk::Format::eR16G16B16A16Sfloat, vk::Format::eD32Sfloat);
    m_vector_render->updateFromGrid(fluid->grid(), /*stride*/{2, 2, 2}, /*minMag*/ 0.01f);

    //m_grid_point_render = std::make_unique<MacGridPointRenderer>(_context);
    //m_grid_point_render->init(vk::Format::eR16G16B16A16Sfloat, vk::Format::eD32Sfloat);
    //m_grid_point_render->updateFromGridUniform(
    //    fluid->grid(),
    //    /*stride*/{2, 2, 2},
    //    /*mode  */ PointScalarMode::SpeedMagnitude,
    //    /*clampMin*/ 0.0f,
    //    /*clampMax*/ -1.0f  // 自动推导 vmax
    //);

    //point_cloud_renderer->getPointData() = makeRandomPointCloudSphere(10000, {0, 0, 0}, 100, false);
    physic_world->getSystemAs<SpringMassSystem>("spring")->getRenderData(point_cloud_renderer->getPointData());
    point_cloud_renderer->updatePoints();
    m_spring_renderer->updateSprings(sm_sys->getParticleData(), sm_sys->getTopology_mut());
    fmt::print("build render data\n");

    init_imgui();

    _input_backend           = std::make_unique<input::InputBackend>(_input_state);
    _input_router            = std::make_unique<input::InputRouter>();
    _camera_input_controller = std::make_unique<CameraInputController>(mainCamera);
    _tool_manager            = std::make_unique<ui::ToolManager>();
    _gizmo_manager           = std::make_unique<ui::GizmoManager>();

    _tool_manager->registerTool(std::make_unique<ui::TranslateTool>(&_gizmo_drag_state));
    _gizmo_manager->registerGizmo(std::make_unique<ui::TranslateGizmo>(&_gizmo_drag_state));
    _tool_manager->setActiveTool(ui::ToolType::Translate);
    _gizmo_manager->setActiveForTool(_tool_manager->activeType());

    _input_router->addConsumer(_gizmo_manager.get(), 100);
    _input_router->addConsumer(_tool_manager.get(), 50);
    _input_router->addConsumer(_camera_input_controller.get(), 0);

    // everything went fine
    _isInitialized = true;

    mainCamera.velocity = glm::vec3(0.f);
    mainCamera.position = glm::vec3(0.f, 0.f, 0.f);
    update_scene();
    glm::vec3 geom_center{0, 0, 0};
    glm::vec3 total_max{-FLT_MAX, -FLT_MAX, -FLT_MAX};
    glm::vec3 total_min{FLT_MAX,FLT_MAX,FLT_MAX};
    auto bound = _render_system->getRenderWorld().getAllBound();
    total_max = bound.max;
    total_min = bound.min;
    fmt::print("total min: {}, max: {}\n", total_min, total_max);
    float total_aabb_length = length(total_max - total_min);

    //mainCamera.position = total_max - total_min;
    //mainCamera.position /= 2;
    mainCamera.velocity_coefficient = total_aabb_length / 1000;
    mainCamera.position             = geom_center;
    mainCamera.pitch                = 0;
    mainCamera.yaw                  = 0;

    fmt::print("calc center: {}\n", geom_center);
}

void VulkanEngine::init_default_data()
{
    std::array<Vertex, 4> rect_vertices;

    rect_vertices[0].position = {0.5, -0.5, 0};
    rect_vertices[1].position = {0.5, 0.5, 0};
    rect_vertices[2].position = {-0.5, -0.5, 0};
    rect_vertices[3].position = {-0.5, 0.5, 0};

    rect_vertices[0].color = {0, 0, 0, 1};
    rect_vertices[1].color = {0.5, 0.5, 0.5, 1};
    rect_vertices[2].color = {1, 0, 0, 1};
    rect_vertices[3].color = {0, 1, 0, 1};

    rect_vertices[0].uv_x = 1;
    rect_vertices[0].uv_y = 0;
    rect_vertices[1].uv_x = 0;
    rect_vertices[1].uv_y = 0;
    rect_vertices[2].uv_x = 1;
    rect_vertices[2].uv_y = 1;
    rect_vertices[3].uv_x = 0;
    rect_vertices[3].uv_y = 1;

    std::array<uint32_t, 6> rect_indices;

    rect_indices[0] = 0;
    rect_indices[1] = 1;
    rect_indices[2] = 2;

    rect_indices[3] = 2;
    rect_indices[4] = 1;
    rect_indices[5] = 3;

    //rectangle = uploadMesh(rect_indices, rect_vertices);

    VkSamplerCreateInfo sampl = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};

    sampl.magFilter = VK_FILTER_NEAREST;
    sampl.minFilter = VK_FILTER_NEAREST;

    vkCreateSampler(_context->getDevice(), &sampl, nullptr, &_defaultSamplerNearest);

    sampl.magFilter = VK_FILTER_LINEAR;
    sampl.minFilter = VK_FILTER_LINEAR;
    vkCreateSampler(_context->getDevice(), &sampl, nullptr, &_defaultSamplerLinear);

    _mainDeletionQueue.push_function([&]()
    {
        vkDestroySampler(_context->getDevice(), _defaultSamplerNearest, nullptr);
        vkDestroySampler(_context->getDevice(), _defaultSamplerLinear, nullptr);
    });
}

void VulkanEngine::test_render_graph()
{
    using MyRes = RGResource<ImageDesc, FrameGraphImage>;

    MyRes* res1 = nullptr;
    MyRes* res2 = nullptr;

    // Task A
    struct CreateTempData
    {
        MyRes* distortion = nullptr;
        MyRes* blur       = nullptr;
    };

    if (render_graph == nullptr)
    {
        render_graph         = std::make_shared<RenderGraph>();
        m_blit_pass          = std::make_shared<BlitPass>();
        m_gaussian_blur_pass = std::make_shared<GaussianBlurPass>();
        m_distortion_pass    = std::make_shared<DistortionPass>();
        m_gaussian_blur_pass->init(_context);
        m_distortion_pass->init(_context);
    }
    auto color_target = _render_system ? _render_system->colorTarget() : nullptr;
    if (!color_target)
    {
        return;
    }

    const auto color_extent = color_target->getExtent();
    ImageDesc desc{
        .width = color_extent.width,
        .height = color_extent.height,
        .depth = color_extent.depth,
        .format = color_target->getFormat()
    };
    color_image = make_shared<MyRes>("color src", desc, ResourceLifetime::External);
    color_image->setExternal(color_target);
    m_blit_pass->setSrc(color_image.get());

    render_graph->addTask<CreateTempData>(
        "Create temp data",
        // setup
        [&](CreateTempData& data, RenderTaskBuilder& b)
        {
            ImageDesc desc{
                .width = color_extent.width,
                .height = color_extent.height,
                .format = color_target->getFormat(),
                .usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eColorAttachment |
                         vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage
            };
            auto* r1        = b.create<MyRes>("color_temp", desc);
            auto* r2        = b.create<MyRes>("color_after_blur", desc);
            auto* distort   = b.create<MyRes>("color_distortion", desc);
            res1            = r1;
            res2            = distort;
            data.distortion = distort;
            data.blur       = r2;
        },
        // execute
        [&](const CreateTempData& data, RenderGraphContext& ctx)
        {
            (void)data;
        }
    );
    m_blit_pass->registerToGraph(*render_graph, color_image.get(), res1);
    //m_distortion_pass->registerToGraph(*render_graph, res1, color_image.get());
    //m_gaussian_blur_pass->registerToGraph(*render_graph, res2, color_image.get());
    RenderGraphContext ctx;
    ctx.vkCtx      = _context;
    ctx.frame_data = &get_current_frame();
    // 编译 + 执行
    render_graph->compile();
    //render_graph->execute(ctx);
}

void VulkanEngine::cleanup()
{
    if (_isInitialized)
    {
        // make sure the gpu has stopped doing its things
        vkDeviceWaitIdle(_context->getDevice());

        if (m_grid_point_render)
        {
            m_grid_point_render->cleanup();
            m_grid_point_render.reset();
        }
        if (m_vector_render)
        {
            m_vector_render->cleanup();
            m_vector_render.reset();
        }
        if (m_spring_renderer)
        {
            m_spring_renderer->cleanup();
            m_spring_renderer.reset();
        }
        if (point_cloud_renderer)
        {
            point_cloud_renderer->cleanup();
            point_cloud_renderer.reset();
        }
        color_image.reset();
        render_graph.reset();
        m_blit_pass.reset();
        m_gaussian_blur_pass.reset();
        m_distortion_pass.reset();
        m_scene_system.reset();
        _render_system.reset();
        if (_upload_pool)
        {
            _upload_ctx.shutdown();
            _upload_pool.reset();
        }

        for (auto& frame : _frames)
        {
            frame._deletionQueue.flush();

            frame._dynamicDescriptorAllocator.reset();

            delete frame.command_buffer_graphic;
            frame.command_buffer_graphic = nullptr;

            delete frame.command_buffer_transfer;
            frame.command_buffer_transfer = nullptr;

            delete frame._command_pool_graphic;
            frame._command_pool_graphic = nullptr;

            delete frame._command_pool_transfer;
            frame._command_pool_transfer = nullptr;
        }

        _mainDeletionQueue.flush();

        delete _context;
    }
}

void VulkanEngine::draw_main(VkCommandBuffer cmd)
{
    auto color_target = _render_system ? _render_system->colorTarget() : nullptr;
    auto depth_target = _render_system ? _render_system->depthTarget() : nullptr;
    if (!color_target || !depth_target)
    {
        return;
    }

    const auto color_extent = color_target->getExtent();

    VkRenderingAttachmentInfo color_attachment{};
    color_attachment.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    color_attachment.imageView   = color_target->getDefaultView();
    color_attachment.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    color_attachment.loadOp      = VK_ATTACHMENT_LOAD_OP_LOAD;
    color_attachment.storeOp     = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingAttachmentInfo depth_attachment{};
    depth_attachment.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depth_attachment.imageView   = depth_target->getDefaultView();
    depth_attachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depth_attachment.loadOp      = VK_ATTACHMENT_LOAD_OP_LOAD;
    depth_attachment.storeOp     = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingInfo renderInfo{};
    renderInfo.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderInfo.renderArea.offset    = {0, 0};
    renderInfo.renderArea.extent    = {color_extent.width, color_extent.height};
    renderInfo.layerCount           = 1;
    renderInfo.colorAttachmentCount = 1;
    renderInfo.pColorAttachments    = &color_attachment;
    renderInfo.pDepthAttachment     = &depth_attachment;

    physic_world->getSystemAs<SpringMassSystem>("spring")->getRenderData(point_cloud_renderer->getPointData());
    auto sm_sys = physic_world->getSystemAs<SpringMassSystem>("spring");
    auto fluid  = physic_world->getSystemAs<FluidSystem>("fluid");

    //translate_points(point_cloud_renderer->getPointData(), { 0.1, 0, 0, 0 });
    point_cloud_renderer->updatePoints();

    m_spring_renderer->updateSprings(sm_sys->getParticleData(), sm_sys->getTopology_mut());

    point_cloud_renderer->draw(*get_current_frame().command_buffer_graphic, { mainCamera.getPVMatrix() }, renderInfo);

    m_vector_render->updateFromGrid(fluid->grid(), /*stride*/{ 2, 2, 2 }, /*minMag*/ 0.01f);

    PushVector pvc{};
    pvc.model = glm::mat4(1.0f);
    pvc.baseScale = 0.1f;
    pvc.headRatio = 0.35f;
    pvc.halfWidth = 0.02f;
    pvc.magScale = 0.8f;

    m_vector_render->draw(*get_current_frame().command_buffer_graphic,
        { mainCamera.getPVMatrix(), mainCamera.getViewMatrix(), mainCamera.getProjectMatrix() }, renderInfo, pvc);

    m_spring_renderer->draw(*get_current_frame().command_buffer_graphic, { mainCamera.getPVMatrix() }, renderInfo);

    RenderGraphContext ctx;
    ctx.vkCtx      = _context;
    ctx.frame_data = &get_current_frame();
    if (_render_system)
    {
        _render_system->execute(ctx);
    }
    else if (render_graph)
    {
        render_graph->execute(ctx);
    }
}

void VulkanEngine::draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView)
{
    VkRenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView   = targetImageView;
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp      = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp     = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingInfo renderInfo{};
    renderInfo.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderInfo.renderArea.offset    = {0, 0};
    renderInfo.renderArea.extent    = _windowExtent;
    renderInfo.layerCount           = 1;
    renderInfo.colorAttachmentCount = 1;
    renderInfo.pColorAttachments    = &colorAttachment;

    vkCmdBeginRendering(cmd, &renderInfo);

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

    vkCmdEndRendering(cmd);
}

void VulkanEngine::draw()
{
    //wait until the gpu has finished rendering the last frame. Timeout of 1 second
    VK_CHECK(vkWaitForFences(_context->getDevice(), 1, &get_current_frame()._renderFence, true, 1000000000));

    get_current_frame()._deletionQueue.flush();
    get_current_frame()._dynamicDescriptorAllocator->reset();

    //request image from the swapchain
    auto result = _context->getSwapchain()->acquire_next_image(get_current_frame()._swapchainSemaphore, nullptr);
    auto e      = static_cast<VkResult>(result.first);
    if (e == VK_ERROR_OUT_OF_DATE_KHR)
    {
        resize_requested = true;
        return;
    }
    uint32_t swapchainImageIndex = result.second;
    auto color_target = _render_system ? _render_system->colorTarget() : nullptr;
    auto depth_target = _render_system ? _render_system->depthTarget() : nullptr;
    if (!color_target || !depth_target)
    {
        return;
    }

    const auto color_extent = color_target->getExtent();

    VK_CHECK(vkResetFences(_context->getDevice(), 1, &get_current_frame()._renderFence));

    //now that we are sure that the commands finished executing, we can safely reset the command buffer to begin recording again.
    //VK_CHECK(vkResetCommandBuffer(get_current_frame()._mainCommandBuffer, 0));
    get_current_frame().command_buffer_graphic->reset();

    VkCommandBuffer cmd = get_current_frame()._mainCommandBuffer;
    cmd                 = get_current_frame().command_buffer_graphic->getHandle();

    get_current_frame().command_buffer_graphic->begin();

    auto* color_image = color_target->getImageResource();
    auto* depth_image = depth_target->getImageResource();
    if (color_image)
    {
        get_current_frame().command_buffer_graphic->transitionImage(*color_image, vkcore::ImageUsage::Storage);
    }
    if (depth_image)
    {
        get_current_frame().command_buffer_graphic->transitionImage(*depth_image, vkcore::ImageUsage::DepthStencilAttachment);
    }

    draw_main(cmd);

    if (color_image)
    {
        get_current_frame().command_buffer_graphic->transitionImage(*color_image, vkcore::ImageUsage::TransferSrc);
    }

    auto swapchain_image = _context->getSwapchain()->get_images()[swapchainImageIndex];
    get_current_frame().command_buffer_graphic->transitionImage(
        swapchain_image,
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eTransferDstOptimal);

    vk::Extent3D src_extent{color_extent.width, color_extent.height, 1};
    vk::Extent3D dst_extent{_context->getSwapchain()->get_extent().width, _context->getSwapchain()->get_extent().height, 1};

    get_current_frame().command_buffer_graphic->blitImage(
        color_target->getImage(),
        vk::ImageLayout::eTransferSrcOptimal,
        swapchain_image,
        vk::ImageLayout::eTransferDstOptimal,
        src_extent,
        dst_extent);

    if (color_image)
    {
        get_current_frame().command_buffer_graphic->transitionImage(*color_image, vkcore::ImageUsage::Storage);
    }

    get_current_frame().command_buffer_graphic->transitionImage(
        swapchain_image,
        vk::ImageLayout::eTransferDstOptimal,
        vk::ImageLayout::eColorAttachmentOptimal);

    //draw imgui into the swapchain image
    draw_imgui(cmd, _context->getSwapchain()->get_image_views()[swapchainImageIndex]);

    // set swapchain image layout to Present so we can draw it
    get_current_frame().command_buffer_graphic->transitionImage(
        swapchain_image,
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::ImageLayout::ePresentSrcKHR);

    get_current_frame().command_buffer_graphic->end();

    auto wait_info = vkcore::makeWait(get_current_frame()._swapchainSemaphore,
                                      vk::PipelineStageFlagBits2::eColorAttachmentOutput);
    auto signal_info = vkcore::makeSignal(get_current_frame()._renderSemaphore,
                                          vk::PipelineStageFlagBits2::eAllGraphics);

    get_current_frame().command_buffer_graphic->submit2(_context->getGraphicsQueue(), get_current_frame()._renderFence,
                                                        {wait_info}, {signal_info});

    //prepare present
    // this will put the image we just rendered to into the visible window.
    // we want to wait on the _renderSemaphore for that, 
    // as its necessary that drawing commands have finished before the image is displayed to the user
    VkPresentInfoKHR     presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    const VkSwapchainKHR swap_chain  = _context->getSwapchain()->get_handle();
    presentInfo.pSwapchains          = &swap_chain;
    presentInfo.swapchainCount       = 1;

    presentInfo.pWaitSemaphores    = &get_current_frame()._renderSemaphore;
    presentInfo.waitSemaphoreCount = 1;

    presentInfo.pImageIndices = &swapchainImageIndex;

    VkResult presentResult = vkQueuePresentKHR(_context->getGraphicsQueue(), &presentInfo);
    if (e == VK_ERROR_OUT_OF_DATE_KHR)
    {
        resize_requested = true;
        return;
    }
    //increase the number of frames drawn
    _frameNumber++;
}

void VulkanEngine::run()
{
    SDL_Event e;
    bool      bQuit = false;

    // --- 持久状态（比如放到你的 App/Scene 里） ---
    bool  sim_run       = false;     // 是否连续运行
    bool  sim_run_fluid = false;     // 是否连续运行
    int   step_N        = 10;        // 每次点击走多少 fixed steps
    int   queued        = 0;         // 等待执行的步数（按钮累加）
    float h             = physic_world->settings().fixed_dt;

    // main loop
    while (!bQuit)
    {
        auto start = std::chrono::system_clock::now();
        _input_state.beginFrame();

        // Handle events on queue
        while (SDL_PollEvent(&e) != 0)
        {
            // close the window when user alt-f4s or clicks the X button
            if (e.type == SDL_EVENT_TERMINATING)
                bQuit = true;

            //if (e.type == SDL_EVENT_WINDOW)
            {
                if (e.window.type == SDL_EVENT_WINDOW_RESIZED)
                {
                    resize_requested = true;
                }
                if (e.window.type == SDL_EVENT_WINDOW_MINIMIZED)
                {
                    freeze_rendering = true;
                }
                if (e.window.type == SDL_EVENT_WINDOW_RESTORED)
                {
                    freeze_rendering = false;
                }
                if (e.window.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
                    bQuit = true;
            }

            if (_input_backend)
            {
                _input_backend->feedSDLEvent(e);
            }
            ImGui_ImplSDL3_ProcessEvent(&e);
        }

        if (freeze_rendering) continue;

        if (resize_requested)
        {
            resize_swapchain();
        }

        // imgui new frame
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL3_NewFrame();

        ImGui::NewFrame();
        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
        {
            ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
        }

        _input_context.window           = _context->getWindow()->get_window();
        _input_context.camera           = &mainCamera;
        _input_context.selectedNode     = hierarchy_panel.selectedNode();
        _input_context.imguiWantsMouse  = io.WantCaptureMouse;
        _input_context.imguiWantsKeyboard = io.WantCaptureKeyboard;
        _input_context.translateEnabled = _translate_gizmo_enabled;
        if (_input_router)
        {
            _input_router->route(_input_state, _input_context);
        }
        if (_tool_manager)
        {
            ui::ToolContext tool_ctx{};
            tool_ctx.camera       = &mainCamera;
            tool_ctx.selectedNode = hierarchy_panel.selectedNode();
            tool_ctx.translateEnabled = _translate_gizmo_enabled;
            _tool_manager->update(tool_ctx);
        }
        static float time = 0;
        time += 0.001f;
        const bool settings_open = ImGui::Begin("设置面板", nullptr, ImGuiWindowFlags_NoCollapse);
        if (settings_open)
        {
            if (ImGui::CollapsingHeader("工具设置", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::Checkbox("启用平移", &_translate_gizmo_enabled);
            }
            if (ImGui::CollapsingHeader("位置信息", ImGuiTreeNodeFlags_DefaultOpen))
            {
                auto mat = mainCamera.getViewMatrix();
                ImGui::Text("view:");
                ImGui::Text("%5.5f, %5.5f, %5.5f, %5.5f", mat[0][0], mat[0][1], mat[0][2], mat[0][3]);
                ImGui::Text("%5.5f, %5.5f, %5.5f, %5.5f", mat[1][0], mat[1][1], mat[1][2], mat[1][3]);
                ImGui::Text("%5.5f, %5.5f, %5.5f, %5.5f", mat[2][0], mat[2][1], mat[2][2], mat[2][3]);
                ImGui::Text("%5.5f, %5.5f, %5.5f, %5.5f", mat[3][0], mat[3][1], mat[3][2], mat[3][3]);


                mat = mainCamera.projection;
                ImGui::Text("projection:");
                ImGui::Text("%5.5f, %5.5f, %5.5f, %5.5f", mat[0][0], mat[0][1], mat[0][2], mat[0][3]);
                ImGui::Text("%5.5f, %5.5f, %5.5f, %5.5f", mat[1][0], mat[1][1], mat[1][2], mat[1][3]);
                ImGui::Text("%5.5f, %5.5f, %5.5f, %5.5f", mat[2][0], mat[2][1], mat[2][2], mat[2][3]);
                ImGui::Text("%5.5f, %5.5f, %5.5f, %5.5f", mat[3][0], mat[3][1], mat[3][2], mat[3][3]);

                mat = mainCamera.ortho;
                ImGui::Text("ortho:");
                ImGui::Text("%5.5f, %5.5f, %5.5f, %5.5f", mat[0][0], mat[0][1], mat[0][2], mat[0][3]);
                ImGui::Text("%5.5f, %5.5f, %5.5f, %5.5f", mat[1][0], mat[1][1], mat[1][2], mat[1][3]);
                ImGui::Text("%5.5f, %5.5f, %5.5f, %5.5f", mat[2][0], mat[2][1], mat[2][2], mat[2][3]);
                ImGui::Text("%5.5f, %5.5f, %5.5f, %5.5f", mat[3][0], mat[3][1], mat[3][2], mat[3][3]);

                auto vect = mainCamera.position;
                ImGui::Text("camera position:");
                ImGui::Text("%5.5f, %5.5f, %5.5f", vect[0], vect[1], vect[2]);

                vect = mainCamera.camera_direction;
                ImGui::Text("camera direction:");
                ImGui::Text("%5.5f, %5.5f, %5.5f", vect[0], vect[1], vect[2]);

                vect = mainCamera.camera_up;
                ImGui::Text("camera up:");
                ImGui::Text("%5.5f, %5.5f, %5.5f", vect[0], vect[1], vect[2]);
            }
            if (ImGui::CollapsingHeader("渲染设置", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::Text("frametime %f ms", stats.frametime);
                ImGui::Text("fps: %i", static_cast<int>(1000.0f / stats.frametime));
                ImGui::Text("drawtime %f ms", stats.mesh_draw_time);
                ImGui::Text("triangles %i", stats.triangle_count);
                ImGui::Text("draws %i", stats.drawcall_count);
                if (ImGui::Checkbox("Show AABB", &_show_aabb_bounds))
                {
                    if (_render_system)
                    {
                        _render_system->setDebugDrawAabb(_show_aabb_bounds);
                    }
                }
            }
            mainCamera.renderUI();
        }
        ImGui::End();
        // 资源树
        hierarchy_panel.onGui();
        // 顶部工具栏
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("文件"))
            {
                if (ImGui::MenuItem("新建", "Ctrl+N"))
                {
                    // 新建操作
                }
                if (ImGui::MenuItem("打开", "Ctrl+O"))
                {
                    // 打开操作
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("编辑"))
            {
                if (ImGui::MenuItem("剪切", "Ctrl+X"))
                {
                    // 剪切操作
                }
                if (ImGui::MenuItem("复制", "Ctrl+C"))
                {
                    // 复制操作
                }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }


        // --- 每帧 UI ---
        ImGui::Begin("Simulation");
        ImGui::Checkbox("Run Spring", &sim_run);
        ImGui::SameLine();
        if (ImGui::Button("Step 1")) queued += 1;
        ImGui::SameLine();
        if (ImGui::Button("Step 10")) queued += 10;
        ImGui::SameLine();
        if (ImGui::Button("Step 100")) queued += 100;

        ImGui::InputInt("Step N", &step_N);
        if (ImGui::Button("Step N Go")) queued += std::max(0, step_N);

        // 可选：按秒推进
        static float secs = 0.5f;
        ImGui::InputFloat("Advance secs", &secs);
        if (ImGui::Button("Advance by secs"))
        {
            queued += std::max(0, static_cast<int>(std::round(secs / h)));
        }
        ImGui::End();

        // --- 每帧更新 ---
        if (sim_run)
        {
            // 正常实时推进
            for (int i = 0; i < step_N; ++i)
            {
                physic_world->tick(physic_world->settings().fixed_dt);  // 你的帧间隔
            }
        }
        else
        {
            // 暂停状态下，按排队步数推进
            if (queued > 0)
            {
                physic_world->tick(h);     // 每次推进正好一个 fixed step
                queued--;
            }
        }

        ImGui::Render();

        update_scene();

        draw();

        auto end     = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        stats.frametime = elapsed.count() / 1000.f;

        FrameMark;
    }
}

void VulkanEngine::update_scene()
{
    mainCamera.update();

    glm::mat4 view = mainCamera.getViewMatrix();

    if (_render_system)
    {
        _render_system->beginUiFrame();
        if (_gizmo_manager)
        {
            ui::GizmoContext gizmo_ctx{};
            gizmo_ctx.camera = &mainCamera;
            gizmo_ctx.selectedNode = hierarchy_panel.selectedNode();
            gizmo_ctx.uiRenderService = &_render_system->uiRenderService();
            _gizmo_manager->render(gizmo_ctx);
        }
        _render_system->finalizeUiFrame();
    }

    // camera projection
    glm::mat4 projection = glm::perspective(glm::radians(60.f),
                                            static_cast<float>(_windowExtent.width) / static_cast<float>(_windowExtent.
                                                height), 0.1f, 1000.0f);

    // invert the Y direction on projection matrix so that we are more similar
    // to opengl and gltf axis
    projection[1][1] *= -1;

    glm::mat4 proj, viewproj;
    if (mainCamera.view_mode == VIEW_MODE::perspective)
    {
        proj     = mainCamera.projection;
        viewproj = mainCamera.projection * view;
    }
    else if (mainCamera.view_mode == VIEW_MODE::orthographic)
    {
        proj     = mainCamera.ortho;
        viewproj = mainCamera.ortho * view;
    }

    if (_render_system && m_scene_system)
    {
        UUID selected_id{};
        if (auto* selected = hierarchy_panel.selectedNode())
        {
            selected_id = selected->id;
        }
        _render_system->setSelectedNodeId(selected_id);
        _render_system->prepareFrame(*m_scene_system->currentScene(),
                                     view,
                                     proj,
                                     mainCamera.position,
                                     {_windowExtent.width, _windowExtent.height});
    }
}

FrameData& VulkanEngine::get_current_frame()
{
    return _frames[_frameNumber % FRAME_OVERLAP];
}

FrameData& VulkanEngine::get_frame(const int id)
{
    return _frames[id];
}

FrameData& VulkanEngine::get_last_frame()
{
    return _frames[(_frameNumber - 1) % FRAME_OVERLAP];
}

void VulkanEngine::init_vulkan()
{
    _context = new vkcore::VulkanContext(_windowExtent.width, _windowExtent.height);
}

void VulkanEngine::init_swapchain()
{
    _windowExtent.height = _context->getSwapchain()->get_extent().height;
    _windowExtent.width  = _context->getSwapchain()->get_extent().width;
}

void VulkanEngine::resize_swapchain()
{
    vkDeviceWaitIdle(_context->getDevice());
    _context->resizeSwapchainAuto();
    _windowExtent.height = _context->getSwapchain()->get_extent().height;
    _windowExtent.width  = _context->getSwapchain()->get_extent().width;
    if (_render_system)
    {
        _render_system->resizeRenderTargets(vk::Extent2D{_windowExtent.width, _windowExtent.height});
    }
    resize_requested     = false;
}

void VulkanEngine::init_commands()
{
    for (int i = 0; i < FRAME_OVERLAP; i++)
    {
        _frames[i]._command_pool_graphic  = new vkcore::CommandPool(_context, _context->getGraphicsQueueIndex());
        _frames[i]._command_pool_transfer = new vkcore::CommandPool(_context, _context->getTransferQueueIndex());

        //_frames[i].command_buffer_graphic = new vkcore::CommandBuffer(_context, _frames[i]._command_pool_graphic);
        _frames[i].command_buffer_graphic = new vkcore::CommandBuffer(_context, _frames[i]._command_pool_graphic);
        std::string name                  = "frame graphic command ";
        name.append(std::to_string(i));
        _frames[i].command_buffer_graphic->setDebugName(name);
        _frames[i].command_buffer_transfer = new vkcore::CommandBuffer(_context, _frames[i]._command_pool_transfer);
    }

    _upload_pool = std::make_unique<vkcore::CommandPool>(_context, _context->getGraphicsQueueIndex());
    _upload_ctx.init(_context, _upload_pool.get(), _context->getGraphicsQueue());
}

void VulkanEngine::init_sync_structures()
{
    // create syncronization structures
    // one fence to control when the gpu has finished rendering the frame,
    // and 2 semaphores to syncronize rendering with swapchain
    // we want the fence to start signalled so we can wait on it on the first
    // frame
    VkFenceCreateInfo fenceCreateInfo{};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    VK_CHECK(vkCreateFence(_context->getDevice(), &fenceCreateInfo, nullptr, &_immFence));

    _mainDeletionQueue.push_function([this]() { _context->getDevice().destroyFence(_immFence); });

    for (int i = 0; i < FRAME_OVERLAP; i++)
    {
        VK_CHECK(vkCreateFence(_context->getDevice(), &fenceCreateInfo, nullptr, &_frames[i]._renderFence));

        VkSemaphoreCreateInfo semaphoreCreateInfo{};
        semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VK_CHECK(vkCreateSemaphore(_context->getDevice(), &semaphoreCreateInfo, nullptr,
                                   &_frames[i]._swapchainSemaphore));
        VK_CHECK(vkCreateSemaphore(_context->getDevice(), &semaphoreCreateInfo, nullptr, &_frames[i]._renderSemaphore));

        _mainDeletionQueue.push_function([=]()
        {
            vkDestroyFence(_context->getDevice(), _frames[i]._renderFence, nullptr);
            vkDestroySemaphore(_context->getDevice(), _frames[i]._swapchainSemaphore, nullptr);
            vkDestroySemaphore(_context->getDevice(), _frames[i]._renderSemaphore, nullptr);
        });
    }
}

void VulkanEngine::init_renderables()
{
    namespace fs = std::filesystem;
    std::string structurePath;
    fs::path    current_dir    = get_exe_dir(); // 当前目录
    fs::path    file_json_path = absolute(current_dir / "assets/config/file.json"); // 矫正分隔符
    fmt::print(fg(fmt::color::bisque), "file config path: {}\n", file_json_path.string());
    if (exists(file_json_path))
    {
        std::ifstream  file(file_json_path);
        nlohmann::json j;
        file >> j;
        structurePath = vkutil::get_model_path(j, j["load_file"]["name"]).string();
    }
    fmt::print(fg(fmt::color::bisque), "model file path: {}\n", structurePath);
    //auto structureFile = loadGltf(this, structurePath);

    auto root = ImporterRegistry::instance().import(structurePath);

    uuids::uuid id = uuid_generate();
    auto id_str = to_string(id);
    uuids::uuid convert_id = uuid_parse(id_str);
    auto convert_id_str = to_string(convert_id);

    AssetDB::instance().open();
    for (const auto& meta : root.metas)
    {
        AssetDB::instance().upsert(meta);
    }
    // 1 假设导入阶段已写入 metas & raw；此处只加载
    _cpu_cache  = ResourceCache{};
    _cpu_loader = std::make_unique<ResourceLoader>("cache/raw", AssetDB::instance(), _cpu_cache);
    //auto           result = loader.load<MeshData>(root.metas[0].uuid);
    //auto           result1 = loader.load<MeshData>(root.metas[0].uuid);

    m_scene_system = std::make_shared<SceneSystem>(std::make_unique<SceneBuilder>(),
                                                   std::make_unique<SceneResourceBinder>());
    m_scene_system->buildSceneFromImport("scene", root);
    m_scene_system->preloadResources(*_cpu_loader, _cpu_cache);

    hierarchy_panel.setRoots(m_scene_system->currentScene()->getRoot().get());
    //assert(structureFile.has_value());
    
    //loadedScenes["structure"] = *structureFile;

    _render_system = std::make_unique<RenderSystem>(*_context, _upload_ctx, *_cpu_loader);
    _render_system->init(_color_format, _depth_format);
    _render_system->resizeRenderTargets(vk::Extent2D{_windowExtent.width, _windowExtent.height});
}

void VulkanEngine::init_imgui()
{
    // 1: create descriptor pool for IMGUI
    //  the size of the pool is very oversize, but it's copied from imgui demo
    //  itself.
    VkDescriptorPoolSize pool_sizes[] = {
        {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}
    };

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType                      = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags                      = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets                    = 1000;
    pool_info.poolSizeCount              = static_cast<uint32_t>(std::size(pool_sizes));
    pool_info.pPoolSizes                 = pool_sizes;

    VkDescriptorPool imguiPool;
    VK_CHECK(vkCreateDescriptorPool(_context->getDevice(), &pool_info, nullptr, &imguiPool));

    // 2: initialize imgui library

    // this initializes the core structures of imgui
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    //io.FontGlobalScale = _context->getWindow()->get_dpi_factor(); // 放大字体
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // 开启docking

    ImFontConfig font_cfg;
    font_cfg.FontBuilderFlags = ImGuiFreeTypeBuilderFlags_ForceAutoHint;  // 额外的 FreeType 设置
    font_cfg.FontBuilderFlags = ImGuiFreeTypeBuilderFlags_ForceAutoHint | ImGuiFreeTypeBuilderFlags_Monochrome;
    namespace fs = std::filesystem;
    fs::path current_dir = get_exe_dir(); // 当前目录
    fs::path target_file = absolute(current_dir / "assets/font/SourceHanSansCN-Regular.otf"); // 矫正分隔符
    io.Fonts->AddFontFromFileTTF(target_file.string().c_str(), 20.0f, &font_cfg, io.Fonts->GetGlyphRangesChineseFull());

    // this initializes imgui for SDL
    ImGui_ImplSDL3_InitForVulkan(_context->getWindow()->get_window());

    // this initializes imgui for Vulkan
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance                  = _context->getInstance();
    init_info.PhysicalDevice            = _context->getPhysicalDevice();
    init_info.Device                    = _context->getDevice();
    init_info.Queue                     = _context->getGraphicsQueue();
    init_info.DescriptorPool            = imguiPool;
    init_info.MinImageCount             = 3;
    init_info.ImageCount                = 3;
    init_info.UseDynamicRendering       = true;

    //dynamic rendering parameters for imgui to use
    init_info.PipelineRenderingCreateInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    init_info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    VkFormat imguiFormat = VK_FORMAT_B8G8R8A8_UNORM;
    init_info.PipelineRenderingCreateInfo.pColorAttachmentFormats = &imguiFormat;

    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&init_info);

    ImGui_ImplVulkan_CreateFontsTexture();

    // add the destroy the imgui created structures
    _mainDeletionQueue.push_function([=]()
    {
        ImGui_ImplVulkan_Shutdown();
        vkDestroyDescriptorPool(_context->getDevice(), imguiPool, nullptr);
    });
}

void VulkanEngine::init_descriptors()
{
    for (int i = 0; i < FRAME_OVERLAP; i++)
    {
        std::vector<vk::DescriptorPoolSize> pool_sizes = {
            {vk::DescriptorType::eSampler, 1000},
            {vk::DescriptorType::eCombinedImageSampler, 1000},
            {vk::DescriptorType::eSampledImage, 1000},
            {vk::DescriptorType::eStorageImage, 1000},
            {vk::DescriptorType::eUniformTexelBuffer, 1000},
            {vk::DescriptorType::eStorageTexelBuffer, 1000},
            {vk::DescriptorType::eUniformBuffer, 1000},
            {vk::DescriptorType::eStorageBuffer, 1000},
            {vk::DescriptorType::eUniformBufferDynamic, 1000},
            {vk::DescriptorType::eStorageBufferDynamic, 1000},
            {vk::DescriptorType::eInputAttachment, 1000}
        };

        _frames[i]._dynamicDescriptorAllocator = std::make_unique<vkcore::GrowableDescriptorAllocator>(
            _context, 1000, pool_sizes);
    }
}

VulkanEngine::VulkanEngine()
{
}

VulkanEngine::~VulkanEngine()
{
}
}
