// vkcore/DescriptorWriter.hpp
#pragma once
#include "vk_types.h"
#include <vector>
#include <cstdint>
#include <cassert>

namespace dk::vkcore {
class DescriptorSetWriter
{
public:
    // 可选：一次性保留容量，避免多次扩容
    void reserve(size_t writes, size_t bufferInfos = 0, size_t imageInfos = 0)
    {
        _writes.reserve(writes);
        _patches.reserve(writes);
        _bufferInfos.reserve(bufferInfos);
        _imageInfos.reserve(imageInfos);
    }

    // 单元素：Buffer
    DescriptorSetWriter& writeBuffer(uint32_t                        binding,
                                     vk::DescriptorType              type,
                                     const vk::DescriptorBufferInfo& info,
                                     uint32_t                        arrayIndex = 0)
    {
        const uint32_t base = static_cast<uint32_t>(_bufferInfos.size());
        _bufferInfos.push_back(info);

        vk::WriteDescriptorSet w{};
        w.dstBinding      = binding;
        w.dstArrayElement = arrayIndex;
        w.descriptorType  = type;
        w.descriptorCount = 1;
        _writes.push_back(w);

        _patches.push_back({Kind::Buffer, base, 1});
        return *this;
    }

    // 单元素：Image/Sampler
    DescriptorSetWriter& writeImage(uint32_t                       binding,
                                    vk::DescriptorType             type,
                                    const vk::DescriptorImageInfo& info,
                                    uint32_t                       arrayIndex = 0)
    {
        const uint32_t base = static_cast<uint32_t>(_imageInfos.size());
        _imageInfos.push_back(info);

        vk::WriteDescriptorSet w{};
        w.dstBinding      = binding;
        w.dstArrayElement = arrayIndex;
        w.descriptorType  = type;
        w.descriptorCount = 1;
        _writes.push_back(w);

        _patches.push_back({Kind::Image, base, 1});
        return *this;
    }

    // 数组：Buffer（bindless 或传统数组）
    DescriptorSetWriter& writeBuffers(uint32_t                                     binding,
                                      vk::DescriptorType                           type,
                                      const std::vector<vk::DescriptorBufferInfo>& infos,
                                      uint32_t                                     startArrayIndex = 0)
    {
        if (infos.empty()) return *this;
        const uint32_t base  = static_cast<uint32_t>(_bufferInfos.size());
        const uint32_t count = static_cast<uint32_t>(infos.size());
        _bufferInfos.insert(_bufferInfos.end(), infos.begin(), infos.end());

        vk::WriteDescriptorSet w{};
        w.dstBinding      = binding;
        w.dstArrayElement = startArrayIndex;
        w.descriptorType  = type;
        w.descriptorCount = count;
        _writes.push_back(w);

        _patches.push_back({Kind::Buffer, base, count});
        return *this;
    }

    // 数组：Image
    DescriptorSetWriter& writeImages(uint32_t                                    binding,
                                     vk::DescriptorType                          type,
                                     const std::vector<vk::DescriptorImageInfo>& infos,
                                     uint32_t                                    startArrayIndex = 0)
    {
        if (infos.empty()) return *this;
        const uint32_t base  = static_cast<uint32_t>(_imageInfos.size());
        const uint32_t count = static_cast<uint32_t>(infos.size());
        _imageInfos.insert(_imageInfos.end(), infos.begin(), infos.end());

        vk::WriteDescriptorSet w{};
        w.dstBinding      = binding;
        w.dstArrayElement = startArrayIndex;
        w.descriptorType  = type;
        w.descriptorCount = count;
        _writes.push_back(w);

        _patches.push_back({Kind::Image, base, count});
        return *this;
    }

    // 最终提交：到这一刻再把指针（pBufferInfo/pImageInfo）指向稳定的存储
    void update(const VulkanContext& context, vk::DescriptorSet set)
    {
        assert(_writes.size() == _patches.size());
        for (size_t i = 0; i < _writes.size(); ++i)
        {
            _writes[i].dstSet = set;
            const Patch& p    = _patches[i];
            if (p.kind == Kind::Buffer)
            {
                _writes[i].pBufferInfo = &_bufferInfos[p.base];     // 连续 count 个
                _writes[i].pImageInfo  = nullptr;
            }
            else
            {
                _writes[i].pImageInfo  = &_imageInfos[p.base];      // 连续 count 个
                _writes[i].pBufferInfo = nullptr;
            }
            // descriptorCount 已在 write* 时写好
        }
        context.getDevice().updateDescriptorSets(_writes, {});
        reset(); // 用完清空，便于复用
    }

    void reset()
    {
        _writes.clear();
        _patches.clear();
        _bufferInfos.clear();
        _imageInfos.clear();
    }

private:
    enum class Kind { Buffer, Image };

    struct Patch
    {
        Kind     kind;
        uint32_t base;   // 在对应 info 向量中的起始索引
        uint32_t count;  // 连续元素个数（descriptorCount）
    };

    std::vector<vk::WriteDescriptorSet>   _writes;
    std::vector<Patch>                    _patches;
    std::vector<vk::DescriptorBufferInfo> _bufferInfos;
    std::vector<vk::DescriptorImageInfo>  _imageInfos;
};
} // namespace dk::vkcore
