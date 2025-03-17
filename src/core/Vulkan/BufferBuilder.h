#pragma once
#include "Buffer.h"

namespace dk::vkcore {
// BufferResource 的 Builder 类
class BufferBuilder
{
public:
    // 构造时传入 VMA 分配器
    BufferBuilder(VmaAllocator allocator)
        : m_allocator(allocator), m_size(0), m_memoryUsage(VMA_MEMORY_USAGE_GPU_ONLY)
    {
    }

    // 设置 buffer 大小（链式接口）
    BufferBuilder& setSize(vk::DeviceSize size)
    {
        m_size = size;
        return *this;
    }

    // 设置 buffer 使用标志
    BufferBuilder& setUsage(vk::BufferUsageFlags usage)
    {
        m_usage = usage;
        return *this;
    }

    // 设置内存使用类型（VMA 内存类型）
    BufferBuilder& setMemoryUsage(VmaMemoryUsage memoryUsage)
    {
        m_memoryUsage = memoryUsage;
        return *this;
    }

    // 未来可以增加更多的配置项，如共享模式、队列族索引等

    // 创建 BufferResource 对象
    std::unique_ptr<BufferResource> build()
    {
        vk::BufferCreateInfo bufferInfo{};
        bufferInfo.size        = m_size;
        bufferInfo.usage       = m_usage;
        bufferInfo.sharingMode = vk::SharingMode::eExclusive;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = m_memoryUsage;

        // 将 vk::BufferCreateInfo 转换为底层 VkBufferCreateInfo
        auto vkBufferInfo = static_cast<VkBufferCreateInfo>(bufferInfo);

        VkBuffer      vkBuffer;
        VmaAllocation allocation;
        if (vmaCreateBuffer(m_allocator, &vkBufferInfo, &allocInfo, &vkBuffer, &allocation, nullptr) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create buffer!");
        }

        vk::Buffer buffer(vkBuffer);
        return std::make_unique<BufferResource>(m_allocator, buffer, allocation);
    }

private:
    VmaAllocator         m_allocator;
    vk::DeviceSize       m_size;
    vk::BufferUsageFlags m_usage{vk::BufferUsageFlagBits::eStorageBuffer };
    VmaMemoryUsage       m_memoryUsage;
};
}
