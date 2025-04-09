#pragma once

#include <vulkan/vulkan.hpp>

#include "Resource.hpp"

namespace dk::vkcore {
class CommandPool : public Resource<vk::CommandPool, vk::ObjectType::eCommandPool>
{
public:
    // 禁止拷贝，允许移动语义
    CommandPool(const CommandPool&)            = delete;
    CommandPool& operator=(const CommandPool&) = delete;
    CommandPool(CommandPool&&)                 = default;
    CommandPool& operator=(CommandPool&&)      = default;

    // 构造函数：传入设备、队列族索引以及可选的创建标志，默认标志为允许单个 Command Buffer 重置。
    CommandPool(VulkanContext*                   context, const uint32_t queueFamilyIndex,
                const vk::CommandPoolCreateFlags flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer)
        : Resource(context)
    {
        vk::CommandPoolCreateInfo pool_info{};
        pool_info.queueFamilyIndex = queueFamilyIndex;
        pool_info.flags            = flags;

        // 创建 Command Pool，vulkan.hpp 在创建失败时会抛出异常
        _handle = _context->getDevice().createCommandPool(pool_info);
    }

    // 析构函数：自动销毁 Command Pool，释放占用的所有资源
    ~CommandPool() override
    {
        if (_handle)
        {
            _context->getDevice().destroyCommandPool(_handle);
        }
    }

    // 重置 Command Pool，可选择传入重置标志（一般在需要重用 Command Buffer 前调用）
    void reset(const vk::CommandPoolResetFlags flags = {})
    {
        _context->getDevice().resetCommandPool(_handle, flags);
    }

    void executeImmediate(const vk::Queue& queue, const std::function<void(vk::CommandBuffer)>& record_function);
};
}
