#pragma once
#include "Resource.h"

// ʹ�� vk::Buffer ��װ�� Buffer ��Դ��
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
            // ͨ����̬ת����ȡ�ײ� VkBuffer����ʹ�� VMA ����
            vmaDestroyBuffer(m_allocator, m_buffer, m_allocation);
        }
    }


    // ӳ���ڴ棬���� CPU �ɷ��ʵ�ָ��
    void* map()
    {
        void* data;
        if (vmaMapMemory(m_allocator, m_allocation, &data) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to map buffer memory!");
        }
        return data;
    }

    // ȡ��ӳ��
    void unmap()
    {
        vmaUnmapMemory(m_allocator, m_allocation);
    }

    // �������ݵ� buffer�������� HOST_VISIBLE �ڴ棩
    void updateData(const void* srcData, vk::DeviceSize size, vk::DeviceSize offset = 0)
    {
        void* mapped = map();
        std::memcpy(static_cast<char*>(mapped) + offset, srcData, size);
        unmap();
    }

    // ͨ����������������� GPU ��ִ�д� srcBuffer �������ݵ��� buffer �Ĳ���
    // ע�⣺�ú�������¼����������ύִ��
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
