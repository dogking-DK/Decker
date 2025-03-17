#pragma once
#include "Image.h"
#include "BaseBuilder.h"

namespace dk::vkcore {
// ImageResource �� Builder ��
class ImageBuilder : BaseBuilder<ImageBuilder, vk::ImageCreateInfo>
{
public:
    ImageBuilder()
    {
        _create_info.format = (vk::Format::eUndefined);
        _create_info.usage = (vk::ImageUsageFlagBits::eSampled);
        _create_info.tiling = (vk::ImageTiling::eOptimal);
        _create_info.imageType = (vk::ImageType::e2D);
        _create_info.extent = vk::Extent3D(0, 0, 1);
        _create_info.initialLayout = vk::ImageLayout::eUndefined;
        _create_info.samples = vk::SampleCountFlagBits::e1;
        _create_info.sharingMode = vk::SharingMode::eExclusive;
    }


    // ���� image ��ʽ
    ImageBuilder& setFormat(const vk::Format format)
    {
        _create_info.format = format;
        return *this;
    }

    // ���� image ʹ�ñ�־
    ImageBuilder& setUsage(const vk::ImageUsageFlags usage)
    {
        _create_info.usage = usage;
        return *this;
    }

    // ���� image �ߴ�
    ImageBuilder& setExtent(const vk::Extent3D extent)
    {
        _create_info.extent = extent;
        return *this;
    }

    // ���� tiling ��ʽ
    ImageBuilder& setTiling(const vk::ImageTiling tiling)
    {
        _create_info.tiling = tiling;
        return *this;
    }

    // ���� image ���ͣ�1D��2D��3D��
    ImageBuilder& setImageType(const vk::ImageType type)
    {
        _create_info.imageType = type;
        return *this;
    }

    // δ������չ mipLevels��arrayLayers����ʼ���ֵ�������

    VmaAllocationCreateInfo const& getVmaCreateInfo() const { return _vma_create_info; }
    vk::ImageCreateInfo const& getCreateInfo() const { return _create_info; }

    // ���� ImageResource ����
    std::unique_ptr<ImageResource> build()
    {
        vk::ImageCreateInfo imageInfo{};
        _create_info.imageType = _image_type;
        _create_info.extent = _extent;
        _create_info.mipLevels = 1;
        _create_info.arrayLayers = 1;
        _create_info.format = _format;
        _create_info.tiling = _tiling;
        _create_info.initialLayout = vk::ImageLayout::eUndefined;
        _create_info.samples = vk::SampleCountFlagBits::e1;
        _create_info.sharingMode   = vk::SharingMode::eExclusive;

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

};
}
