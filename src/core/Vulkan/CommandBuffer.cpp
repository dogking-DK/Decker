#include "CommandBuffer.h"

#include "Buffer.h"
#include "CommandPool.h"
#include "Image.h"

#include "vk_initializers.h"

#include <Eigen/Eigen>

namespace {
using namespace dk::vkcore;

struct StageAccess
{
    vk::PipelineStageFlags2 stage;
    vk::AccessFlags2        access;
};

vk::ImageLayout layout_for_usage(const ImageUsage& usage)
{
    switch (usage)
    {
    case ImageUsage::TransferDst: return vk::ImageLayout::eTransferDstOptimal;
    case ImageUsage::TransferSrc: return vk::ImageLayout::eTransferSrcOptimal;
    case ImageUsage::Sampled: return vk::ImageLayout::eShaderReadOnlyOptimal;
    case ImageUsage::ColorAttachment: return vk::ImageLayout::eColorAttachmentOptimal;
    case ImageUsage::DepthStencilAttachment: return vk::ImageLayout::eDepthStencilAttachmentOptimal;
    case ImageUsage::Present: return vk::ImageLayout::ePresentSrcKHR;
    case ImageUsage::Undefined: return vk::ImageLayout::eUndefined;
    }
    return vk::ImageLayout::eUndefined;
}

StageAccess stage_access_for_usage(const ImageUsage& usage, const bool is_src)
{
    switch (usage)
    {
    case ImageUsage::TransferDst:
    case ImageUsage::TransferSrc:
        return {
            vk::PipelineStageFlagBits2::eTransfer,
            is_src
                ? vk::AccessFlagBits2::eTransferWrite
                : vk::AccessFlagBits2::eTransferRead
        };

    case ImageUsage::Sampled:
        return {
            vk::PipelineStageFlagBits2::eFragmentShader | vk::PipelineStageFlagBits2::eComputeShader,
            vk::AccessFlagBits2::eShaderRead
        };

    case ImageUsage::ColorAttachment:
        return {
            vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            is_src
                ? vk::AccessFlagBits2::eColorAttachmentWrite
                : (vk::AccessFlagBits2::eColorAttachmentRead | vk::AccessFlagBits2::eColorAttachmentWrite)
        };

    case ImageUsage::DepthStencilAttachment:
        return {
            vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
            is_src
                ? vk::AccessFlagBits2::eDepthStencilAttachmentWrite
                : (vk::AccessFlagBits2::eDepthStencilAttachmentRead |
                   vk::AccessFlagBits2::eDepthStencilAttachmentWrite)
        };

    case ImageUsage::Present:
        // Present 这边通常 srcStage 选 ColorAttachmentOutput / AllCommands，access = 0 也行
        return {
            vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            vk::AccessFlagBits2::eNone
        };

    case ImageUsage::Undefined:
        // Undefined: 作为 src 时通常是 TOP_OF_PIPE + 无访问
        return {
            vk::PipelineStageFlagBits2::eTopOfPipe,
            vk::AccessFlagBits2::eNone
        };
    }
    return {
        vk::PipelineStageFlagBits2::eTopOfPipe,
        vk::AccessFlagBits2::eNone
    };
}
}

