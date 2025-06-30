#include <fastgltf/types.hpp>
#include <fastgltf/core.hpp>
#include <span>
#include <filesystem>

#include "AssetNode.hpp"
#include "UUID.hpp"
#include "GLTF_Importer.h"

#include <fastgltf/tools.hpp>
#include <fmt/core.h>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "AssetHelpers.hpp"
#include "RawTypes.hpp"


namespace {
dk::UUID makeUUID(const std::string& tag, size_t idx)
{
    return dk::uuid_from_string(tag + std::to_string(idx));
}

auto make_node_recursive =
    [](auto self, const fastgltf::Asset& asset, const uint32_t node_idx) -> std::shared_ptr<dk::AssetNode>
{
    const auto& n    = asset.nodes[node_idx];
    auto        node = std::make_shared<dk::AssetNode>();
    node->kind       = dk::AssetKind::Node;
    node->name       = n.name.empty() ? "Node" : n.name;
    node->id         = makeUUID("node", node_idx);

    /* Mesh 依赖 */
    if (n.meshIndex.has_value())
    {
        const uint32_t mi   = static_cast<uint32_t>(n.meshIndex.value());
        const auto     mesh = std::make_shared<dk::AssetNode>();
        mesh->kind          = dk::AssetKind::Mesh;
        mesh->name          = asset.meshes[mi].name.empty() ? "Mesh" : asset.meshes[mi].name;
        mesh->id            = makeUUID("mesh", mi);

        const auto& gltf_mesh = asset.meshes[mi];
        for (size_t pi = 0; pi < gltf_mesh.primitives.size(); ++pi)
        {
            const auto& prim      = gltf_mesh.primitives[pi];
            auto        primitive = std::make_shared<dk::AssetNode>();
            primitive->kind       = dk::AssetKind::Primitive;
            primitive->name       = fmt::format("Surface {}", pi);
            primitive->id         = makeUUID("primitive", mi * 100 + pi); // 100 is arbitrary, just to ensure uniqueness
            mesh->children.push_back(primitive);
        }
        node->children.push_back(mesh);
    }

    /* 子节点递归 */
    for (const auto& child : n.children)
        node->children.push_back(self(self, asset, static_cast<uint32_t>(child)));

    return node;
};

dk::VertexAttribute make_sem(const std::string& attr, const int set)
{
    if (attr == "POSITION") return dk::VertexAttribute::Position;
    if (attr == "NORMAL") return dk::VertexAttribute::Normal;
    if (attr == "TANGENT") return dk::VertexAttribute::Tangent;
    if (attr == "COLOR") return dk::colorN(set);
    if (attr == "TEXCOORD") return dk::texCoordN(set);
    if (attr == "JOINTS") return dk::jointsN(set);
    if (attr == "WEIGHTS") return dk::weightsN(set);
    throw std::runtime_error("Unknown attr");
}
}

