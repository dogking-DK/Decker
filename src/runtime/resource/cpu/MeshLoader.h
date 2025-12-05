// runtime/MeshLoaderFlex.hpp
#pragma once
#include <filesystem>
#include <span>
#include <memory>
#include <vector>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "RawTypes.hpp"

namespace dk {
    struct MaterialData;                  // fwd
    struct MeshData
    {
        uint32_t               vertex_count = 0;
        uint32_t               index_count = 0;

        std::vector<uint32_t>  indices;

        // 常用属性
        std::vector<glm::vec3> positions;
        std::vector<glm::vec3> normals;
        std::vector<glm::vec4> tangents;

        // 多套 UV / 颜色可以用 vector-of-vector 或 map
        std::vector<std::vector<glm::vec2>> texcoords; // texcoords[set][i]
        std::vector<std::vector<glm::vec4>> colors;    // colors[set][i]

        // 骨骼权重等以后再加
        // std::vector<glm::uvec4> joints;
        // std::vector<glm::vec4>  weights;

        std::shared_ptr<MaterialData> material{ nullptr };
    };

std::shared_ptr<MeshData> load_mesh(const std::filesystem::path& f);


}
