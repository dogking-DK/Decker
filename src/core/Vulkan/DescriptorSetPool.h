#include <vulkan/vulkan.hpp>
#include <vector>
#include <stdexcept>

namespace dk::vkcore {
class DynamicDescriptorPool
{
public:
    // ����ʱ�����豸����ʼ PoolSizes �ͳ�ʼ��� descriptor set ����
    DynamicDescriptorPool(const vk::Device& device,
                          const std::vector<vk::DescriptorPoolSize>& basePoolSizes,
                          uint32_t initialMaxSets = 100,
                          vk::DescriptorPoolCreateFlags flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
        : mDevice(device),
          mBasePoolSizes(basePoolSizes),
          mCurrentMaxSets(initialMaxSets),
          mFlags(flags)
    {
        // ������һ�� DescriptorPool
        createPool(mCurrentMaxSets);
    }

    ~DynamicDescriptorPool()
    {
        // ����ʱ�������� DescriptorPool
        for (auto pool : mPools)
        {
            mDevice.destroyDescriptorPool(pool);
        }
    }

    // ��װ descriptor set ���亯�������� vk::DescriptorSetAllocateInfo���� pool �⣩
    vk::DescriptorSet allocateDescriptorSet(const vk::DescriptorSetLayout& layout)
    {
        vk::DescriptorSetAllocateInfo allocInfo{};
        // ʹ�����һ�� DescriptorPool ���з���
        allocInfo.descriptorPool     = mPools.back();
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts        = &layout;

        try
        {
            auto sets = mDevice.allocateDescriptorSets(allocInfo);
            return sets.front();
        }
        catch (const vk::SystemError& e)
        {
            // �������ʧ�ܣ�����Ϊ���Ѻľ��������µĳز�����
            // ������Ը�����Ҫ��ӡ������Ϣ
            // ���ݲ��ԣ����罫���������
            mCurrentMaxSets *= 2;
            createPool(mCurrentMaxSets);

            allocInfo.descriptorPool = mPools.back();
            auto sets                = mDevice.allocateDescriptorSets(allocInfo);
            return sets.front();
        }
    }

    // ��ѡ�������гؽ������ã�����ÿ֡����һ�Σ��Ա��������� descriptor set��
    void resetPools(vk::DescriptorPoolResetFlags flags = {})
    {
        for (auto pool : mPools)
        {
            mDevice.resetDescriptorPool(pool, flags);
        }
    }

private:
    // �ڲ����������ݵ�ǰ��������һ���µ� DescriptorPool
    void createPool(uint32_t maxSets)
    {
        // ��������չ���� Descriptor ���͵�����
        std::vector<vk::DescriptorPoolSize> poolSizes;
        for (const auto& baseSize : mBasePoolSizes)
        {
            // �򵥲��ԣ�����������ɱ�������
            poolSizes.emplace_back(vk::DescriptorPoolSize{
                baseSize.type,
                baseSize.descriptorCount * maxSets
            });
        }
        vk::DescriptorPoolCreateInfo poolInfo(
            mFlags,
            maxSets,
            static_cast<uint32_t>(poolSizes.size()),
            poolSizes.data()
        );
        vk::DescriptorPool newPool = mDevice.createDescriptorPool(poolInfo);
        mPools.push_back(newPool);
    }

    vk::Device                          mDevice;
    std::vector<vk::DescriptorPoolSize> mBasePoolSizes;
    std::vector<vk::DescriptorPool>     mPools;
    uint32_t                            mCurrentMaxSets;
    vk::DescriptorPoolCreateFlags       mFlags;
};
}
