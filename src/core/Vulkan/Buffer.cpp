#include "Buffer.h"
#include <spdlog/spdlog.h>

namespace dk::vkcore {
BufferResource::BufferResource(VulkanContext& context, Builder& builder)
    : Resource(&context, nullptr)
{
    VkResult result = vmaCreateBuffer(
        context.getVmaAllocator(),
        reinterpret_cast<const VkBufferCreateInfo*>(&builder.getCreateInfo()),
        &builder.getAllocationCreateInfo(),
        reinterpret_cast<VkBuffer*>(&_handle),
        &_allocation,
        &_allocation_info
    );

    if (result != VK_SUCCESS)
    {
        auto resStr = to_string(static_cast<vk::Result>(result));
        spdlog::error("vmaCreateBuffer failed: {} (vkResult={})",
                      resStr, static_cast<int>(result));
    }

    VkMemoryPropertyFlags props{};
    vmaGetAllocationMemoryProperties(context.getVmaAllocator(), _allocation, &props);
    _host_visible  = (props & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0;
    _host_coherent = (props & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0;

    if (_allocation_info.pMappedData)
    {
        _data         = _allocation_info.pMappedData;
        _owns_mapping = false; // 不是我们调用 map 得到的
    }
    _size = builder.getCreateInfo().size;

}

// 下面这些函数保持不变
void BufferResource::update(const void* src_data, vk::DeviceSize size, vk::DeviceSize offset)
{
    uint8_t* dst = static_cast<uint8_t*>(map()) + offset;

    std::memcpy(dst, src_data, size);

    if (!_host_coherent)
    {
        VkDeviceSize start = offset & ~(_atom_size - 1);
        VkDeviceSize end   = (offset + size + _atom_size - 1) & ~(_atom_size - 1);
        vmaFlushAllocation(_context->getVmaAllocator(), _allocation, start, end - start);
    }
}

BufferResource::~BufferResource()
{
    if (_handle)
    {
        if (_owns_mapping)
        {
            vmaUnmapMemory(_context->getVmaAllocator(), _allocation);
        }
        // 通过静态转换获取底层 VkBuffer，再使用 VMA 销毁
        vmaDestroyBuffer(_context->getVmaAllocator(), _handle, _allocation);
    }
}

void* BufferResource::map()
{
    if (!_host_visible)
    {
        throw std::runtime_error("Buffer is not host-visible and cannot be mapped.");
    }

    if (_data)
    {
        return _data;
    }

    VkResult r = vmaMapMemory(_context->getVmaAllocator(), _allocation, &_data);
    if (r != VK_SUCCESS)
    {
        throw std::runtime_error("vmaMapMemory failed");
    }
    _owns_mapping = true;
    return _data;
}

void BufferResource::unmap()
{
    if (!_data) return;

    if (_owns_mapping)
    {
        vmaUnmapMemory(_context->getVmaAllocator(), _allocation);
    }

    _data         = nullptr;
    _owns_mapping = false;
}
}
