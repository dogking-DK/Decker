#include "vk_types.h"
#include "Transform.h"

#include <fastgltf/glm_element_traits.hpp>  // GLM 类型的 ElementTraits 专门化
#include <fastgltf/types.hpp>
#include <fastgltf/core.hpp>

#include <span>
#include <filesystem>
#include <cstdint>
#include <algorithm>

#include "Prefab.hpp"
#include "UUID.hpp"
#include "GLTF_Importer.h"

#include <stb_image.h>
#include <fastgltf/tools.hpp>
#include <fmt/core.h>

#include "AssetHelpers.hpp"
#include "RawTypes.hpp"
#include <fstream>
#include <optional>

namespace {
struct GltfUuidCache
{
    std::vector<dk::UUID>                 image_uuids;
    std::vector<dk::UUID>                 material_uuids;
    std::vector<std::vector<dk::UUID>>    mesh_uuids; // [mesh][primitive]
};

dk::Transform to_transform(const fastgltf::Node& node)
{
    dk::Transform t{};
    std::visit(fastgltf::visitor{
                   [&](fastgltf::math::fmat4x4 matrix)
                   {
                       glm::mat4 m{1.0f};
                       std::memcpy(glm::value_ptr(m), matrix.data(), sizeof(glm::mat4));

                       glm::vec3    skew{};
                       glm::vec4    perspective{};
                       glm::decompose(m, t.scale, t.rotation, t.translation, skew, perspective);
                   },
                   [&](fastgltf::TRS transform)
                   {
                       t.translation = {transform.translation[0], transform.translation[1], transform.translation[2]};
                       t.rotation    = {transform.rotation[3], transform.rotation[0], transform.rotation[1],
                                        transform.rotation[2]};
                       t.scale       = {transform.scale[0], transform.scale[1], transform.scale[2]};
                   }
               },
               node.transform);
    return t;
}

dk::PrefabNode make_prefab_node_recursive(const fastgltf::Asset& asset, const GltfUuidCache& cache,
                                          const uint32_t node_idx)
{
    const auto& n  = asset.nodes[node_idx];
    dk::PrefabNode node{};
    node.kind            = dk::AssetKind::Node;
    node.name            = n.name.empty() ? "Node" : n.name;
    node.id              = dk::uuid_generate();
    node.local_transform = to_transform(n);

    if (n.meshIndex.has_value())
    {
        const uint32_t mi       = static_cast<uint32_t>(n.meshIndex.value());
        const auto&    gltf_mesh = asset.meshes[mi];

        dk::PrefabNode mesh{};
        mesh.kind = dk::AssetKind::Mesh;
        mesh.name = gltf_mesh.name.empty() ? "Mesh" : gltf_mesh.name;
        mesh.id   = dk::uuid_generate();

        for (size_t pi = 0; pi < gltf_mesh.primitives.size(); ++pi)
        {
            dk::PrefabNode primitive{};
            primitive.kind = dk::AssetKind::Primitive;
            primitive.name = fmt::format("Surface {}", pi);
            primitive.id   = cache.mesh_uuids[mi][pi];
            mesh.children.push_back(std::move(primitive));
        }
        node.children.push_back(std::move(mesh));
    }

    for (const auto& child : n.children)
    {
        node.children.push_back(make_prefab_node_recursive(asset, cache, static_cast<uint32_t>(child)));
    }

    return node;
}

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

std::optional<fastgltf::Asset> load_gltf_asset(const std::filesystem::path& file_path)
{
    using fastgltf::Asset;
    using fastgltf::Parser;

    Parser parser{};

    constexpr auto gltf_options = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble |
                                  fastgltf::Options::LoadExternalBuffers;

    auto gltf_data = fastgltf::GltfDataBuffer::FromPath(file_path);

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
            return std::nullopt;
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
            return std::nullopt;
        }
    }
    else
    {
        fmt::print(stderr, "Failed to determine glTF container\n");
        return std::nullopt;
    }

    return gltf;
}

