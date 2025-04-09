#include "CommandPool.h"

void dk::vkcore::CommandPool::executeImmediate(const vk::Queue&                              queue,
                                               const std::function<void(vk::CommandBuffer)>& record_function)
{
    // 分配一次性命令缓冲区
    vk::CommandBufferAllocateInfo allocInfo{
        _handle,
        vk::CommandBufferLevel::ePrimary,
        1
    };
    auto              commandBuffers = _context->getDevice().allocateCommandBuffers(allocInfo);
    vk::CommandBuffer commandBuffer  = commandBuffers.front();

    // 开始命令缓冲区录制
    vk::CommandBufferBeginInfo beginInfo{vk::CommandBufferUsageFlagBits::eOneTimeSubmit};
    commandBuffer.begin(beginInfo);

    // 录制用户指定的命令（通过 lambda 完成）
    record_function(commandBuffer);

    // 结束录制
    commandBuffer.end();

    // 提交命令缓冲区到队列
    vk::SubmitInfo submitInfo;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &commandBuffer;
    queue.submit(submitInfo, vk::Fence{});

    // 等待队列完成执行
    queue.waitIdle();

    // 清理：因为是从 command pool 分配的，通常可以在稍后重置 command pool 时统一释放
    // 如果需要立刻释放，可以调用 device.freeCommandBuffers(commandPool, commandBuffers);
}
