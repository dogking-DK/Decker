#pragma once

#include "Context.h"
#include "DescriptorSetLayout.h"
#include <vector>

namespace dk::vkcore {
// 用于每帧、短暂的描述符集分配。通过 reset() 高效回收所有内存。
class GrowableDescriptorAllocator
{
public:
    GrowableDescriptorAllocator(VulkanContext*                             context, uint32_t sets_per_pool,
                                const std::vector<vk::DescriptorPoolSize>& pool_sizes);
    ~GrowableDescriptorAllocator();

    GrowableDescriptorAllocator(const GrowableDescriptorAllocator&)            = delete;
    GrowableDescriptorAllocator& operator=(const GrowableDescriptorAllocator&) = delete;

    // 将所有池标记为可用，为下一帧做准备
    void reset();

    // 分配一个描述符集
    vk::DescriptorSet allocate(const DescriptorSetLayout& layout);

private:
    vk::DescriptorPool createPool();
    vk::DescriptorPool getCurrentPool();

    VulkanContext*                      _context;
    uint32_t                            _sets_per_pool;
    std::vector<vk::DescriptorPoolSize> _pool_sizes;

    vk::DescriptorPool              _current_pool{nullptr};
    std::vector<vk::DescriptorPool> _used_pools;
    std::vector<vk::DescriptorPool> _free_pools;
};
} // namespace dk::vkcore
