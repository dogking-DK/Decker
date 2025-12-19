#include "vk_engine.h"

#include "vk_images.h"
#include "vk_loader.h"
#include "vk_descriptors.h"
#include "importer/GLTF_Importer.h"
#include <vk_types.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <vk_initializers.h>

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

#include "Scene.h"
#include "render graph/renderpass/GaussianBlurPass.h"
#include "resource/cpu/MeshLoader.h"

// 定义全局默认分发器的存储（只在一个 TU 里写！）
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

#include "AssetDB.h"
#include "resource/cpu/ResourceCache.h"
#include "resource/cpu/ResourceLoader.h"
#include "vk_debug_util.h"
#include "Vulkan/CommandBuffer.h"
#include <Vulkan/DescriptorSetLayout.h>
#include <Vulkan/DescriptorSetPool.h>
#include <Vulkan/DescriptorWriter.h>
#include "render/PointCloudRender.h"
#include "render/SpringRender.h"

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

    init_pipelines();

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

    // everything went fine
    _isInitialized = true;

    mainCamera.velocity = glm::vec3(0.f);
    mainCamera.position = glm::vec3(30.f, -00.f, -085.f);

    glm::vec3 geom_center{0, 0, 0};
    int       count = 0;
    glm::vec3 total_max{-FLT_MAX, -FLT_MAX, -FLT_MAX};
    glm::vec3 total_min{FLT_MAX,FLT_MAX,FLT_MAX};
    for (const auto& mesh : loadedScenes["structure"]->meshes)
    {
        fmt::print("{}: {}\n", mesh.first, mesh.second->name);
        for (const auto& surface : mesh.second->surfaces)
        {
            geom_center += surface.bounds.origin;
            total_max.x = std::max(total_max.x, surface.bounds.max_edge.x);
            total_max.y = std::max(total_max.y, surface.bounds.max_edge.y);
            total_max.z = std::max(total_max.z, surface.bounds.max_edge.z);
            total_min.x = std::min(total_min.x, surface.bounds.min_edge.x);
            total_min.y = std::min(total_min.y, surface.bounds.min_edge.y);
            total_min.z = std::min(total_min.z, surface.bounds.min_edge.z);
            fmt::print("{}: center: {},min: {},max: {}\n", count, surface.bounds.origin, surface.bounds.min_edge,
                       surface.bounds.max_edge);

            ++count;
        }
        //geom_center += mesh.second->surfaces[0].bounds.sphereRadius* glm::vec3{ 1,0,0 };
    }
    fmt::print("total min: {}, max: {}\n", total_min, total_max);
    float total_aabb_length = length(total_max - total_min);
    geom_center /= count;

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

    //3 default textures, white, grey, black. 1 pixel each
    uint32_t white = packUnorm4x8(glm::vec4(1, 1, 1, 1));
    _whiteImage    = create_image(&white, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM,
                                  VK_IMAGE_USAGE_SAMPLED_BIT);

    uint32_t grey = packUnorm4x8(glm::vec4(0.66f, 0.66f, 0.66f, 1));
    _greyImage    = create_image(&grey, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM,
                                 VK_IMAGE_USAGE_SAMPLED_BIT);

    uint32_t black = packUnorm4x8(glm::vec4(0, 0, 0, 0));
    _blackImage    = create_image(&black, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM,
                                  VK_IMAGE_USAGE_SAMPLED_BIT);

    //checkerboard image
    uint32_t                      magenta = packUnorm4x8(glm::vec4(1, 0, 1, 1));
    std::array<uint32_t, 16 * 16> pixels; //for 16x16 checkerboard texture
    for (int x = 0; x < 16; x++)
    {
        for (int y = 0; y < 16; y++)
        {
            pixels[y * 16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
        }
    }

    _errorCheckerboardImage = create_image(pixels.data(), VkExtent3D{16, 16, 1}, VK_FORMAT_R8G8B8A8_UNORM,
                                           VK_IMAGE_USAGE_SAMPLED_BIT);

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
        MyRes* color = nullptr;
    };

    if (render_graph == nullptr)
    {
        render_graph         = std::make_shared<RenderGraph>();
        m_blit_pass          = std::make_shared<BlitPass>();
        m_gaussian_blur_pass = std::make_shared<GaussianBlurPass>();
        m_gaussian_blur_pass->init(_context);
    }
    ImageDesc desc{
        .width = _drawImage.imageExtent.width,
        .height = _drawImage.imageExtent.height,
        .depth = _drawImage.imageExtent.depth,
        .format = static_cast<vk::Format>(_drawImage.imageFormat)
    };
    color_image = make_shared<MyRes>("color src", desc, ResourceLifetime::External);
    color_image->setExternal(
        _drawImage.image,
        _drawImage.imageView  // 或者你封装里的 view
    );
    m_blit_pass->setSrc(color_image.get());

    render_graph->addTask<CreateTempData>(
        "Create temp data",
        // setup
        [&](CreateTempData& data, RenderTaskBuilder& b)
        {
            ImageDesc desc{
                .width = _drawImage.imageExtent.width,
                .height = _drawImage.imageExtent.height,
                .format = static_cast<vk::Format>(_drawImage.imageFormat),
                .usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage
            };
            auto* r1 = b.create<MyRes>("color_temp", desc);
            auto* r2 = b.create<MyRes>("color_after_blur", desc);
            //data.r1 = r1;
            res1 = r1;
            res2 = r2;
        },
        // execute
        [&](const CreateTempData& data, RenderGraphContext& ctx)
        {
            //std::cout << "      [TaskA] running. R1=" << data.r1->get() << "\n";
        }
    );
    m_blit_pass->registerToGraph(*render_graph, color_image.get(), res1);
    //m_gaussian_blur_pass->registerToGraph(*render_graph, res1, color_image.get());
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
        m_scene_system.reset();

        loadedScenes.clear();

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

        destroy_image(_whiteImage);
        destroy_image(_blackImage);
        destroy_image(_greyImage);
        destroy_image(_errorCheckerboardImage);

        _mainDeletionQueue.flush();

        delete _context;
    }
}

