#pragma once
#include <tsl/robin_set.h>
#include <tsl/robin_map.h>
#include "Resource.hpp"

namespace dk::vkcore {
class ImageViewResource;
class ImageBuilder;
}

namespace dk::vkcore {
class SamplerResource : public Resource<vk::Sampler, vk::ObjectType::eSampler>
{
public:
    SamplerResource(VulkanContext& ctx, const vk::SamplerCreateInfo& info)
        : Resource(&ctx, nullptr)
    {
        _handle = _context->getDevice().createSampler(info);
    }

    ~SamplerResource() override
    {
        if (_handle)
        {
            _context->getDevice().destroySampler(_handle);
        }
    }
};

inline vk::SamplerCreateInfo makeLinearClampSamplerInfo()
{
    vk::SamplerCreateInfo info{};
    info.magFilter    = vk::Filter::eLinear;
    info.minFilter    = vk::Filter::eLinear;
    info.mipmapMode   = vk::SamplerMipmapMode::eLinear;
    info.addressModeU = vk::SamplerAddressMode::eClampToEdge;
    info.addressModeV = vk::SamplerAddressMode::eClampToEdge;
    info.addressModeW = vk::SamplerAddressMode::eClampToEdge;
    info.maxLod       = 1000.0f;
    return info;
}
}
