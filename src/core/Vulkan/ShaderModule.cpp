#include "ShaderModule.h"
#include <stdexcept>

// 为了进行反射，我们需要包含 spirv-cross 的头文件。
// 请确保你的项目已经正确配置了 spirv-cross 的 include 路径。
#include <spirv_cross/spirv_cross.hpp>

namespace dk::vkcore {
ShaderModule::ShaderModule(VulkanContext* context, const std::vector<uint32_t>& spirv_code)
    : Resource(context) // 调用基类构造函数
{
    if (spirv_code.empty())
    {
        throw std::runtime_error("Cannot create shader module from empty SPIR-V code.");
    }

    // 1. 创建 vk::ShaderModuleCreateInfo 结构体
    //    注意：codeSize 是以字节为单位的
    vk::ShaderModuleCreateInfo create_info;
    create_info.codeSize = spirv_code.size() * sizeof(uint32_t);
    create_info.pCode    = spirv_code.data();

    // 2. 创建 Vulkan 着色器模块
    try
    {
        _handle = _context->getDevice().createShaderModule(create_info);
    }
    catch (const vk::SystemError& e)
    {
        throw std::runtime_error("Failed to create shader module: " + std::string(e.what()));
    }

    // 3. 从 SPIR-V 反射出着色器阶段
    findShaderStage(spirv_code);
}

ShaderModule::~ShaderModule()
{
    // RAII 清理：如果句柄有效，则销毁它
    if (hasDevice() && hasHandle())
    {
        _context->getDevice().destroyShaderModule(_handle);
        _handle = nullptr; // 将句柄置空，防止悬挂指针
    }
}

void ShaderModule::findShaderStage(const std::vector<uint32_t>& spirv_code)
{
    // 使用 spirv-cross 编译器进行反射
    const spirv_cross::Compiler comp(spirv_code);

    // 获取 SPIR-V 中的所有入口点和它们对应的阶段
    auto entry_points = comp.get_entry_points_and_stages();

    if (entry_points.empty())
    {
        throw std::runtime_error("SPIR-V has no entry points.");
    }

    // 为简单起见，我们假设每个 ShaderModule 只有一个入口点
    // 这是一个非常常见的实践

    // 将 SPIR-V 的执行模型映射到 Vulkan 的着色器阶段标志
    switch (entry_points[0].execution_model)
    {
    case spv::ExecutionModel::ExecutionModelVertex:
        _stage = vk::ShaderStageFlagBits::eVertex;
        break;
    case spv::ExecutionModel::ExecutionModelFragment:
        _stage = vk::ShaderStageFlagBits::eFragment;
        break;
    case spv::ExecutionModel::ExecutionModelGLCompute:
        _stage = vk::ShaderStageFlagBits::eCompute;
        break;
    case spv::ExecutionModel::ExecutionModelTaskEXT:
        _stage = vk::ShaderStageFlagBits::eTaskEXT;
        break;
    case spv::ExecutionModel::ExecutionModelMeshEXT:
        _stage = vk::ShaderStageFlagBits::eMeshEXT;
        break;
    case spv::ExecutionModel::ExecutionModelRayGenerationKHR:
        _stage = vk::ShaderStageFlagBits::eRaygenKHR;
        break;
    case spv::ExecutionModel::ExecutionModelAnyHitKHR:
        _stage = vk::ShaderStageFlagBits::eAnyHitKHR;
        break;
    case spv::ExecutionModel::ExecutionModelClosestHitKHR:
        _stage = vk::ShaderStageFlagBits::eClosestHitKHR;
        break;
    case spv::ExecutionModel::ExecutionModelMissKHR:
        _stage = vk::ShaderStageFlagBits::eMissKHR;
        break;
    case spv::ExecutionModel::ExecutionModelIntersectionKHR:
        _stage = vk::ShaderStageFlagBits::eIntersectionKHR;
        break;
    default:
        throw std::runtime_error("Unsupported execution model / shader stage in SPIR-V.");
    }
}
} // namespace dk::vkcore
