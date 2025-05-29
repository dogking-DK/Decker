#pragma once
//=== 1. ������� ==============================================
#include <flecs.h>
#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/util.hpp>

#include <vulkan/vulkan.hpp>

// �󶨵� fastgltf::primitive ������
struct GltfPrimitiveRef
{
    //fastgltf::Primitive const* prim;
    int index;
};

// CPU �࿽��/��ͼ
struct CpuVertexData
{
    std::vector<float> positions;  // interleaved ���� �������� vector
    std::vector<float> normals;
    std::vector<float> uvs;
};

struct CpuIndexData
{
    std::vector<uint32_t> indices;
};

// ���ʲ���
struct CpuMaterialData
{
    int       baseColorTextureIndex;    // �� fastgltf::model.textures �е�����
    int       metallicRoughnessTextureIndex;
    glm::vec4 baseColorFactor;
    float     metallicFactor, roughnessFactor;
    // �����ߡ��ڵ�����������������
};

// GPU ��Դ�������
struct GpuMesh
{
    vk::Buffer vertexBuffer;
    vk::Buffer indexBuffer;
    uint32_t   indexCount;
};

struct GpuMaterial
{
    vk::DescriptorSet descriptorSet;  // ���� UniformBuffer & �������� sampler
};

//=== 2. ����ϵͳ���� fastgltf Model ���� ECS ʵ�� ==============
void importPrimitives(flecs::world& ecs, const fastgltf::Asset& model)
{
    for (const auto& mesh : model.meshes)
    {
        for (const auto& prim : mesh.primitives)
        {
            // ����һ��ʵ��
            auto e = ecs.entity();
            // 1) ��¼ primitive ���ã��������ֱ�ӷ���ԭʼ����
            e.set<GltfPrimitiveRef>({prim.});

            // 2) �� accessor ��������� CPU ���ݿ��������
            CpuVertexData vcpu;
            vcpu.positions = prim.get_positions();
            if (prim.get_normals().size()) vcpu.normals = prim.get_normals();
            if (prim.get_texcoords(0).size()) vcpu.uvs = prim.get_texcoords(0);
            e.set<CpuVertexData>(std::move(vcpu));

            CpuIndexData icpu;
            icpu.indices = prim.get_indices();
            e.set<CpuIndexData>(std::move(icpu));
            e.child_of()
            // 3) �� primitive.material �����һ�� CPU �������
            CpuMaterialData mcpu;
            const auto&     mat                = model.materials[prim.material];
            mcpu.baseColorTextureIndex         = mat.pbrMetallicRoughness.baseColorTexture.index;
            mcpu.metallicRoughnessTextureIndex =
                mat.pbrMetallicRoughness.metallicRoughnessTexture.index;
            mcpu.baseColorFactor = glm::make_vec4(mat.pbrMetallicRoughness.baseColorFactor.data());
            mcpu.metallicFactor  = mat.pbrMetallicRoughness.metallicFactor;
            mcpu.roughnessFactor = mat.pbrMetallicRoughness.roughnessFactor;
            e.set<CpuMaterialData>(std::move(mcpu));
        }
    }

    for (fastgltf::Mesh& mesh : gltf.meshes)
    {
        auto newmesh = std::make_shared<MeshAsset>();
        meshes.push_back(newmesh);
        file.meshes[mesh.name.c_str()] = newmesh;
        newmesh->name = mesh.name;
        fmt::print("loading mesh: {}\n", mesh.name);
        // clearSwapchain the mesh arrays each mesh, we dont want to merge them by error
        indices.clear();
        vertices.clear();
        std::size_t idx = 0;
        fmt::print("primitive iterating...\n");
        for (auto&& p : mesh.primitives)
        {
            GeoSurface newSurface;
            newSurface.startIndex = static_cast<uint32_t>(indices.size());
            newSurface.count = static_cast<uint32_t>(gltf.accessors[p.indicesAccessor.value()].count);
            fmt::print("{}: {}\n", idx, magic_enum::enum_name(p.type));
            size_t initial_vtx = vertices.size();
            size_t initial_idx = indices.size();

            // load indexes
            {
                fastgltf::Accessor& indexaccessor = gltf.accessors[p.indicesAccessor.value()];
                indices.reserve(indices.size() + indexaccessor.count);

                fastgltf::iterateAccessor<std::uint32_t>(gltf, indexaccessor,
                    [&](std::uint32_t idx)
                    {
                        indices.push_back(idx + initial_vtx);
                    });
            }

            // load vertex positions
            {
                fastgltf::Accessor& posAccessor = gltf.accessors[p.findAttribute("POSITION")->accessorIndex];
                vertices.resize(vertices.size() + posAccessor.count);

                flecs::entity e;

                fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, posAccessor,
                    [&](glm::vec3 v, size_t index)
                    {
                        Vertex newvtx;
                        newvtx.position = v;
                        newvtx.normal = { 1, 0, 0 };
                        newvtx.color = glm::vec4{ 1.f };
                        newvtx.uv_x = 0;
                        newvtx.uv_y = 0;
                        vertices[initial_vtx + index] = newvtx;
                    });
            }

            // load vertex normals
            auto normals = p.findAttribute("NORMAL");
            if (normals != p.attributes.end())
            {
                fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, gltf.accessors[normals->accessorIndex],
                    [&](glm::vec3 v, size_t index)
                    {
                        vertices[initial_vtx + index].normal = v;
                    });
            }

            // load UVs
            auto uv = p.findAttribute("TEXCOORD_0");
            if (uv != p.attributes.end())
            {
                fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf, gltf.accessors[uv->accessorIndex],
                    [&](glm::vec2 v, size_t index)
                    {
                        vertices[initial_vtx + index].uv_x = v.x;
                        vertices[initial_vtx + index].uv_y = v.y;
                    });
            }

            // load vertex colors
            auto colors = p.findAttribute("COLOR_0");
            if (colors != p.attributes.end())
            {
                fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf, gltf.accessors[colors->accessorIndex],
                    [&](glm::vec4 v, size_t index)
                    {
                        vertices[initial_vtx + index].color = v;
                    });
            }

            if (p.materialIndex.has_value())
            {
                newSurface.material = materials[p.materialIndex.value()];
            }
            else
            {
                newSurface.material = materials[0];
            }

            glm::vec3 minpos = vertices[initial_vtx].position;
            glm::vec3 maxpos = vertices[initial_vtx].position;
            for (int i = initial_vtx; i < vertices.size(); i++)
            {
                minpos = min(minpos, vertices[i].position);
                maxpos = max(maxpos, vertices[i].position);
            }

            newSurface.bounds.max_edge = maxpos;
            newSurface.bounds.min_edge = minpos;
            newSurface.bounds.origin = (maxpos + minpos) / 2.f;
            newSurface.bounds.extents = (maxpos - minpos) / 2.f;
            newSurface.bounds.sphereRadius = length(newSurface.bounds.extents);
            newmesh->surfaces.push_back(newSurface);
            ++idx;
        }

        newmesh->meshBuffers = engine->uploadMesh(indices, vertices);
    }
}