namespace dk::vkcore {
void CommandBuffer::generateMipmaps(ImageResource& image, vk::Extent2D image_size)
{
    const int mip_levels = static_cast<int>(std::floor(std::log2(std::max(image_size.width, image_size.height)))) + 1;
    for (int mip = 0; mip < mip_levels; mip++)
    {
        vk::Extent2D half_size = image_size;
        half_size.width /= 2;
        half_size.height /= 2;

        vk::ImageMemoryBarrier2 image_barrier;

        image_barrier.srcStageMask  = vk::PipelineStageFlagBits2::eAllCommands;
        image_barrier.srcAccessMask = vk::AccessFlagBits2::eMemoryWrite;
        image_barrier.dstStageMask  = vk::PipelineStageFlagBits2::eAllCommands;
        image_barrier.dstAccessMask = vk::AccessFlagBits2::eMemoryWrite | vk::AccessFlagBits2::eMemoryRead;

        image_barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
        image_barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;

        constexpr vk::ImageAspectFlags aspect_mask  = vk::ImageAspectFlagBits::eColor;
        image_barrier.subresourceRange              = vkinit::image_subresource_range(aspect_mask);
        image_barrier.subresourceRange.levelCount   = 1;
        image_barrier.subresourceRange.baseMipLevel = mip;
        image_barrier.image                         = image.getHandle();

        vk::DependencyInfo dep_info;
        dep_info.imageMemoryBarrierCount = 1;
        dep_info.pImageMemoryBarriers    = &image_barrier;

        _handle.pipelineBarrier2(&dep_info);

        if (mip < mip_levels - 1)
        {
            vk::ImageBlit2 blit_region;

            blit_region.srcOffsets[1].x = static_cast<int32_t>(image_size.width);
            blit_region.srcOffsets[1].y = static_cast<int32_t>(image_size.height);
            blit_region.srcOffsets[1].z = 1;

            blit_region.dstOffsets[1].x = static_cast<int32_t>(half_size.width);
            blit_region.dstOffsets[1].y = static_cast<int32_t>(half_size.height);
            blit_region.dstOffsets[1].z = 1;

            blit_region.srcSubresource.aspectMask     = vk::ImageAspectFlagBits::eColor;
            blit_region.srcSubresource.baseArrayLayer = 0;
            blit_region.srcSubresource.layerCount     = 1;
            blit_region.srcSubresource.mipLevel       = mip;

            blit_region.dstSubresource.aspectMask     = vk::ImageAspectFlagBits::eColor;
            blit_region.dstSubresource.baseArrayLayer = 0;
            blit_region.dstSubresource.layerCount     = 1;
            blit_region.dstSubresource.mipLevel       = mip + 1;

            vk::BlitImageInfo2 blit_info;
            blit_info.dstImage       = image.getHandle();
            blit_info.dstImageLayout = vk::ImageLayout::eTransferDstOptimal;
            blit_info.srcImage       = image.getHandle();
            blit_info.srcImageLayout = vk::ImageLayout::eTransferSrcOptimal;
            blit_info.filter         = vk::Filter::eLinear;
            blit_info.regionCount    = 1;
            blit_info.pRegions       = &blit_region;

            _handle.blitImage2(&blit_info);

            image_size = half_size;
        }
    }

    // transition all mip levels into the final read_only layout
    image.setCurrentLayout(vk::ImageLayout::eTransferSrcOptimal);
    image.setUsage(ImageUsage::TransferSrc);

    // 转为 Sampled（内部会更新 currentLayout + usage）
    transitionImage(image, ImageUsage::Sampled);
}

void CommandBuffer::copyBuffer(const BufferResource& src, const BufferResource& dst, vk::BufferCopy2 copy)
{
    vk::CopyBufferInfo2 info;
    info.setSrcBuffer(src.getHandle()).setDstBuffer(dst.getHandle()).setRegions(copy).setRegionCount(1);
    _handle.copyBuffer2(info);
}

void CommandBuffer::copyBufferToImage(const BufferResource& src,
                                      const ImageResource&  dst,
                                      uint32_t              mipLevel,
                                      uint32_t              baseArrayLayer,
                                      uint32_t              layerCount)
{
    vk::BufferImageCopy2 region{};
    region.bufferOffset      = 0;
    region.bufferRowLength   = 0; // 0 = 紧凑排列
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask     = vk::ImageAspectFlagBits::eColor;
    region.imageSubresource.mipLevel       = mipLevel;
    region.imageSubresource.baseArrayLayer = baseArrayLayer;
    region.imageSubresource.layerCount     = layerCount;

    region.imageOffset = vk::Offset3D{0, 0, 0};
    region.imageExtent = vk::Extent3D{dst.getExtent().width, dst.getExtent().height, dst.getExtent().depth};

    vk::CopyBufferToImageInfo2 info{};
    info.srcBuffer      = src.getHandle();
    info.dstImage       = dst.getHandle();
    info.dstImageLayout = vk::ImageLayout::eTransferDstOptimal;
    info.regionCount    = 1;
    info.pRegions       = &region;

    _handle.copyBufferToImage2(info);
}

void CommandBuffer::submit(const vk::Queue& queue, const vk::Fence& fence)
{
    vk::SubmitInfo submitInfo;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &_handle;
    queue.submit(submitInfo, fence);
}

void CommandBuffer::submit2(const vk::Queue&                              queue, vk::Fence fence,
                            vk::ArrayProxy<const vk::SemaphoreSubmitInfo> waits,
                            vk::ArrayProxy<const vk::SemaphoreSubmitInfo> signals) const
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

void CommandBuffer::transitionImage(ImageResource&          image, vk::ImageLayout      new_layout,
                                    vk::PipelineStageFlags2 src_stage, vk::AccessFlags2 src_access,
                                    vk::PipelineStageFlags2 dst_stage, vk::AccessFlags2 dst_access)
{
    vk::ImageMemoryBarrier2 image_barrier;
    image_barrier.pNext = nullptr;

    image_barrier.srcStageMask  = src_stage;
    image_barrier.srcAccessMask = src_access;
    image_barrier.dstStageMask  = dst_stage;
    image_barrier.dstAccessMask = dst_access;

    image_barrier.oldLayout = image.getCurrentLayout();
    image_barrier.newLayout = new_layout;

    const vk::ImageAspectFlags aspect_mask = (new_layout == vk::ImageLayout::eDepthAttachmentOptimal)
                                                 ? vk::ImageAspectFlagBits::eDepth
                                                 : vk::ImageAspectFlagBits::eColor;
    image_barrier.subresourceRange = vkinit::image_subresource_range(aspect_mask);
    image_barrier.image            = image.getHandle();

    vk::DependencyInfo dep_info;
    dep_info.sType = vk::StructureType::eDependencyInfo;
    dep_info.pNext = nullptr;

    dep_info.imageMemoryBarrierCount = 1;
    dep_info.pImageMemoryBarriers    = &image_barrier;

    _handle.pipelineBarrier2(&dep_info);

    image.setCurrentLayout(new_layout);
}

void CommandBuffer::transitionImage(ImageResource& image, const ImageUsage& new_usage)
{
    if (image.getUsage() == new_usage) return;

    const vk::ImageLayout new_layout = layout_for_usage(new_usage);

    const auto [src_stage, src_access] = stage_access_for_usage(image.getUsage(), true);
    const auto [dst_stage, dst_access] = stage_access_for_usage(new_usage, false);

    // 3. 调用低层版本真正录 barrier
    transitionImage(image, new_layout, src_stage, src_access, dst_stage, dst_access);

    // 4. 更新 ImageResource 的状态
    image.setUsage(new_usage);
}

void CommandBuffer::copyImageToImage(const ImageResource& source, const ImageResource& destination)
{
    vk::ImageBlit2 blit_region;

    blit_region.srcOffsets[1].x = static_cast<int32_t>(source.getExtent().width);
    blit_region.srcOffsets[1].y = static_cast<int32_t>(source.getExtent().height);
    blit_region.srcOffsets[1].z = static_cast<int32_t>(source.getExtent().depth);

    blit_region.dstOffsets[1].x = static_cast<int32_t>(destination.getExtent().width);
    blit_region.dstOffsets[1].y = static_cast<int32_t>(destination.getExtent().height);
    blit_region.dstOffsets[1].z = static_cast<int32_t>(destination.getExtent().depth);

    blit_region.srcSubresource.aspectMask     = vk::ImageAspectFlagBits::eColor;
    blit_region.srcSubresource.baseArrayLayer = 0;
    blit_region.srcSubresource.layerCount     = 1;
    blit_region.srcSubresource.mipLevel       = 0;

    blit_region.dstSubresource.aspectMask     = vk::ImageAspectFlagBits::eColor;
    blit_region.dstSubresource.baseArrayLayer = 0;
    blit_region.dstSubresource.layerCount     = 1;
    blit_region.dstSubresource.mipLevel       = 0;

    vk::BlitImageInfo2 blit_info;
    blit_info.dstImage       = destination.getHandle();
    blit_info.dstImageLayout = destination.getCurrentLayout();
    blit_info.srcImage       = source.getHandle();
    blit_info.srcImageLayout = source.getCurrentLayout();
    blit_info.filter         = vk::Filter::eLinear;
    blit_info.regionCount    = 1;
    blit_info.pRegions       = &blit_region;

    _handle.blitImage2(&blit_info);
}

void copy_buffer_immediate(VulkanContext* ctx, CommandPool* pool, const vk::Queue& queue, const BufferResource& src,
                           const BufferResource& dst, vk::ArrayProxy<const vk::BufferCopy2> regions,
                           bool prepForShaderRead, vk::PipelineStageFlags2 dstStages, vk::AccessFlags2 dstAccess)
{
    execute_immediate(ctx, pool, queue, [&](CommandBuffer& cmd)
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

void upload_buffer_data_immediate(VulkanContext* ctx, CommandPool* pool, const vk::Queue& queue, const void* srcData,
                                  vk::DeviceSize size, BufferResource& dst, vk::DeviceSize dstOffset,
                                  vk::PipelineStageFlags2 dstStages,
                                  vk::AccessFlags2 dstAccess)

{
    // 1. 内部创建临时的 Staging Buffer
    //    使用 VMA_MEMORY_USAGE_CPU_ONLY 或 CPU_TO_GPU 确保 host 可写
    BufferResource::Builder builder;
    builder.setSize(size)
           .setUsage(vk::BufferUsageFlagBits::eTransferSrc)
           .withVmaUsage(VMA_MEMORY_USAGE_AUTO)
           .withVmaFlags(VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                         VMA_ALLOCATION_CREATE_MAPPED_BIT); // 自动映射，方便 update

    auto staging = builder.buildUnique(*ctx);

    // 2. 写入数据 (staging 是 host visible 的)
    staging->update(srcData, size);

    // 3. 执行拷贝
    vk::BufferCopy2 region{};
    region.srcOffset = 0;
    region.dstOffset = dstOffset;
    region.size      = size;

    copy_buffer_immediate(ctx, pool, queue,
                          *staging, dst,
                          {region},
                          true,        // prepForShaderRead (自动加 Barrier)
                          dstStages,
                          dstAccess);

    // 函数结束，staging 智能指针析构，临时 Buffer 自动释放
}

void upload_image_data_immediate(VulkanContext*   ctx,
                                 CommandPool*     pool,
                                 const vk::Queue& queue,
                                 const void*      srcPixels,
                                 size_t           dataSize,
                                 ImageResource&   dstImage,
                                 ImageUsage       finalUsage)
{
    // 1. 临时 staging buffer
    BufferResource::Builder builder;
    builder.setSize(dataSize)
           .setUsage(vk::BufferUsageFlagBits::eTransferSrc)
           .withVmaUsage(VMA_MEMORY_USAGE_AUTO)
           .withVmaFlags(VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                         VMA_ALLOCATION_CREATE_MAPPED_BIT);

    auto staging = builder.buildUnique(*ctx);

    // 2. 写入像素数据
    staging->update(srcPixels, dataSize);

    // 3. 录制：Undefined/当前状态 -> TransferDst -> 拷贝 -> finalUsage
    execute_immediate(ctx, pool, queue, [&](CommandBuffer& cmd)
    {
        // 确保 CPU 侧状态从一个合理的起点开始
        // 如果你知道创建时就是 UNDEFINED，可以这里重置一下：
        // dstImage.setCurrentLayout(vk::ImageLayout::eUndefined);
        // dstImage.setUsage(ImageUsage::Undefined);

        // (A) 转为 TransferDst（自动选 stage/access/layout）
        cmd.transitionImage(dstImage, ImageUsage::TransferDst);

        // (B) 拷贝 Buffer -> Image
        cmd.copyBufferToImage(*staging, dstImage);

        // (C) 转为最终用法（Sampled / ColorAttachment / Present 等）
        cmd.transitionImage(dstImage, finalUsage);
    });
}
}
