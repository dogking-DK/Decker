#pragma once

#include <filesystem>
#include <fstream>

#include "Resource.hpp" // 你的 Resource 基类
#include <vector>

namespace dk::vkcore {
inline std::vector<uint32_t> loadSpirvFromFile(const std::filesystem::path& filePath)
{
    // 1. 以二进制模式打开文件，并将初始位置设置在文件末尾（ate）
    std::ifstream file(filePath, std::ios::ate | std::ios::binary);

    // 2. 检查文件是否成功打开
    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open SPIR-V file: " + filePath.string());
    }

    // 3. 获取文件大小（以字节为单位）
    size_t fileSize = file.tellg();

    // 4. 安全性检查：SPIR-V 文件大小必须是 4 字节（sizeof(uint32_t)）的整数倍
    if (fileSize % sizeof(uint32_t) != 0)
    {
        file.close();
        throw std::runtime_error("SPIR-V file size is not a multiple of 4 bytes: " + filePath.string());
    }

    // 5. 创建一个大小合适的向量来存储 SPIR-V 代码
    //    向量的大小是字节数除以 4
    std::vector<uint32_t> spirvCode(fileSize / sizeof(uint32_t));

    file.seekg(0);

    file.read(reinterpret_cast<char*>(spirvCode.data()), fileSize);

    file.close();

    return spirvCode;
}

class ShaderModule : public Resource<vk::ShaderModule, vk::ObjectType::eShaderModule>
{
public:
    ShaderModule(VulkanContext* context, const std::vector<uint32_t>& spirv_code);

    ~ShaderModule() override;
    ShaderModule(const ShaderModule&)            = delete;
    ShaderModule& operator=(const ShaderModule&) = delete;
    ShaderModule(ShaderModule&&)            = default;
    ShaderModule& operator=(ShaderModule&&) = default;

    // 获得着色器阶段
    vk::ShaderStageFlagBits getStage() const { return _stage; }

private:
    
    void findShaderStage(const std::vector<uint32_t>& spirv_code);

    vk::ShaderStageFlagBits _stage;
};
} // namespace dk::vkcore
