#pragma once

#include <vulkan/vulkan.hpp>
#include <memory>

#include "Context.h"
#include "Resource.hpp"

namespace dk::vkcore {
class ImageResource;
class ImageViewBuilder;
}

namespace dk::vkcore {
// ImageViewResource ��װ�� vk::ImageView����������ʱ�Զ�����
class ImageViewResource : Resource<vk::ImageView, vk::ObjectType::eImageView>
{
public:
    // ����ʱ���봴�� ImageView ���豸�ʹ����õ� vk::ImageView ����
    ImageViewResource(VulkanContext* context, vk::ImageView image_view, ImageResource* image)
        : Resource(context, image_view), _image(image)
    {
    }
    ImageViewResource(VulkanContext& context, ImageViewBuilder& builder);

    ~ImageViewResource() override
    {
        if (_handle)
        {
            _context->getDevice().destroyImageView(_handle);
        }
    }

    vk::Format           get_format() const;
    ImageResource const& get_image() const;

private:
    ImageResource* _image{nullptr};
};
}
