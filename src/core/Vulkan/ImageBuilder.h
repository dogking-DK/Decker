#pragma once
#include "Image.h"

// ImageResource 的 Builder 类
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

    // 设置 image 格式
    ImageBuilder& setFormat(vk::Format format)
    {
        m_format = format;
        return *this;
    }

    // 设置 image 使用标志
    ImageBuilder& setUsage(vk::ImageUsageFlags usage)
    {
        m_usage = usage;
        return *this;
    }

    // 设置 image 尺寸
    ImageBuilder& setExtent(vk::Extent3D extent)
    {
        m_extent = extent;
        return *this;
    }

    // 设置 tiling 方式
    ImageBuilder& setTiling(vk::ImageTiling tiling)
    {
        m_tiling = tiling;
        return *this;
    }

    // 设置 image 类型（1D、2D、3D）
    ImageBuilder& setImageType(vk::ImageType type)
    {
        m_imageType = type;
        return *this;
    }

    // 未来可扩展 mipLevels、arrayLayers、初始布局等配置项

    // 创建 ImageResource 对象
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
        // 默认分配 GPU 专用内存，可根据需要扩展配置接口
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        // 将 vk::ImageCreateInfo 转换为 VkImageCreateInfo
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
