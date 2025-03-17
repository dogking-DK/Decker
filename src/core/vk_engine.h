#pragma once

#include <vk_types.h>

#include <deque>
#include <functional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include <camera.h>
#include <vk_descriptors.h>
#include <vk_loader.h>
#include <vk_pipelines.h>

#include "Scene/Node.h"
#include "Vulkan/Context.h"

namespace fastgltf
{
    struct Mesh;
}

namespace dk {
struct DeletionQueue
{
    std::deque<std::function<void()>> deletors;

    void push_function(std::function<void()>&& function)
    {
        deletors.push_back(function);
    }

    void flush()
    {
        // reverse iterate the deletion queue to execute all the functions
        for (auto it = deletors.rbegin(); it != deletors.rend(); ++it)
        {
            (*it)(); // call functors
        }

        deletors.clear();
    }
};

struct ComputePushConstants
{
    glm::vec4 data1;
    glm::vec4 data2;
    glm::vec4 data3;
    glm::vec4 data4;
};

struct ComputeEffect
{
    const char* name;

    VkPipeline pipeline;
    VkPipelineLayout layout;

    ComputePushConstants data;
};



struct FrameData
{
    VkSemaphore _swapchainSemaphore, _renderSemaphore;
    VkFence _renderFence;

    DescriptorAllocatorGrowable _frameDescriptors;
    DeletionQueue _deletionQueue;

    VkCommandPool _commandPool;
    VkCommandBuffer _mainCommandBuffer;
};

constexpr unsigned int FRAME_OVERLAP = 2;



struct EngineStats
{
    float frametime;
    int triangle_count;
    int drawcall_count;
    float mesh_draw_time;
};

struct GLTFMetallic_Roughness
{
    MaterialPipeline opaquePipeline;
    MaterialPipeline transparentPipeline;

    VkDescriptorSetLayout materialLayout;

    struct MaterialConstants
    {
        glm::vec4 colorFactors;
        glm::vec4 metal_rough_factors;
        // padding, we need it anyway for uniform buffers
        uint32_t colorTexID;
        uint32_t metalRoughTexID;
        uint32_t normalTexID;
        uint32_t pad1;
        glm::vec4 extra[13];
    };

    struct MaterialResources
    {
        AllocatedImage colorImage;
        VkSampler colorSampler;
        AllocatedImage metalRoughImage;
        VkSampler metalRoughSampler;
        AllocatedImage normal_image;
        VkSampler normal_sampler;
        VkBuffer dataBuffer;
        uint32_t dataBufferOffset;
    };

    DescriptorWriter writer;

    void build_pipelines(VulkanEngine* engine);
    void clear_resources(VkDevice device);

    MaterialInstance write_material(VkDevice device, MaterialPass pass, const MaterialResources& resources,
        DescriptorAllocatorGrowable& descriptorAllocator);
};

struct TextureID
{
    uint32_t Index;
};

struct TextureCache
{
    std::vector<VkDescriptorImageInfo> Cache;
    std::unordered_map<std::string, TextureID> NameMap;
    TextureID AddTexture(const VkImageView& image, VkSampler sampler);
};

class VulkanEngine
{
public:
    bool _isInitialized{ false };
    int _frameNumber{ 0 };

    vkcore::VulkanContext* _context;

    VkExtent2D _windowExtent{ 1920, 1080 };

    AllocatedBuffer _defaultGLTFMaterialData;

    FrameData _frames[FRAME_OVERLAP];

    VkExtent2D _drawExtent;
    VkDescriptorPool _descriptorPool;

    DescriptorAllocator globalDescriptorAllocator;

    VkPipeline _gradientPipeline;
    VkPipelineLayout _gradientPipelineLayout;



    VkDescriptorSet _drawImageDescriptors;
    VkDescriptorSetLayout _drawImageDescriptorLayout;

    DeletionQueue _mainDeletionQueue;

    VkDescriptorSetLayout _gpuSceneDataDescriptorLayout;

    GLTFMetallic_Roughness metalRoughMaterial;

    // draw resources
    AllocatedImage _drawImage;
    AllocatedImage _depthImage;

    // immediate submit structures
    VkFence _immFence;
    VkCommandBuffer _immCommandBuffer;
    VkCommandPool _immCommandPool;

    AllocatedImage _whiteImage;
    AllocatedImage _blackImage;
    AllocatedImage _greyImage;
    AllocatedImage _errorCheckerboardImage;

    VkSampler _defaultSamplerLinear;
    VkSampler _defaultSamplerNearest;

    TextureCache texCache;

    GPUMeshBuffers rectangle;
    DrawContext drawCommands;

    GPUSceneData sceneData;

    Camera mainCamera;

    EngineStats stats;

    std::vector<ComputeEffect> backgroundEffects;
    int currentBackgroundEffect{ 0 };

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

    void render_nodes();

    void draw_geometry(VkCommandBuffer cmd);

    // run main loop
    void run();

    void update_scene();

    // upload a mesh into a pair of gpu buffers. If descriptor allocator is not
    // null, it will also create a descriptor that points to the vertex buffer
    GPUMeshBuffers uploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices);

    FrameData& get_current_frame();
    FrameData& get_last_frame();

    AllocatedBuffer create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);

    AllocatedImage create_image(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
    AllocatedImage create_image(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage,
        bool mipmapped = false);

    void immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function);

    std::unordered_map<std::string, std::shared_ptr<LoadedGLTF>> loadedScenes;

    void destroy_image(const AllocatedImage& img);
    void destroy_buffer(const AllocatedBuffer& buffer);

    bool resize_requested{ false };
    bool freeze_rendering{ false };

private:
    void init_vulkan();

    void init_swapchain();


    void resize_swapchain();

    void init_commands();

    void init_pipelines();
    void init_background_pipelines();

    void init_descriptors();

    void init_sync_structures();

    void init_renderables();

    void init_imgui();

    void init_default_data();
};

}
