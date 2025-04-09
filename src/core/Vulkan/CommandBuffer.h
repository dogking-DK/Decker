#include <vulkan/vulkan.hpp>

#include "CommandPool.h"
#include "Resource.hpp"

namespace dk::vkcore {

class CommandBuffer : public Resource<vk::CommandBuffer, vk::ObjectType::eCommandBuffer>
{
public:
    // 禁止拷贝，允许移动
    CommandBuffer(const CommandBuffer&)            = delete;
    CommandBuffer& operator=(const CommandBuffer&) = delete;
    CommandBuffer(CommandBuffer&&)                 = default;
    CommandBuffer& operator=(CommandBuffer&&)      = default;

    // 构造时通过指定设备和命令池来分配一个命令缓冲区
    CommandBuffer(VulkanContext* context, CommandPool* command_pool)
        : Resource(context), _command_pool(command_pool)
    {
        vk::CommandBufferAllocateInfo allocInfo{
            command_pool->getHandle(),
            vk::CommandBufferLevel::ePrimary,
            1
        };

        // 分配命令缓冲区可能会抛出异常（vulkan.hpp 默认采用异常处理模型）
        auto buffers = _context->getDevice().allocateCommandBuffers(allocInfo);
        _handle      = buffers.front();
    }
    // 注意：在 Vulkan 中，一般不需要对单个 command buffer 调用 destroy，
    // 而是统一由 command pool 负责释放。如果需要手动重置或回收，
    // 则可调用 m_device.freeCommandBuffers(m_commandPool, {_handle});
    ~CommandBuffer() override = default;

    // 开始录制命令，可以指定使用的标志，默认 eOneTimeSubmit
    void begin(const vk::CommandBufferUsageFlags usageFlags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit)
    {
        const vk::CommandBufferBeginInfo begin_info{usageFlags};
        _handle.begin(begin_info);
    }

    // 结束录制
    void end()
    {
        _handle.end();
    }

    // 提供一个基于 Lambda 的录制封装
    // 使用范例：
    //   cmdBuffer.record([](vk::CommandBuffer cb){
    //       cb.beginRenderPass(...);
    //       // ...
    //       cb.endRenderPass();
    //   });
    template <typename Func>
    void record(Func&&                      recordingFunction,
                vk::CommandBufferUsageFlags usageFlags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit)
    {
        begin(usageFlags);
        // 将内部的 command buffer 对象传入 lambda 中
        recordingFunction(_handle);
        end();
    }

    // 如有需要，还可以提供 reset 功能
    void reset(const vk::CommandBufferResetFlags flags = {})
    {
        _handle.reset(flags);
    }

private:
    CommandPool* _command_pool;
};
}
