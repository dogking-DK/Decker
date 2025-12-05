#include "MeshLoader.h"

#include "AssetHelpers.hpp"
#include "RawTypes.hpp"

namespace dk {
using namespace helper;

// 计算某个 AttrDesc 在 blob 里的偏移（你之前 MeshData::get() 已经写过一版）
inline size_t attr_data_offset_bytes(const RawMeshHeader& hdr,
                                     size_t               attr_count,
                                     const AttrDesc&      d)
{
    // 文件里 offset_bytes 是“从文件头起”的偏移
    // blob 只包含: header + AttrDesc[] 之后的那一段
    const size_t headerBytes = sizeof(RawMeshHeader) + attr_count * sizeof(AttrDesc);
    return d.offset_bytes - headerBytes;
}


std::shared_ptr<RawMeshData> read_raw_mesh(const std::filesystem::path& f)
{
    auto   raw    = std::make_shared<RawMeshData>();
    size_t offset = 0;

    // 1. header
    raw->header = read_pod<RawMeshHeader>(f);
    offset += sizeof(RawMeshHeader);

    // 2. AttrDesc[]
    raw->table = read_blob<AttrDesc>(f, offset, raw->header.attr_count);
    offset += raw->header.attr_count * sizeof(AttrDesc);

    // 3. 属性区
    std::ifstream is(f, std::ios::binary | std::ios::ate);
    size_t        fileSz    = is.tellg();
    size_t        attrBytes = fileSz - offset - raw->header.index_count * sizeof(uint32_t);
    raw->blob.resize(attrBytes);
    is.seekg(offset);
    is.read(reinterpret_cast<char*>(raw->blob.data()), attrBytes);

    // 4. indices
    raw->indices = read_blob<uint32_t>(f, offset + attrBytes, raw->header.index_count);

    return raw;
}


std::shared_ptr<MeshData> build_runtime_mesh(const RawMeshData& raw)
{
    auto mesh          = std::make_shared<MeshData>();
    mesh->vertex_count = raw.header.vertex_count;
    mesh->index_count  = raw.header.index_count;
    mesh->indices      = raw.indices; // 直接拷一份（或 std::move 外层自行决定）

    // 预留一些空间（可选）
    mesh->texcoords.clear();
    mesh->colors.clear();

    // 遍历每个属性描述
    for (const auto& d : raw.table)
    {
        // 只支持我们当前导出的 float 属性
        const size_t offsetInBlob = attr_data_offset_bytes(raw.header,
                                                           raw.header.attr_count,
                                                           d);
        if (offsetInBlob + d.byte_size > raw.blob.size())
            continue; // 防御

        auto data = reinterpret_cast<const float*>(
            raw.blob.data() + offsetInBlob);
        const size_t elemCount = d.elem_count;            // 多少个 float 一个顶点
        const size_t vertCount = raw.header.vertex_count;

        // 语义解析：和 GLTF_Importer 里 make_sem 对应 :contentReference[oaicite:6]{index=6}
        const VertexAttribute sem = d.semantic;

        auto fill_vec3 = [&](std::vector<glm::vec3>& dst)
        {
            dst.resize(vertCount);
            for (size_t i = 0; i < vertCount; ++i)
            {
                const float* src = data + i * elemCount;
                dst[i]           = glm::vec3(src[0], src[1], src[2]);
            }
        };

        auto fill_vec4 = [&](std::vector<glm::vec4>& dst)
        {
            dst.resize(vertCount);
            for (size_t i = 0; i < vertCount; ++i)
            {
                const float* src = data + i * elemCount;
                dst[i]           = glm::vec4(src[0], src[1], src[2], src[3]);
            }
        };

        auto fill_vec2 = [&](std::vector<glm::vec2>& dst)
        {
            dst.resize(vertCount);
            for (size_t i = 0; i < vertCount; ++i)
            {
                const float* src = data + i * elemCount;
                dst[i]           = glm::vec2(src[0], src[1]);
            }
        };

        switch (sem)
        {
        case Position:
            if (elemCount >= 3) fill_vec3(mesh->positions);
            break;
        case Normal:
            if (elemCount >= 3) fill_vec3(mesh->normals);
            break;
        case Tangent:
            if (elemCount >= 4) fill_vec4(mesh->tangents);
            break;

        default:
            // TEXCOORD/COLOR/JOINTS/WEIGHTS 这些你可能有自己的编码方式
            // 比如如果你有 texCoordN(set)，可以根据 semantic 还原 set index
            // 这里只演示 texcoord0 的处理
            //if (isTexCoord0(sem))  // 伪代码：你可以根据 semantic 范围判断
            //{
            //    if (mesh->texcoords.size() < 1) mesh->texcoords.resize(1);
            //    if (elemCount >= 2) fill_vec2(mesh->texcoords[0]);
            //}
        // 其他语义先忽略或放到一个“extras”里
            break;
        }
    }

    return mesh;
}

std::shared_ptr<MeshData> load_mesh(const std::filesystem::path& f)
{
    auto raw  = read_raw_mesh(f);
    auto mesh = build_runtime_mesh(*raw);
    return mesh;
}
}
