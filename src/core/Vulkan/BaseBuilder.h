#pragma once
#include <string>
#include <vk_mem_alloc.h>
#include "vk_types.h"

namespace dk::vkcore {
template <typename BuilderType, typename CreateInfoType>
class BaseBuilder
{
public:
    BaseBuilder();
    const VmaAllocationCreateInfo& getAllocationCreateInfo() const { return _alloc_create_info; }
    const CreateInfoType&          getCreateInfo() const { return _create_info; }
    const std::string&             getDebugName() const { return _debug_name; }
    CreateInfoType&                getCreateInfo() { return _create_info; }

    BuilderType& withDebugName(const std::string& name);
    BuilderType& withVmaFlags(VmaAllocationCreateFlags flags);
    BuilderType& withVmaPool(VmaPool pool);
    BuilderType& withVmaPreferredFlags(vk::MemoryPropertyFlags flags);
    BuilderType& withVmaRequiredFlags(vk::MemoryPropertyFlags flags);
    BuilderType& withVmaUsage(VmaMemoryUsage usage);

protected:
    VmaAllocationCreateInfo _alloc_create_info = {};
    CreateInfoType          _create_info       = {};
    std::string             _debug_name        = {};
};

template <typename BuilderType, typename CreateInfoType>
BaseBuilder<BuilderType, CreateInfoType>::BaseBuilder()
{
    _alloc_create_info.usage         = VMA_MEMORY_USAGE_GPU_ONLY;
    _alloc_create_info.requiredFlags = static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
}

template <typename BuilderType, typename CreateInfoType>
BuilderType& BaseBuilder<BuilderType, CreateInfoType>::withDebugName(const std::string& name)
{
    _debug_name = name;
    return *static_cast<BuilderType*>(this);
}

template <typename BuilderType, typename CreateInfoType>
BuilderType& BaseBuilder<BuilderType, CreateInfoType>::withVmaFlags(VmaAllocationCreateFlags flags)
{
    _alloc_create_info.flags = flags;
    return *static_cast<BuilderType*>(this);
}

template <typename BuilderType, typename CreateInfoType>
BuilderType& BaseBuilder<BuilderType, CreateInfoType>::withVmaPool(VmaPool pool)
{
    _alloc_create_info.pool = pool;
    return *static_cast<BuilderType*>(this);
}

template <typename BuilderType, typename CreateInfoType>
BuilderType& BaseBuilder<BuilderType, CreateInfoType>::withVmaPreferredFlags(vk::MemoryPropertyFlags flags)
{
    _alloc_create_info.preferredFlags = static_cast<VkMemoryPropertyFlags>(flags);
    return *static_cast<BuilderType*>(this);
}

template <typename BuilderType, typename CreateInfoType>
BuilderType& BaseBuilder<BuilderType, CreateInfoType>::withVmaRequiredFlags(vk::MemoryPropertyFlags flags)
{
    _alloc_create_info.requiredFlags = static_cast<VkMemoryPropertyFlags>(flags);
    return *static_cast<BuilderType*>(this);
}

template <typename BuilderType, typename CreateInfoType>
BuilderType& BaseBuilder<BuilderType, CreateInfoType>::withVmaUsage(VmaMemoryUsage usage)
{
    _alloc_create_info.usage = usage;
    return *static_cast<BuilderType*>(this);
}
}
