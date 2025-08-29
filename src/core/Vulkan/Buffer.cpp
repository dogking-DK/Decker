#include "Buffer.h"
#include "BufferBuilder.h"

namespace dk::vkcore {
BufferResource::BufferResource(VulkanContext& context, BufferBuilder& builder)
    : Resource(&context, nullptr)
{
    //_allocation_create_info = builder.getAllocationCreateInfo(); // 储存分配的创建信息
    VmaAllocationInfo info{};
    VkResult          result = vmaCreateBuffer(context.getVmaAllocator(),
                                      reinterpret_cast<const VkBufferCreateInfo*>(&builder.getCreateInfo()),
                                      &builder.getAllocationCreateInfo(),
                                      reinterpret_cast<VkBuffer*>(&_handle),
                                      &_allocation,
                                      &info);

    if (result != VK_SUCCESS)
    {
        fmt::print("image create fail\n");
    }

    VkMemoryPropertyFlags props{};
    vmaGetAllocationMemoryProperties(context.getVmaAllocator(), _allocation, &props);
    _host_visible  = (props & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0;
    _host_coherent = (props & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0;

    if (info.pMappedData)
    {
        _data         = info.pMappedData;
        _owns_mapping = false;             // 不是我们调用 map 得到的
    }
}

void BufferResource::update(const void* src_data, vk::DeviceSize size, vk::DeviceSize offset)
{
    uint8_t* dst = static_cast<uint8_t*>(map()) + offset;
    std::memcpy(dst, src_data, size);

    if (!_host_coherent)
    {
        // 按 nonCoherentAtomSize 对齐 flush 范围
        VkDeviceSize start = offset & ~(_atom_size - 1);
        VkDeviceSize end   = (offset + size + _atom_size - 1) & ~(_atom_size - 1);
        vmaFlushAllocation(_context->getVmaAllocator(), _allocation, start, end - start);
    }
    unmap();
}
}
