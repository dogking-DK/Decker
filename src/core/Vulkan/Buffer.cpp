#include "Buffer.h"
#include "BufferBuilder.h"

namespace dk::vkcore {
    BufferResource::BufferResource(VulkanContext& context, BufferBuilder& builder)
        : Resource(&context, nullptr)
{
        _allocation_create_info = builder.getAllocationCreateInfo(); // 储存分配的创建信息

        VkResult result = vmaCreateBuffer(context.getVmaAllocator(),
            reinterpret_cast<const VkBufferCreateInfo*>(&builder.getCreateInfo()),
            &_allocation_create_info,
            reinterpret_cast<VkBuffer*>(&_handle),
            &_allocation,
            nullptr);


        if (result != VK_SUCCESS)
        {
            fmt::print("image create fail\n");
        }
}

void BufferResource::updateData(const void* srcData, vk::DeviceSize size, vk::DeviceSize offset)
{
    void* mapped = map();
    std::memcpy(static_cast<char*>(mapped) + offset, srcData, size);
    unmap();
}

}
