#pragma once
#include "Resource.h"

// 使用 vk::Buffer 封装的 Buffer 资源类
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
            // 通过静态转换获取底层 VkBuffer，再使用 VMA 销毁
            vmaDestroyBuffer(m_allocator, m_buffer, m_allocation);
        }
    }

    vk::Buffer getBuffer() const { return m_buffer; }

private:
    vk::Buffer m_buffer;
};