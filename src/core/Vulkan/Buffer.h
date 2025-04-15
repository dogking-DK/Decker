#pragma once
#include "Resource.hpp"

namespace dk::vkcore {
class BufferBuilder;
class VulkanContext;
}

namespace dk::vkcore {
// 使用 vk::Buffer 封装的 Buffer 资源类
class BufferResource : public Resource<vk::Buffer, vk::ObjectType::eBuffer>
{
public:

    BufferResource(VulkanContext& context, BufferBuilder& builder);
    ~BufferResource() override
    {
        if (_handle)
        {
            // 通过静态转换获取底层 VkBuffer，再使用 VMA 销毁
            vmaDestroyBuffer(_context->getVmaAllocator(), _handle, _allocation);
        }
    }


    // 映射内存，返回 CPU 可访问的指针
    void* map()
    {
        void* data;
        if (vmaMapMemory(_context->getVmaAllocator(), _allocation, &data) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to map buffer memory!");
        }
        return data;
    }

    // 取消映射
    void unmap()
    {
        vmaUnmapMemory(_context->getVmaAllocator(), _allocation);
    }

    // 更新数据到 buffer（适用于 HOST_VISIBLE 内存）
    void updateData(const void* srcData, vk::DeviceSize size, vk::DeviceSize offset = 0);

private:
    VmaAllocationCreateInfo _allocation_create_info = {};
    VmaAllocation           _allocation = VK_NULL_HANDLE;
};
}
