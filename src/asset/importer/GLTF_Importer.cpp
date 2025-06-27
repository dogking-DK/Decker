#include <fastgltf/types.hpp>
#include <fastgltf/core.hpp>

#include "AssetNode.hpp"
#include "UUID.hpp"
#include "GLTF_Importer.h"

#include <fmt/core.h>

using fastgltf::Asset;
using fastgltf::Parser;

namespace {
dk::UUID makeUUID(const std::string& tag, size_t idx)
{
    return dk::uuid_from_string(tag + std::to_string(idx));
}

auto make_node_recursive =
    [](auto self, const Asset& asset, uint32_t nodeIdx) -> std::shared_ptr<dk::AssetNode>
{
    const auto& n    = asset.nodes[nodeIdx];
    auto        node = std::make_shared<dk::AssetNode>();
    node->kind       = dk::AssetKind::Node;
    node->name       = n.name.empty() ? "Node" : n.name;
    node->id         = makeUUID("node", nodeIdx);

    /* Mesh 依赖 */
    if (n.meshIndex.has_value())
    {
        uint32_t mi   = n.meshIndex.value();
        auto     mesh = std::make_shared<dk::AssetNode>();
        mesh->kind    = dk::AssetKind::Mesh;
        mesh->name    = asset.meshes[mi].name.empty() ? "Mesh" : asset.meshes[mi].name;
        mesh->id      = makeUUID("mesh", mi);

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
    for (uint32_t child : n.children)
        node->children.push_back(self(self, asset, child));

    return node;
};
}

namespace dk {
std::vector<std::shared_ptr<AssetNode>> GltfImporter::import(const std::filesystem::path& file_path)
{
    fmt::print("Loading GLTF: {}\n", file_path.string());

    Parser parser{};

    constexpr auto gltfOptions = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble |
                                 fastgltf::Options::LoadExternalBuffers;
    // fastgltf::Options::LoadExternalImages;

    auto data = fastgltf::GltfDataBuffer::FromPath(file_path);

    Asset gltf;

    std::filesystem::path path = file_path;

    // 读取到原始数据
    auto type = determineGltfFileType(data.get());
    if (type == fastgltf::GltfType::glTF)
    {
        auto load = parser.loadGltf(data.get(), path.parent_path(), gltfOptions);
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
        auto load = parser.loadGltfBinary(data.get(), path.parent_path(), gltfOptions);
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

    /* 2️⃣ 生成 AssetNode 树 */
    std::vector<std::shared_ptr<AssetNode>> roots;
    for (size_t s = 0; s < gltf.scenes.size(); ++s)
    {
        const auto& sc = gltf.scenes[s];

        auto sceneRoot  = std::make_shared<AssetNode>();
        sceneRoot->kind = AssetKind::Scene;
        sceneRoot->name = sc.name.empty() ? "Scene" : sc.name;
        sceneRoot->id   = makeUUID("scene", s);

        for (uint32_t ni : sc.nodeIndices)
            sceneRoot->children.push_back(make_node_recursive(make_node_recursive,
                                                              gltf, ni));
        roots.push_back(sceneRoot);
    }
    return roots;
}

bool GltfImporter::supportsExtension(const std::string& ext) const
{
    return ext == "gltf" || ext == "glb" || ".gltf" || ".glb";
}
}
