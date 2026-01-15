#include "Context.h"

#include <VkBootstrap.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include "vk_debug_util.h"

namespace dk::vkcore {
VulkanContext::VulkanContext(const uint32_t width, const uint32_t height)
{
    initVulkan(width, height);
}

void VulkanContext::initVulkan(const uint32_t width, const uint32_t height)
{
    _window = new core::SDLWindow({.title{"Vulkan"}, .extent{width, height}});
    initInstance();
    vk::SurfaceKHR surface = _window->create_surface(_instance);

    initDevice(surface);

    initVkhppDispatchers(_instance, _device);

    _swapchain = new Swapchain(*this, surface, vk::PresentModeKHR::eMailbox);

    initVma();
}

void VulkanContext::initVma()
{
    // initialize the memory allocator
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice         = _physical_device;
    allocatorInfo.device                 = _device;
    allocatorInfo.instance               = _instance;
    allocatorInfo.flags                  = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    vmaCreateAllocator(&allocatorInfo, &_allocator);
}

void VulkanContext::initVkhppDispatchers(vk::Instance& instance, vk::Device& device)
{
    // 1) 先把 vkGetInstanceProcAddr 填进去
    //    你已链接 vulkan-1.lib，直接用它即可；不需要 volk
    VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

    // 2) 有了 Instance 后，装载 instance 级函数
    VULKAN_HPP_DEFAULT_DISPATCHER.init(instance);

    // 3) 有了 Device 后，装载 device 级函数（★ 包括 vkCmdDrawMeshTasksEXT 等）
    VULKAN_HPP_DEFAULT_DISPATCHER.init(device);

    // 可选自检：如果这里还是空，说明前两步没成功
    // assert(VULKAN_HPP_DEFAULT_DISPATCHER.vkCreateCommandPool && "dispatcher not initialized");
}

void VulkanContext::deleteVma()
{
    if (_allocator != VK_NULL_HANDLE)
    {
        VmaTotalStatistics stats;
        vmaCalculateStatistics(_allocator, &stats);
        auto bytes_to_mb = [](std::uint64_t bytes) -> double {
            constexpr double MB = 1024.0 * 1024.0;  // 1 MiB = 1,048,576 bytes
            return std::round(bytes / MB * 100.0) / 100.0;  // 四舍五入到两位小数
            };
        fmt::print("Total device memory leaked: {} MB.\n", bytes_to_mb(stats.total.statistics.allocationBytes));
        vmaDestroyAllocator(_allocator);
        _allocator = VK_NULL_HANDLE;
    }
}

void VulkanContext::initInstance()
{
    vkb::InstanceBuilder builder;


    // make the vulkan instance, with basic debug features
    auto inst_ret = builder.set_app_name("Example Vulkan Application")
                           .request_validation_layers(bUseValidationLayers)
                           .use_default_debug_messenger()
                           .enable_extension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME)
                           .require_api_version(1, 3, 0)
                           .build();
    if (!inst_ret)
    {
        fmt::print(stderr, "Failed to create vulkan instance: {}\n", inst_ret.error().message());
    }

    _vkb_inst = inst_ret.value();

    // grab the instance
    _instance        = _vkb_inst.instance;
    _debug_messenger = _vkb_inst.debug_messenger;

    DebugUtils::getInstance().initialize(_instance);
}

