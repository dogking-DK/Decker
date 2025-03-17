#pragma once

#include "Resource.hpp"

namespace dk::vkcore {
class ImageBuilder;
}

namespace dk::vkcore {
// 使用 vk::Image 封装的 Image 资源类
class ImageResource : public Resource<vk::Image, vk::ObjectType::eImage>
{
public:

    ImageResource(VulkanContext& context, ImageBuilder& builder);

    ~ImageResource() override
    {
        if (m_image)
        {
            // 通过静态转换获取底层 VkImage，再使用 VMA 销毁
            vmaDestroyImage(_context->getVmaAllocator(), m_image, allocation);
        }
    }

    // 创建对应的 ImageView，用于在着色器中作为采样器输入
    // 调用者负责销毁返回的 vk::ImageView
    vk::ImageView createImageView(vk::Device device, vk::Format format, vk::ImageAspectFlags aspectFlags) const
    {
        vk::ImageViewCreateInfo viewInfo{};
        viewInfo.image                           = m_image;
        viewInfo.viewType                        = vk::ImageViewType::e2D;
        viewInfo.format                          = format;
        viewInfo.subresourceRange.aspectMask     = aspectFlags;
        viewInfo.subresourceRange.baseMipLevel   = 0;
        viewInfo.subresourceRange.levelCount     = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount     = 1;
        
        return device.createImageView(viewInfo);
    }

    vk::Image getImage() const { return m_image; }

private:
    vk::Image m_image;
    VmaAllocationCreateInfo allocation_create_info = {};
    VmaAllocation           allocation = VK_NULL_HANDLE;
};
}
