#pragma once
#include <array>
#include <filesystem>
#include "../core/UUID.hpp"
#include <memory>
#include <vector>

namespace dk {
struct TextureData;                      // fwd
class ResourceLoader;                    // fwd

struct MaterialData
{
    float                        metallic, roughness;
    std::array<uint8_t, 4> base_color;
    std::shared_ptr<TextureData> base_color_tex{nullptr};
    std::shared_ptr<TextureData> metal_rough_tex{ nullptr };
    std::shared_ptr<TextureData> normal_tex{ nullptr };
    std::shared_ptr<TextureData> occlusion_tex{ nullptr };
    std::shared_ptr<TextureData> emissive_tex{ nullptr };
};

} // namespace dk::runtime
