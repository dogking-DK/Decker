#pragma once
#include <filesystem>
#include <nlohmann/json_fwd.hpp>

namespace dk::vkutil {

    std::filesystem::path get_model_path(const nlohmann::json& j, const std::string& model_name);
    

    bool readShaderFile(const std::string& file_path, std::string& code);
    
}
