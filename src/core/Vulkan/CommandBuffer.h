#pragma once

#include "vk_types.h"

#include "CommandPool.h"
#include "Resource.hpp"
#include "Buffer.h"

namespace dk::vkcore {
class ImageResource;
}

namespace dk::vkcore {
class BufferResource;


// 等待某个信号量（binary: value=0；timeline: value=目标值）
// stage = 本次提交里“首次用到该资源”的阶段
inline vk::SemaphoreSubmitInfo makeWait(vk::Semaphore           sem,
                                        vk::PipelineStageFlags2 stage,
                                        uint64_t                value       = 0,
                                        uint32_t                deviceIndex = 0)
{
    return vk::SemaphoreSubmitInfo{sem, value, stage, deviceIndex};
}

// 在本次提交“完成到某阶段”后发出信号
inline vk::SemaphoreSubmitInfo makeSignal(vk::Semaphore           sem,
                                          vk::PipelineStageFlags2 stage,
                                          uint64_t                value       = 0,
                                          uint32_t                deviceIndex = 0)
{
    return vk::SemaphoreSubmitInfo{sem, value, stage, deviceIndex};
}


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


    // --- Dynamic Rendering ---
    void beginRendering(const vk::RenderingInfo& info)
    {
        _handle.beginRendering(info);
    }

    void endRendering()
    {
        _handle.endRendering();
    }

    // 便捷版：只给一个 color attachment（可选深度）
    void beginRenderingColor(
        const vk::Rect2D&                  renderArea,
        const vk::RenderingAttachmentInfo& colorAttachment,
        const vk::RenderingAttachmentInfo* depthAttachment   = nullptr,
        const vk::RenderingAttachmentInfo* stencilAttachment = nullptr,
        uint32_t                           layerCount        = 1,
        uint32_t                           viewMask          = 0)
    {
        vk::RenderingInfo ri{};
        ri.renderArea           = renderArea;
        ri.layerCount           = layerCount;
        ri.viewMask             = viewMask;
        ri.colorAttachmentCount = 1;
        ri.pColorAttachments    = &colorAttachment;
        ri.pDepthAttachment     = depthAttachment;
        ri.pStencilAttachment   = stencilAttachment;
        _handle.beginRendering(ri);
    }

    // --- Dynamic states: viewport & scissor ---
    void setViewport(const vk::Viewport& vp)
    {
        _handle.setViewport(0, 1, &vp);
    }

    void setViewport(float x, float y, float w, float h, float minDepth = 0.f, float maxDepth = 1.f)
    {
        // 注意 Vulkan 的 viewport y 轴是向下的，所以 height 取负值
        vk::Viewport vp{x, y, w, h, minDepth, maxDepth};
        _handle.setViewport(0, 1, &vp);
    }

    // 支持多 viewport（如果用到）
    void setViewports(uint32_t first, vk::ArrayProxy<const vk::Viewport> vps)
    {
        _handle.setViewport(first, vps.size(), vps.data());
    }

    void setScissor(const vk::Rect2D& rc)
    {
        _handle.setScissor(0, 1, &rc);
    }

    void setScissor(int32_t x, int32_t y, uint32_t w, uint32_t h)
    {
        vk::Rect2D rc{{x, y}, {w, h}};
        _handle.setScissor(0, 1, &rc);
    }

    // 常用便捷：用整帧大小一次性设置 viewport+scissor
    void setViewportAndScissor(uint32_t width, uint32_t height)
    {
        setViewport(0.f, static_cast<float>(height), static_cast<float>(width), -static_cast<float>(height));
        setScissor(0, 0, width, height);
    }


    void transitionImage(const ImageResource&    image,
                         vk::ImageLayout         current_layout,
                         vk::ImageLayout         new_layout,
                         vk::PipelineStageFlags2 src_stage  = vk::PipelineStageFlagBits2::eAllCommands,
                         vk::AccessFlags2        src_access = vk::AccessFlagBits2::eMemoryWrite,
                         vk::PipelineStageFlags2 dst_stage  = vk::PipelineStageFlagBits2::eAllCommands,
                         vk::AccessFlags2        dst_access =
                             vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eMemoryWrite);

    void copyImageToImage(const ImageResource& source, const ImageResource& destination, vk::Extent2D src_size,
                          vk::Extent2D         dst_size);

    void copyImageToImage(const vk::Image& source, const vk::Image& destination, vk::Extent2D src_size,
                          vk::Extent2D     dst_size);

    void generateMipmaps(const ImageResource& image, vk::Extent2D image_size);

    void copyBuffer(const BufferResource& src, const BufferResource& dst, vk::BufferCopy2 copy);

