#include "ShaderCompiler.h"

#include <glslang/Public/ResourceLimits.h>
#include <glslang/Public/ShaderLang.h>
#include <glslang/SPIRV/GlslangToSpv.h>

#include <fmt/core.h>

namespace dk::vkcore {
namespace {
EShLanguage to_glslang_stage(vk::ShaderStageFlagBits stage)
{
    switch (stage)
    {
    case vk::ShaderStageFlagBits::eVertex: return EShLangVertex;
    case vk::ShaderStageFlagBits::eFragment: return EShLangFragment;
    case vk::ShaderStageFlagBits::eCompute: return EShLangCompute;
    case vk::ShaderStageFlagBits::eMeshEXT: return EShLangMesh;
    case vk::ShaderStageFlagBits::eTaskEXT: return EShLangTask;
    default: return EShLangVertex;
    }
}
} // namespace

void ShaderCompiler::initGlslang()
{
    glslang::InitializeProcess();
}

void ShaderCompiler::finalizeGlslang()
{
    glslang::FinalizeProcess();
}

bool ShaderCompiler::compileGLSLToSPV(const std::string&      source,
                                      std::vector<uint32_t>&  spirv,
                                      vk::ShaderStageFlagBits stage,
                                      bool                    is_hlsl) const
{
    const auto shader_type  = to_glslang_stage(stage);
    glslang::TShader shader(shader_type);

    const char* shader_source = source.c_str();
    shader.setStrings(&shader_source, 1);

    if (is_hlsl)
    {
        shader.setEnvInput(glslang::EShSourceHlsl, shader_type, glslang::EShClientVulkan, 100);
    }
    else
    {
        shader.setEnvInput(glslang::EShSourceGlsl, shader_type, glslang::EShClientVulkan, 100);
    }

    shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_3);
    shader.setEnvTarget(glslang::EshTargetSpv, glslang::EShTargetSpv_1_3);

    EShMessages messages = EShMessages::EShMsgDefault;
    if (!shader.parse(GetDefaultResources(), 100, false, messages))
    {
        fmt::print("Shader parse failed: {}\n", shader.getInfoLog());
        return false;
    }

    glslang::TProgram program;
    program.addShader(&shader);

    if (!program.link(messages))
    {
        fmt::print("Shader link failed: {}\n", program.getInfoLog());
        return false;
    }

    glslang::GlslangToSpv(*program.getIntermediate(shader_type), spirv);
    return true;
}
} // namespace dk::vkcore
