#pragma once

#include <vulkan/vulkan.hpp>
#include <memory>

// ImageViewResource ��װ�� vk::ImageView����������ʱ�Զ�����
class ImageViewResource
{
public:
    // ����ʱ���봴�� ImageView ���豸�ʹ����õ� vk::ImageView ����
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
    vk::Device    m_device;       // �������� ImageView
    vk::ImageView m_imageView; // ��װ�� ImageView ����
};
