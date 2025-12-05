#include "MeshLoader.h"

#include "AssetHelpers.hpp"
#include "RawTypes.hpp"

namespace dk {
using namespace helper;

namespace {
    // 计算某个 AttrDesc 在 blob 里的偏移（你之前 MeshData::get() 已经写过一版）
    size_t attr_data_offset_bytes(const RawMeshHeader& hdr, size_t attr_count, const AttrDesc& d)
    {
        // 文件里 offset_bytes 是“从文件头起”的偏移
        // blob 只包含: header + AttrDesc[] 之后的那一段
        const size_t headerBytes = sizeof(RawMeshHeader) + attr_count * sizeof(AttrDesc);
        return d.offset_bytes - headerBytes;
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

        auto load_attr = [&]<typename T0>(T0& target_vector, const AttrDesc& desc)
        {
            // 获取源数据指针
            const size_t offsetInBlob = attr_data_offset_bytes(raw.header, raw.header.attr_count, desc);
            if (offsetInBlob + desc.byte_size > raw.blob.size()) return; // 安全检查

            const void*  src_ptr    = raw.blob.data() + offsetInBlob;
            const size_t vert_count = raw.header.vertex_count;

            // 目标类型 T 的维度 (比如 vec3 = 3)
            // 注意：这里假设 glm::vec3 是 3 个 float 紧密排列
            using T                            = typename std::decay_t<T0>::value_type;
            constexpr size_t target_elem_count = sizeof(T) / sizeof(float);

            // 检查：只有当文件里的 float 数量和目标类型的 float 数量一致时，才能直接 memcpy
            // 比如文件里是 vec3，目标也是 vec3。
            if (desc.elem_count == target_elem_count && desc.comp_type == 0 /*Float32*/)
            {
                target_vector.resize(vert_count);
                // 直接内存拷贝，速度最快
                std::memcpy(target_vector.data(), src_ptr, vert_count * sizeof(T));
            }
            else
            {
                // 慢速路径：如果维度不匹配（例如文件存了 vec4 但 runtime 只要 vec3），或者类型不是 float
                // 这里可以处理，或者直接报错。为了高性能引擎，通常要求 Asset Importer 阶段就对齐数据。
                assert(false && "Mismatch in attribute element count or type! Please fix Importer.");
            }
        };

        // 3. 遍历属性表，只需要做简单的路由分发
        for (const auto& d : raw.table)
        {
            switch (static_cast<VertexAttribute>(d.semantic))
            {
            case Position:
                load_attr(mesh->positions, d); // 自动推导为 glm::vec3
                break;
            case Normal:
                load_attr(mesh->normals, d);   // 自动推导为 glm::vec3
                break;
            case Tangent:
                load_attr(mesh->tangents, d);  // 自动推导为 glm::vec4
                break;
            // 处理 UV (可能支持多套)
            case Texcoord0:
                // 假设 mesh->texcoords 是 vector<vec2>，如果是 vector<vector<vec2>> 需要调整逻辑
                //load_attr(mesh->texcoords0, d);
                break;
            case Color0:
                //load_attr(mesh->colors, d);    // 自动推导为 glm::vec4
                break;

            case Joints0:
                // 骨骼索引通常是整数，不能用 float 处理逻辑，需要单独写一个 load_int_attr
                // load_int_attr(mesh->joints, d);
                break;
            case Weights0:
                //load_attr(mesh->weights, d);
                break;
            }
        }

        return mesh;
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
}

std::shared_ptr<MeshData> load_mesh(const std::filesystem::path& f)
{
    auto raw  = read_raw_mesh(f);
    auto mesh = build_runtime_mesh(*raw);
    return mesh;
}
}
