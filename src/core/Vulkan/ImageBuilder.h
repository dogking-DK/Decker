#pragma once
#include "Image.h"
#include "BaseBuilder.h"

namespace dk::vkcore {
// ImageResource 的 Builder 类
class ImageBuilder : public BaseBuilder<ImageBuilder, vk::ImageCreateInfo>
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

    ImageResource build(VulkanContext& context)
    {
        return { context, *this };
    }

private:
    friend ImageResource;
};
}
