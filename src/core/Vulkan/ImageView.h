#pragma once

#include "vk_types.h"
#include <memory>

#include "Context.h"
#include "Image.h"
#include "Resource.hpp"

namespace dk::vkcore {
class ImageResource;
class ImageViewBuilder;
}

namespace dk::vkcore {
// ImageViewResource 封装了 vk::ImageView，并在析构时自动销毁
class ImageViewResource : public Resource<vk::ImageView, vk::ObjectType::eImageView>
{
public:
    ImageViewResource(VulkanContext& context, ImageViewBuilder& builder);

    ~ImageViewResource() override
    {
        _image->getImageViews().erase(this);
        if (_handle)
        {
            _context->getDevice().destroyImageView(_handle);
        }
    }

    vk::Format           get_format() const;
    const ImageResource& get_image() const;

private:
    vk::Format                _format;
    vk::ImageSubresourceRange _subresource_range;
    ImageResource*            _image{nullptr};
};

inline vk::DescriptorImageInfo makeCombinedImageInfo(
    vk::ImageView view,
    vk::Sampler              sampler,
    vk::ImageLayout          layout)
{
    return vk::DescriptorImageInfo{
        sampler,
        view,   // 你基类 RGResource<vk::ImageView,...> 应该有 getHandle()
        layout
    };
}

inline vk::DescriptorImageInfo makeStorageImageInfo(
    vk::ImageView view,
    vk::ImageLayout          layout)
{
    return vk::DescriptorImageInfo{
        VK_NULL_HANDLE,     // storage image 不需要 sampler
        view,
        layout
    };
}
}