void VulkanEngine::init_background_pipelines()
{
    VkPipelineLayoutCreateInfo computeLayout{};
    computeLayout.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    computeLayout.pNext          = nullptr;
    computeLayout.pSetLayouts    = &_drawImageDescriptorLayout;
    computeLayout.setLayoutCount = 1;

    VkPushConstantRange pushConstant{};
    pushConstant.offset     = 0;
    pushConstant.size       = sizeof(ComputePushConstants);
    pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    computeLayout.pPushConstantRanges    = &pushConstant;
    computeLayout.pushConstantRangeCount = 1;

    VK_CHECK(vkCreatePipelineLayout(_context->getDevice(), &computeLayout, nullptr, &_gradientPipelineLayout));
    DebugUtils::getInstance().setDebugName(_context->getDevice(), VK_OBJECT_TYPE_PIPELINE_LAYOUT, reinterpret_cast<
                                               uint64_t>(_gradientPipelineLayout), "background layout");
    VkShaderModule gradientShader;
    namespace fs = std::filesystem;
    fs::path current_dir = fs::current_path(); // 当前目录
    fs::path target_file = absolute(current_dir / "../../assets/shaders/spv/gradient_color.comp.spv"); // 矫正分隔符
    if (!vkutil::load_shader_module(target_file.string(), _context->getDevice(), &gradientShader))
    {
        fmt::print("Error when building the compute shader \n");
    }

    VkShaderModule skyShader;
    target_file = absolute(current_dir / "../../assets/shaders/spv/sky.comp.spv"); // 矫正分隔符
    if (!vkutil::load_shader_module(target_file.string(), _context->getDevice(),
                                    &skyShader))
    {
        fmt::print("Error when building the compute shader\n");
    }

    VkPipelineShaderStageCreateInfo stageinfo{};
    stageinfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageinfo.pNext  = nullptr;
    stageinfo.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
    stageinfo.module = gradientShader;
    stageinfo.pName  = "main";

    VkComputePipelineCreateInfo computePipelineCreateInfo{};
    computePipelineCreateInfo.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    computePipelineCreateInfo.pNext  = nullptr;
    computePipelineCreateInfo.layout = _gradientPipelineLayout;
    computePipelineCreateInfo.stage  = stageinfo;

    ComputeEffect gradient;
    gradient.layout = _gradientPipelineLayout;
    gradient.name   = "gradient";
    gradient.data   = {};

    //default colors
    gradient.data.data1 = glm::vec4(1, 0, 0, 1);
    gradient.data.data2 = glm::vec4(0, 0, 1, 1);

    VK_CHECK(vkCreateComputePipelines(_context->getDevice(), VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr,
                                      &gradient.pipeline));
    DebugUtils::getInstance().setDebugName(_context->getDevice(), VK_OBJECT_TYPE_PIPELINE,
                                           reinterpret_cast<uint64_t>(gradient.pipeline), "gradient pipeline");


    //change the shader module only to create the sky shader
    computePipelineCreateInfo.stage.module = skyShader;

    ComputeEffect sky;
    sky.layout = _gradientPipelineLayout;
    sky.name   = "sky";
    sky.data   = {};
    //default sky parameters
    sky.data.data1 = glm::vec4(0.1, 0.2, 0.4, 0.97);

    VK_CHECK(vkCreateComputePipelines(_context->getDevice(), VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr,
                                      &sky.pipeline));
    DebugUtils::getInstance().setDebugName(_context->getDevice(), VK_OBJECT_TYPE_PIPELINE,
                                           reinterpret_cast<uint64_t>(sky.pipeline),
                                           "sky pipeline");

    //add the 2 background effects into the array
    backgroundEffects.push_back(gradient);
    backgroundEffects.push_back(sky);

    //destroy structures properly
    vkDestroyShaderModule(_context->getDevice(), gradientShader, nullptr);
    vkDestroyShaderModule(_context->getDevice(), skyShader, nullptr);

    print(fg(fmt::color::aqua), "Created sky pipeline: {:#x}\n", reinterpret_cast<uint64_t>(sky.pipeline));
    print(fg(fmt::color::aqua), "Created gradient pipeline: {:#x}\n",
          reinterpret_cast<uint64_t>(gradient.pipeline));
    _mainDeletionQueue.push_function([&]()
    {
        print(fg(fmt::color::aqua), "back ground release begin\n");
        if (sky.pipeline != VK_NULL_HANDLE)
        {
            vkDestroyPipeline(_context->getDevice(), backgroundEffects[1].pipeline, nullptr);
            backgroundEffects[1].pipeline = VK_NULL_HANDLE; // 将句柄置为空
            print(fg(fmt::color::aqua), "destroy sky pipeline\n");
        }

        // 销毁 gradient.pipeline

        if (gradient.pipeline != VK_NULL_HANDLE)
        {
            vkDestroyPipeline(_context->getDevice(), backgroundEffects[0].pipeline, nullptr);
            backgroundEffects[0].pipeline = VK_NULL_HANDLE; // 将句柄置为空
            print(fg(fmt::color::aqua), "destroy gradient pipeline\n");
        }

        // 销毁 PipelineLayout
        if (_gradientPipelineLayout != VK_NULL_HANDLE)
        {
            vkDestroyPipelineLayout(_context->getDevice(), _gradientPipelineLayout, nullptr);
            _gradientPipelineLayout = VK_NULL_HANDLE; // 将句柄置为空
            print(fg(fmt::color::aqua), "destroy gradient pipeline layout\n");
        }
        print(fg(fmt::color::aqua), "back ground release end\n");
    });
}

