#include "ResourceLoader.h"
#include "AssetHelpers.hpp"
#include "RawTypes.hpp"

namespace dk {
using namespace helper;
std::shared_ptr<MeshDataFlex> load_raw_mesh_flex(const std::filesystem::path& f)
{
    auto   m = std::make_shared<MeshDataFlex>();
    size_t offset = 0;

    /* 1. Header */
    auto hdr = read_pod<RawMeshHeader>(f);
    m->vertexCount = hdr.vertex_count;
    m->indexCount = hdr.index_count;
    offset += sizeof(RawMeshHeader);

    /* 2. AttrDesc 数组 */
    m->table = read_blob<AttrDesc>(f, offset, hdr.attr_count);
    offset += hdr.attr_count * sizeof(AttrDesc);

    /* 3. 读取所有属性块到一块大 buffer（按文件大小推断） */
    std::ifstream is(f, std::ios::binary | std::ios::ate);
    size_t        fileSz = is.tellg();
    size_t        attrBytes = fileSz - offset - hdr.index_count * 4;   // indices 最后
    m->blob.resize(attrBytes);
    is.seekg(offset);
    is.read(reinterpret_cast<char*>(m->blob.data()), attrBytes);

    /* 4. indices */
    m->indices = read_blob<uint32_t>(f, offset + attrBytes, hdr.index_count);

    return m;
}

/* 便利函数：根据 AttrDesc 找 span */
template <typename T>
std::span<const T> MeshDataFlex::get(VertexAttribute s) const
{
    for (auto& d : table)
        if (d.semantic == static_cast<uint16_t>(s))
            return {
                reinterpret_cast<const T*>(blob.data() + d.offset_bytes - sizeof(RawMeshHeader) - table.size() * sizeof(
                                               AttrDesc)),
                d.byte_size / sizeof(T)
            };
    return {};
}

}