void export_prefabs(const fastgltf::Asset& gltf, const GltfUuidCache& cache, const dk::ImportOptions& opts,
                    dk::ImportResult& result)
{
    for (size_t s = 0; s < gltf.scenes.size(); ++s)
    {
        const auto& [nodeIndices, name] = gltf.scenes[s];

        dk::PrefabNode scene_root{};
        scene_root.kind = dk::AssetKind::Scene;
        scene_root.name = name.empty() ? "Scene" : name;
        scene_root.id   = dk::uuid_generate();

        for (const auto& ni : nodeIndices)
        {
            scene_root.children.push_back(make_prefab_node_recursive(gltf, cache, static_cast<uint32_t>(ni)));
        }

        dk::UUID    uuid      = dk::uuid_generate();
        std::string file_name = to_string(uuid) + ".prefab";

        if (opts.write_raw)
        {
            nlohmann::json j = scene_root;
            std::ofstream   os(opts.raw_dir / file_name);
            os << j.dump(2);
        }

        dk::AssetMeta m;
        m.uuid     = uuid;
        m.asset_type = dk::AssetType::Prefab;
        m.importer = "gltf";
        m.raw_path = file_name;

        if (opts.do_hash)
        {
            nlohmann::json j = scene_root;
            const auto     dump = j.dump();
            m.content_hash     = dk::helper::hash_buffer(reinterpret_cast<const uint8_t*>(dump.data()), dump.size());
        }

        result.metas.push_back(std::move(m));
    }
}

