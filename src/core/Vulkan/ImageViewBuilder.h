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
    ImageViewBuilder(vk::Device device, ImageResource& image)
        : m_device(device), m_image(image),
          m_viewType(vk::ImageViewType::e2D),
          m_format(vk::Format::eUndefined),
          m_aspectFlags(vk::ImageAspectFlagBits::eColor),
          m_baseMipLevel(0), m_levelCount(1),
          m_baseArrayLayer(0), m_layerCount(1)
    {
    }

    // 设置视图类型（如 1D、2D、3D 或 Cube 等）
    ImageViewBuilder& setViewType(vk::ImageViewType type)
    {
        m_viewType = type;
        return *this;
    }

    // 设置格式，通常与底层 image 格式保持一致
    ImageViewBuilder& setFormat(vk::Format format)
    {
        m_format = format;
        return *this;
    }

    // 设置需要描述的子资源的 aspect（例如颜色、深度或模板）
    ImageViewBuilder& setAspectFlags(vk::ImageAspectFlags flags)
    {
        m_aspectFlags = flags;
        return *this;
    }

    // 设置 mipmap 层级范围
    ImageViewBuilder& setMipLevels(uint32_t baseMipLevel, uint32_t levelCount)
    {
        m_baseMipLevel = baseMipLevel;
        m_levelCount   = levelCount;
        return *this;
    }

    // 设置数组层级范围（例如 CubeMap 可能有 6 个层）
    ImageViewBuilder& setArrayLayers(uint32_t baseArrayLayer, uint32_t layerCount)
    {
        m_baseArrayLayer = baseArrayLayer;
        m_layerCount     = layerCount;
        return *this;
    }

    // 调用 build() 创建 ImageViewResource 对象
    std::unique_ptr<ImageViewResource> build()
    {
        vk::ImageViewCreateInfo viewInfo{};
        viewInfo.image                           = m_image;
        viewInfo.viewType                        = m_viewType;
        viewInfo.format                          = m_format;
        viewInfo.subresourceRange.aspectMask     = m_aspectFlags;
        viewInfo.subresourceRange.baseMipLevel   = m_baseMipLevel;
        viewInfo.subresourceRange.levelCount     = m_levelCount;
        viewInfo.subresourceRange.baseArrayLayer = m_baseArrayLayer;
        viewInfo.subresourceRange.layerCount     = m_layerCount;

        vk::ImageView imageView = m_device.createImageView(viewInfo);
        return std::make_unique<ImageViewResource>(m_device, imageView);
    }

private:
    vk::Device           m_device;
    ImageResource*       m_image;
    vk::ImageViewType    m_viewType;
    vk::Format           m_format;
    vk::ImageAspectFlags m_aspectFlags;
    uint32_t             m_baseMipLevel;
    uint32_t             m_levelCount;
    uint32_t             m_baseArrayLayer;
    uint32_t             m_layerCount;
};
}
