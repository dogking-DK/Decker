#pragma once

#include <memory>

#include <glm/vec4.hpp>

#include "GPUMesh.h"
#include "GPUTexture.h"

namespace dk {
struct GPUMaterial
{
    float metallic{0.0f};
    float roughness{1.0f};
    glm::vec4 base_color{1.0f};

    std::shared_ptr<GPUTexture> base_color_tex{nullptr};
    std::shared_ptr<GPUTexture> metal_rough_tex{nullptr};
    std::shared_ptr<GPUTexture> normal_tex{nullptr};
    std::shared_ptr<GPUTexture> occlusion_tex{nullptr};
    std::shared_ptr<GPUTexture> emissive_tex{nullptr};
};
} // namespace dk
