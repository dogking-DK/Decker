#include <fastgltf/glm_element_traits.hpp>  // GLM 类型的 ElementTraits 专门化
#include <fastgltf/types.hpp>
#include <fastgltf/core.hpp>

#include <span>
#include <filesystem>

#include "AssetNode.hpp"
#include "UUID.hpp"
#include "GLTF_Importer.h"

#include <stb_image.h>
#include <fastgltf/tools.hpp>
#include <fmt/core.h>

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

    auto gltf_data = fastgltf::GltfDataBuffer::FromPath(file_path);

    /*-------------------------------读取原始数据-------------------------------*/
    Asset gltf;
    if (const auto type = determineGltfFileType(gltf_data.get()); type == fastgltf::GltfType::glTF)
    {
        auto load = parser.loadGltf(gltf_data.get(), file_path.parent_path(), gltf_options);
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
        auto load = parser.loadGltfBinary(gltf_data.get(), file_path.parent_path(), gltf_options);
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

    /* ---------- 导出 RawMesh ---------- */
    for (size_t mi = 0; mi < gltf.meshes.size(); ++mi)
    {
        for (size_t pi = 0; pi < gltf.meshes[mi].primitives.size(); ++pi)
        {
            const auto&   prim = gltf.meshes[mi].primitives[pi];
            RawMeshHeader raw_mesh_header;

            std::vector<AttrDesc>             vertex_attributes;
            std::vector<std::vector<uint8_t>> blocks;
            std::vector<uint32_t>             indices;

            /*添加索引*/
            if (prim.indicesAccessor)
            {
                auto& acc                   = gltf.accessors[prim.indicesAccessor.value()];
                raw_mesh_header.index_count = static_cast<uint32_t>(acc.count);
                indices.resize(raw_mesh_header.index_count);
                copyFromAccessor<std::uint32_t>(gltf, acc, indices.data());
            }

            // 获取顶点数量
            raw_mesh_header.vertex_count = static_cast<uint32_t>(gltf.accessors[prim.findAttribute("POSITION")->
                    accessorIndex].
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
                        if (accessor.type == fastgltf::AccessorType::Vec2)
                            copyFromAccessor<glm::vec2>(gltf, accessor, tmp.data());
                        else if (accessor.type == fastgltf::AccessorType::Vec3)
                            copyFromAccessor<glm::vec3>(gltf, accessor, tmp.data());
                        else if (accessor.type == fastgltf::AccessorType::Vec4)
                            copyFromAccessor<glm::vec4>(gltf, accessor, tmp.data());
                        else if (accessor.type == fastgltf::AccessorType::Scalar)
                            copyFromAccessor<float>(gltf, accessor, tmp.data());

                        add(sem_of(set), 0, accessor.type == fastgltf::AccessorType::Vec3 ? 3 : 4, tmp);
                        ++raw_mesh_header.attr_count;
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
                    if (accessor.type == fastgltf::AccessorType::Vec3)
                        copyFromAccessor<glm::vec3>(gltf, accessor, tmp.data());
                    else if (accessor.type == fastgltf::AccessorType::Vec4)
                        copyFromAccessor<glm::vec4>(gltf, accessor, tmp.data());

                    add(make_sem(attr, set), 0, accessor.type == fastgltf::AccessorType::Vec3 ? 3 : 4, tmp);
                    ++raw_mesh_header.attr_count;
                }
            }
            // 添加带编号的属性
            visit_set("TEXCOORD", texCoordN);
            visit_set("COLOR", colorN);
            visit_set("JOINTS", jointsN);
            visit_set("WEIGHTS", weightsN);

            uint32_t offset = static_cast<uint32_t>(sizeof(raw_mesh_header) + vertex_attributes.size() * sizeof(AttrDesc));
            for (auto& d : vertex_attributes)
            {
                d.offset_bytes = offset;
                offset += d.byte_size;
            }

            /* 文件 & meta */
            UUID        uuid      = makeUUID("meshraw", mi * 10 + pi);
            std::string file_name = to_string(uuid) + ".rawmesh";
            if (opts.write_raw)
            {
                auto path = opts.raw_dir / file_name;
                // 先写入mesh 头
                helper::write_pod(path, raw_mesh_header);
                // 再写入属性描述
                helper::append_blob(path, std::span(vertex_attributes));
                // 写入每个顶点属性
                for (auto& b : blocks)
                    helper::append_blob(path, std::span(b));
                // 最后写入索引
                helper::append_blob(path, std::span(indices));
            }
            AssetMeta m;
            m.uuid     = uuid;
            m.importer = "gltf";
            m.raw_path  = file_name;
            m.dependencies.insert_or_assign("material", makeUUID("mat", prim.materialIndex.value())); // 关联材质
            if (opts.do_hash)
            {
                m.content_hash = helper::hash_buffer(indices);
                for (auto& b : blocks)
                    m.content_hash ^= helper::hash_buffer(b);
            }
            result.metas.push_back(std::move(m));
        }
    }

    /* ---------- 3. 导出 RawImage ---------- */
    for (size_t ii = 0; ii < gltf.images.size(); ++ii)
    {
        auto&                [data, name] = gltf.images[ii];
        RawImageHeader       raw;
        std::vector<uint8_t> pixels;

        std::visit(
            fastgltf::visitor{
                [&](fastgltf::sources::URI& filePath)
                {
                    assert(filePath.fileByteOffset == 0); // We don't support offsets with stbi.
                    assert(filePath.uri.isLocalPath()); // We're only capable of loading

                    // 生成绝对路径
                    auto path(file_path / "");
                    path.append(filePath.uri.path());
                    if (filePath.uri.isLocalPath()) fmt::print("image path: {}\n", path.generic_string());

                    if (name.empty())
                    {
                        name = filePath.uri.path();
                        fmt::print("generate image name({})\n", name);
                    }

                    unsigned char* image_data = stbi_load(path.generic_string().c_str(), &raw.width, &raw.height,
                                                          &raw.channels, 4);
                    if (image_data)
                    {
                        pixels.assign(image_data, image_data + raw.width * raw.height * 4);

                        stbi_image_free(image_data);
                    }
                },
                [&](fastgltf::sources::Vector& vector)
                {
                    unsigned char* image_data = stbi_load_from_memory(
                        reinterpret_cast<const stbi_uc*>(vector.bytes.data()),
                        static_cast<int>(vector.bytes.size()),
                        &raw.width, &raw.height, &raw.channels, 4);
                    if (image_data)
                    {
                        pixels.assign(image_data, image_data + raw.width * raw.height * 4);

                        stbi_image_free(image_data);
                    }
                },
                [&](fastgltf::sources::BufferView& view)
                {
                    auto& bufferView = gltf.bufferViews[view.bufferViewIndex];
                    auto& buffer     = gltf.buffers[bufferView.bufferIndex];

                    std::visit(fastgltf::visitor{
                                   // We only care about VectorWithMime here, because we
                                   // specify LoadExternalBuffers, meaning all buffers
                                   // are already loaded into a vector.
                                   [](auto& arg)
                                   {
                                   },
                                   [&](fastgltf::sources::Vector& vector)
                                   {
                                       unsigned char* image_data = stbi_load_from_memory(
                                           reinterpret_cast<const stbi_uc*>(
                                               vector.bytes.data() + bufferView.byteOffset),
                                           static_cast<int>(bufferView.byteLength),
                                           &raw.width, &raw.height, &raw.channels, 4);
                                       if (image_data)
                                       {
                                           pixels.assign(image_data, image_data + raw.width * raw.height * 4);

                                           stbi_image_free(image_data);
                                       }
                                   }
                               },
                               buffer.data);
                },
                [](auto& arg)
                {
                    fmt::print(stderr, "the image data source is not support yet\n");
                }
            },
            data);

        UUID        uuid      = makeUUID("img", ii);
        std::string file_name = to_string(uuid) + ".rawimg";
        if (opts.write_raw)
        {
            helper::write_pod(opts.raw_dir / file_name, raw);
            helper::append_blob(opts.raw_dir / file_name, std::span(pixels));
        }

        AssetMeta m;
        m.uuid     = uuid;
        m.importer = "gltf";
        m.raw_path  = file_name;
        if (opts.do_hash) m.content_hash = helper::hash_buffer(pixels);
        result.metas.push_back(std::move(m));
    }

    /* ---------- 4. 导出 RawMaterial ---------- */
    for (size_t mi = 0; mi < gltf.materials.size(); ++mi)
    {
        auto&       mat = gltf.materials[mi];
        RawMaterial raw{};
        AssetMeta m;

        raw.metallic_factor  = mat.pbrData.metallicFactor;
        raw.roughness_factor = mat.pbrData.roughnessFactor;
        if (mat.pbrData.baseColorTexture)
        {
            raw.base_color_texture = makeUUID("img", mat.pbrData.baseColorTexture->textureIndex);
            m.dependencies.insert_or_assign("baseColorTexture", raw.base_color_texture);
        }
        if (mat.pbrData.metallicRoughnessTexture)
        {
            raw.metal_rough_texture = makeUUID("img", mat.pbrData.metallicRoughnessTexture->textureIndex);
            m.dependencies.insert_or_assign("metallicRoughnessTexture",raw.metal_rough_texture);
        }
        if (mat.normalTexture)
        {
            raw.normal_texture = makeUUID("img", mat.normalTexture->textureIndex);
            m.dependencies.insert_or_assign("normalTexture",raw.normal_texture);
        }
        if (mat.occlusionTexture)
        {
            raw.occlusion_texture = makeUUID("img", mat.occlusionTexture->textureIndex);
            m.dependencies.insert_or_assign("occlusionTexture",raw.occlusion_texture);
        }
        if (mat.emissiveTexture)
        {
            raw.emissive_texture = makeUUID("img", mat.emissiveTexture->textureIndex);
            m.dependencies.insert_or_assign("emissiveTexture", raw.emissive_texture);
        }
        UUID        uuid      = makeUUID("mat", mi);
        std::string file_name = to_string(uuid) + ".rawmat";
        if (opts.write_raw) helper::write_pod(opts.raw_dir / file_name, raw);

        m.uuid = uuid;
        m.importer = "gltf";
        m.raw_path = file_name;
        if (opts.do_hash)
        {
            m.content_hash = helper::hash_buffer(std::as_bytes(std::span(&raw, 1)));
        }
        result.metas.push_back(std::move(m));
    }
    return result;
}

bool GltfImporter::supportsExtension(const std::string& ext) const
{
    return ext == "gltf" || ext == "glb" || ext == ".gltf" || ext == ".glb";
}
}
