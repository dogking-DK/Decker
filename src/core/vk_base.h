#pragma once
#include <vulkan/vulkan_core.h>

#include "vk_descriptors.h"

namespace dk{
namespace vkcore {
    class CommandBuffer;
    class CommandPool;
    class GrowableDescriptorAllocator;
}

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
    VkFence     _renderFence;

    DescriptorAllocatorGrowable                          _frameDescriptors;
    std::shared_ptr<vkcore::GrowableDescriptorAllocator> _dynamicDescriptorAllocator{nullptr};
    DeletionQueue                                        _deletionQueue;
    vkcore::CommandPool*                                 _command_pool_graphic{ nullptr };
    vkcore::CommandPool*                                 _command_pool_transfer{nullptr};
    vkcore::CommandBuffer*                               command_buffer_graphic;
    vkcore::CommandBuffer*                               command_buffer_transfer;
    VkCommandPool                                        _commandPool;
    VkCommandBuffer                                      _mainCommandBuffer;
};
}
