#pragma once
#include "Resource.h"

// 使用 vk::Buffer 封装的 Buffer 资源类
class BufferResource : public Resource
{
public:
    BufferResource(VmaAllocator allocator, vk::Buffer buffer, VmaAllocation allocation)
        : m_buffer(buffer)
    {
        m_allocator  = allocator;
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


    // 映射内存，返回 CPU 可访问的指针
    void* map()
    {
        void* data;
        if (vmaMapMemory(m_allocator, m_allocation, &data) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to map buffer memory!");
        }
        return data;
    }

    // 取消映射
    void unmap()
    {
        vmaUnmapMemory(m_allocator, m_allocation);
    }

    // 更新数据到 buffer（适用于 HOST_VISIBLE 内存）
    void updateData(const void* srcData, vk::DeviceSize size, vk::DeviceSize offset = 0)
    {
        void* mapped = map();
        std::memcpy(static_cast<char*>(mapped) + offset, srcData, size);
        unmap();
    }

    // 通过传入命令缓冲区，在 GPU 上执行从 srcBuffer 拷贝数据到本 buffer 的操作
    // 注意：该函数仅记录拷贝命令，不提交执行
    void copyFrom(vk::CommandBuffer commandBuffer, const BufferResource& srcBuffer, vk::DeviceSize size)
    {
        vk::BufferCopy copyRegion;
        copyRegion.srcOffset = 0;
        copyRegion.dstOffset = 0;
        copyRegion.size      = size;
        commandBuffer.copyBuffer(srcBuffer.getBuffer(), this->getBuffer(), copyRegion);
    }

    vk::Buffer getBuffer() const { return m_buffer; }

private:
    vk::Buffer m_buffer;
};
