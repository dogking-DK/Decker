#include "Buffer.h"
#include "BufferBuilder.h"

namespace dk::vkcore {
    BufferResource::BufferResource(VulkanContext& context, BufferBuilder& builder)
        : Resource(&context, nullptr)
{

}

void BufferResource::updateData(const void* srcData, vk::DeviceSize size, vk::DeviceSize offset)
{
    void* mapped = map();
    std::memcpy(static_cast<char*>(mapped) + offset, srcData, size);
    unmap();
}

void BufferResource::copyFrom(vk::CommandBuffer commandBuffer, const BufferResource& srcBuffer, vk::DeviceSize size)
{
    vk::BufferCopy copyRegion;
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size      = size;
    commandBuffer.copyBuffer(srcBuffer.getBuffer(), this->getBuffer(), copyRegion);
}
}
