#pragma once

#include <memory>

#include "Image.h"
#include "ImageView.h"
#include "Sampler.h"

namespace dk {
struct GPUTexture
{
    std::unique_ptr<vkcore::ImageResource>     image;
    std::unique_ptr<vkcore::ImageViewResource> view;
    std::unique_ptr<vkcore::SamplerResource>   sampler;
    vk::ImageLayout                            layout{vk::ImageLayout::eUndefined};
};
} // namespace dk
