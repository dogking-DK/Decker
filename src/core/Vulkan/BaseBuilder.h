#pragma once
#include <string>
#include <vk_mem_alloc.h>
#include "vk_types.h"

namespace dk::vkcore {
template <typename BuilderType, typename CreateInfoType>
class BaseBuilder
{
public:
    BaseBuilder() = default;
    const VmaAllocationCreateInfo& get_allocation_create_info() const { return _alloc_create_info; }
    const CreateInfoType&          get_create_info() const { return _create_info; }
    const std::string&             get_debug_name() const { return _debug_name; }

    BuilderType& with_debug_name(const std::string& name);
    BuilderType& with_vma_flags(VmaAllocationCreateFlags flags);
    BuilderType& with_vma_pool(VmaPool pool);
    BuilderType& with_vma_preferred_flags(vk::MemoryPropertyFlags flags);
    BuilderType& with_vma_required_flags(vk::MemoryPropertyFlags flags);
    BuilderType& with_vma_usage(VmaMemoryUsage usage);

protected:
    CreateInfoType& get_create_info() { return _create_info; }

    VmaAllocationCreateInfo _alloc_create_info = {};
    CreateInfoType          _create_info       = {};
    std::string             _debug_name        = {};
};

template <typename BuilderType, typename CreateInfoType>
BuilderType& BaseBuilder<BuilderType, CreateInfoType>::with_debug_name(const std::string& name)
{
    _debug_name = name;
    return *static_cast<BuilderType*>(this);
}

template <typename BuilderType, typename CreateInfoType>
BuilderType& BaseBuilder<BuilderType, CreateInfoType>::with_vma_flags(VmaAllocationCreateFlags flags)
{
    _alloc_create_info.flags = flags;
    return *static_cast<BuilderType*>(this);
}

template <typename BuilderType, typename CreateInfoType>
BuilderType& BaseBuilder<BuilderType, CreateInfoType>::with_vma_pool(VmaPool pool)
{
    _alloc_create_info.pool = pool;
    return *static_cast<BuilderType*>(this);
}

template <typename BuilderType, typename CreateInfoType>
BuilderType& BaseBuilder<BuilderType, CreateInfoType>::with_vma_preferred_flags(vk::MemoryPropertyFlags flags)
{
    _alloc_create_info.preferredFlags = static_cast<VkMemoryPropertyFlags>(flags);
    return *static_cast<BuilderType*>(this);
}

template <typename BuilderType, typename CreateInfoType>
BuilderType& BaseBuilder<BuilderType, CreateInfoType>::with_vma_required_flags(vk::MemoryPropertyFlags flags)
{
    _alloc_create_info.requiredFlags = static_cast<VkMemoryPropertyFlags>(flags);
    return *static_cast<BuilderType*>(this);
}

template <typename BuilderType, typename CreateInfoType>
BuilderType& BaseBuilder<BuilderType, CreateInfoType>::with_vma_usage(VmaMemoryUsage usage)
{
    _alloc_create_info.usage = usage;
    return *static_cast<BuilderType*>(this);
}
}
