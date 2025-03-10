#pragma once

#include <vulkan/vulkan.hpp>
#include <memory>

// ImageViewResource 封装了 vk::ImageView，并在析构时自动销毁
class ImageViewResource
{
public:
    // 构造时传入创建 ImageView 的设备和创建好的 vk::ImageView 对象
    ImageViewResource(vk::Device device, vk::ImageView imageView)
        : m_device(device), m_imageView(imageView)
    {
    }

    ~ImageViewResource()
    {
        if (m_imageView)
        {
            m_device.destroyImageView(m_imageView);
        }
    }

    vk::ImageView get() const { return m_imageView; }

private:
    vk::Device    m_device;       // 用于销毁 ImageView
    vk::ImageView m_imageView; // 封装的 ImageView 对象
};
