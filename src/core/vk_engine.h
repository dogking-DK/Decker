#pragma once

#include <vk_types.h>

#include <deque>
#include <functional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>

#include <camera.h>

#include "Scene/Node.h"
#include "Vulkan/Context.h"
#include "HierarchyPanel.h"
#include "input/InputBackend.h"
#include "input/InputContext.h"
#include "input/InputRouter.h"
#include "input/InputState.h"
#include "ui/gizmo/GizmoDragState.h"
#include "ui/gizmo/GizmoManager.h"
#include "ui/tools/ToolManager.h"
#include "CameraInputController.h"
//#include "render/PointCloudRender.h"
#include "World.h"
//#include "render/SpringRender.h"
#include "vk_base.h"
#include "render graph/Resource.h"
#include "Vulkan/CommandPool.h"
#include "Vulkan/CommandBuffer.h"
#include "resource/cpu/ResourceCache.h"

namespace dk {
class SceneSystem;
}

namespace dk {
class Scene;
}

namespace dk {
class ResourceLoader;
}

namespace dk::render {
class RenderSystem;
}

namespace dk {
class GaussianBlurPass;
class DistortionPass;
}

namespace dk::vkcore {
class GrowableDescriptorAllocator;
}
namespace fastgltf
{
    struct Mesh;
}

namespace dk {
struct ImageDesc;
class FrameGraphImage;
class SpringRenderer;
class PointCloudRenderer;   // ← 前置声明（不需要包含头）
class BlitPass;
class MacGridPointRenderer;
class MacGridVectorRenderer;
class RenderGraph;

struct EngineStats
{
    float frametime;
    int triangle_count;
    int drawcall_count;
    float mesh_draw_time;
};

class VulkanEngine
{
public:
    VulkanEngine();
    ~VulkanEngine();
    bool _isInitialized{ false };
    int _frameNumber{ 0 };

    vkcore::VulkanContext* _context;

    VkExtent2D _windowExtent{ 1280, 720 };

    std::array<FrameData, FRAME_OVERLAP> _frames;

    DeletionQueue _mainDeletionQueue;

    vk::Format _color_format{vk::Format::eR16G16B16A16Sfloat};
    vk::Format _depth_format{vk::Format::eD32Sfloat};

    std::shared_ptr<RGResource<ImageDesc, FrameGraphImage>> color_image;

    // immediate submit structures
    VkFence _immFence;
    VkCommandBuffer _immCommandBuffer;
    VkCommandPool _immCommandPool;

    VkSampler _defaultSamplerLinear;
    VkSampler _defaultSamplerNearest;

    Camera mainCamera;
    input::InputState _input_state;
    std::unique_ptr<input::InputBackend> _input_backend;
    std::unique_ptr<input::InputRouter>  _input_router;
    input::InputContext                  _input_context;
    std::unique_ptr<CameraInputController> _camera_input_controller;
    std::unique_ptr<ui::ToolManager>       _tool_manager;
    std::unique_ptr<ui::GizmoManager>      _gizmo_manager;
    ui::GizmoDragState                    _gizmo_drag_state;

    EngineStats stats;
    bool        _show_aabb_bounds{false};
    bool        _translate_gizmo_enabled{true};

    HierarchyPanel hierarchy_panel;

    std::shared_ptr<PointCloudRenderer> point_cloud_renderer;
    std::shared_ptr<SpringRenderer> m_spring_renderer;
    std::shared_ptr<MacGridVectorRenderer> m_vector_render;
    std::shared_ptr<MacGridPointRenderer> m_grid_point_render;
    std::shared_ptr<RenderGraph> render_graph;
    std::shared_ptr<BlitPass> m_blit_pass;
    std::shared_ptr<GaussianBlurPass> m_gaussian_blur_pass;
    std::shared_ptr<DistortionPass> m_distortion_pass;
    std::shared_ptr<SceneSystem> m_scene_system;

    std::unique_ptr<World> physic_world;

    std::unique_ptr<render::RenderSystem> _render_system;
    std::unique_ptr<ResourceLoader>       _cpu_loader;
    ResourceCache                         _cpu_cache;
    std::unique_ptr<vkcore::CommandPool>  _upload_pool;
    vkcore::UploadContext                 _upload_ctx;

    // singleton style getter.multiple engines is not supported
    static VulkanEngine& Get();

    // initializes everything in the engine
    void init();

    // shuts down the engine
    void cleanup();

    // draw loop
    void draw();
    void draw_main(VkCommandBuffer cmd);
    void draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView);

    // run main loop
    void run();

    void update_scene();

    FrameData& get_current_frame();
    FrameData& get_frame(const int id);
    FrameData& get_last_frame();

    bool resize_requested{ false };
    bool freeze_rendering{ false };

private:
    void init_vulkan();

    void init_swapchain();

    void resize_swapchain();

    void init_commands();

    void init_descriptors();

    void init_sync_structures();

    void init_renderables();

    void init_imgui();

    void init_default_data();

    void test_render_graph();

};

}
