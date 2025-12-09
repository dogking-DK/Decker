#pragma once
#include <tsl/robin_set.h>
#include <tsl/robin_map.h>

#include "BaseBuilder.h"
#include "Resource.hpp"

namespace dk::vkcore {
class ImageViewResource;

class ImageResource : public Resource<vk::Image, vk::ObjectType::eImage>
{
public:
    //  内部类 Builder
    class Builder : public BaseBuilder<Builder, vk::ImageCreateInfo>
    {
    public:
        Builder()
        {
            _create_info.format        = vk::Format::eUndefined;
            _create_info.usage         = vk::ImageUsageFlagBits::eSampled;
            _create_info.tiling        = vk::ImageTiling::eOptimal;
            _create_info.imageType     = vk::ImageType::e2D;
            _create_info.extent        = vk::Extent3D(0, 0, 1);
            _create_info.initialLayout = vk::ImageLayout::eUndefined;
            _create_info.samples       = vk::SampleCountFlagBits::e1;
            _create_info.sharingMode   = vk::SharingMode::eExclusive;
            _create_info.mipLevels     = 1;
            _create_info.arrayLayers   = 1;
        }

        Builder& setFormat(vk::Format format)
        {
            _create_info.format = format;
            return *this;
        }

        Builder& setUsage(vk::ImageUsageFlags usage)
        {
            _create_info.usage = usage;
            return *this;
        }

        Builder& setExtent(vk::Extent3D extent)
        {
            _create_info.extent = extent;
            return *this;
        }

        Builder& setTiling(vk::ImageTiling tiling)
        {
            _create_info.tiling = tiling;
            return *this;
        }

        Builder& setImageType(vk::ImageType type)
        {
            _create_info.imageType = type;
            return *this;
        }

        ImageResource build(VulkanContext& context)
        {
            return ImageResource(context, *this);
        }

        std::unique_ptr<ImageResource> buildUnique(VulkanContext& context)
        {
            return std::make_unique<ImageResource>(context, *this);
        }

    private:
        friend class ImageResource;
    };

    ImageResource(VulkanContext& context, Builder& builder);

    ~ImageResource() override
    {
        if (_handle)
        {
            vmaDestroyImage(_context->getVmaAllocator(), _handle, _allocation);
        }
    }

    vk::ImageView createImageView(vk::Device device, vk::Format format, vk::ImageAspectFlags aspectFlags) const;

    tsl::robin_set<ImageViewResource*>& getImageViews() { return _image_views; }

    VmaAllocation getAllocation() const { return _allocation; }

private:
    VmaAllocationCreateInfo            _allocation_create_info{};
    VmaAllocation                      _allocation = VK_NULL_HANDLE;
    VmaAllocationInfo                  _allocation_info{};
    tsl::robin_set<ImageViewResource*> _image_views;
};
}
