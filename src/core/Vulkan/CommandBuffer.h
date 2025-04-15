#include <vulkan/vulkan.hpp>

#include "CommandPool.h"
#include "Resource.hpp"

namespace dk::vkcore {
class BufferResource;

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
        vk::CommandBufferAllocateInfo allocInfo{command_pool->getHandle(), vk::CommandBufferLevel::ePrimary, 1};

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

    template <typename Func>
    void record(Func&&                      recordingFunction,
                vk::CommandBufferUsageFlags usageFlags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit)
    {
        // 将内部的 command buffer 对象传入 lambda 中
        recordingFunction(_handle);
    }

    // 如有需要，还可以提供 reset 功能
    void reset(const vk::CommandBufferResetFlags flags = {})
    {
        _handle.reset(flags);
    }


    void transitionImage(vk::Image image, vk::ImageLayout currentLayout, vk::ImageLayout newLayout);

    void copyImageToImage(vk::Image source, vk::Image destination, vk::Extent2D src_size, vk::Extent2D dst_size);

    void generateMipmaps(vk::Image image, vk::Extent2D image_size);

    void copyBuffer(const BufferResource& src, const BufferResource& dst, vk::BufferCopy2 copy);

    // 提交命令缓冲区到队列
    void submit(const vk::Queue& queue, const vk::Fence& fence = nullptr)
    {
        vk::SubmitInfo submitInfo;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers    = &_handle;
        _context->getGraphicsQueue().submit(submitInfo, fence);
    }

    void submit2(const vk::Queue& queue,
                 const vk::Fence& fence = nullptr)
    {
        // 构造 vk::CommandBufferSubmitInfo 封装底层命令缓冲区
        vk::CommandBufferSubmitInfo cmdBufInfo{};
        cmdBufInfo.commandBuffer = _handle;

        vk::SubmitInfo2 submitInfo2;
        submitInfo2.commandBufferInfoCount = 1;
        submitInfo2.pCommandBufferInfos    = &cmdBufInfo;
        // 若需要设置信号量或等待信号量信息，可填充对应的 wait/semaphore 信息结构

        // 提交到队列（注意：需要支持 VK_KHR_synchronization2）
        queue.submit2({submitInfo2}, fence);
    }

private:
    CommandPool* _command_pool;
};

template <typename Func>
void executeImmediate(VulkanContext*                                 context,
                      CommandPool*                                   command_pool,
                      const vk::Queue&                               queue,
                      const std::function<void(CommandBuffer& cmd)>& function)
{
    CommandBuffer cmd(context, command_pool);
    cmd.begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

    // 3. 调用传入的 lambda 录制具体命令（例如：blitImage、copyImage、generateMipMap 等）
    function(cmd);

    // 4. 结束录制
    cmd.end();

    // 6. 创建 fence 同步等待立即提交命令执行完毕
    vk::Fence fence = context->getDevice().createFence({});

    cmd.submit2(queue, fence);

    // 7. 等待 fence 信号
    auto result = context->getDevice().waitForFences({fence}, VK_TRUE, UINT64_MAX);
    VK_CHECK(static_cast<VkResult>(result));
    context->getDevice().destroyFence(fence);

    // 可选：如果不采用 CommandPool 内统一重置，也可以手动释放当前 CommandBuffer
    context->getDevice().freeCommandBuffers(command_pool->getHandle(), cmd.getHandle());
}
}