void VulkanEngine::draw_main(VkCommandBuffer cmd)
{
    ComputeEffect& effect = backgroundEffects[currentBackgroundEffect];

    // bind the background compute pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, effect.pipeline);

    // bind the descriptor set containing the draw image for the compute pipeline
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _gradientPipelineLayout, 0, 1, &_drawImageDescriptors,
                            0, nullptr);

    vkCmdPushConstants(cmd, _gradientPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants),
                       &effect.data);
    // execute the compute pipeline dispatch. We are using 16x16 workgroup size so we need to divide by it
    vkCmdDispatch(cmd, std::ceil(_drawExtent.width / 16.0), std::ceil(_drawExtent.height / 16.0), 1);

    //draw the triangle

    VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(_drawImage.imageView, nullptr,
                                                                        VK_IMAGE_LAYOUT_GENERAL);
    VkRenderingAttachmentInfo depthAttachment = vkinit::depth_attachment_info(
        _depthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    VkRenderingInfo renderInfo = vkinit::rendering_info(_drawExtent, &colorAttachment, &depthAttachment);

    vkCmdBeginRendering(cmd, &renderInfo);
    auto start = std::chrono::system_clock::now();
    draw_geometry(cmd);
    auto end     = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    stats.mesh_draw_time = elapsed.count() / 1000.f;

    vkCmdEndRendering(cmd);
    //srand(time(nullptr));
    //point_cloud_renderer->getPointData() = makeRandomPointCloudSphere(10000, { 0, 0, 0 }, 100, true, rand());
    physic_world->getSystemAs<SpringMassSystem>("spring")->getRenderData(point_cloud_renderer->getPointData());
    auto sm_sys = physic_world->getSystemAs<SpringMassSystem>("spring");
    auto fluid  = physic_world->getSystemAs<FluidSystem>("fluid");

    //translate_points(point_cloud_renderer->getPointData(), { 0.1, 0, 0, 0 });
    point_cloud_renderer->updatePoints();

    m_spring_renderer->updateSprings(sm_sys->getParticleData(), sm_sys->getTopology_mut());

    point_cloud_renderer->draw(*get_current_frame().command_buffer_graphic, {sceneData.viewproj}, renderInfo);

    m_vector_render->updateFromGrid(fluid->grid(), /*stride*/{2, 2, 2}, /*minMag*/ 0.01f);

    PushVector pvc{};
    pvc.model     = glm::mat4(1.0f);
    pvc.baseScale = 0.1f;
    pvc.headRatio = 0.35f;
    pvc.halfWidth = 0.02f;
    pvc.magScale  = 0.8f;

    m_vector_render->draw(*get_current_frame().command_buffer_graphic,
                          {sceneData.viewproj, sceneData.view, sceneData.proj}, renderInfo, pvc);

    //m_grid_point_render->updateFromGridUniform(
    //    fluid->grid(),
    //    /*stride*/{2, 2, 2},
    //    /*mode  */ PointScalarMode::SpeedMagnitude,
    //    /*clampMin*/ 0.0f,
    //    /*clampMax*/ -1.0f  // 自动推导 vmax
    //);

    //m_grid_point_render->draw(*get_current_frame().command_buffer_graphic, { sceneData.viewproj, sceneData.view, sceneData.proj }, renderInfo, 0.5, 0.0, -1.0);

    m_spring_renderer->draw(*get_current_frame().command_buffer_graphic, {sceneData.viewproj}, renderInfo);

    RenderGraphContext ctx;
    ctx.vkCtx      = _context;
    ctx.frame_data = &get_current_frame();
    render_graph->execute(ctx);
}

void VulkanEngine::draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView)
{
    VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(targetImageView, nullptr,
                                                                        VK_IMAGE_LAYOUT_GENERAL);
    VkRenderingInfo renderInfo = vkinit::rendering_info(_windowExtent, &colorAttachment, nullptr);

    vkCmdBeginRendering(cmd, &renderInfo);

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

    vkCmdEndRendering(cmd);
}

