#include "DescriptorSetPool.h"
#include <stdexcept>

namespace dk::vkcore {
GrowableDescriptorAllocator::GrowableDescriptorAllocator(VulkanContext* context, uint32_t sets_per_pool,
                                                         const std::vector<vk::DescriptorPoolSize>& pool_sizes)
    : _context(context), _sets_per_pool(sets_per_pool), _pool_sizes(pool_sizes)
{
}

GrowableDescriptorAllocator::~GrowableDescriptorAllocator()
{
    for (auto pool : _free_pools)
    {
        _context->getDevice().destroyDescriptorPool(pool);
    }
    for (auto pool : _used_pools)
    {
        _context->getDevice().destroyDescriptorPool(pool);
    }
    if (_current_pool)
    {
        _context->getDevice().destroyDescriptorPool(_current_pool);
    }
}

void GrowableDescriptorAllocator::reset()
{
    for (auto pool : _used_pools)
    {
        _context->getDevice().resetDescriptorPool(pool);
        _free_pools.push_back(pool);
    }
    _used_pools.clear();

    if (_current_pool)
    {
        _context->getDevice().resetDescriptorPool(_current_pool);
        _free_pools.push_back(_current_pool);
        _current_pool = nullptr;
    }
}

vk::DescriptorSet GrowableDescriptorAllocator::allocate(const DescriptorSetLayout& layout)
{
    _current_pool = getCurrentPool();

    vk::DescriptorSetLayout       layout_handle = layout.getHandle();
    vk::DescriptorSetAllocateInfo alloc_info(_current_pool, 1, &layout_handle);

    try
    {
        return _context->getDevice().allocateDescriptorSets(alloc_info).front();
    }
    catch (const vk::OutOfPoolMemoryError& e)
    {
        // 当前池已满，将其移至 used 列表
        _used_pools.push_back(_current_pool);

        // 获取一个新池并重试
        _current_pool             = getCurrentPool();
        alloc_info.descriptorPool = _current_pool;

        try
        {
            return _context->getDevice().allocateDescriptorSets(alloc_info).front();
        }
        catch (const vk::SystemError& e)
        {
            // 如果还是失败，说明有问题
            throw std::runtime_error("Failed to allocate descriptor set even after growing pool.");
        }
    }
    catch (const vk::FragmentedPoolError& e)
    {
        // 与 OutOfPoolMemoryError 处理方式相同
        _used_pools.push_back(_current_pool);
        _current_pool             = getCurrentPool();
        alloc_info.descriptorPool = _current_pool;
        try
        {
            return _context->getDevice().allocateDescriptorSets(alloc_info).front();
        }
        catch (const vk::SystemError& e)
        {
            throw std::runtime_error("Failed to allocate descriptor set even after growing pool (fragmented).");
        }
    }
}

vk::DescriptorPool GrowableDescriptorAllocator::getCurrentPool()
{
    if (_current_pool)
    {
        return _current_pool;
    }

    if (!_free_pools.empty())
    {
        vk::DescriptorPool pool = _free_pools.back();
        _free_pools.pop_back();
        return pool;
    }

    return createPool();
}

vk::DescriptorPool GrowableDescriptorAllocator::createPool()
{
    vk::DescriptorPoolCreateInfo pool_info;
    pool_info.maxSets       = _sets_per_pool;
    pool_info.poolSizeCount = static_cast<uint32_t>(_pool_sizes.size());
    pool_info.pPoolSizes    = _pool_sizes.data();
    // 这个分配器模型不支持单独释放，而是通过 reset() 批量回收
    // pool_info.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;

    return _context->getDevice().createDescriptorPool(pool_info);
}
} // namespace dk::vkcore
