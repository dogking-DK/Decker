#pragma once

#include <vulkan/vulkan.hpp>
#include <memory>

#include "Context.h"
#include "Resource.hpp"

namespace dk::vkcore {
// ImageViewResource 封装了 vk::ImageView，并在析构时自动销毁
class ImageViewResource : Resource<vk::ImageView, vk::ObjectType::eImageView>
{
public:
    // 构造时传入创建 ImageView 的设备和创建好的 vk::ImageView 对象
    ImageViewResource(VulkanContext* context, vk::ImageView image_view, ImageResource* image)
        : Resource(_context, image_view), _image(image)
    {
    }

    ~ImageViewResource() override
    {
        if (_handle)
        {
            _context->getDevice().destroyImageView(_handle);
        }
    }

    vk::Format                 get_format() const;
    vkb::core::HPPImage const& get_image() const;

private:
    ImageResource* _image;
};
}