void VulkanEngine::draw()
{
    //wait until the gpu has finished rendering the last frame. Timeout of 1 second
    VK_CHECK(vkWaitForFences(_context->getDevice(), 1, &get_current_frame()._renderFence, true, 1000000000));

    get_current_frame()._deletionQueue.flush();
    get_current_frame()._frameDescriptors.clear_pools(_context->getDevice());
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
    _drawExtent.height = std::min(_context->getSwapchain()->get_extent().height, _drawImage.imageExtent.height) * 1;
    _drawExtent.width = std::min(_context->getSwapchain()->get_extent().width, _drawImage.imageExtent.width) * 1;

    VK_CHECK(vkResetFences(_context->getDevice(), 1, &get_current_frame()._renderFence));

    //now that we are sure that the commands finished executing, we can safely reset the command buffer to begin recording again.
    //VK_CHECK(vkResetCommandBuffer(get_current_frame()._mainCommandBuffer, 0));
    get_current_frame().command_buffer_graphic->reset();

    //naming it cmd for shorter writing
    VkCommandBuffer cmd = get_current_frame()._mainCommandBuffer;
    cmd                 = get_current_frame().command_buffer_graphic->getHandle();

    //begin the command buffer recording. We will use this command buffer exactly once, so we want to let vulkan know that
    //VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    //VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    get_current_frame().command_buffer_graphic->begin();

    // transition our main draw image into general layout so we can update into it
    // we will overwrite it all so we dont care about what was the older layout
    //get_current_frame().command_buffer_graphic->transitionImage(_drawImage.image, vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);
    vkutil::transition_image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    vkutil::transition_image(cmd, _depthImage.image, VK_IMAGE_LAYOUT_UNDEFINED,
                             VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    draw_main(cmd);


    //transtion the draw image and the swapchain image into their correct transfer layouts
    vkutil::transition_image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    vkutil::transition_image(cmd, _context->getSwapchain()->get_images()[swapchainImageIndex],
                             VK_IMAGE_LAYOUT_UNDEFINED,
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VkExtent2D extent;
    extent.height = _windowExtent.height;
    extent.width  = _windowExtent.width;
    //extent.depth = 1;

    // execute a copy from the draw image into the swapchain
    vkutil::copy_image_to_image(cmd, _drawImage.image, _context->getSwapchain()->get_images()[swapchainImageIndex],
                                _drawExtent,
                                _context->getSwapchain()->get_extent());

    // set swapchain image layout to Attachment Optimal so we can draw it
    vkutil::transition_image(cmd, _context->getSwapchain()->get_images()[swapchainImageIndex],
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                             VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    //draw imgui into the swapchain image
    draw_imgui(cmd, _context->getSwapchain()->get_image_views()[swapchainImageIndex]);

    // set swapchain image layout to Present so we can draw it
    vkutil::transition_image(cmd, _context->getSwapchain()->get_images()[swapchainImageIndex],
                             VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                             VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    if (false) // 旧的手动结束并提交命令缓冲区的代码
    {
        //finalize the command buffer (we can no longer add commands, but it can now be executed)
        VK_CHECK(vkEndCommandBuffer(cmd));

        //prepare the submission to the queue. 
        //we want to wait on the _presentSemaphore, as that semaphore is signaled when the swapchain is ready
        //we will signal the _renderSemaphore, to signal that rendering has finished
        VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);

        VkSemaphoreSubmitInfo waitInfo = vkinit::semaphore_submit_info(
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
            get_current_frame()._swapchainSemaphore);
        VkSemaphoreSubmitInfo signalInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
                                                                         get_current_frame()._renderSemaphore);

        VkSubmitInfo2 submit = vkinit::submit_info(&cmdinfo, &signalInfo, &waitInfo);

        //submit command buffer to the queue and execute it.
        // _renderFence will now block until the graphic commands finish execution
        VK_CHECK(vkQueueSubmit2(_context->getGraphicsQueue(), 1, &submit, get_current_frame()._renderFence));
    }

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
    VkPresentInfoKHR     presentInfo = vkinit::present_info();
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

bool is_visible(const RenderObject& obj, const glm::mat4& viewproj)
{
    std::array<glm::vec3, 8> corners
    {
        glm::vec3{1, 1, 1},
        glm::vec3{1, 1, -1},
        glm::vec3{1, -1, 1},
        glm::vec3{1, -1, -1},
        glm::vec3{-1, 1, 1},
        glm::vec3{-1, 1, -1},
        glm::vec3{-1, -1, 1},
        glm::vec3{-1, -1, -1},
    };

    glm::mat4 matrix = viewproj * obj.transform;

    glm::vec3 min = {1.5, 1.5, 1.5};
    glm::vec3 max = {-1.5, -1.5, -1.5};

    for (int c = 0; c < 8; c++)
    {
        // project each corner into clip space
        glm::vec4 v = matrix * glm::vec4(obj.bounds.origin + (corners[c] * obj.bounds.extents), 1.f);

        // perspective correction
        v.x = v.x / v.w;
        v.y = v.y / v.w;
        v.z = v.z / v.w;

        min = glm::min(glm::vec3{v.x, v.y, v.z}, min);
        max = glm::max(glm::vec3{v.x, v.y, v.z}, max);
    }

    // check the clip space box is within the view
    if (min.z > 1.f || max.z < 0.f || min.x > 1.f || max.x < -1.f || min.y > 1.f || max.y < -1.f)
    {
        return false;
    }
    return true;
}

void VulkanEngine::draw_geometry(VkCommandBuffer cmd)
{
    std::vector<uint32_t> opaque_draws;
    opaque_draws.reserve(drawCommands.OpaqueSurfaces.size());

    for (int i = 0; i < drawCommands.OpaqueSurfaces.size(); i++)
    {
        if (is_visible(drawCommands.OpaqueSurfaces[i], sceneData.viewproj) || true)
        {
            opaque_draws.push_back(i);
        }
    }

    //allocate a new uniform buffer for the scene data
    AllocatedBuffer gpuSceneDataBuffer = create_buffer(sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                       VMA_MEMORY_USAGE_CPU_TO_GPU);

    //add it to the deletion queue of this frame so it gets deleted once its been used
    get_current_frame()._deletionQueue.push_function([gpuSceneDataBuffer, this]()
    {
        destroy_buffer(gpuSceneDataBuffer);
    });

    //update the buffer
    auto sceneUniformData = static_cast<GPUSceneData*>(gpuSceneDataBuffer.info.pMappedData);
    *sceneUniformData     = sceneData;

    VkDescriptorSetVariableDescriptorCountAllocateInfo allocArrayInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO, .pNext = nullptr
    };

    uint32_t descriptorCounts         = texCache.Cache.size();
    allocArrayInfo.pDescriptorCounts  = &descriptorCounts;
    allocArrayInfo.descriptorSetCount = 1;


    //create a descriptor set that binds that buffer and update it
    VkDescriptorSet globalDescriptor = get_current_frame()._frameDescriptors.allocate(
        _context->getDevice(), _gpuSceneDataDescriptorLayout, &allocArrayInfo);

    DescriptorWriter writer;
    writer.write_buffer(0, gpuSceneDataBuffer.buffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);


    if (texCache.Cache.size() > 0)
    {
        VkWriteDescriptorSet arraySet{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        arraySet.descriptorCount = texCache.Cache.size();
        arraySet.dstArrayElement = 0;
        arraySet.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        arraySet.dstBinding      = 1;
        arraySet.pImageInfo      = texCache.Cache.data();
        writer.writes.push_back(arraySet);
    }

    writer.update_set(_context->getDevice(), globalDescriptor);

    MaterialPipeline* lastPipeline    = nullptr;
    MaterialInstance* lastMaterial    = nullptr;
    VkBuffer          lastIndexBuffer = VK_NULL_HANDLE;

    auto draw = [&](const RenderObject& r)
    {
        if (r.material != lastMaterial)
        {
            lastMaterial = r.material;
            if (r.material->pipeline != lastPipeline)
            {
                lastPipeline = r.material->pipeline;
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.material->pipeline->pipeline);
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.material->pipeline->layout, 0, 1,
                                        &globalDescriptor, 0, nullptr);

                VkViewport viewport = {};
                viewport.width      = static_cast<float>(_drawExtent.width);
                // vulkan的屏幕坐标与dx、ogl的y轴相反，所以这里使用vulkan的特性，将高反向
                viewport.height = -static_cast<float>(_drawExtent.height);
                viewport.x      = 0;
                // 同时原点也需要修改，因为vulkan本身是倒置的，所以现在整个窗口需要向y方向平移
                viewport.y        = static_cast<float>(_drawExtent.height);
                viewport.minDepth = 0.f;
                viewport.maxDepth = 1.f;

                vkCmdSetViewport(cmd, 0, 1, &viewport);

                VkRect2D scissor      = {};
                scissor.offset.x      = 0;
                scissor.offset.y      = 0;
                scissor.extent.width  = _drawExtent.width;
                scissor.extent.height = _drawExtent.height;

                vkCmdSetScissor(cmd, 0, 1, &scissor);
            }

            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.material->pipeline->layout, 1, 1,
                                    &r.material->materialSet, 0, nullptr);
        }
        if (r.indexBuffer != lastIndexBuffer)
        {
            lastIndexBuffer = r.indexBuffer;
            vkCmdBindIndexBuffer(cmd, r.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        }
        // calculate final mesh matrix
        GPUDrawPushConstants push_constants;
        push_constants.worldMatrix  = r.transform;
        push_constants.vertexBuffer = r.vertexBufferAddress;

        vkCmdPushConstants(cmd, r.material->pipeline->layout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                           sizeof(GPUDrawPushConstants), &push_constants);

        stats.drawcall_count++;
        stats.triangle_count += r.indexCount / 3;
        vkCmdDrawIndexed(cmd, r.indexCount, 1, r.firstIndex, 0, 0);
    };

    stats.drawcall_count = 0;
    stats.triangle_count = 0;

    for (auto& r : opaque_draws)
    {
        draw(drawCommands.OpaqueSurfaces[r]);
    }

    for (auto& r : drawCommands.TransparentSurfaces)
    {
        draw(r);
    }

    // we delete the draw commands now that we processed them
    drawCommands.OpaqueSurfaces.clear();
    drawCommands.TransparentSurfaces.clear();
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

            mainCamera.processSDLEvent(_context->getWindow()->get_window(), e);
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
        static float time                                       = 0;
        backgroundEffects[currentBackgroundEffect].data.data1.x = mainCamera.position.x;
        backgroundEffects[currentBackgroundEffect].data.data1.y = time;
        time += 0.001f;
        if (ImGui::Begin("设置面板", nullptr, ImGuiWindowFlags_NoCollapse))
        {
            if (ImGui::CollapsingHeader("背景设置", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ComputeEffect& selected = backgroundEffects[currentBackgroundEffect];

                ImGui::Text("Selected effect: ", selected.name);

                ImGui::SliderInt("Effect Index", &currentBackgroundEffect, 0, backgroundEffects.size() - 1);

                ImGui::InputFloat4("data1", reinterpret_cast<float*>(&selected.data.data1));
                ImGui::InputFloat4("data2", reinterpret_cast<float*>(&selected.data.data2));
                ImGui::InputFloat4("data3", reinterpret_cast<float*>(&selected.data.data3));
                ImGui::InputFloat4("data4", reinterpret_cast<float*>(&selected.data.data4));
                ImGui::InputInt("view mode", reinterpret_cast<int*>(&mainCamera.view_mode));
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
            }
            mainCamera.renderUI();
            ImGui::End();
        }
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

    // camera projection
    glm::mat4 projection = glm::perspective(glm::radians(60.f),
                                            static_cast<float>(_windowExtent.width) / static_cast<float>(_windowExtent.
                                                height), 0.1f, 1000.0f);

    // invert the Y direction on projection matrix so that we are more similar
    // to opengl and gltf axis
    projection[1][1] *= -1;

    if (mainCamera.view_mode == VIEW_MODE::perspective)
    {
        sceneData.proj     = mainCamera.projection;
        sceneData.viewproj = mainCamera.projection * view;
    }
    else if (mainCamera.view_mode == VIEW_MODE::orthographic)
    {
        sceneData.proj     = mainCamera.ortho;
        sceneData.viewproj = mainCamera.ortho * view;
    }
    sceneData.view = view;


    loadedScenes["structure"]->draw(glm::mat4{1.f}, drawCommands);
}

AllocatedBuffer VulkanEngine::create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage)
{
    // allocate buffer
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType              = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.pNext              = nullptr;
    bufferInfo.size               = allocSize;

    bufferInfo.usage = usage;

    VmaAllocationCreateInfo vmaallocInfo = {};
    vmaallocInfo.usage                   = memoryUsage;
    vmaallocInfo.flags                   = VMA_ALLOCATION_CREATE_MAPPED_BIT;
    AllocatedBuffer newBuffer;

    // allocate the buffer
    VK_CHECK(vmaCreateBuffer(_context->getVmaAllocator(), &bufferInfo, &vmaallocInfo, &newBuffer.buffer,
                             &newBuffer.allocation,
                             &newBuffer.info));

    return newBuffer;
}

AllocatedImage VulkanEngine::create_image(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped)
{
    AllocatedImage newImage;
    newImage.imageFormat = format;
    newImage.imageExtent = size;

    VkImageCreateInfo img_info = vkinit::image_create_info(format, usage, size);
    if (mipmapped)
    {
        img_info.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(size.width, size.height)))) + 1;
    }

    // always allocate images on dedicated GPU memory
    VmaAllocationCreateInfo allocinfo = {};
    allocinfo.usage                   = VMA_MEMORY_USAGE_GPU_ONLY;
    allocinfo.requiredFlags           = static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // allocate and create the image
    VK_CHECK(vmaCreateImage(_context->getVmaAllocator(), &img_info, &allocinfo, &newImage.image, &newImage.allocation,
                            nullptr));

    // if the format is a depth format, we will need to have it use the correct
    // aspect flag
    VkImageAspectFlags aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;
    if (format == VK_FORMAT_D32_SFLOAT)
    {
        aspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT;
    }

    // build a image-view for the image
    VkImageViewCreateInfo view_info       = vkinit::imageview_create_info(format, newImage.image, aspectFlag);
    view_info.subresourceRange.levelCount = img_info.mipLevels;

    VK_CHECK(vkCreateImageView(_context->getDevice(), &view_info, nullptr, &newImage.imageView));

    return newImage;
}

