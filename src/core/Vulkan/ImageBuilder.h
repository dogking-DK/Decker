#pragma once
#include "Image.h"
#include "BaseBuilder.h"

namespace dk::vkcore {
// ImageResource 的 Builder 类
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


    // 设置 image 格式
    ImageBuilder& setFormat(const vk::Format format)
    {
        _create_info.format = format;
        return *this;
    }

    // 设置 image 使用标志
    ImageBuilder& setUsage(const vk::ImageUsageFlags usage)
    {
        _create_info.usage = usage;
        return *this;
    }

    // 设置 image 尺寸
    ImageBuilder& setExtent(const vk::Extent3D extent)
    {
        _create_info.extent = extent;
        return *this;
    }

    // 设置 tiling 方式
    ImageBuilder& setTiling(const vk::ImageTiling tiling)
    {
        _create_info.tiling = tiling;
        return *this;
    }

    // 设置 image 类型（1D、2D、3D）
    ImageBuilder& setImageType(const vk::ImageType type)
    {
        _create_info.imageType = type;
        return *this;
    }

    // 未来可扩展 mipLevels、arrayLayers、初始布局等配置项

    VmaAllocationCreateInfo const& getVmaCreateInfo() const { return _vma_create_info; }
    vk::ImageCreateInfo const& getCreateInfo() const { return _create_info; }

    // 创建 ImageResource 对象
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
        // 默认分配 GPU 专用内存，可根据需要扩展配置接口
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        // 将 vk::ImageCreateInfo 转换为 VkImageCreateInfo
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
