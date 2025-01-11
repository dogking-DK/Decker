#pragma once

#include <vk_types.h>
#include <glslang/Public/ShaderLang.h>
#include <glslang/SPIRV/GlslangToSpv.h>
#include <glslang/Public/ResourceLimits.h>

VKUTIL_BEGIN

class ShaderCompiler
{
public:
    static ShaderCompiler& getInstance()
    {
        static ShaderCompiler instance;
        return instance;
    }

    static void initGlslang() { glslang::InitializeProcess(); }
    static void finalizeGlslang() { glslang::FinalizeProcess(); }
    bool compileGLSLtoSPV(const std::string& src_code, std::vector<uint32_t>& dist_code, EShLanguage shader_type, bool isHLSL = false);

private:
    ShaderCompiler() = default;
};
bool load_shader_module(const char* filePath, VkDevice device, VkShaderModule* outShaderModule);
bool load_shader_module(std::vector<uint32_t>& spv, VkDevice device, VkShaderModule* outShaderModule);

VKUTIL_END

class PipelineBuilder
{
public:
    std::vector<VkPipelineShaderStageCreateInfo> _shaderStages;
   
    VkPipelineInputAssemblyStateCreateInfo _inputAssembly;
    VkPipelineRasterizationStateCreateInfo _rasterizer;
    VkPipelineColorBlendAttachmentState _colorBlendAttachment;
    VkPipelineMultisampleStateCreateInfo _multisampling;
    VkPipelineLayout _pipelineLayout;
    VkPipelineDepthStencilStateCreateInfo _depthStencil;
    VkPipelineRenderingCreateInfo _renderInfo;
    VkFormat _colorAttachmentformat;

	PipelineBuilder(){ clear(); }

    void clear();

    VkPipeline build_pipeline(VkDevice device);

    void set_shaders(VkShaderModule vertexShader, VkShaderModule fragmentShader);
    void set_input_topology(VkPrimitiveTopology topology);
    void set_polygon_mode(VkPolygonMode mode);
    void set_cull_mode(VkCullModeFlags cullMode, VkFrontFace frontFace);
    void set_multisampling_none();
    void disable_blending();
    void enable_blending_additive();
    void enable_blending_alphablend();

    void set_color_attachment_format(VkFormat format);
	void set_depth_format(VkFormat format);
	void disable_depthtest();
    void enable_depthtest(bool depthWriteEnable,VkCompareOp op);
};