AllocatedImage VulkanEngine::create_image(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage,
                                          bool  mipmapped)
{
    size_t          data_size    = size.depth * size.width * size.height * 4;
    AllocatedBuffer uploadbuffer = create_buffer(data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                 VMA_MEMORY_USAGE_CPU_TO_GPU);

    memcpy(uploadbuffer.info.pMappedData, data, data_size);

    AllocatedImage new_image = create_image(size, format,
                                            usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                                            mipmapped);


    immediate_submit([&](VkCommandBuffer cmd)
    {
        vkutil::transition_image(cmd, new_image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        VkBufferImageCopy copyRegion = {};
        copyRegion.bufferOffset      = 0;
        copyRegion.bufferRowLength   = 0;
        copyRegion.bufferImageHeight = 0;

        copyRegion.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.mipLevel       = 0;
        copyRegion.imageSubresource.baseArrayLayer = 0;
        copyRegion.imageSubresource.layerCount     = 1;
        copyRegion.imageExtent                     = size;

        // copy the buffer into the image
        vkCmdCopyBufferToImage(cmd, uploadbuffer.buffer, new_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                               &copyRegion);

        if (mipmapped)
        {
            vkutil::generate_mipmaps(cmd, new_image.image,
                                     VkExtent2D{new_image.imageExtent.width, new_image.imageExtent.height});
        }
        else
        {
            vkutil::transition_image(cmd, new_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
    });
    destroy_buffer(uploadbuffer);
    return new_image;
}

GPUMeshBuffers VulkanEngine::uploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices)
{
    const size_t vertexBufferSize = vertices.size() * sizeof(Vertex);
    const size_t indexBufferSize  = indices.size() * sizeof(uint32_t);

    GPUMeshBuffers newSurface;

    newSurface.vertexBuffer = create_buffer(vertexBufferSize,
                                            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                            VMA_MEMORY_USAGE_GPU_ONLY);

    VkBufferDeviceAddressInfo deviceAdressInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = newSurface.vertexBuffer.buffer
    };
    newSurface.vertexBufferAddress = vkGetBufferDeviceAddress(_context->getDevice(), &deviceAdressInfo);

    newSurface.indexBuffer = create_buffer(indexBufferSize,
                                           VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                           VMA_MEMORY_USAGE_GPU_ONLY);

    AllocatedBuffer staging = create_buffer(vertexBufferSize + indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                            VMA_MEMORY_USAGE_CPU_ONLY);

    void* data = staging.info.pMappedData;

    // copy vertex buffer
    memcpy(data, vertices.data(), vertexBufferSize);
    // copy index buffer
    memcpy(static_cast<char*>(data) + vertexBufferSize, indices.data(), indexBufferSize);

    immediate_submit([&](VkCommandBuffer cmd)
    {
        VkBufferCopy vertexCopy{0};
        vertexCopy.dstOffset = 0;
        vertexCopy.srcOffset = 0;
        vertexCopy.size      = vertexBufferSize;

        vkCmdCopyBuffer(cmd, staging.buffer, newSurface.vertexBuffer.buffer, 1, &vertexCopy);

        VkBufferCopy indexCopy{0};
        indexCopy.dstOffset = 0;
        indexCopy.srcOffset = vertexBufferSize;
        indexCopy.size      = indexBufferSize;

        vkCmdCopyBuffer(cmd, staging.buffer, newSurface.indexBuffer.buffer, 1, &indexCopy);
    });

    destroy_buffer(staging);

    return newSurface;
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

void VulkanEngine::immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function)
{
    VK_CHECK(vkResetFences(_context->getDevice(), 1, &_immFence));
    VK_CHECK(vkResetCommandBuffer(_immCommandBuffer, 0));

    VkCommandBuffer cmd = _immCommandBuffer;
    // begin the command buffer recording. We will use this command buffer exactly
    // once, so we want to let vulkan know that
    VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(
        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    function(cmd);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);
    VkSubmitInfo2             submit  = vkinit::submit_info(&cmdinfo, nullptr, nullptr);

    // submit command buffer to the queue and execute it.
    //  _renderFence will now block until the graphic commands finish execution
    VK_CHECK(vkQueueSubmit2(_context->getGraphicsQueue(), 1, &submit, _immFence));

    VK_CHECK(vkWaitForFences(_context->getDevice(), 1, &_immFence, true, 9999999999));
}

void VulkanEngine::destroy_image(const AllocatedImage& img)
{
    vkDestroyImageView(_context->getDevice(), img.imageView, nullptr);
    vmaDestroyImage(_context->getVmaAllocator(), img.image, img.allocation);
}

void VulkanEngine::destroy_buffer(const AllocatedBuffer& buffer)
{
    vmaDestroyBuffer(_context->getVmaAllocator(), buffer.buffer, buffer.allocation);
}

void VulkanEngine::init_vulkan()
{
    _context = new vkcore::VulkanContext(_windowExtent.width, _windowExtent.height);
}

void VulkanEngine::init_swapchain()
{
    //create_swapchain(_windowExtent.width, _windowExtent.height);

    //depth image size will match the window
    VkExtent3D drawImageExtent = {
        _windowExtent.width,
        _windowExtent.height,
        1
    };

    //hardcoding the draw format to 32 bit float
    _drawImage.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    _drawImage.imageExtent = drawImageExtent;

    VkImageUsageFlags drawImageUsages{};
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    VkImageCreateInfo rimg_info = vkinit::image_create_info(_drawImage.imageFormat, drawImageUsages, drawImageExtent);

    //for the draw image, we want to allocate it from gpu local memory
    VmaAllocationCreateInfo rimg_allocinfo = {};
    rimg_allocinfo.usage                   = VMA_MEMORY_USAGE_GPU_ONLY;
    rimg_allocinfo.requiredFlags           = static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    //allocate and create the image
    vmaCreateImage(_context->getVmaAllocator(), &rimg_info, &rimg_allocinfo, &_drawImage.image, &_drawImage.allocation,
                   nullptr);

    //build a image-view for the draw image to use for rendering
    VkImageViewCreateInfo rview_info = vkinit::imageview_create_info(_drawImage.imageFormat, _drawImage.image,
                                                                     VK_IMAGE_ASPECT_COLOR_BIT);

    VK_CHECK(vkCreateImageView(_context->getDevice(), &rview_info, nullptr, &_drawImage.imageView));

    //create a depth image too
    //hardcoding the draw format to 32 bit float
    _depthImage.imageFormat = VK_FORMAT_D32_SFLOAT;
    _depthImage.imageExtent = drawImageExtent;
    VkImageUsageFlags depthImageUsages{};
    depthImageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    VkImageCreateInfo dimg_info = vkinit::image_create_info(_depthImage.imageFormat, depthImageUsages, drawImageExtent);

    //allocate and create the image
    vmaCreateImage(_context->getVmaAllocator(), &dimg_info, &rimg_allocinfo, &_depthImage.image,
                   &_depthImage.allocation, nullptr);

    //build a image-view for the draw image to use for rendering
    VkImageViewCreateInfo dview_info = vkinit::imageview_create_info(_depthImage.imageFormat, _depthImage.image,
                                                                     VK_IMAGE_ASPECT_DEPTH_BIT);

    VK_CHECK(vkCreateImageView(_context->getDevice(), &dview_info, nullptr, &_depthImage.imageView));


    //add to deletion queues
    _mainDeletionQueue.push_function([this]()
    {
        _context->getDevice().destroyImageView(_drawImage.imageView);
        //vkDestroyImageView(_context->getDevice(), _drawImage.imageView, nullptr);
        vmaDestroyImage(_context->getVmaAllocator(), _drawImage.image, _drawImage.allocation);

        _context->getDevice().destroyImageView(_depthImage.imageView);
        //vkDestroyImageView(_context->getDevice(), _depthImage.imageView, nullptr);
        vmaDestroyImage(_context->getVmaAllocator(), _depthImage.image, _depthImage.allocation);
    });
}

void VulkanEngine::resize_swapchain()
{
    vkDeviceWaitIdle(_context->getDevice());
    _context->resizeSwapchainAuto();
    _windowExtent.height = _context->getSwapchain()->get_extent().height;
    _windowExtent.width  = _context->getSwapchain()->get_extent().width;
    resize_requested     = false;
}

void VulkanEngine::init_commands()
{
    // create a command pool for commands submitted to the graphics queue.
    // we also want the pool to allow for resetting of individual command buffers
    VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(
        _context->getGraphicsQueueIndex(), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    for (int i = 0; i < FRAME_OVERLAP; i++)
    {
        VK_CHECK(vkCreateCommandPool(_context->getDevice(), &commandPoolInfo, nullptr, &_frames[i]._commandPool));

        _frames[i]._command_pool_graphic  = new vkcore::CommandPool(_context, _context->getGraphicsQueueIndex());
        _frames[i]._command_pool_transfer = new vkcore::CommandPool(_context, _context->getTransferQueueIndex());

        //_frames[i].command_buffer_graphic = new vkcore::CommandBuffer(_context, _frames[i]._command_pool_graphic);
        _frames[i].command_buffer_graphic  = new vkcore::CommandBuffer(_context, _frames[i]._command_pool_graphic);
        std::string name = "frame graphic command ";
        name.append(std::to_string(i));
        _frames[i].command_buffer_graphic->setDebugName(name);
        _frames[i].command_buffer_transfer = new vkcore::CommandBuffer(_context, _frames[i]._command_pool_transfer);

        // allocate the default command buffer that we will use for rendering
        VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_frames[i]._commandPool, 1);

        VK_CHECK(vkAllocateCommandBuffers(_context->getDevice(), &cmdAllocInfo, &_frames[i]._mainCommandBuffer));

        _mainDeletionQueue.push_function([=]()
        {
            vkDestroyCommandPool(_context->getDevice(), _frames[i]._commandPool, nullptr);
        });
    }

    VK_CHECK(vkCreateCommandPool(_context->getDevice(), &commandPoolInfo, nullptr, &_immCommandPool));

    // allocate the default command buffer that we will use for rendering
    VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_immCommandPool, 1);

    VK_CHECK(vkAllocateCommandBuffers(_context->getDevice(), &cmdAllocInfo, &_immCommandBuffer));

    _mainDeletionQueue.push_function(
        [this]() { vkDestroyCommandPool(_context->getDevice(), _immCommandPool, nullptr); });
}

