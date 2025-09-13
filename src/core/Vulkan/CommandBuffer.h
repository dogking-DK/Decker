#pragma once

#include <vulkan/vulkan.hpp>

#include "CommandPool.h"
#include "Resource.hpp"
#include "Buffer.h"

namespace dk::vkcore {
class ImageResource;
}

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
        std::lock_guard<std::mutex>   lock(command_pool->getMutex()); // 加锁
        // 分配命令缓冲区可能会抛出异常（vulkan.hpp 默认采用异常处理模型）
        auto buffers = _context->getDevice().allocateCommandBuffers(allocInfo);
        _handle      = buffers.front();
    }

    // 注意：在 Vulkan 中，一般不需要对单个 command buffer 调用 destroy，
    // 而是统一由 command pool 负责释放。如果需要手动重置或回收，
    // 则可调用 m_device.freeCommandBuffers(m_commandPool, {_handle});
    ~CommandBuffer() override
    {
        // 如果 handle 有效，并且它不是从外部传入的（需要一个标志来判断所有权）
        // 就将它释放回池中
        if (_handle && _command_pool)
        {
            // 注意：这里假设一个 CommandBuffer 总是从一个池中分配的
            _context->getDevice().freeCommandBuffers(_command_pool->getHandle(), {_handle});
        }
    }

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


    void transitionImage(const ImageResource&    image,
                         vk::ImageLayout         current_layout,
                         vk::ImageLayout         new_layout,
                         vk::PipelineStageFlags2 src_stage  = vk::PipelineStageFlagBits2::eTransfer,
                         vk::AccessFlags2        src_access = vk::AccessFlagBits2::eTransferWrite,
                         vk::PipelineStageFlags2 dst_stage  = vk::PipelineStageFlagBits2::eAllCommands,
                         vk::AccessFlags2        dst_access =
                             vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eMemoryWrite);

    void copyImageToImage(const ImageResource& source, const ImageResource& destination, vk::Extent2D src_size,
                          vk::Extent2D         dst_size);

    void generateMipmaps(const ImageResource& image, vk::Extent2D image_size);

    void copyBuffer(const BufferResource& src, const BufferResource& dst, vk::BufferCopy2 copy);

    // 提交命令缓冲区到队列
    void submit(const vk::Queue& queue, const vk::Fence& fence = nullptr)
    {
        vk::SubmitInfo submitInfo;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers    = &_handle;
        queue.submit(submitInfo, fence);
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

// C++20 约束（可选）：只有能以 (CommandBuffer&) 调用的才匹配
template <std::invocable<CommandBuffer&> Func>
void executeImmediate(VulkanContext*   ctx,
                      CommandPool*     pool,
                      const vk::Queue& queue,
                      Func&&           record)          // ⭐ 转发引用，保留值类别与可变性
{
    CommandBuffer cmd(ctx, pool);
    cmd.begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

    std::invoke(std::forward<Func>(record), cmd);  // ⭐ 完美转发调用

    cmd.end();

    vk::UniqueFence fence = ctx->getDevice().createFenceUnique({});
    cmd.submit2(queue, fence.get());
    (void)ctx->getDevice().waitForFences({fence.get()}, VK_TRUE, UINT64_MAX);

    // 视你的实现可 reset 复用或直接销毁
    cmd.reset();
}

inline void copyBufferImmediate(VulkanContext*                        ctx,
                                CommandPool*                          pool,
                                const vk::Queue&                      queue,
                                const BufferResource&                 src,
                                const BufferResource&                 dst,
                                vk::ArrayProxy<const vk::BufferCopy2> regions,
                                // 可选：把目标准备成 Shader 读
                                bool                    prepForShaderRead = true,
                                vk::PipelineStageFlags2 dstStages         = vk::PipelineStageFlagBits2::eAllCommands,
                                vk::AccessFlags2        dstAccess         = vk::AccessFlagBits2::eShaderStorageRead)
{
    executeImmediate(ctx, pool, queue, [&](CommandBuffer& cmd)
    {
        // copy2（你已有封装）
        for (const auto& r : regions)
        {
            cmd.copyBuffer(src, dst, r); // 内部用 vk::CopyBufferInfo2 + copyBuffer2 ✅
        }

        if (prepForShaderRead)
        {
            vk::BufferMemoryBarrier2 b{};
            b.srcStageMask  = vk::PipelineStageFlagBits2::eTransfer;
            b.srcAccessMask = vk::AccessFlagBits2::eTransferWrite;
            b.dstStageMask  = dstStages;   // 例如 eComputeShader / eMeshShaderEXT / eFragmentShader
            b.dstAccessMask = dstAccess;   // 例如 eShaderStorageRead / eUniformRead
            b.buffer        = dst.getHandle();
            b.offset        = 0;
            b.size          = VK_WHOLE_SIZE;

            vk::DependencyInfo dep{};
            dep.bufferMemoryBarrierCount = 1;
            dep.pBufferMemoryBarriers    = &b;

            // 直接在同一 CB 末尾做可见性转换
            cmd.getHandle().pipelineBarrier2(dep);
        }
    });
}
}
