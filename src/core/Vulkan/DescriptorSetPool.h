#include <vulkan/vulkan.hpp>
#include <vector>
#include <stdexcept>

namespace dk::vkcore {
class DynamicDescriptorPool
{
public:
    // 构造时传入设备、初始 PoolSizes 和初始最大 descriptor set 数量
    DynamicDescriptorPool(const vk::Device& device,
                          const std::vector<vk::DescriptorPoolSize>& basePoolSizes,
                          uint32_t initialMaxSets = 100,
                          vk::DescriptorPoolCreateFlags flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
        : mDevice(device),
          mBasePoolSizes(basePoolSizes),
          mCurrentMaxSets(initialMaxSets),
          mFlags(flags)
    {
        // 创建第一个 DescriptorPool
        createPool(mCurrentMaxSets);
    }

    ~DynamicDescriptorPool()
    {
        // 析构时销毁所有 DescriptorPool
        for (auto pool : mPools)
        {
            mDevice.destroyDescriptorPool(pool);
        }
    }

    // 封装 descriptor set 分配函数，传入 vk::DescriptorSetAllocateInfo（除 pool 外）
    vk::DescriptorSet allocateDescriptorSet(const vk::DescriptorSetLayout& layout)
    {
        vk::DescriptorSetAllocateInfo allocInfo{};
        // 使用最后一个 DescriptorPool 进行分配
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
            // 如果分配失败，则认为池已耗尽，创建新的池并重试
            // 这里可以根据需要打印调试信息
            // 扩容策略：例如将最大集数翻倍
            mCurrentMaxSets *= 2;
            createPool(mCurrentMaxSets);

            allocInfo.descriptorPool = mPools.back();
            auto sets                = mDevice.allocateDescriptorSets(allocInfo);
            return sets.front();
        }
    }

    // 可选：对所有池进行重置（例如每帧重置一次，以便重用所有 descriptor set）
    void resetPools(vk::DescriptorPoolResetFlags flags = {})
    {
        for (auto pool : mPools)
        {
            mDevice.resetDescriptorPool(pool, flags);
        }
    }

private:
    // 内部函数：根据当前容量创建一个新的 DescriptorPool
    void createPool(uint32_t maxSets)
    {
        // 按比例扩展各个 Descriptor 类型的数量
        std::vector<vk::DescriptorPoolSize> poolSizes;
        for (const auto& baseSize : mBasePoolSizes)
        {
            // 简单策略：按照最大集数成比例扩大
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
