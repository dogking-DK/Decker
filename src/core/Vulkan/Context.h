#pragma once
#define bUseValidationLayers true

#include <vk_types.h>

#include "Instance.h"
#include "Macros.h"
#include "Swapchain.h"
#include "Window/SDLWindow.h"

namespace dk::vkcore {

enum class VulkanContextType
{
    Graphics,
    Compute,
    Transfer
};

class VulkanContext
{
public:
    VulkanContext(const uint32_t width = 1920, const uint32_t height = 1080);
    ~VulkanContext();

    vk::Device               getDevice() const { return _device; }
    vk::PhysicalDevice       getPhysicalDevice() const { return _physical_device; }
    vk::Instance             getInstance() const { return _instance; }
    VkDebugUtilsMessengerEXT getDebugMessenger() const { return _debug_messenger; }
    vk::Queue                getGraphicsQueue() const { return _graphics_queue; }
    vk::Queue                getComputeQueue() const { return _compute_queue; }
    vk::Queue                getTransferQueue() const { return _transfer_queue; }
    uint32_t                 getGraphicsQueueIndex() const { return _graphics_queue_family; }
    uint32_t                 getComputeQueueIndex() const { return _compute_queue_family; }
    uint32_t                 getTransferQueueIndex() const { return _transfer_queue_family; }
    Swapchain*               getSwapchain() const { return _swapchain; }
    core::SDLWindow*         getWindow() const { return _window; }
    VmaAllocator             getVmaAllocator() { return _allocator; }

    void resizeSwapchainAuto();

private:
    VkInstance               _instance;
    VkDebugUtilsMessengerEXT _debug_messenger;
    vk::Device               _device;
    vk::PhysicalDevice       _physical_device;
    vkb::Instance            _vkb_inst;

    Swapchain* _swapchain;

    core::SDLWindow* _window;

    vk::Queue _graphics_queue;
    vk::Queue _compute_queue;
    vk::Queue _transfer_queue;
    uint32_t  _graphics_queue_family;
    uint32_t  _compute_queue_family;
    uint32_t  _transfer_queue_family;

    VmaAllocator _allocator; // vma lib allocator

    void initInstance();
    void initDevice(vk::SurfaceKHR& surface);

    void initVulkan(const uint32_t width = 1920, const uint32_t height = 1080);
    void initVma();

    void deleteVma();

    void cleanup();
};
} // vkcore
