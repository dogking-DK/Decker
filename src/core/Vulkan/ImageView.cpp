#include "ImageView.h"
#include <fmt/color.h>

namespace dk::vkcore {
ImageViewResource::ImageViewResource(
    VulkanContext&       context,
    ImageResource&       image,
    vk::ImageViewType    viewType,
    vk::Format           format,
    vk::ImageAspectFlags aspectFlags,
    uint32_t             baseMipLevel,
    uint32_t             levelCount,
    uint32_t             baseArrayLayer,
    uint32_t             layerCount)
    : Resource(&context, nullptr)
    , _image(&image)
{
    vk::ImageViewCreateInfo ci{};
    ci.image    = _image->getHandle();
    ci.viewType = viewType;
    ci.format   = format;

    ci.subresourceRange.aspectMask     = aspectFlags;
    ci.subresourceRange.baseMipLevel   = baseMipLevel;
    ci.subresourceRange.levelCount     = levelCount;
    ci.subresourceRange.baseArrayLayer = baseArrayLayer;
    ci.subresourceRange.layerCount     = layerCount;

    _handle = _context->getDevice().createImageView(ci);
    if (!_handle)
    {
        print(fg(fmt::color::red), "vulkan image view create fail\n");
    }

    _image->getImageViews().insert(this);
    _format            = ci.format;
    _subresource_range = ci.subresourceRange;
}

std::unique_ptr<ImageViewResource> ImageViewResource::create2D(
    VulkanContext&       context,
    ImageResource&       image,
    vk::Format           format,
    vk::ImageAspectFlags aspectFlags)
{
    // 默认视图：整张 2D 贴图、单 mip、单 layer
    return std::make_unique<ImageViewResource>(
        context, image,
        vk::ImageViewType::e2D,
        format,
        aspectFlags,
        0, 1, 0, 1);
}
}
