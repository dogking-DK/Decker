#include <vulkan/vulkan.hpp>

#include "CommandPool.h"
#include "Resource.hpp"

namespace dk::vkcore {

class CommandBuffer : public Resource<vk::CommandBuffer, vk::ObjectType::eCommandBuffer>
{
public:
    // ��ֹ�����������ƶ�
    CommandBuffer(const CommandBuffer&)            = delete;
    CommandBuffer& operator=(const CommandBuffer&) = delete;
    CommandBuffer(CommandBuffer&&)                 = default;
    CommandBuffer& operator=(CommandBuffer&&)      = default;

    // ����ʱͨ��ָ���豸�������������һ���������
    CommandBuffer(VulkanContext* context, CommandPool* command_pool)
        : Resource(context), _command_pool(command_pool)
    {
        vk::CommandBufferAllocateInfo allocInfo{
            command_pool->getHandle(),
            vk::CommandBufferLevel::ePrimary,
            1
        };

        // ��������������ܻ��׳��쳣��vulkan.hpp Ĭ�ϲ����쳣����ģ�ͣ�
        auto buffers = _context->getDevice().allocateCommandBuffers(allocInfo);
        _handle      = buffers.front();
    }
    // ע�⣺�� Vulkan �У�һ�㲻��Ҫ�Ե��� command buffer ���� destroy��
    // ����ͳһ�� command pool �����ͷš������Ҫ�ֶ����û���գ�
    // ��ɵ��� m_device.freeCommandBuffers(m_commandPool, {_handle});
    ~CommandBuffer() override = default;

    // ��ʼ¼���������ָ��ʹ�õı�־��Ĭ�� eOneTimeSubmit
    void begin(const vk::CommandBufferUsageFlags usageFlags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit)
    {
        const vk::CommandBufferBeginInfo begin_info{usageFlags};
        _handle.begin(begin_info);
    }

    // ����¼��
    void end()
    {
        _handle.end();
    }

    // �ṩһ������ Lambda ��¼�Ʒ�װ
    // ʹ�÷�����
    //   cmdBuffer.record([](vk::CommandBuffer cb){
    //       cb.beginRenderPass(...);
    //       // ...
    //       cb.endRenderPass();
    //   });
    template <typename Func>
    void record(Func&&                      recordingFunction,
                vk::CommandBufferUsageFlags usageFlags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit)
    {
        begin(usageFlags);
        // ���ڲ��� command buffer ������ lambda ��
        recordingFunction(_handle);
        end();
    }

    // ������Ҫ���������ṩ reset ����
    void reset(const vk::CommandBufferResetFlags flags = {})
    {
        _handle.reset(flags);
    }

private:
    CommandPool* _command_pool;
};
}
