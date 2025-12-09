#include "Image.h"
#include <spdlog/spdlog.h>

namespace dk::vkcore {
ImageResource::ImageResource(VulkanContext& context, Builder& builder)
    : Resource(&context, nullptr)
{
    VkResult result = vmaCreateImage(
        context.getVmaAllocator(),
        reinterpret_cast<const VkImageCreateInfo*>(&builder.getCreateInfo()),
        &builder.getAllocationCreateInfo(),
        reinterpret_cast<VkImage*>(&_handle),
        &_allocation,
        &_allocation_info
    );

    _allocation_create_info = builder.getAllocationCreateInfo();

    if (result != VK_SUCCESS)
    {
        auto resStr = to_string(static_cast<vk::Result>(result));
        spdlog::error("vmaCreateImage failed: {} (vkResult={})", resStr, static_cast<int>(result));
    }
}

vk::ImageView ImageResource::createImageView(
    vk::Device           device,
    vk::Format           format,
    vk::ImageAspectFlags aspectFlags) const
{
    vk::ImageViewCreateInfo viewInfo{};
    viewInfo.image                           = _handle;
    viewInfo.viewType                        = vk::ImageViewType::e2D;
    viewInfo.format                          = format;
    viewInfo.subresourceRange.aspectMask     = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel   = 0;
    viewInfo.subresourceRange.levelCount     = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount     = 1;

    return device.createImageView(viewInfo);
}
}
