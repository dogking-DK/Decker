#pragma once

#include <vulkan/vulkan.hpp>
#include <memory>

#include "Image.h"
#include "ImageView.h"

namespace dk::vkcore {
// ImageViewBuilder �ṩ��ʽ�ӿ����� ImageView �ĸ������
class ImageViewBuilder
{
public:
    // ����ʱ��Ҫ���������豸��Ҫ������ͼ�� vk::Image
    ImageViewBuilder(ImageResource& image)
        : _image(&image)
    {
        _create_info.subresourceRange.setAspectMask(vk::ImageAspectFlagBits::eColor)
                    .setBaseMipLevel(0)
                    .setLevelCount(1)
                    .setBaseArrayLayer(0)
                    .setLevelCount(1);

        _create_info.setFormat(vk::Format::eR8G8B8A8Unorm);
        _create_info.setImage(_image->getHandle());
        _create_info.setViewType(vk::ImageViewType::e2D);
    }

    // ������ͼ���ͣ��� 1D��2D��3D �� Cube �ȣ�
    ImageViewBuilder& setViewType(vk::ImageViewType type)
    {
        _create_info.viewType = type;
        return *this;
    }

    // ���ø�ʽ��ͨ����ײ� image ��ʽ����һ��
    ImageViewBuilder& setFormat(vk::Format format)
    {
        _create_info.format = format;
        return *this;
    }

    // ������Ҫ����������Դ�� aspect��������ɫ����Ȼ�ģ�壩
    ImageViewBuilder& setAspectFlags(vk::ImageAspectFlags flags)
    {
        _create_info.subresourceRange.aspectMask = flags;
        return *this;
    }

    // ���� mipmap �㼶��Χ
    ImageViewBuilder& setMipLevels(uint32_t baseMipLevel, uint32_t levelCount)
    {
        _create_info.subresourceRange.baseMipLevel = baseMipLevel;
        _create_info.subresourceRange.levelCount   = levelCount;
        return *this;
    }

    // ��������㼶��Χ������ CubeMap ������ 6 ���㣩
    ImageViewBuilder& setArrayLayers(uint32_t baseArrayLayer, uint32_t layerCount)
    {
        _create_info.subresourceRange.baseArrayLayer = baseArrayLayer;
        _create_info.subresourceRange.layerCount     = layerCount;
        return *this;
    }

    ImageViewResource build(VulkanContext& context)
    {
        return { context, *this};
    }

    ImageResource& getImage() { return *_image; }

private:
    ImageResource*          _image;
    vk::ImageViewCreateInfo _create_info;
};
}
