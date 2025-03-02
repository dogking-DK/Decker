#pragma once

#include "Resource.h"

// ʹ�� vk::Image ��װ�� Image ��Դ��
class ImageResource : public Resource
{
public:
    ImageResource(VmaAllocator allocator, vk::Image image, VmaAllocation allocation)
        : m_image(image)
    {
        m_allocator = allocator;
        m_allocation = allocation;
    }

    ~ImageResource() override
    {
        if (m_image)
        {
            // ͨ����̬ת����ȡ�ײ� VkImage����ʹ�� VMA ����
            vmaDestroyImage(m_allocator, m_image, m_allocation);
        }
    }

    vk::Image getImage() const { return m_image; }

private:
    vk::Image m_image;
};