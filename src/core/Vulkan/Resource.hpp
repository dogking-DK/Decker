#pragma once
#include "vk_types.h"
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

    Resource()          = default;
    virtual ~Resource() = default;

    Handle&        getHandle() { return _handle; }
    const Handle&  getHandle() const { return _handle; }
    VulkanContext* getContext() { return _context; }
    bool           hasDevice() const { return _context != nullptr; }
    bool           hasHandle() const { return _handle != nullptr; }
    void           setDebugName(const std::string& name);
    vk::ObjectType getObjectType() { return Type; }

protected:
    VulkanContext* _context = nullptr;
    Handle         _handle = VK_NULL_HANDLE;
    std::string _name;
};

template <typename Handle, vk::ObjectType Type>
void Resource<Handle, Type>::setDebugName(const std::string& name)
{
    _name = name;
    // Handle 是 vk::Image / vk::Buffer / vk::Device 等
    // Handle::CType 是 VkImage / VkBuffer / VkDevice（C API）
    auto rawHandle = static_cast<typename Handle::CType>(_handle);

    // 按 Vulkan 规范，把指针或非指针句柄都 reinterpret 成 uint64_t
    uint64_t handleValue = reinterpret_cast<uint64_t>(rawHandle);

    vk::DebugUtilsObjectNameInfoEXT debugNameInfo{
        Type,
        handleValue,
        name.c_str(),
        nullptr  // pNext
    };

    _context->getDevice().setDebugUtilsObjectNameEXT(debugNameInfo);
}

}
