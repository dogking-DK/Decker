#include <vulkan/vulkan.hpp>
#include "vk_mem_alloc.h"
#include <stdexcept>
#include <memory>

// 资源基类
class Resource
{
public:
    virtual ~Resource() = default;

protected:
    // 所有资源共用 VMA 分配器和内存分配句柄
    VmaAllocator  m_allocator  = VK_NULL_HANDLE;
    VmaAllocation m_allocation = VK_NULL_HANDLE;
};