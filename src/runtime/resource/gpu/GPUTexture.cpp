#pragma once

#include <memory>

#include "Vulkan/DescriptorSetLayout.h"

namespace dk {
namespace vkcore {
    class SamplerResource;
    class ImageViewResource;
    class ImageResource;
}

struct GPUTexture
{
    std::unique_ptr<vkcore::ImageResource>     image;
    std::unique_ptr<vkcore::ImageViewResource> view;
    std::unique_ptr<vkcore::SamplerResource>   sampler;
    vk::ImageLayout                            layout{vk::ImageLayout::eUndefined};
};
} // namespace dk
