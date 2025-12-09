#pragma once
#include "Resource.hpp"
#include "BaseBuilder.h"
#include <memory>

namespace dk::vkcore {
class VulkanContext;

// 使用 vk::Buffer 封装的 Buffer 资源类
class BufferResource : public Resource<vk::Buffer, vk::ObjectType::eBuffer>
{
public:
    //  内部类 Builder
    class Builder : public BaseBuilder<Builder, vk::BufferCreateInfo>
    {
    public:
        Builder()
        {
            // 这里也可以顺便把 _create_info 一些默认值配好
            _create_info.size        = 0;
            _create_info.usage       = {};
            _create_info.sharingMode = vk::SharingMode::eExclusive;
        }

        // 设置 buffer 大小（链式接口）
        Builder& setSize(vk::DeviceSize size)
        {
            _create_info.size = size;
            return *this;
        }

        // 设置 buffer 使用标志
        Builder& setUsage(vk::BufferUsageFlags usage)
        {
            _create_info.usage = usage;
            return *this;
        }

        // 未来可以增加更多的配置项，如共享模式、队列族索引等

        // 创建 BufferResource 对象
        BufferResource build(VulkanContext& context)
        {
            return BufferResource(context, *this);
        }

        std::unique_ptr<BufferResource> buildUnique(VulkanContext& context)
        {
            return std::make_unique<BufferResource>(context, *this);
        }

    private:
        friend class BufferResource;
    };

    //  BufferResource 本体
    BufferResource(VulkanContext& context, Builder& builder);
    BufferResource() = delete;

    ~BufferResource() override
    {
        if (_handle)
        {
            // 通过静态转换获取底层 VkBuffer，再使用 VMA 销毁
            vmaDestroyBuffer(_context->getVmaAllocator(), _handle, _allocation);
        }
    }

    // 映射内存，返回 CPU 可访问的指针
    void* map();

    // 取消映射
    void unmap();

    void* data() const { return _data; }

    // CPU→GPU 写入（会自动 flush 非 coherent）
    void update(const void* src_data, vk::DeviceSize size, vk::DeviceSize offset = 0);

    // GPU→CPU 读前的 invalidate 同步GPU的修改
    void invalidate(VkDeviceSize size, VkDeviceSize offset = 0)
    {
        if (!_host_coherent)
        {
            VkDeviceSize start = offset & ~(_atom_size - 1);
            VkDeviceSize end   = (offset + size + _atom_size - 1) & ~(_atom_size - 1);
            vmaInvalidateAllocation(_context->getVmaAllocator(), _allocation, start, end - start);
        }
    }

    vk::DescriptorBufferInfo getDescriptorInfo(uint64_t offset = 0) const
    {
        return vk::DescriptorBufferInfo{_handle, offset, VK_WHOLE_SIZE};
    }

private:
    VmaAllocation     _allocation{};
    VmaAllocationInfo _allocation_info{};

    void*        _data          = nullptr;
    bool         _owns_mapping  = false;
    bool         _host_visible  = false;
    bool         _host_coherent = false;
    VkDeviceSize _atom_size     = 8;  // 由物理设备属性 limits.nonCoherentAtomSize 传入
};
}
