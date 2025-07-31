#pragma once

#include <vulkan/vulkan.hpp>
#include <memory>

#include "Image.h"
#include "ImageView.h"

namespace dk::vkcore {
// ImageViewBuilder 提供链式接口配置 ImageView 的各项参数
class ImageViewBuilder
{
public:
    // 构造时需要传入所属设备和要创建视图的 vk::Image
    ImageViewBuilder(ImageResource& image)
        : _image(&image)
    {
        _create_info.subresourceRange.setAspectMask(vk::ImageAspectFlagBits::eColor)
                    .setBaseMipLevel(0)
                    .setLevelCount(1)
                    .setBaseArrayLayer(0)
                    .setLayerCount(1);

        _create_info.setFormat(vk::Format::eR8G8B8A8Unorm);
        _create_info.setImage(_image->getHandle());
        _create_info.setViewType(vk::ImageViewType::e2D);
    }

    // 设置视图类型（如 1D、2D、3D 或 Cube 等）
    ImageViewBuilder& setViewType(vk::ImageViewType type)
    {
        _create_info.viewType = type;
        return *this;
    }

    // 设置格式，通常与底层 image 格式保持一致
    ImageViewBuilder& setFormat(vk::Format format)
    {
        _create_info.format = format;
        return *this;
    }

    // 设置需要描述的子资源的 aspect（例如颜色、深度或模板）
    ImageViewBuilder& setAspectFlags(vk::ImageAspectFlags flags)
    {
        _create_info.subresourceRange.aspectMask = flags;
        return *this;
    }

    // 设置 mipmap 层级范围
    ImageViewBuilder& setMipLevels(uint32_t baseMipLevel, uint32_t levelCount)
    {
        _create_info.subresourceRange.baseMipLevel = baseMipLevel;
        _create_info.subresourceRange.levelCount   = levelCount;
        return *this;
    }

    // 设置数组层级范围（例如 CubeMap 可能有 6 个层）
    ImageViewBuilder& setArrayLayers(uint32_t baseArrayLayer, uint32_t layerCount)
    {
        _create_info.subresourceRange.baseArrayLayer = baseArrayLayer;
        _create_info.subresourceRange.layerCount     = layerCount;
        return *this;
    }

    ImageViewResource build(VulkanContext& context)
    {
        return {context, *this};
    }

    ImageResource&          getImage() { return *_image; }
    vk::ImageViewCreateInfo getImageViewCreateInfo() { return _create_info; }

private:
    ImageResource*          _image;
    vk::ImageViewCreateInfo _create_info;
};
}
