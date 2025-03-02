#pragma once

#include "Resource.h"

// 使用 vk::Image 封装的 Image 资源类
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
            // 通过静态转换获取底层 VkImage，再使用 VMA 销毁
            vmaDestroyImage(m_allocator, m_image, m_allocation);
        }
    }

    vk::Image getImage() const { return m_image; }

private:
    vk::Image m_image;
};