#pragma once

#include <string>
#include <vector>

#include <vulkan/vulkan.hpp>

namespace dk::vkcore {
class ShaderCompiler
{
public:
    static void initGlslang();
    static void finalizeGlslang();

    bool compileGLSLToSPV(const std::string&               source,
                          std::vector<uint32_t>&           spirv,
                          vk::ShaderStageFlagBits          stage,
                          bool                             is_hlsl = false) const;
};
} // namespace dk::vkcore
