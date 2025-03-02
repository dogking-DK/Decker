#pragma once
#include "Image.h"

// ImageResource �� Builder ��
class ImageBuilder
{
public:
    ImageBuilder(VmaAllocator allocator)
        : m_allocator(allocator),
        m_format(vk::Format::eUndefined),
        m_usage(vk::ImageUsageFlagBits::eSampled),
        m_tiling(vk::ImageTiling::eOptimal),
        m_imageType(vk::ImageType::e2D),
        m_extent{ 0, 0, 1 }
    {
    }

    // ���� image ��ʽ
    ImageBuilder& setFormat(vk::Format format)
    {
        m_format = format;
        return *this;
    }

    // ���� image ʹ�ñ�־
    ImageBuilder& setUsage(vk::ImageUsageFlags usage)
    {
        m_usage = usage;
        return *this;
    }

    // ���� image �ߴ�
    ImageBuilder& setExtent(vk::Extent3D extent)
    {
        m_extent = extent;
        return *this;
    }

    // ���� tiling ��ʽ
    ImageBuilder& setTiling(vk::ImageTiling tiling)
    {
        m_tiling = tiling;
        return *this;
    }

    // ���� image ���ͣ�1D��2D��3D��
    ImageBuilder& setImageType(vk::ImageType type)
    {
        m_imageType = type;
        return *this;
    }

    // δ������չ mipLevels��arrayLayers����ʼ���ֵ�������

    // ���� ImageResource ����
    std::unique_ptr<ImageResource> build()
    {
        vk::ImageCreateInfo imageInfo{};
        imageInfo.imageType = m_imageType;
        imageInfo.extent = m_extent;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = m_format;
        imageInfo.tiling = m_tiling;
        imageInfo.initialLayout = vk::ImageLayout::eUndefined;
        imageInfo.usage = m_usage;
        imageInfo.samples = vk::SampleCountFlagBits::e1;
        imageInfo.sharingMode = vk::SharingMode::eExclusive;

        VmaAllocationCreateInfo allocInfo{};
        // Ĭ�Ϸ��� GPU ר���ڴ棬�ɸ�����Ҫ��չ���ýӿ�
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        // �� vk::ImageCreateInfo ת��Ϊ VkImageCreateInfo
        auto vkImageInfo = static_cast<VkImageCreateInfo>(imageInfo);

        VkImage       vkImage;
        VmaAllocation allocation;
        if (vmaCreateImage(m_allocator, &vkImageInfo, &allocInfo, &vkImage, &allocation, nullptr) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create image!");
        }

        vk::Image image(vkImage);
        return std::make_unique<ImageResource>(m_allocator, image, allocation);
    }

private:
    VmaAllocator        m_allocator;
    vk::Format          m_format;
    vk::ImageUsageFlags m_usage;
    vk::ImageTiling     m_tiling;
    vk::ImageType       m_imageType;
    vk::Extent3D        m_extent;
};
