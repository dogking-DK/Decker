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
    ImageResource const& get_image() const;

private:
    vk::Format                _format;
    vk::ImageSubresourceRange _subresource_range;
    ImageResource* _image{nullptr};
};
}