    // 新增：从 buffer 拷贝到 image（假定 image 已处于 eTransferDstOptimal）
    void copyBufferToImage(const BufferResource& src,
                           const ImageResource&  dst,
                           vk::Extent3D          extent,
                           uint32_t              mipLevel       = 0,
                           uint32_t              baseArrayLayer = 0,
                           uint32_t              layerCount     = 1);


    // 提交命令缓冲区到队列
    void submit(const vk::Queue& queue, const vk::Fence& fence = nullptr)
    {
        vk::SubmitInfo submitInfo;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers    = &_handle;
        queue.submit(submitInfo, fence);
    }

    // 单个 CommandBuffer 的封装方法
    void submit2(const vk::Queue&                              queue,
                 vk::Fence                                     fence   = {},
                 vk::ArrayProxy<const vk::SemaphoreSubmitInfo> waits   = {},
                 vk::ArrayProxy<const vk::SemaphoreSubmitInfo> signals = {}) const
    {
        // 本方法封装的是“只有这一个 _handle”的提交
        vk::CommandBufferSubmitInfo cmdBufInfo{_handle};

        vk::SubmitInfo2 si{};
        si.waitSemaphoreInfoCount   = waits.size();
        si.pWaitSemaphoreInfos      = waits.data();
        si.commandBufferInfoCount   = 1;
        si.pCommandBufferInfos      = &cmdBufInfo;
        si.signalSemaphoreInfoCount = signals.size();
        si.pSignalSemaphoreInfos    = signals.data();

        // Vulkan 1.3 同步2：queue.submit2(...)
        // 如果你走扩展而非 1.3，请改为 queue.submit2KHR(si, fence);
        queue.submit2(si, fence);
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

inline void uploadBufferDataImmediate(VulkanContext*          ctx,
                                      CommandPool*            pool,
                                      const vk::Queue&        queue,
                                      BufferResource&         staging,          // host-visible + transfer-src
                                      const void*             srcData,
                                      vk::DeviceSize          size,
                                      BufferResource&         dst,              // device-local + transfer-dst
                                      vk::DeviceSize          dstOffset = 0,
                                      bool                    prepForShaderRead = false,
                                      vk::PipelineStageFlags2 dstStages = vk::PipelineStageFlagBits2::eAllCommands,
                                      vk::AccessFlags2        dstAccess = vk::AccessFlagBits2::eShaderStorageRead)
{
    // 1. 先把 CPU 数据写进 staging buffer（它应该是 HOST_VISIBLE）
    staging.update(srcData, size);

    // 2. 录制一次性 copy 命令并提交
    vk::BufferCopy2 region{};
    region.srcOffset = 0;
    region.dstOffset = dstOffset;
    region.size      = size;

    copyBufferImmediate(ctx, pool, queue,
                        staging, dst,
                        {region},
                        prepForShaderRead, dstStages, dstAccess);
}


inline void uploadImageDataImmediate(VulkanContext*   ctx,
                                     CommandPool*     pool,
                                     const vk::Queue& queue,
                                     BufferResource&  staging,        // HOST_VISIBLE + TRANSFER_SRC
                                     const void*      srcPixels,
                                     size_t           dataSize,
                                     ImageResource&   dstImage,       // DEVICE_LOCAL + TRANSFER_DST | SAMPLED
                                     vk::Extent3D     extent,
                                     vk::ImageLayout  initialLayout = vk::ImageLayout::eUndefined,
                                     vk::ImageLayout  finalLayout   = vk::ImageLayout::eShaderReadOnlyOptimal)
{
    // 1. 先把像素写入 staging buffer
    staging.update(srcPixels, dataSize);

    // 2. 一次性 command buffer：layout 转换 + 拷贝 + 再转换
    executeImmediate(ctx, pool, queue, [&](CommandBuffer& cmd)
    {
        // (1) old -> TRANSFER_DST_OPTIMAL
        cmd.transitionImage(dstImage,
                            initialLayout,
                            vk::ImageLayout::eTransferDstOptimal,
                            vk::PipelineStageFlagBits2::eTopOfPipe,
                            vk::AccessFlagBits2::eNone,
                            vk::PipelineStageFlagBits2::eTransfer,
                            vk::AccessFlagBits2::eTransferWrite);

        // (2) buffer -> image
        cmd.copyBufferToImage(staging, dstImage, extent);

        // (3) TRANSFER_DST_OPTIMAL -> finalLayout（通常是 SHADER_READ_ONLY_OPTIMAL）
        cmd.transitionImage(dstImage,
                            vk::ImageLayout::eTransferDstOptimal,
                            finalLayout,
                            vk::PipelineStageFlagBits2::eTransfer,
                            vk::AccessFlagBits2::eTransferWrite,
                            vk::PipelineStageFlagBits2::eFragmentShader,
                            vk::AccessFlagBits2::eShaderRead);
    });
}
}
