#pragma once
#include "Resource.h"

// ʹ�� vk::Buffer ��װ�� Buffer ��Դ��
class BufferResource : public Resource
{
public:
    BufferResource(VmaAllocator allocator, vk::Buffer buffer, VmaAllocation allocation)
        : m_buffer(buffer)
    {
        m_allocator = allocator;
        m_allocation = allocation;
    }

    ~BufferResource() override
    {
        if (m_buffer)
        {
            // ͨ����̬ת����ȡ�ײ� VkBuffer����ʹ�� VMA ����
            vmaDestroyBuffer(m_allocator, m_buffer, m_allocation);
        }
    }

    vk::Buffer getBuffer() const { return m_buffer; }

private:
    vk::Buffer m_buffer;
};