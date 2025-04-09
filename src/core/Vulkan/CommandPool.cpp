#include "CommandPool.h"

void dk::vkcore::CommandPool::executeImmediate(const vk::Queue&                              queue,
                                               const std::function<void(vk::CommandBuffer)>& record_function)
{
    // ����һ�����������
    vk::CommandBufferAllocateInfo allocInfo{
        _handle,
        vk::CommandBufferLevel::ePrimary,
        1
    };
    auto              commandBuffers = _context->getDevice().allocateCommandBuffers(allocInfo);
    vk::CommandBuffer commandBuffer  = commandBuffers.front();

    // ��ʼ�������¼��
    vk::CommandBufferBeginInfo beginInfo{vk::CommandBufferUsageFlagBits::eOneTimeSubmit};
    commandBuffer.begin(beginInfo);

    // ¼���û�ָ�������ͨ�� lambda ��ɣ�
    record_function(commandBuffer);

    // ����¼��
    commandBuffer.end();

    // �ύ�������������
    vk::SubmitInfo submitInfo;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &commandBuffer;
    queue.submit(submitInfo, vk::Fence{});

    // �ȴ��������ִ��
    queue.waitIdle();

    // ������Ϊ�Ǵ� command pool ����ģ�ͨ���������Ժ����� command pool ʱͳһ�ͷ�
    // �����Ҫ�����ͷţ����Ե��� device.freeCommandBuffers(commandPool, commandBuffers);
}