void VulkanEngine::init_sync_structures()
{
    // create syncronization structures
    // one fence to control when the gpu has finished rendering the frame,
    // and 2 semaphores to syncronize rendering with swapchain
    // we want the fence to start signalled so we can wait on it on the first
    // frame
    VkFenceCreateInfo fenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
    VK_CHECK(vkCreateFence(_context->getDevice(), &fenceCreateInfo, nullptr, &_immFence));

    _mainDeletionQueue.push_function([this]() { _context->getDevice().destroyFence(_immFence); });

    for (int i = 0; i < FRAME_OVERLAP; i++)
    {
        VK_CHECK(vkCreateFence(_context->getDevice(), &fenceCreateInfo, nullptr, &_frames[i]._renderFence));

        VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();

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
    fs::path    current_dir    = fs::current_path(); // 当前目录
    fs::path    file_json_path = absolute(current_dir / "../../assets/config/file.json"); // 矫正分隔符
    fmt::print(fg(fmt::color::bisque), "file config path: {}\n", file_json_path.string());
    if (exists(file_json_path))
    {
        std::ifstream  file(file_json_path);
        nlohmann::json j;
        file >> j;
        structurePath = vkutil::get_model_path(j, j["load_file"]["name"]).string();
    }
    fmt::print(fg(fmt::color::bisque), "model file path: {}\n", structurePath);
    auto structureFile = loadGltf(this, structurePath);

    auto root = ImporterRegistry::instance().import(structurePath);

    AssetDB::instance().open();
    for (const auto& meta : root.metas)
    {
        AssetDB::instance().upsert(meta);
    }
    // 1 假设导入阶段已写入 metas & raw；此处只加载
    ResourceCache  cache;
    ResourceLoader loader("cache/raw", AssetDB::instance(), cache);
    //auto           result = loader.load<MeshData>(root.metas[0].uuid);
    //auto           result1 = loader.load<MeshData>(root.metas[0].uuid);

    m_scene_system = std::make_shared<SceneSystem>(std::make_unique<SceneBuilder>(), std::make_unique<SceneResourceBinder>());
    m_scene_system->buildSceneFromImport("scene", root);
    m_scene_system->preloadResources(loader, cache);

    hierarchy_panel.setRoots(m_scene_system->currentScene()->getRoot().get());
    assert(structureFile.has_value());

    loadedScenes["structure"] = *structureFile;
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
    //io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // 开启docking

    ImFontConfig font_cfg;
    font_cfg.FontBuilderFlags = ImGuiFreeTypeBuilderFlags_ForceAutoHint;  // 额外的 FreeType 设置
    font_cfg.FontBuilderFlags = ImGuiFreeTypeBuilderFlags_ForceAutoHint | ImGuiFreeTypeBuilderFlags_Monochrome;
    namespace fs = std::filesystem;
    fs::path current_dir = fs::current_path(); // 当前目录
    fs::path target_file = absolute(current_dir / "../../assets/font/SourceHanSansCN-Regular.otf"); // 矫正分隔符
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

void VulkanEngine::init_pipelines()
{
    // COMPUTE PIPELINES
    init_background_pipelines();

    metalRoughMaterial.build_pipelines(this);

    _mainDeletionQueue.push_function([&]()
    {
        metalRoughMaterial.clear_resources(_context->getDevice());
    });
}

void VulkanEngine::init_descriptors()
{
    // 创建descriptor pool
    std::vector<DescriptorAllocator::PoolSizeRatio> sizes = {
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3},
    };

    globalDescriptorAllocator.init_pool(_context->getDevice(), 10, sizes);
    _mainDeletionQueue.push_function(
        [&]() { vkDestroyDescriptorPool(_context->getDevice(), globalDescriptorAllocator.pool, nullptr); });

    // 创建各类descriptor layout
    { // 创建绘制窗口的layout
        DescriptorLayoutBuilder builder;
        builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        _drawImageDescriptorLayout = builder.build(_context->getDevice(), VK_SHADER_STAGE_COMPUTE_BIT);
    }

    { // 创建
        DescriptorLayoutBuilder builder;
        builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        builder.add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);


        VkDescriptorSetLayoutBindingFlagsCreateInfo bindFlags = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO, .pNext = nullptr
        };

        std::array<VkDescriptorBindingFlags, 2> flagArray{
            0, VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
        };

        builder.bindings[1].descriptorCount = 4048;

        bindFlags.bindingCount  = 2;
        bindFlags.pBindingFlags = flagArray.data();

        _gpuSceneDataDescriptorLayout = builder.build(
            _context->getDevice(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, &bindFlags);
    }

    _mainDeletionQueue.push_function([&]()
    {
        vkDestroyDescriptorSetLayout(_context->getDevice(), _drawImageDescriptorLayout, nullptr);
        vkDestroyDescriptorSetLayout(_context->getDevice(), _gpuSceneDataDescriptorLayout, nullptr);
    });

    _drawImageDescriptors = globalDescriptorAllocator.allocate(_context->getDevice(), _drawImageDescriptorLayout);
    {
        DescriptorWriter writer;
        writer.write_image(0, _drawImage.imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL,
                           VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        writer.update_set(_context->getDevice(), _drawImageDescriptors);
    }
    for (int i = 0; i < FRAME_OVERLAP; i++)
    {
        // create a descriptor pool
        std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> frame_sizes = {
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4},
        };

        _frames[i]._frameDescriptors = DescriptorAllocatorGrowable{};
        _frames[i]._frameDescriptors.init(_context->getDevice(), 1000, frame_sizes);

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
        _mainDeletionQueue.push_function([&, i]()
        {
            _frames[i]._frameDescriptors.destroy_pools(_context->getDevice());
        });
    }
}

