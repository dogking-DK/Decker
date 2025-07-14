// runtime/MeshLoaderFlex.hpp
#pragma once
#include "../asset/RawTypes.hpp"
#include <span>
#include <memory>
#include <vector>

namespace dk {
struct MeshDataFlex
{
    uint32_t               vertexCount, indexCount;
    std::vector<AttrDesc>  table;     // 属性表
    std::vector<std::byte> blob;      // 整个属性区 (pos/norm/…)
    std::vector<uint32_t>  indices;
    /* 便利视图 */
    template <typename T>
    std::span<const T> get(VertexAttribute sem) const;   // 下例实现
};

std::shared_ptr<MeshDataFlex> load_raw_mesh_flex(const std::filesystem::path& f);
}
