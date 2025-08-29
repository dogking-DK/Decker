// vkcore/DescriptorWriter.hpp
#pragma once
#include <vulkan/vulkan.hpp>
#include <vector>

namespace dk::vkcore {
class DescriptorSetWriter
{
public:
    // 单元素
    DescriptorSetWriter& writeBuffer(uint32_t                        binding, vk::DescriptorType type,
                                     const vk::DescriptorBufferInfo& info, uint32_t              arrayIndex = 0)
    {
        _bufferInfos.push_back(info);
        vk::WriteDescriptorSet w{};
        w.dstBinding      = binding;
        w.dstArrayElement = arrayIndex;
        w.descriptorType  = type;
        w.descriptorCount = 1;
        w.pBufferInfo     = &_bufferInfos.back();
        _writes.push_back(w);
        return *this;
    }

    DescriptorSetWriter& writeImage(uint32_t                       binding, vk::DescriptorType type,
                                    const vk::DescriptorImageInfo& info, uint32_t              arrayIndex = 0)
    {
        _imageInfos.push_back(info);
        vk::WriteDescriptorSet w{};
        w.dstBinding      = binding;
        w.dstArrayElement = arrayIndex;
        w.descriptorType  = type;
        w.descriptorCount = 1;
        w.pImageInfo      = &_imageInfos.back();
        _writes.push_back(w);
        return *this;
    }

    // 数组批量写入（bindless 或传统数组）
    DescriptorSetWriter& writeBuffers(uint32_t                                     binding, vk::DescriptorType type,
                                      const std::vector<vk::DescriptorBufferInfo>& infos,
                                      uint32_t                                     startArrayIndex = 0)
    {
        if (infos.empty()) return *this;
        size_t base = _bufferInfos.size();
        _bufferInfos.insert(_bufferInfos.end(), infos.begin(), infos.end());
        vk::WriteDescriptorSet w{};
        w.dstBinding      = binding;
        w.dstArrayElement = startArrayIndex;
        w.descriptorType  = type;
        w.descriptorCount = static_cast<uint32_t>(infos.size());
        w.pBufferInfo     = &_bufferInfos[base];
        _writes.push_back(w);
        return *this;
    }

    DescriptorSetWriter& writeImages(uint32_t                                    binding, vk::DescriptorType type,
                                     const std::vector<vk::DescriptorImageInfo>& infos,
                                     uint32_t                                    startArrayIndex = 0)
    {
        if (infos.empty()) return *this;
        size_t base = _imageInfos.size();
        _imageInfos.insert(_imageInfos.end(), infos.begin(), infos.end());
        vk::WriteDescriptorSet w{};
        w.dstBinding      = binding;
        w.dstArrayElement = startArrayIndex;
        w.descriptorType  = type;
        w.descriptorCount = static_cast<uint32_t>(infos.size());
        w.pImageInfo      = &_imageInfos[base];
        _writes.push_back(w);
        return *this;
    }

    void update(const VulkanContext& context, const vk::DescriptorSet set)
    {
        for (auto& w : _writes) w.dstSet = set;
        context.getDevice().updateDescriptorSets(_writes, {});
        _writes.clear();
        _bufferInfos.clear();
        _imageInfos.clear();
    }

private:
    std::vector<vk::WriteDescriptorSet>   _writes;
    std::vector<vk::DescriptorBufferInfo> _bufferInfos;
    std::vector<vk::DescriptorImageInfo>  _imageInfos;
};
} // namespace vkcore