void GLTFMetallic_Roughness::build_pipelines(VulkanEngine* engine)
{
    auto compiler = vkutil::ShaderCompiler::getInstance();
    vkutil::ShaderCompiler::initGlslang();
    std::string code1;
    std::string code2;
    vkutil::readShaderFile("C:/code/code_file/Decker/assets/shaders/mesh_pbr.frag", code1);
    vkutil::readShaderFile("C:/code/code_file/Decker/assets/shaders/mesh.vert", code2);
    std::vector<uint32_t> spv_code1;
    std::vector<uint32_t> spv_code2;
    compiler.compileGLSLtoSPV(code1, spv_code1, EShLangFragment, false);
    compiler.compileGLSLtoSPV(code2, spv_code2, EShLangVertex, false);
    vkutil::ShaderCompiler::finalizeGlslang();


    print(fg(fmt::color::alice_blue), "-----------------------temp use shader-----------------------\n");
    //layout code
    VkShaderModule meshFragShader;
    //if (!vkutil::load_shader_module(spv_code1, engine->_context->getDevice(), &meshFragShader))
    VkShaderModule gradientShader;
    namespace fs = std::filesystem;
    fs::path current_dir = fs::current_path(); // 当前目录
    fs::path target_file = absolute(current_dir / "../../assets/shaders/spv/mesh_pbr.frag.spv"); // 矫正分隔符
    if (!vkutil::load_shader_module(target_file.string(),
                                    engine->_context->getDevice(),
                                    &meshFragShader))
    {
        print(fg(fmt::color::red), "Error when building the fragment shader \n");
    }

    VkShaderModule meshVertexShader;
    //if (!vkutil::load_shader_module(spv_code1, engine->_context->getDevice(), &meshVertexShader))
    target_file = absolute(current_dir / "../../assets/shaders/spv/mesh.vert.spv"); // 矫正分隔符
    if (!vkutil::load_shader_module(target_file.string(),
                                    engine->_context->getDevice(),
                                    &meshVertexShader))
    {
        print(fg(fmt::color::red), "Error when building the vertex shader \n");
    }

    VkPushConstantRange matrixRange{};
    matrixRange.offset     = 0;
    matrixRange.size       = sizeof(GPUDrawPushConstants);
    matrixRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    DescriptorLayoutBuilder layoutBuilder;
    layoutBuilder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

    materialLayout = layoutBuilder.build(engine->_context->getDevice(),
                                         VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

    VkDescriptorSetLayout layouts[] =
    {
        engine->_gpuSceneDataDescriptorLayout,
        materialLayout
    };

    VkPipelineLayoutCreateInfo mesh_layout_info = vkinit::pipeline_layout_create_info();
    mesh_layout_info.setLayoutCount             = 2;
    mesh_layout_info.pSetLayouts                = layouts;
    mesh_layout_info.pPushConstantRanges        = &matrixRange;
    mesh_layout_info.pushConstantRangeCount     = 1;

    VkPipelineLayout newLayout;
    VK_CHECK(vkCreatePipelineLayout(engine->_context->getDevice(), &mesh_layout_info, nullptr, &newLayout));
    DebugUtils::getInstance().setDebugName(engine->_context->getDevice(), VK_OBJECT_TYPE_PIPELINE_LAYOUT,
                                           reinterpret_cast<uint64_t>(newLayout), "mesh layout");

    opaquePipeline.layout      = newLayout;
    transparentPipeline.layout = newLayout;

    // build the stage-create-info for both vertex and fragment stages. This lets
    // the pipeline know the shader modules per stage
    PipelineBuilder pipelineBuilder;

    pipelineBuilder.set_shaders(meshVertexShader, meshFragShader);

    pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

    pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);

    pipelineBuilder.set_cull_mode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);

    pipelineBuilder.set_multisampling_none();

    pipelineBuilder.disable_blending();

    pipelineBuilder.enable_depthtest(true, VK_COMPARE_OP_LESS_OR_EQUAL);

    //render format
    pipelineBuilder.set_color_attachment_format(engine->_drawImage.imageFormat);
    pipelineBuilder.set_depth_format(engine->_depthImage.imageFormat);

    // use the triangle layout we created
    pipelineBuilder._pipelineLayout = newLayout;

    // finally build the pipeline
    opaquePipeline.pipeline = pipelineBuilder.build_pipeline(engine->_context->getDevice());
    DebugUtils::getInstance().setDebugName(engine->_context->getDevice(), VK_OBJECT_TYPE_PIPELINE,
                                           reinterpret_cast<uint64_t>(opaquePipeline.pipeline), "opaque pipeline");

    // create the transparent variant
    pipelineBuilder.enable_blending_additive();

    pipelineBuilder.enable_depthtest(false, VK_COMPARE_OP_GREATER_OR_EQUAL);

    transparentPipeline.pipeline = pipelineBuilder.build_pipeline(engine->_context->getDevice());
    DebugUtils::getInstance().setDebugName(engine->_context->getDevice(), VK_OBJECT_TYPE_PIPELINE,
                                           reinterpret_cast<uint64_t>(transparentPipeline.pipeline),
                                           "transparent pipeline");

    vkDestroyShaderModule(engine->_context->getDevice(), meshFragShader, nullptr);
    vkDestroyShaderModule(engine->_context->getDevice(), meshVertexShader, nullptr);
}

