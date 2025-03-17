#pragma once
#include "Image.h"

namespace dk::vkcore {
// ImageResource �� Builder ��
class ImageBuilder
{
public:
    ImageBuilder(VmaAllocator allocator)
        : _allocator(allocator),
          _format(vk::Format::eUndefined),
          _usage(vk::ImageUsageFlagBits::eSampled),
          _tiling(vk::ImageTiling::eOptimal),
        _image_type(vk::ImageType::e2D),
          _extent{0, 0, 1}
    {
    }

    // ���� image ��ʽ
    ImageBuilder& setFormat(vk::Format format)
    {
        _format = format;
        return *this;
    }

    // ���� image ʹ�ñ�־
    ImageBuilder& setUsage(vk::ImageUsageFlags usage)
    {
        _usage = usage;
        return *this;
    }

    // ���� image �ߴ�
    ImageBuilder& setExtent(vk::Extent3D extent)
    {
        _extent = extent;
        return *this;
    }

    // ���� tiling ��ʽ
    ImageBuilder& setTiling(vk::ImageTiling tiling)
    {
        _tiling = tiling;
        return *this;
    }

    // ���� image ���ͣ�1D��2D��3D��
    ImageBuilder& setImageType(vk::ImageType type)
    {
        _image_type = type;
        return *this;
    }

    // δ������չ mipLevels��arrayLayers����ʼ���ֵ�������

    // ���� ImageResource ����
    std::unique_ptr<ImageResource> build()
    {
        vk::ImageCreateInfo imageInfo{};
        imageInfo.imageType     = _image_type;
        imageInfo.extent        = _extent;
        imageInfo.mipLevels     = 1;
        imageInfo.arrayLayers   = 1;
        imageInfo.format        = _format;
        imageInfo.tiling        = _tiling;
        imageInfo.initialLayout = vk::ImageLayout::eUndefined;
        imageInfo.usage         = _usage;
        imageInfo.samples       = vk::SampleCountFlagBits::e1;
        imageInfo.sharingMode   = vk::SharingMode::eExclusive;

        VmaAllocationCreateInfo allocInfo{};
        // Ĭ�Ϸ��� GPU ר���ڴ棬�ɸ�����Ҫ��չ���ýӿ�
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        // �� vk::ImageCreateInfo ת��Ϊ VkImageCreateInfo
        auto vkImageInfo = static_cast<VkImageCreateInfo>(imageInfo);

        VkImage       vkImage;
        VmaAllocation allocation;
        if (vmaCreateImage(_allocator, &vkImageInfo, &allocInfo, &vkImage, &allocation, nullptr) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create image!");
        }

        vk::Image image(vkImage);
        return std::make_unique<ImageResource>(_allocator, image, allocation);
    }

private:
    VmaAllocator        _allocator;
    vk::ImageCreateInfo _create_info;
    vk::Format          _format;
    vk::ImageUsageFlags _usage;
    vk::ImageTiling     _tiling;
    vk::ImageType       _image_type;
    vk::Extent3D        _extent;
};
}
