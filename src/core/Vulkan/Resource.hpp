#pragma once
#include <vulkan/vulkan.hpp>
#include "vk_mem_alloc.h"
#include <stdexcept>
#include <memory>

#include "Context.h"

namespace dk::vkcore {
// 资源基类
template <typename Handle, vk::ObjectType Type>
class Resource
{
public:
    Resource(VulkanContext* context, Handle handle = nullptr) : _context(context), _handle(handle)
    {
    }
    Resource() = default;
    virtual ~Resource() = default;

    Handle&        getHandle() { return _handle; }
    const Handle&  getHandle() const { return _handle; }
    bool           hasDevice() const { return _context != nullptr; }
    bool           hasHandle() const { return _handle != nullptr; }
    void           setDebugName(const std::string& name);
    vk::ObjectType getObjectType() { return Type; }

protected:
    VulkanContext* _context;
    Handle         _handle;
};

template <typename Handle, vk::ObjectType Type>
void Resource<Handle, Type>::setDebugName(const std::string& name)
{
    vk::DebugUtilsObjectNameInfoEXT debugNameInfo{Type,_handle,name.c_str()};

    _context->getDevice().setDebugUtilsObjectNameEXT(debugNameInfo);
}
}
