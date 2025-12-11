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
    const auto& create_info = builder.getCreateInfo();
    _current_layout         = create_info.initialLayout;
    _usage                  = ImageUsage::Undefined;
    _extent                 = {create_info.extent.width, create_info.extent.height, create_info.extent.depth};

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
