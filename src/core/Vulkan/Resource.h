#include <vulkan/vulkan.hpp>
#include "vk_mem_alloc.h"
#include <stdexcept>
#include <memory>

// ��Դ����
class Resource
{
public:
    virtual ~Resource() = default;

protected:
    // ������Դ���� VMA ���������ڴ������
    VmaAllocator  m_allocator  = VK_NULL_HANDLE;
    VmaAllocation m_allocation = VK_NULL_HANDLE;
};