void export_raw_meshes(const fastgltf::Asset& gltf, const GltfUuidCache& cache, const dk::ImportOptions& opts,
                       dk::ImportResult& result)
{
    for (size_t mi = 0; mi < gltf.meshes.size(); ++mi)
    {
        for (size_t pi = 0; pi < gltf.meshes[mi].primitives.size(); ++pi)
        {
            const auto&       prim = gltf.meshes[mi].primitives[pi];
            dk::RawMeshHeader raw_mesh_header{};

            std::vector<dk::AttrDesc>         vertex_attributes;
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
            auto add = [&](const dk::VertexAttribute sem, const uint8_t comp, const uint8_t elems, auto& vec)
            {
                dk::AttrDesc d;
                d.semantic   = sem;
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
                        const fastgltf::Accessor& accessor = gltf.accessors[index->accessorIndex];

                        auto components = [&]()
                        {
                            switch (accessor.type)
                            {
                            case fastgltf::AccessorType::Vec2:
                                return 2;
                            case fastgltf::AccessorType::Vec3:
                                return 3;
                            case fastgltf::AccessorType::Vec4:
                                return 4;
                            case fastgltf::AccessorType::Scalar:
                                return 1;
                            case fastgltf::AccessorType::Invalid:
                                return 0;
                            case fastgltf::AccessorType::Mat2:
                                return 4;
                            case fastgltf::AccessorType::Mat3:
                                return 9;
                            case fastgltf::AccessorType::Mat4:
                                return 16;
                            }
                        }();

                        std::vector<float> tmp(accessor.count * components);
                        if (accessor.type == fastgltf::AccessorType::Vec2)
                            copyFromAccessor<glm::vec2>(gltf, accessor, tmp.data());
                        else if (accessor.type == fastgltf::AccessorType::Vec3)
                            copyFromAccessor<glm::vec3>(gltf, accessor, tmp.data());
                        else if (accessor.type == fastgltf::AccessorType::Vec4)
                            copyFromAccessor<glm::vec4>(gltf, accessor, tmp.data());
                        else if (accessor.type == fastgltf::AccessorType::Scalar)
                            copyFromAccessor<float>(gltf, accessor, tmp.data());

                        add(sem_of(set), 0, static_cast<uint8_t>(components), tmp);
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
                    const fastgltf::Accessor& accessor = gltf.accessors[prim.findAttribute(attr)->accessorIndex];
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
            visit_set("TEXCOORD", dk::texCoordN);
            visit_set("COLOR", dk::colorN);
            visit_set("JOINTS", dk::jointsN);
            visit_set("WEIGHTS", dk::weightsN);

            uint32_t offset = static_cast<uint32_t>(
                sizeof(raw_mesh_header) + vertex_attributes.size() * sizeof(dk::AttrDesc));
            for (auto& d : vertex_attributes)
            {
                d.offset_bytes = offset;
                offset += d.byte_size;
            }

            /* 文件 & meta */
            dk::UUID    uuid      = cache.mesh_uuids[mi][pi];
            std::string file_name = to_string(uuid) + ".rawmesh";
            if (opts.write_raw)
            {
                auto path = opts.raw_dir / file_name;
                // 先写入mesh 头
                dk::helper::write_pod(path, raw_mesh_header);
                // 再写入属性描述
                dk::helper::append_blob(path, std::span(vertex_attributes));
                // 写入每个顶点属性
                for (auto& b : blocks)
                    dk::helper::append_blob(path, std::span(b));
                // 最后写入索引
                dk::helper::append_blob(path, std::span(indices));
            }
            dk::AssetMeta m;
            m.uuid     = uuid;
            m.asset_type = dk::AssetType::Mesh;
            m.importer = "gltf";
            m.raw_path = file_name;
            if (prim.materialIndex.has_value())
                m.dependencies.insert_or_assign(dk::AssetDependencyType::Material,
                    cache.material_uuids[prim.materialIndex.value()]);
            if (opts.do_hash)
            {
                m.content_hash = dk::helper::hash_buffer(indices);
                for (auto& b : blocks)
                    m.content_hash ^= dk::helper::hash_buffer(b);
            }
            result.metas.push_back(std::move(m));
        }
    }
}

void export_raw_images(const fastgltf::Asset&   gltf, const GltfUuidCache& cache,
                       const std::filesystem::path& file_path, const dk::ImportOptions& opts,
                       dk::ImportResult&            result)
{
    for (size_t ii = 0; ii < gltf.images.size(); ++ii)
    {
        auto& [data, name] = gltf.images[ii];

        std::string image_name{ name };
        dk::RawImageHeader raw{};
        raw.comp_type = dk::PixelDataType::UBYTE;
        std::vector<uint8_t> pixels;

        std::visit(
            fastgltf::visitor{
                [&](const fastgltf::sources::URI& filePath)
                {
                    assert(filePath.fileByteOffset == 0); // We don't support offsets with stbi.
                    assert(filePath.uri.isLocalPath()); // We're only capable of loading

                    // 生成绝对路径
                    auto path(file_path.parent_path() / filePath.uri.path());
                    //if (filePath.uri.isLocalPath()) fmt::print("image path: {}\n", path.generic_string());

                    if (name.empty())
                    {
                        image_name = filePath.uri.path();
                        //fmt::print("generate image name({})\n", name);
                    }

                    unsigned char* image_data = stbi_load(path.generic_string().c_str(), &raw.width, &raw.height, &raw.channels, 4);
                    if (image_data)
                    {
                        pixels.assign(image_data, image_data + raw.width * raw.height * 4);

                        stbi_image_free(image_data);
                    }
                },
                [&](const fastgltf::sources::Vector& vector)
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
                [&](const fastgltf::sources::BufferView& view)
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

        if (pixels.empty())
        {
            fmt::print(stderr, "Failed to decode image {}\n", image_name);
            continue;
        }

        dk::UUID    uuid      = cache.image_uuids[ii];
        std::string file_name = to_string(uuid) + ".rawimg";
        if (opts.write_raw)
        {
            dk::helper::write_pod(opts.raw_dir / file_name, raw);
            dk::helper::append_blob(opts.raw_dir / file_name, std::span(pixels));
        }

        dk::AssetMeta m;
        m.uuid     = uuid;
        m.asset_type = dk::AssetType::Image;
        m.importer = "gltf";
        m.raw_path = file_name;
        if (opts.do_hash) m.content_hash = dk::helper::hash_buffer(pixels);
        result.metas.push_back(std::move(m));
    }
}

void export_raw_materials(const fastgltf::Asset& gltf, const GltfUuidCache& cache, const dk::ImportOptions& opts,
                          dk::ImportResult& result)
{
    fmt::print("---------------------------\n");
    for (size_t mi = 0; mi < gltf.materials.size(); ++mi)
    {
        auto&           mat = gltf.materials[mi];
        dk::RawMaterial raw{};
        dk::AssetMeta   m;

        auto texture_to_uuid = [&](const auto& texture_info) -> dk::UUID
        {
            if (!texture_info) return {};
            if (texture_info->textureIndex >= gltf.textures.size()) return {};
            const auto& texture = gltf.textures[texture_info->textureIndex];
            if (!texture.imageIndex.has_value()) return {};
            return cache.image_uuids[texture.imageIndex.value()];
        };

        raw.metallic_factor  = mat.pbrData.metallicFactor;
        raw.roughness_factor = mat.pbrData.roughnessFactor;
        if (mat.pbrData.baseColorTexture)
        {
            raw.base_color_texture = texture_to_uuid(mat.pbrData.baseColorTexture);
            if (!raw.base_color_texture.is_nil())
                m.dependencies.insert_or_assign(dk::AssetDependencyType::BaseColorTexture, raw.base_color_texture);
        }
        if (mat.pbrData.metallicRoughnessTexture)
        {
            raw.metal_rough_texture = texture_to_uuid(mat.pbrData.metallicRoughnessTexture);
            if (!raw.metal_rough_texture.is_nil())
                m.dependencies.insert_or_assign(dk::AssetDependencyType::MetallicRoughnessTexture, raw.metal_rough_texture);
        }
        if (mat.normalTexture)
        {
            raw.normal_texture = texture_to_uuid(mat.normalTexture);
            if (!raw.normal_texture.is_nil())
                m.dependencies.insert_or_assign(dk::AssetDependencyType::NormalTexture, raw.normal_texture);
        }
        if (mat.occlusionTexture)
        {
            raw.occlusion_texture = texture_to_uuid(mat.occlusionTexture);
            if (!raw.occlusion_texture.is_nil())
                m.dependencies.insert_or_assign(dk::AssetDependencyType::OcclusionTexture, raw.occlusion_texture);
        }
        if (mat.emissiveTexture)
        {
            raw.emissive_texture = texture_to_uuid(mat.emissiveTexture);
            if (!raw.emissive_texture.is_nil())
                m.dependencies.insert_or_assign(dk::AssetDependencyType::EmissiveTexture, raw.emissive_texture);
        }
        dk::UUID uuid = cache.material_uuids[mi];
        //fmt::print("material {}: {}\n", mi, to_string(uuid));
        std::string file_name = to_string(uuid) + ".rawmat";
        if (opts.write_raw) dk::helper::write_pod(opts.raw_dir / file_name, raw);

        m.uuid     = uuid;
        m.asset_type = dk::AssetType::Material;
        m.importer = "gltf";
        m.raw_path = file_name;
        if (opts.do_hash)
        {
            m.content_hash = dk::helper::hash_buffer(std::as_bytes(std::span(&raw, 1)));
        }
        result.metas.push_back(std::move(m));
    }
}
}

namespace dk {
ImportResult GltfImporter::import(const std::filesystem::path& file_path, const ImportOptions& opts)
{
    ImportResult result;
    result.raw_dir = opts.raw_dir;

    fmt::print("Loading GLTF: {}\n", file_path.string());

    auto gltf = load_gltf_asset(file_path);
    if (!gltf) return {};

    // 仅生成资源节点树
    if (opts.only_nodes) return result;

    fmt::print("当前目录: {}\n", std::filesystem::current_path().string());
    create_directories(opts.raw_dir);

    GltfUuidCache uuid_cache;
    uuid_cache.image_uuids.resize(gltf->images.size());
    std::ranges::generate(uuid_cache.image_uuids, dk::uuid_generate);
    uuid_cache.material_uuids.resize(gltf->materials.size());
    std::ranges::generate(uuid_cache.material_uuids, dk::uuid_generate);
    uuid_cache.mesh_uuids.resize(gltf->meshes.size());
    for (size_t mi = 0; mi < gltf->meshes.size(); ++mi)
    {
        uuid_cache.mesh_uuids[mi].resize(gltf->meshes[mi].primitives.size());
        std::ranges::generate(uuid_cache.mesh_uuids[mi], dk::uuid_generate);
    }

    export_prefabs(*gltf, uuid_cache, opts, result); // 导出 prefab
    export_raw_meshes(*gltf, uuid_cache, opts, result); // 导出raw mesh
    export_raw_images(*gltf, uuid_cache, file_path, opts, result); // 导出raw image
    export_raw_materials(*gltf, uuid_cache, opts, result); // 导出raw material

    for (size_t mi = 0; mi < gltf->meshes.size(); ++mi)
    {
        for (size_t pi = 0; pi < gltf->meshes[mi].primitives.size(); ++pi)
        {
            fmt::print("prim: {}\n", to_string(uuid_cache.mesh_uuids[mi][pi]));
        }
    }
    fmt::print("-------------------------meta info--------------------\n");
    for (auto meta : result.metas)
    {
        fmt::print("{}: {}\n", to_string(meta.asset_type.value()), to_string(meta.uuid));
    }
    return result;
}

bool GltfImporter::supportsExtension(const std::string& ext) const
{
    return ext == "gltf" || ext == "glb" || ext == ".gltf" || ext == ".glb";
}
}
