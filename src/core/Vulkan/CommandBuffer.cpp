#include "CommandBuffer.h"

#include "Buffer.h"
#include "CommandPool.h"
#include "Image.h"

#include "vk_initializers.h"

#include <Eigen/Eigen>
namespace dk::vkcore {
void CommandBuffer::generateMipmaps(const ImageResource& image, vk::Extent2D image_size)
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
    transitionImage(image, vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
}

void CommandBuffer::copyBuffer(const BufferResource& src, const BufferResource& dst, vk::BufferCopy2 copy)
{
    vk::CopyBufferInfo2 info;
    info.setSrcBuffer(src.getHandle()).setDstBuffer(dst.getHandle()).setRegions(copy).setRegionCount(1);
    _handle.copyBuffer2(info);
}

void CommandBuffer::transitionImage(const ImageResource& image, vk::ImageLayout currentLayout, vk::ImageLayout newLayout)
{
    vk::ImageMemoryBarrier2 image_barrier;
    image_barrier.pNext = nullptr;

    image_barrier.srcStageMask  = vk::PipelineStageFlagBits2::eAllCommands;
    image_barrier.srcAccessMask = vk::AccessFlagBits2::eMemoryWrite;
    image_barrier.dstStageMask  = vk::PipelineStageFlagBits2::eAllCommands;
    image_barrier.dstAccessMask = vk::AccessFlagBits2::eMemoryWrite | vk::AccessFlagBits2::eMemoryRead;

    image_barrier.oldLayout = currentLayout;
    image_barrier.newLayout = newLayout;

    const vk::ImageAspectFlags aspect_mask = (newLayout == vk::ImageLayout::eDepthAttachmentOptimal)
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
}

void CommandBuffer::copyImageToImage(const ImageResource& source, const ImageResource& destination, const vk::Extent2D src_size,
                                     const vk::Extent2D dst_size)
{
    vk::ImageBlit2 blit_region;

    blit_region.srcOffsets[1].x = static_cast<int32_t>(src_size.width);
    blit_region.srcOffsets[1].y = static_cast<int32_t>(src_size.height);
    blit_region.srcOffsets[1].z = 1;

    blit_region.dstOffsets[1].x = static_cast<int32_t>(dst_size.width);
    blit_region.dstOffsets[1].y = static_cast<int32_t>(dst_size.height);
    blit_region.dstOffsets[1].z = 1;

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
    blit_info.dstImageLayout = vk::ImageLayout::eTransferDstOptimal;
    blit_info.srcImage       = source.getHandle();
    blit_info.srcImageLayout = vk::ImageLayout::eTransferSrcOptimal;
    blit_info.filter         = vk::Filter::eLinear;
    blit_info.regionCount    = 1;
    blit_info.pRegions       = &blit_region;

    _handle.blitImage2(&blit_info);
}
}
