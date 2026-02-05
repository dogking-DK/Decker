#include "loader.h"

#include <fstream>
#include <fmt/core.h>
#include <nlohmann/json.hpp>

std::filesystem::path dk::vkutil::get_model_path(const nlohmann::json& j, const std::string& model_name)
{
    // 遍历 file_paths 数组，查找 name 匹配 model_name 的项
    const auto it = std::ranges::find_if(j["file_paths"],
        [&](const nlohmann::json& item)
        {
            return item["name"] == model_name;
        });

// 如果找到了，返回对应的路径
    if (it != j["file_paths"].end())
    {
        return (*it)["path"].get<std::string>();
    }

    // 否则返回空路径
    return {};
}

bool dk::vkutil::readShaderFile(const std::string& file_path, std::string& code)
{
    std::ifstream fp(file_path);

    if (!fp.is_open())
    {
        fmt::print("can't open: {}\n", file_path);
        return false;
    }

    std::stringstream ss;
    ss << fp.rdbuf();

    fp.close();

    code = ss.str();

    return true;
}