void GLTFMetallic_Roughness::clear_resources(VkDevice device)
{
    vkDestroyPipeline(device, opaquePipeline.pipeline, nullptr);
    vkDestroyPipeline(device, transparentPipeline.pipeline, nullptr);
    vkDestroyPipelineLayout(device, transparentPipeline.layout, nullptr);
    vkDestroyDescriptorSetLayout(device, materialLayout, nullptr);
}

MaterialInstance GLTFMetallic_Roughness::write_material(VkDevice                     device, MaterialPass pass,
                                                        const MaterialResources&     resources,
                                                        DescriptorAllocatorGrowable& descriptorAllocator)
{
    MaterialInstance matData;
    matData.passType = pass;
    if (pass == MaterialPass::Transparent)
    {
        matData.pipeline = &transparentPipeline;
    }
    else
    {
        matData.pipeline = &opaquePipeline;
    }

    matData.materialSet = descriptorAllocator.allocate(device, materialLayout);


    writer.clear();
    writer.write_buffer(0, resources.dataBuffer, sizeof(MaterialConstants), resources.dataBufferOffset,
                        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

    writer.update_set(device, matData.materialSet);

    return matData;
}

TextureID TextureCache::AddTexture(const VkImageView& image, VkSampler sampler)
{
    for (unsigned int i = 0; i < Cache.size(); i++)
    {
        if (Cache[i].imageView == image && Cache[i].sampler == sampler)
        {
            //found, return it
            return TextureID{i};
        }
    }

    uint32_t idx = Cache.size();

    Cache.push_back(VkDescriptorImageInfo{
        .sampler = sampler, .imageView = image, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    });

    return TextureID{idx};
}
}
