#include "Buffer.h"

#include <spdlog/spdlog.h>

#include "BufferBuilder.h"

namespace dk::vkcore {
BufferResource::BufferResource(VulkanContext& context, BufferBuilder& builder)
    : Resource(&context, nullptr)
{
    //_allocation_create_info = builder.getAllocationCreateInfo(); // 储存分配的创建信息
    VkResult result = vmaCreateBuffer(context.getVmaAllocator(),
                                      reinterpret_cast<const VkBufferCreateInfo*>(&builder.getCreateInfo()),
                                      &builder.getAllocationCreateInfo(),
                                      reinterpret_cast<VkBuffer*>(&_handle),
                                      &_allocation,
                                      &_allocation_info);

    if (result != VK_SUCCESS)
    {
        //fmt::print("image create fail\n");
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
        _owns_mapping = false;             // 不是我们调用 map 得到的
    }
}

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

// map() 函数的逻辑也需要调整
void* BufferResource::map()
{
    if (!_host_visible)
    {
        throw std::runtime_error("Buffer is not host-visible and cannot be mapped.");
    }

    // 如果已经有映射指针（无论是 VMA 持久映射的还是我们自己映射的），直接返回。
    // 这使得重复调用 map() 几乎没有开销。
    if (_data)
    {
        return _data;
    }

    // 如果没有，我们才执行一次真正的映射操作。
    VkResult r = vmaMapMemory(_context->getVmaAllocator(), _allocation, &_data);
    if (r != VK_SUCCESS)
    {
        throw std::runtime_error("vmaMapMemory failed");
    }
    _owns_mapping = true; // 我们调用了 map, 所以我们“拥有”这个映射
    return _data;
}

// unmap() 函数的逻辑
void BufferResource::unmap()
{
    // 如果没有映射指针，什么都不做
    if (!_data) return;

    // **只有当我们自己调用 map() 时才 unmap**。
    // 如果是 VMA 创建时就提供的持久映射，我们不应该 unmap。
    if (_owns_mapping)
    {
        vmaUnmapMemory(_context->getVmaAllocator(), _allocation);
    }

    // 总是重置内部状态
    _data         = nullptr;
    _owns_mapping = false;
}
}
