#include <vulkan/vulkan.hpp>

#include "CommandPool.h"
#include "Resource.hpp"

namespace dk::vkcore {
class BufferResource;

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
        vk::CommandBufferAllocateInfo allocInfo{command_pool->getHandle(), vk::CommandBufferLevel::ePrimary, 1};

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

    template <typename Func>
    void record(Func&&                      recordingFunction,
                vk::CommandBufferUsageFlags usageFlags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit)
    {
        // ���ڲ��� command buffer ������ lambda ��
        recordingFunction(_handle);
    }

    // ������Ҫ���������ṩ reset ����
    void reset(const vk::CommandBufferResetFlags flags = {})
    {
        _handle.reset(flags);
    }


    void transitionImage(vk::Image image, vk::ImageLayout currentLayout, vk::ImageLayout newLayout);

    void copyImageToImage(vk::Image source, vk::Image destination, vk::Extent2D src_size, vk::Extent2D dst_size);

    void generateMipmaps(vk::Image image, vk::Extent2D image_size);

    void copyBuffer(const BufferResource& src, const BufferResource& dst, vk::BufferCopy2 copy);

    // �ύ�������������
    void submit(const vk::Queue& queue, const vk::Fence& fence = nullptr)
    {
        vk::SubmitInfo submitInfo;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers    = &_handle;
        _context->getGraphicsQueue().submit(submitInfo, fence);
    }

    void submit2(const vk::Queue& queue,
                 const vk::Fence& fence = nullptr)
    {
        // ���� vk::CommandBufferSubmitInfo ��װ�ײ��������
        vk::CommandBufferSubmitInfo cmdBufInfo{};
        cmdBufInfo.commandBuffer = _handle;

        vk::SubmitInfo2 submitInfo2;
        submitInfo2.commandBufferInfoCount = 1;
        submitInfo2.pCommandBufferInfos    = &cmdBufInfo;
        // ����Ҫ�����ź�����ȴ��ź�����Ϣ��������Ӧ�� wait/semaphore ��Ϣ�ṹ

        // �ύ�����У�ע�⣺��Ҫ֧�� VK_KHR_synchronization2��
        queue.submit2({submitInfo2}, fence);
    }

private:
    CommandPool* _command_pool;
};

template <typename Func>
void executeImmediate(VulkanContext*                                 context,
                      CommandPool*                                   command_pool,
                      const vk::Queue&                               queue,
                      const std::function<void(CommandBuffer& cmd)>& function)
{
    CommandBuffer cmd(context, command_pool);
    cmd.begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

    // 3. ���ô���� lambda ¼�ƾ���������磺blitImage��copyImage��generateMipMap �ȣ�
    function(cmd);

    // 4. ����¼��
    cmd.end();

    // 6. ���� fence ͬ���ȴ������ύ����ִ�����
    vk::Fence fence = context->getDevice().createFence({});

    cmd.submit2(queue, fence);

    // 7. �ȴ� fence �ź�
    auto result = context->getDevice().waitForFences({fence}, VK_TRUE, UINT64_MAX);
    VK_CHECK(static_cast<VkResult>(result));
    context->getDevice().destroyFence(fence);

    // ��ѡ����������� CommandPool ��ͳһ���ã�Ҳ�����ֶ��ͷŵ�ǰ CommandBuffer
    context->getDevice().freeCommandBuffers(command_pool->getHandle(), cmd.getHandle());
}
}
