#pragma once

#include <vulkan/vulkan.hpp>

#include "Resource.hpp"

namespace dk::vkcore {
class CommandPool : public Resource<vk::CommandPool, vk::ObjectType::eCommandPool>
{
public:
    // ��ֹ�����������ƶ�����
    CommandPool(const CommandPool&)            = delete;
    CommandPool& operator=(const CommandPool&) = delete;
    CommandPool(CommandPool&&)                 = default;
    CommandPool& operator=(CommandPool&&)      = default;

    // ���캯���������豸�������������Լ���ѡ�Ĵ�����־��Ĭ�ϱ�־Ϊ������ Command Buffer ���á�
    CommandPool(VulkanContext*                   context, const uint32_t queueFamilyIndex,
                const vk::CommandPoolCreateFlags flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer)
        : Resource(context)
    {
        vk::CommandPoolCreateInfo pool_info{};
        pool_info.queueFamilyIndex = queueFamilyIndex;
        pool_info.flags            = flags;

        // ���� Command Pool��vulkan.hpp �ڴ���ʧ��ʱ���׳��쳣
        _handle = _context->getDevice().createCommandPool(pool_info);
    }

    // �����������Զ����� Command Pool���ͷ�ռ�õ�������Դ
    ~CommandPool() override
    {
        if (_handle)
        {
            _context->getDevice().destroyCommandPool(_handle);
        }
    }

    // ���� Command Pool����ѡ�������ñ�־��һ������Ҫ���� Command Buffer ǰ���ã�
    void reset(const vk::CommandPoolResetFlags flags = {})
    {
        _context->getDevice().resetCommandPool(_handle, flags);
    }

    void executeImmediate(const vk::Queue& queue, const std::function<void(vk::CommandBuffer)>& record_function);
};
}