namespace dk {
ImportResult GltfImporter::import(const std::filesystem::path& file_path, const ImportOptions& opts)
{
    using fastgltf::Asset;
    using fastgltf::Parser;

    ImportResult result;

    fmt::print("Loading GLTF: {}\n", file_path.string());

    Parser parser{};

    constexpr auto gltf_options = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble |
                                  fastgltf::Options::LoadExternalBuffers;

    auto data = fastgltf::GltfDataBuffer::FromPath(file_path);

    /*-------------------------------读取原始数据-------------------------------*/
    Asset gltf;
    if (const auto type = determineGltfFileType(data.get()); type == fastgltf::GltfType::glTF)
    {
        auto load = parser.loadGltf(data.get(), file_path.parent_path(), gltf_options);
        if (load)
        {
            gltf = std::move(load.get());
        }
        else
        {
            fmt::print(stderr, "Failed to load GLTF: {}\n", to_underlying(load.error()));
            return {};
        }
    }
    else if (type == fastgltf::GltfType::GLB)
    {
        auto load = parser.loadGltfBinary(data.get(), file_path.parent_path(), gltf_options);
        if (load)
        {
            gltf = std::move(load.get());
        }
        else
        {
            fmt::print(stderr, "Failed to load GLB: {}\n", to_underlying(load.error()));
            return {};
        }
    }
    else
    {
        fmt::print(stderr, "Failed to determine glTF container\n");
        return {};
    }

    /*-------------------------------生成 AssetNode 树-------------------------------*/
    for (size_t s = 0; s < gltf.scenes.size(); ++s)
    {
        const auto& [nodeIndices, name] = gltf.scenes[s];

        auto scene_root  = std::make_shared<AssetNode>();
        scene_root->kind = AssetKind::Scene;
        scene_root->name = name.empty() ? "Scene" : name;
        scene_root->id   = makeUUID("scene", s);

        for (const auto& ni : nodeIndices)
            scene_root->children.push_back(make_node_recursive(make_node_recursive, gltf, static_cast<uint32_t>(ni)));

        result.nodes.push_back(scene_root);
    }

    fmt::print("{}\n", std::filesystem::current_path().string());
    create_directories(opts.raw_dir);

    /* ---------- 2. 导出 RawMesh ---------- */
    for (size_t mi = 0; mi < gltf.meshes.size(); ++mi)
    {
        for (size_t pi = 0; pi < gltf.meshes[mi].primitives.size(); ++pi)
        {
            const auto&   prim = gltf.meshes[mi].primitives[pi];
            RawMeshHeader raw;

            std::vector<AttrDesc>             vertex_attributes;
            std::vector<std::vector<uint8_t>> blocks;
            std::vector<uint32_t>             indices;

            /*添加索引*/
            if (prim.indicesAccessor)
            {
                auto& acc       = gltf.accessors[prim.indicesAccessor.value()];
                raw.index_count = static_cast<uint32_t>(acc.count);
                copyFromAccessor<std::uint32_t>(gltf, acc, indices.data());
            }

            // 获取顶点数量
            raw.vertex_count = static_cast<uint32_t>(gltf.accessors[prim.findAttribute("POSITION")->accessorIndex].
                count);

            /*添加顶点属性*/
            auto add = [&](const VertexAttribute sem, const uint8_t comp, const uint8_t elems, auto& vec)
            {
                AttrDesc d;
                d.semantic   = static_cast<uint16_t>(sem);
                d.comp_type  = comp;
                d.elem_count = elems;
                d.byte_size  = static_cast<uint32_t>(vec.size());
                vertex_attributes.push_back(d);
                blocks.emplace_back(reinterpret_cast<uint8_t*>(vec.data()),
                                    reinterpret_cast<uint8_t*>(vec.data()) + vec.size());
            };
            // 遍历可能存在多值的属性
            auto visit_set = [&](const char* base, auto sem_of)
            {
                for (int set = 0;; ++set)
                {
                    if (const auto index = prim.findAttribute(std::string(base) + "_" + std::to_string(set));
                        index != prim.attributes.end())
                    {
                        fastgltf::Accessor& accessor = gltf.accessors[index->accessorIndex];
                        std::vector<float>  tmp(
                            accessor.count * getElementByteSize(accessor.type, accessor.componentType));
                        copyFromAccessor<float>(gltf, accessor, tmp.data());
                        add(sem_of(set), 0, accessor.type == fastgltf::AccessorType::Vec3 ? 3 : 4, tmp);
                    }
                    else break;
                }
            };

            // 添加可能的基础属性
            for (auto [attr, set] : {std::pair("POSITION", 0), {"NORMAL", 0}, {"TANGENT", 0}})
            {
                if (prim.findAttribute(attr) != prim.attributes.end())
                {
                    fastgltf::Accessor& accessor = gltf.accessors[prim.findAttribute(attr)->accessorIndex];
                    std::vector<float>  tmp(accessor.count * getElementByteSize(accessor.type, accessor.componentType));
                    copyFromAccessor<float>(gltf, accessor, tmp.data());
                    add(make_sem(attr, set), 0, accessor.type == fastgltf::AccessorType::Vec3 ? 3 : 4, tmp);
                }
            }
            // 添加带编号的属性
            visit_set("TEXCOORD", texCoordN);
            visit_set("COLOR", colorN);
            visit_set("JOINTS", jointsN);
            visit_set("WEIGHTS", weightsN);

            uint32_t offset = static_cast<uint32_t>(sizeof(raw) + vertex_attributes.size() * sizeof(AttrDesc));
            for (auto& d : vertex_attributes)
            {
                d.offset_bytes = offset;
                offset += d.byte_size;
            }

            /* 文件 & meta */
            UUID        uuid  = makeUUID("meshraw", mi * 10 + pi);
            std::string file_name = to_string(uuid) + ".rawmesh";
            if (opts.write_raw)
            {
                auto path = opts.raw_dir / file_name;

                helper::write_pod(path, raw);                        // truncate & write

                helper::append_blob(path, std::span(vertex_attributes));

                /* 3. 写每个属性块（追加）*/
                for (auto& b : blocks)
                    helper::append_blob(path, std::span(b));

                /* 4. 写 indices（追加）*/
                helper::append_blob(path, std::span(indices));
            }
            AssetMeta m;
            m.uuid     = uuid;
            m.importer = "gltf";
            m.rawPath  = file_name;
            if (opts.do_hash)
            {
                m.contentHash = hashBuffer(raw.vertices) ^
                                hashBuffer(std::as_bytes(std::span(raw.indices)));
            }
            R.metas.push_back(std::move(m));
        }
    }

    /* ---------- 3. 导出 RawImage ---------- */
    //for (size_t ii = 0; ii < gltf.images.size(); ++ii)
    //{
    //    auto&    img = gltf.images[ii];
    //    RawImage raw{img.size.width, img.size.height, 4};
    //    raw.pixels.assign(img.data.begin(), img.data.end());

    //    core::UUID  uuid  = makeUUID("img", ii);
    //    std::string fname = uuid.to_string() + ".rawimg";
    //    if (opts.writeRaw) helper::write_blob(opts.rawDir / fname, raw.pixels);

    //    AssetMeta m;
    //    m.uuid     = uuid;
    //    m.importer = "gltf";
    //    m.rawPath  = fname;
    //    if (opts.doHash) m.contentHash = hashBuffer(raw.pixels);
    //    R.metas.push_back(std::move(m));
    //}

    /* ---------- 4. 导出 RawMaterial ---------- */
    //for (size_t mi = 0; mi < gltf.materials.size(); ++mi)
    //{
    //    auto&       mat = gltf.materials[mi];
    //    RawMaterial raw{};
    //    raw.metallicFactor  = mat.pbrData.metallicFactor;
    //    raw.roughnessFactor = mat.pbrData.roughnessFactor;
    //    if (mat.pbrData.baseColorTexture)
    //    {
    //        raw.baseColorTexture = makeUUID("img", mat.pbrData
    //                                                  .baseColorTexture->textureIndex);
    //    }

    //    core::UUID  uuid  = makeUUID("mat", mi);
    //    std::string fname = uuid.to_string() + ".rawmat";
    //    if (opts.writeRaw) writePOD(opts.rawDir / fname, raw);

    //    AssetMeta m;
    //    m.uuid     = uuid;
    //    m.importer = "gltf";
    //    m.rawPath  = fname;
    //    if (mat.pbrData.baseColorTexture)
    //        m.dependencies = {raw.baseColorTexture};
    //    if (opts.doHash)
    //        m.contentHash = hashBuffer(
    //            std::as_bytes(std::span(&raw, 1)));
    //    R.metas.push_back(std::move(m));
    //}
    return result;
}

bool GltfImporter::supportsExtension(const std::string& ext) const
{
    return ext == "gltf" || ext == "glb" || ".gltf" || ".glb";
}
}
