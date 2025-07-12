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
    std::vector<AttrDesc>  table;     // ���Ա�
    std::vector<std::byte> blob;      // ���������� (pos/norm/��)
    std::vector<uint32_t>  indices;
    /* ������ͼ */
    template <typename T>
    std::span<const T> get(VertexAttribute sem) const;   // ����ʵ��
};

std::shared_ptr<MeshDataFlex> load_raw_mesh_flex(const std::filesystem::path& f);
}
