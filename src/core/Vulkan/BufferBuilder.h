#pragma once
#include "Buffer.h"
#include "BaseBuilder.h"
namespace dk::vkcore {
// BufferResource 的 Builder 类
class BufferBuilder : BaseBuilder<BufferBuilder, vk::BufferCreateInfo>
{
public:
    BufferBuilder()
    {

    }

    // 设置 buffer 大小（链式接口）
    BufferBuilder& setSize(vk::DeviceSize size)
    {
        _create_info.size = size;
        return *this;
    }

    // 设置 buffer 使用标志
    BufferBuilder& setUsage(vk::BufferUsageFlags usage)
    {
        _create_info.usage = usage;
        return *this;
    }

    // 未来可以增加更多的配置项，如共享模式、队列族索引等

    // 创建 BufferResource 对象
    BufferResource build(VulkanContext& context)
    {
        return { context, *this };
    }

private:
    friend BufferResource;
};
}
