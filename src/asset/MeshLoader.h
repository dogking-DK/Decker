// runtime/MeshLoaderFlex.hpp
#pragma once
#include "../asset/RawTypes.hpp"
#include <span>
#include <memory>
#include <vector>

namespace dk {
    struct MaterialData;                  // fwd
struct MeshData
{
    uint32_t               vertex_count, index_count;
    std::vector<AttrDesc>  table;     // 属性表
    std::vector<std::byte> blob;      // 整个属性区 (pos/norm/…)
    std::vector<uint32_t>  indices;
    std::shared_ptr<MaterialData> material{nullptr};  // 材质（可选）

    /* 便利视图 */
    template <typename T>
    std::span<const T> get(VertexAttribute sem) const;   // 下例实现
};

std::shared_ptr<MeshData> load_raw_mesh(const std::filesystem::path& f);
}
