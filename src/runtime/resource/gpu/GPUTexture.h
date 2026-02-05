#pragma once

#include <memory>

#include "Vulkan/DescriptorSetLayout.h"
#include "Vulkan/Texture.h"

namespace dk {
namespace vkcore {
    class SamplerResource;
    class TextureResource;
}

struct GPUTexture
{
    std::shared_ptr<vkcore::TextureResource>   texture;
    std::unique_ptr<vkcore::SamplerResource>   sampler;
    vk::ImageLayout                            layout{vk::ImageLayout::eUndefined};
};
} // namespace dk
