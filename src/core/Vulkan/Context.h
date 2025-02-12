#pragma once
#define bUseValidationLayers true

#include <SDL_video.h>
#include <vk_types.h>

#include "Macros.h"

namespace dk::vkcore {
class VulkanContext
{
public:
    VulkanContext();
    ~VulkanContext();

    vk::Device getDevice() const { return _device; }
    vk::PhysicalDevice getPhysicalDevice() const { return _physical_device; }
    vk::Instance getInstance() const { return _instance; }
    vk::Queue getGraphicsQueue() const { return _graphics_queue; }
    vk::Queue getComputeQueue() const { return _compute_queue; }
    vk::Queue getTransferQueue() const { return _transfer_queue; }
    uint32_t getGraphicsQueueIndex() const { return _graphics_queue_family; }
    uint32_t getComputeQueueIndex() const { return _compute_queue_family; }
    uint32_t getTransferQueueIndex() const { return _transfer_queue_family; }

private:
    SDL_Window* _window{nullptr};

    vk::Device _device;
    vk::PhysicalDevice _physical_device;
    VkSurfaceKHR _surface;

    vk::Instance _instance;
    VkDebugUtilsMessengerEXT _debug_messenger;

    vk::Queue _graphics_queue;
    vk::Queue _compute_queue;
    vk::Queue _transfer_queue;
    uint32_t _graphics_queue_family;
    uint32_t _compute_queue_family;
    uint32_t _transfer_queue_family;

    void initVulkan();
    void cleanup();
};
} // vkcore
