#include <stdexcept>
#include <iostream>
#include <VkBootstrap.h>
#include "Context.h"

#include <SDL_vulkan.h>

#include "vk_debug_util.h"


VulkanContext::VulkanContext()
{
    initVulkan();
}

void VulkanContext::initVulkan()
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


    vkb::Instance vkb_inst = inst_ret.value();

    // grab the instance
    _instance = vkb_inst.instance;
    _debug_messenger = vkb_inst.debug_messenger;

    dk::DebugUtils::getInstance().initialize(_instance);

    uint32_t instance_extension_count;
    VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &instance_extension_count, nullptr));

    std::vector<VkExtensionProperties> available_instance_extensions(instance_extension_count);
    VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &instance_extension_count,
                                                    available_instance_extensions.data()));

    auto sdl_err = SDL_Vulkan_CreateSurface(_window, _instance, &_surface);
    if (!sdl_err)
    {
        fmt::print(stderr, "Failed to create SDL\n");
    }

    VkPhysicalDeviceVulkan13Features features13;
    features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features13.pNext = nullptr;
    features13.dynamicRendering = true;
    features13.synchronization2 = true;

    VkPhysicalDeviceVulkan12Features features12;
    features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features12.pNext = nullptr;
    features12.bufferDeviceAddress = true;
    features12.descriptorIndexing = true;
    features12.descriptorBindingPartiallyBound = true;
    features12.descriptorBindingVariableDescriptorCount = true;
    features12.runtimeDescriptorArray = true;

    // 按照设置选择对应GPU
    vkb::PhysicalDeviceSelector selector{vkb_inst};
    selector.set_minimum_version(1, 3); // 至少使用vulkan1.3
    selector.set_required_features_13(features13);
    selector.set_required_features_12(features12);
    selector.set_surface(_surface);
    auto select_return = selector.select();
    if (!select_return)
    {
        fmt::print(stderr, "Failed to select physical device: {}\n",
                   select_return.error().message());
    }
    const vkb::PhysicalDevice& physical_device = select_return.value();

    // 创建逻辑设备
    vkb::DeviceBuilder deviceBuilder{physical_device};
    vkb::Device vkbDevice = deviceBuilder.build().value();

    // 储存各种设备
    _device = vk::Device(vkbDevice.device);
    _physical_device = vk::PhysicalDevice(physical_device.physical_device);

    // 准备各种需要的queue
    _graphics_queue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
    _compute_queue = vkbDevice.get_queue(vkb::QueueType::compute).value();
    _transfer_queue = vkbDevice.get_queue(vkb::QueueType::transfer).value();
    _graphics_queue_family = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();
    _graphics_queue_family = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();
    _graphics_queue_family = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();
}

VulkanContext::~VulkanContext()
{
    cleanup();
}

void VulkanContext::cleanup()
{
    vkDestroySurfaceKHR(_instance, _surface, nullptr);

    vkDestroyDevice(_device, nullptr);
        vkb::destroy_debug_utils_messenger(_instance, _debug_messenger, nullptr);
        vkDestroyInstance(_instance, nullptr);

    SDL_DestroyWindow(_window);
}
