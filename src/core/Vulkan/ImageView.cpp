#include "ImageView.h"

#include "ImageViewBuilder.h"

namespace dk::vkcore {
ImageViewResource::ImageViewResource(VulkanContext& context, ImageViewBuilder& builder)
    : Resource(&context, nullptr), _image(&builder.getImage())
{
    _handle = _context->getDevice().createImageView(builder.getImageViewCreateInfo());
    if (_handle == nullptr)
    {
        fmt::print(fmt::fg(fmt::color::red), "vulkan image view create fail\n");
    }
    _image->getImageViews().insert(this);
    _format = builder.getImageViewCreateInfo().format;
    _subresource_range = builder.getImageViewCreateInfo().subresourceRange;
}

vk::Format ImageViewResource::get_format() const
{
    return _format;
}

ImageResource const& ImageViewResource::get_image() const
{
    return *_image;
}
}
