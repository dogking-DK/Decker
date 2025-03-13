#pragma once

#include <vulkan/vulkan.hpp>
#include <memory>

#include "ImageView.h"

// ImageViewBuilder �ṩ��ʽ�ӿ����� ImageView �ĸ������
class ImageViewBuilder
{
public:
    // ����ʱ��Ҫ���������豸��Ҫ������ͼ�� vk::Image
    ImageViewBuilder(vk::Device device, vk::Image image)
        : m_device(device), m_image(image),
          m_viewType(vk::ImageViewType::e2D),
          m_format(vk::Format::eUndefined),
          m_aspectFlags(vk::ImageAspectFlagBits::eColor),
          m_baseMipLevel(0), m_levelCount(1),
          m_baseArrayLayer(0), m_layerCount(1)
    {
    }

    // ������ͼ���ͣ��� 1D��2D��3D �� Cube �ȣ�
    ImageViewBuilder& setViewType(vk::ImageViewType type)
    {
        m_viewType = type;
        return *this;
    }

    // ���ø�ʽ��ͨ����ײ� image ��ʽ����һ��
    ImageViewBuilder& setFormat(vk::Format format)
    {
        m_format = format;
        return *this;
    }

    // ������Ҫ����������Դ�� aspect��������ɫ����Ȼ�ģ�壩
    ImageViewBuilder& setAspectFlags(vk::ImageAspectFlags flags)
    {
        m_aspectFlags = flags;
        return *this;
    }

    // ���� mipmap �㼶��Χ
    ImageViewBuilder& setMipLevels(uint32_t baseMipLevel, uint32_t levelCount)
    {
        m_baseMipLevel = baseMipLevel;
        m_levelCount   = levelCount;
        return *this;
    }

    // ��������㼶��Χ������ CubeMap ������ 6 ���㣩
    ImageViewBuilder& setArrayLayers(uint32_t baseArrayLayer, uint32_t layerCount)
    {
        m_baseArrayLayer = baseArrayLayer;
        m_layerCount     = layerCount;
        return *this;
    }

    // ���� build() ���� ImageViewResource ����
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
    vk::Image            m_image;
    vk::ImageViewType    m_viewType;
    vk::Format           m_format;
    vk::ImageAspectFlags m_aspectFlags;
    uint32_t             m_baseMipLevel;
    uint32_t             m_levelCount;
    uint32_t             m_baseArrayLayer;
    uint32_t             m_layerCount;
};
