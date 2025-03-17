#pragma once

#include "Resource.hpp"

namespace dk::vkcore {
// ʹ�� vk::Image ��װ�� Image ��Դ��
class ImageResource : public Resource<vk::Image, vk::ObjectType::eImage>
{
public:


    ImageResource(VmaAllocator allocator, vk::Image image, VmaAllocation allocation)
        : m_image(image)
    {
        m_allocator  = allocator;
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

    // ������Ӧ�� ImageView����������ɫ������Ϊ����������
    // �����߸������ٷ��ص� vk::ImageView
    vk::ImageView createImageView(vk::Device device, vk::Format format, vk::ImageAspectFlags aspectFlags) const
    {
        vk::ImageViewCreateInfo viewInfo{};
        viewInfo.image                           = m_image;
        viewInfo.viewType                        = vk::ImageViewType::e2D;
        viewInfo.format                          = format;
        viewInfo.subresourceRange.aspectMask     = aspectFlags;
        viewInfo.subresourceRange.baseMipLevel   = 0;
        viewInfo.subresourceRange.levelCount     = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount     = 1;
        
        return device.createImageView(viewInfo);
    }

    vk::Image getImage() const { return m_image; }

private:
    vk::Image m_image;
    // ������Դ���� VMA ���������ڴ������
    VmaAllocator  m_allocator = VK_NULL_HANDLE;
    VmaAllocation m_allocation = VK_NULL_HANDLE;
};
}
