#pragma once
#include <tsl/robin_set.h>
#include <tsl/robin_map.h>
#include "Resource.hpp"

namespace dk::vkcore {
class ImageViewResource;
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
        if (_handle)
        {
            // 通过静态转换获取底层 VkImage，再使用 VMA 销毁
            vmaDestroyImage(_context->getVmaAllocator(), _handle, _allocation);
        }
    }

    // 创建对应的 ImageView，用于在着色器中作为采样器输入
    // 调用者负责销毁返回的 vk::ImageView
    vk::ImageView createImageView(vk::Device device, vk::Format format, vk::ImageAspectFlags aspectFlags) const
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

    tsl::robin_set<ImageViewResource*>& getImageViews() { return _image_views; }

    VmaAllocation getAllocation() const { return _allocation; }

private:
    VmaAllocationCreateInfo _allocation_create_info = {};
    VmaAllocation           _allocation = VK_NULL_HANDLE;
    VmaAllocationInfo           _allocation_info = {};
    tsl::robin_set<ImageViewResource*> _image_views;
};
}
