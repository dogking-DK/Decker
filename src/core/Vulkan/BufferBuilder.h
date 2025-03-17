#pragma once
#include "Buffer.h"

namespace dk::vkcore {
// BufferResource �� Builder ��
class BufferBuilder
{
public:
    // ����ʱ���� VMA ������
    BufferBuilder(VmaAllocator allocator)
        : m_allocator(allocator), m_size(0), m_memoryUsage(VMA_MEMORY_USAGE_GPU_ONLY)
    {
    }

    // ���� buffer ��С����ʽ�ӿڣ�
    BufferBuilder& setSize(vk::DeviceSize size)
    {
        m_size = size;
        return *this;
    }

    // ���� buffer ʹ�ñ�־
    BufferBuilder& setUsage(vk::BufferUsageFlags usage)
    {
        m_usage = usage;
        return *this;
    }

    // �����ڴ�ʹ�����ͣ�VMA �ڴ����ͣ�
    BufferBuilder& setMemoryUsage(VmaMemoryUsage memoryUsage)
    {
        m_memoryUsage = memoryUsage;
        return *this;
    }

    // δ���������Ӹ����������繲��ģʽ��������������

    // ���� BufferResource ����
    std::unique_ptr<BufferResource> build()
    {
        vk::BufferCreateInfo bufferInfo{};
        bufferInfo.size        = m_size;
        bufferInfo.usage       = m_usage;
        bufferInfo.sharingMode = vk::SharingMode::eExclusive;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = m_memoryUsage;

        // �� vk::BufferCreateInfo ת��Ϊ�ײ� VkBufferCreateInfo
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
