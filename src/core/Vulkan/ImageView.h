#pragma once

#include "vk_types.h"
#include <memory>

#include "Context.h"
#include "Image.h"
#include "Resource.hpp"

namespace dk::vkcore {
class ImageResource;

// ImageViewResource 封装了 vk::ImageView，并在析构时自动销毁
class ImageViewResource : public Resource<vk::ImageView, vk::ObjectType::eImageView>
{
public:
    // 直接构造，内部自己拼 ImageViewCreateInfo
    ImageViewResource(
        VulkanContext&       context,
        ImageResource&       image,
        vk::ImageViewType    viewType,
        vk::Format           format,
        vk::ImageAspectFlags aspectFlags,
        uint32_t             baseMipLevel   = 0,
        uint32_t             levelCount     = 1,
        uint32_t             baseArrayLayer = 0,
        uint32_t             layerCount     = 1);

    ~ImageViewResource() override
    {
        _image->getImageViews().erase(this);
        if (_handle)
        {
            _context->getDevice().destroyImageView(_handle);
        }
    }

    vk::Format           get_format() const { return _format; }
    const ImageResource& get_image() const { return *_image; }

    // 创建2D image view，不带mipmap
    static std::unique_ptr<ImageViewResource> create2D(
        VulkanContext&       context,
        ImageResource&       image,
        vk::Format           format,
        vk::ImageAspectFlags aspectFlags = vk::ImageAspectFlagBits::eColor);

private:
    vk::Format                _format{};
    vk::ImageSubresourceRange _subresource_range{};
    ImageResource*            _image{nullptr};
};

// 你的 descriptor helper 保持不变
inline vk::DescriptorImageInfo makeCombinedImageInfo(
    vk::ImageView   view,
    vk::Sampler     sampler,
    vk::ImageLayout layout)
{
    return vk::DescriptorImageInfo{
        sampler,
        view,
        layout
    };
}

inline vk::DescriptorImageInfo makeStorageImageInfo(
    vk::ImageView   view,
    vk::ImageLayout layout)
{
    return vk::DescriptorImageInfo{
        VK_NULL_HANDLE,
        view,
        layout
    };
}
} // namespace dk::vkcore