void VulkanContext::initDevice(vk::SurfaceKHR& surface)
{
    // --- Vulkan 1.3/1.2 Core Features ---
    VkPhysicalDeviceVulkan13Features features13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    features13.dynamicRendering = VK_TRUE;
    features13.synchronization2 = VK_TRUE;
    features13.maintenance4     = VK_TRUE;   // ★ 用了 LocalSizeId 才需要；否则可留 false

    VkPhysicalDeviceVulkan12Features features12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    features12.bufferDeviceAddress                      = VK_TRUE;
    features12.descriptorIndexing                       = VK_TRUE;
    features12.descriptorBindingPartiallyBound          = VK_TRUE;
    features12.descriptorBindingVariableDescriptorCount = VK_TRUE;
    features12.runtimeDescriptorArray                   = VK_TRUE;

    VkPhysicalDeviceFeatures coreFeat{};               // 旧式 core 特性
    coreFeat.fillModeNonSolid = VK_TRUE;               // 只有用了 POINT/LINE 才必须

    // --- Mesh Shader EXT Features ---
    // 只开你要用的位，其他明确保持为 false（默认即 0）
    VkPhysicalDeviceMeshShaderFeaturesEXT meshFeat{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT};
    meshFeat.meshShader = VK_TRUE;
    meshFeat.taskShader = VK_TRUE;                     // 如未用 Task，可改为 VK_FALSE
    // 显式不启用这些“联动”位（避免 07032/07033）：
    meshFeat.multiviewMeshShader                    = VK_FALSE;
    meshFeat.primitiveFragmentShadingRateMeshShader = VK_FALSE;
    meshFeat.meshShaderQueries                      = VK_FALSE;

    // 添加shader保持原始可读信息
    VkPhysicalDeviceShaderRelaxedExtendedInstructionFeaturesKHR relaxedFeat{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_RELAXED_EXTENDED_INSTRUCTION_FEATURES_KHR
    };
    relaxedFeat.shaderRelaxedExtendedInstruction = VK_TRUE;

    // --- 选择物理设备 ---
    auto phys_ret = vkb::PhysicalDeviceSelector{_vkb_inst}
                   .set_surface(surface)
                   .set_minimum_version(1, 3)
                   .set_required_features(coreFeat)
                   .set_required_features_12(features12)
                   .set_required_features_13(features13)
                    // 设备扩展
                   .add_required_extension(VK_EXT_MESH_SHADER_EXTENSION_NAME)
                   .add_required_extension(VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME)
                   .add_required_extension(VK_KHR_SHADER_RELAXED_EXTENDED_INSTRUCTION_EXTENSION_NAME)
                    // 扩展 feature
                   .add_required_extension_features(meshFeat)
                   .add_required_extension_features(relaxedFeat)
                   .select();

    if (!phys_ret)
    {
        fmt::print(stderr, "Failed to select physical device: {}\n", phys_ret.error().message());
    }
    vkb::PhysicalDevice physical_device = phys_ret.value();

    // --- 创建设备（vk-bootstrap 会把上面的扩展与 features 链好） ---
    vkb::Device vkbDevice = vkb::DeviceBuilder{physical_device}.build().value();
    _device               = vk::Device(vkbDevice.device);
    _physical_device      = vk::PhysicalDevice(physical_device.physical_device);

    VkSurfaceKHR vk_surface_khr = surface;
    DebugUtils::getInstance().setDebugName(_device, VK_OBJECT_TYPE_SURFACE_KHR, reinterpret_cast<
                                               uint64_t>(vk_surface_khr), "surface");

    // 准备各种需要的queue
    _graphics_queue        = vkbDevice.get_queue(vkb::QueueType::graphics).value();
    _compute_queue         = vkbDevice.get_queue(vkb::QueueType::compute).value();
    _transfer_queue        = vkbDevice.get_queue(vkb::QueueType::transfer).value();
    _graphics_queue_family = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();
    _compute_queue_family  = vkbDevice.get_queue_index(vkb::QueueType::compute).value();
    _transfer_queue_family = vkbDevice.get_queue_index(vkb::QueueType::transfer).value();
}

VulkanContext::~VulkanContext()
{
    cleanup();
}

void VulkanContext::resizeSwapchainAuto()
{
    _swapchain->clearSwapchain();
    int w, h;
    SDL_GetWindowSize(_window->get_window(), &w, &h);
    const vk::Extent2D extent{static_cast<uint32_t>(w), static_cast<uint32_t>(h)};
    _swapchain = new Swapchain(*_swapchain, extent);
}

void VulkanContext::cleanup()
{
    deleteVma();
    delete _swapchain;
    delete _window;
    vkDestroyDevice(_device, nullptr);
    vkb::destroy_debug_utils_messenger(_instance, _debug_messenger, nullptr);
    vkDestroyInstance(_instance, nullptr);
}
} // dk::vkcore
