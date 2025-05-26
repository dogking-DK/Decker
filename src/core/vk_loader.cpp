#include <iostream>
#include <typeinfo>
#include <vk_loader.h>

#include "vk_engine.h"
#include "vk_initializers.h"
#include "vk_types.h"
#include <glm/gtx/quaternion.hpp>

#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/util.hpp>

#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#endif
#include <nlohmann/json.hpp>

#include "stb_image.h"
template <>
struct fmt::formatter<glm::vec3>
{
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

    template <typename Context>
    constexpr auto format(const glm::vec3& foo, Context& ctx) const
    {
        return format_to(ctx.out(), "[{:7.2f}, {:7.2f}, {:7.2f}]", foo.x, foo.y, foo.z);
    }
};
namespace dk {
static std::filesystem::path parent_path;

template <typename Variant>
static std::string get_variant_type(const Variant& var)
{
    return std::visit([](const auto& value)
    {
        return std::string(typeid(value).name());
    }, var);
}

std::optional<AllocatedImage> load_image(VulkanEngine* engine, fastgltf::Asset& asset, fastgltf::Image& image)
{
    fmt::print("image type({}) name({})\n", get_variant_type(image.data), image.name);

    AllocatedImage newImage{};

    int width, height, nrChannels;

    std::visit(
        fastgltf::visitor{
            [&](fastgltf::sources::URI& filePath)
            {
                assert(filePath.fileByteOffset == 0); // We don't support offsets with stbi.
                assert(filePath.uri.isLocalPath()); // We're only capable of loading

                // 生成绝对路径
                auto path(parent_path);
                path.append(filePath.uri.path());
                if (filePath.uri.isLocalPath()) fmt::print("image path: {}\n", path.generic_string());

                if (image.name.empty())
                {
                    image.name = filePath.uri.path();
                    fmt::print("generate image name({})\n", image.name);
                }

                unsigned char* data = stbi_load(path.generic_string().c_str(), &width, &height, &nrChannels, 4);
                if (data)
                {
                    VkExtent3D imagesize;
                    imagesize.width  = width;
                    imagesize.height = height;
                    imagesize.depth  = 1;

                    newImage = engine->create_image(data, imagesize, VK_FORMAT_R8G8B8A8_UNORM,
                                                    VK_IMAGE_USAGE_SAMPLED_BIT, false);

                    stbi_image_free(data);
                }
            },
            [&](fastgltf::sources::Vector& vector)
            {
                unsigned char* data = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(vector.bytes.data()),
                                                            static_cast<int>(vector.bytes.size()),
                                                            &width, &height, &nrChannels, 4);
                if (data)
                {
                    VkExtent3D imagesize;
                    imagesize.width  = width;
                    imagesize.height = height;
                    imagesize.depth  = 1;

                    newImage = engine->create_image(data, imagesize, VK_FORMAT_R8G8B8A8_UNORM,
                                                    VK_IMAGE_USAGE_SAMPLED_BIT, false);

                    stbi_image_free(data);
                }
            },
            [&](fastgltf::sources::BufferView& view)
            {
                auto& bufferView = asset.bufferViews[view.bufferViewIndex];
                auto& buffer     = asset.buffers[bufferView.bufferIndex];

                std::visit(fastgltf::visitor{
                               // We only care about VectorWithMime here, because we
                               // specify LoadExternalBuffers, meaning all buffers
                               // are already loaded into a vector.
                               [](auto& arg)
                               {
                               },
                               [&](fastgltf::sources::Vector& vector)
                               {
                                   unsigned char* data = stbi_load_from_memory(
                                       reinterpret_cast<const stbi_uc*>(vector.bytes.data() + bufferView.byteOffset),
                                       static_cast<int>(bufferView.byteLength),
                                       &width, &height, &nrChannels, 4);
                                   if (data)
                                   {
                                       VkExtent3D imagesize;
                                       imagesize.width  = width;
                                       imagesize.height = height;
                                       imagesize.depth  = 1;

                                       newImage = engine->create_image(data, imagesize, VK_FORMAT_R8G8B8A8_UNORM,
                                                                       VK_IMAGE_USAGE_SAMPLED_BIT, false);

                                       stbi_image_free(data);
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
        image.data);

    // if any of the attempts to load the data failed, we havent written the image
    // so handle is null
    if (newImage.image == VK_NULL_HANDLE)
    {
        return {};
    }
    return newImage;
}

VkFilter extract_filter(fastgltf::Filter filter)
{
    switch (filter)
    {
    // nearest samplers
    case fastgltf::Filter::Nearest:
    case fastgltf::Filter::NearestMipMapNearest:
    case fastgltf::Filter::NearestMipMapLinear:
        return VK_FILTER_NEAREST;

    // linear samplers
    case fastgltf::Filter::Linear:
    case fastgltf::Filter::LinearMipMapNearest:
    case fastgltf::Filter::LinearMipMapLinear:
    default:
        return VK_FILTER_LINEAR;
    }
}

VkSamplerMipmapMode extract_mipmap_mode(fastgltf::Filter filter)
{
    switch (filter)
    {
    case fastgltf::Filter::NearestMipMapNearest:
    case fastgltf::Filter::LinearMipMapNearest:
        return VK_SAMPLER_MIPMAP_MODE_NEAREST;

    case fastgltf::Filter::NearestMipMapLinear:
    case fastgltf::Filter::LinearMipMapLinear:
    default:
        return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    }
}

std::optional<std::shared_ptr<LoadedGLTF>> loadGltf(VulkanEngine* engine, std::string_view filePath)
{
    fmt::print("Loading GLTF: {}\n", filePath);

    auto scene     = std::make_shared<LoadedGLTF>();
    scene->creator = engine;
    auto& file     = *scene;

    fastgltf::Parser parser{};

    constexpr auto gltfOptions = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble |
                                 fastgltf::Options::LoadExternalBuffers;
    // fastgltf::Options::LoadExternalImages;

    auto data = fastgltf::GltfDataBuffer::FromPath(filePath);

    fastgltf::Asset gltf;

    std::filesystem::path path = filePath;

    parent_path = path.parent_path();
    parent_path += "/";

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

    // we can estimate the descriptors we will need accurately
    std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes = {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1}
    };

    file.descriptorPool.init(engine->_context->getDevice(), gltf.materials.size(), sizes);

    // 读取sampler
    for (fastgltf::Sampler& sampler : gltf.samplers)
    {
        VkSamplerCreateInfo sampl = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, .pNext = nullptr};
        sampl.maxLod              = VK_LOD_CLAMP_NONE;
        sampl.minLod              = 0;

        sampl.magFilter = extract_filter(sampler.magFilter.value_or(fastgltf::Filter::Nearest));
        sampl.minFilter = extract_filter(sampler.minFilter.value_or(fastgltf::Filter::Nearest));

        sampl.mipmapMode = extract_mipmap_mode(sampler.minFilter.value_or(fastgltf::Filter::Nearest));

        VkSampler newSampler;
        vkCreateSampler(engine->_context->getDevice(), &sampl, nullptr, &newSampler);

        file.samplers.push_back(newSampler);
    }

    // 临时数据
    std::vector<std::shared_ptr<MeshAsset>>    meshes;
    std::vector<std::shared_ptr<Node>>         nodes;
    std::vector<AllocatedImage>                images;
    std::vector<TextureID>                     imageIDs;
    std::vector<std::shared_ptr<GLTFMaterial>> materials;

    // 读取贴图
    for (fastgltf::Image& image : gltf.images)
    {
        std::optional<AllocatedImage> img = load_image(engine, gltf, image);

        if (img.has_value())
        {
            images.push_back(*img);
            file.images[image.name.c_str()] = *img;
            //imageIDs.push_back( engine->texCache.AddTexture(materialResources.colorImage.imageView, materialResources.colorSampler, );
        }
        else
        {
            // we failed to load, so lets give the slot a default white texture to not completely break loading
            images.push_back(engine->_errorCheckerboardImage);
            fmt::print(stderr, "gltf failed to load texture: {}\n", image.name);
        }
    }

    // create buffer to hold the material data
    file.materialDataBuffer = engine->create_buffer(
        sizeof(GLTFMetallic_Roughness::MaterialConstants) * gltf.materials.size(),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    int  data_index             = 0;
    auto sceneMaterialConstants = static_cast<GLTFMetallic_Roughness::MaterialConstants*>(file.materialDataBuffer.info.
        pMappedData);


    for (fastgltf::Material& mat : gltf.materials)
    {
        auto newMat = std::make_shared<GLTFMaterial>();
        materials.push_back(newMat);
        file.materials[mat.name.c_str()] = newMat;

        GLTFMetallic_Roughness::MaterialConstants constants;
        constants.colorFactors.x = mat.pbrData.baseColorFactor[0];
        constants.colorFactors.y = mat.pbrData.baseColorFactor[1];
        constants.colorFactors.z = mat.pbrData.baseColorFactor[2];
        constants.colorFactors.w = mat.pbrData.baseColorFactor[3];

        constants.metal_rough_factors.x = mat.pbrData.metallicFactor;
        constants.metal_rough_factors.y = mat.pbrData.roughnessFactor;


        auto passType = MaterialPass::MainColor;
        // 混合材质
        if (mat.alphaMode == fastgltf::AlphaMode::Blend)
        {
            //passType = MaterialPass::Transparent;
        }

        GLTFMetallic_Roughness::MaterialResources materialResources;
        // default the material textures
        materialResources.colorImage        = engine->_whiteImage;
        materialResources.colorSampler      = engine->_defaultSamplerLinear;
        materialResources.metalRoughImage   = engine->_whiteImage;
        materialResources.metalRoughSampler = engine->_defaultSamplerLinear;

        // set the uniform buffer for the material data
        materialResources.dataBuffer       = file.materialDataBuffer.buffer;
        materialResources.dataBufferOffset = data_index * sizeof(GLTFMetallic_Roughness::MaterialConstants);
        // grab textures from gltf file
        if (mat.pbrData.baseColorTexture.has_value())
        {
            size_t img     = gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex].imageIndex.value();
            size_t sampler = gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex].samplerIndex.value();

            materialResources.colorImage   = images[img];
            materialResources.colorSampler = file.samplers[sampler];

            constants.colorTexID = engine->texCache.AddTexture(materialResources.colorImage.imageView,
                                                               materialResources.colorSampler).Index;
        }

        if (mat.pbrData.metallicRoughnessTexture.has_value())
        {
            size_t img = gltf.textures[mat.pbrData.metallicRoughnessTexture.value().textureIndex].imageIndex.value();
            size_t sampler = gltf.textures[mat.pbrData.metallicRoughnessTexture.value().textureIndex].samplerIndex.
                value();

            materialResources.metalRoughImage   = images[img];
            materialResources.metalRoughSampler = file.samplers[sampler];

            constants.metalRoughTexID = engine->texCache.AddTexture(materialResources.metalRoughImage.imageView,
                                                                    materialResources.metalRoughSampler).Index;
        }

        if (mat.normalTexture.has_value())
        {
            size_t img     = gltf.textures[mat.normalTexture.value().textureIndex].imageIndex.value();
            size_t sampler = gltf.textures[mat.normalTexture.value().textureIndex].samplerIndex.value();

            materialResources.normal_image   = images[img];
            materialResources.normal_sampler = file.samplers[sampler];

            constants.normalTexID = engine->texCache.AddTexture(materialResources.normal_image.imageView,
                                                                materialResources.normal_sampler).Index;
        }

        // write material parameters to buffer
        sceneMaterialConstants[data_index] = constants;
        // build material
        newMat->data = engine->metalRoughMaterial.write_material(engine->_context->getDevice(), passType,
                                                                 materialResources,
                                                                 file.descriptorPool);

        data_index++;
    }

    // use the same vectors for all meshes so that the memory doesn't reallocate as often
    std::vector<uint32_t> indices;
    std::vector<Vertex>   vertices;
    print(fg(fmt::color::azure),
          "--------------------------------start load meshes--------------------------------\n");

    // 读取网格
    for (fastgltf::Mesh& mesh : gltf.meshes)
    {
        auto newmesh = std::make_shared<MeshAsset>();
        meshes.push_back(newmesh);
        file.meshes[mesh.name.c_str()] = newmesh;
        newmesh->name                  = mesh.name;
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
            newSurface.count      = static_cast<uint32_t>(gltf.accessors[p.indicesAccessor.value()].count);
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

                fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, posAccessor,
                                                              [&](glm::vec3 v, size_t index)
                                                              {
                                                                  Vertex newvtx;
                                                                  newvtx.position               = v;
                                                                  newvtx.normal                 = {1, 0, 0};
                                                                  newvtx.color                  = glm::vec4{1.f};
                                                                  newvtx.uv_x                   = 0;
                                                                  newvtx.uv_y                   = 0;
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

            newSurface.bounds.max_edge     = maxpos;
            newSurface.bounds.min_edge     = minpos;
            newSurface.bounds.origin       = (maxpos + minpos) / 2.f;
            newSurface.bounds.extents      = (maxpos - minpos) / 2.f;
            newSurface.bounds.sphereRadius = length(newSurface.bounds.extents);
            newmesh->surfaces.push_back(newSurface);
            ++idx;
        }

        newmesh->meshBuffers = engine->uploadMesh(indices, vertices);
    }

    // load all nodes and their meshes
    for (fastgltf::Node& node : gltf.nodes)
    {
        std::shared_ptr<Node> newNode;

        // find if the node has a mesh, and if it does hook it to the mesh pointer and allocate it with the meshnode class
        if (node.meshIndex.has_value())
        {
            newNode                                     = std::make_shared<MeshNode>();
            static_cast<MeshNode*>(newNode.get())->mesh = meshes[*node.meshIndex];

        }
        else
        {
            newNode = std::make_shared<Node>();
        }

        nodes.push_back(newNode);
        file.nodes[node.name.c_str()];

        std::visit(fastgltf::visitor{
                       [&](fastgltf::math::fmat4x4 matrix)
                       {
                           memcpy(&newNode->localTransform, matrix.data(), sizeof(matrix));
                       },
                       [&](fastgltf::TRS transform)
                       {
                           glm::vec3 tl(transform.translation[0], transform.translation[1],
                                        transform.translation[2]);
                           glm::quat rot(transform.rotation[3], transform.rotation[0], transform.rotation[1],
                                         transform.rotation[2]);
                           glm::vec3 sc(transform.scale[0], transform.scale[1], transform.scale[2]);

                           glm::mat4 tm = glm::translate(glm::mat4(1.f), tl);
                           glm::mat4 rm = glm::toMat4(rot);
                           glm::mat4 sm = scale(glm::mat4(1.f), sc);

                           newNode->localTransform = tm * rm * sm;
                       }
                   },
                   node.transform);

        // 修改node下方的mesh的相对位置，因为可能有变换矩阵，注意这里只使用了一层，但实际上是树形结构
        // TODO 后续需要改成遍历树形结构
        if (node.meshIndex.has_value())
        {
            const auto& mesh = meshes[*node.meshIndex];
            //for (auto& primitive : mesh->surfaces)
            //{
            //    primitive.bounds.min_edge = newNode->localTransform * glm::vec4(primitive.bounds.min_edge, 1.0);
            //    primitive.bounds.max_edge = newNode->localTransform * glm::vec4(primitive.bounds.max_edge, 1.0);
            //    primitive.bounds.origin = newNode->localTransform * glm::vec4(primitive.bounds.origin, 1.0);
            //}
        }
    }
    // run loop again to setup transform hierarchy
    for (int i = 0; i < gltf.nodes.size(); i++)
    {
        fastgltf::Node&        node      = gltf.nodes[i];
        std::shared_ptr<Node>& sceneNode = nodes[i];
        
        for (auto& c : node.children)
        {
            sceneNode->children.push_back(nodes[c]);
            nodes[c]->parent = sceneNode;
        }
    }

    // find the top nodes, with no parents
    for (auto& node : nodes)
    {
        if (node->parent.lock() == nullptr)
        {
            file.topNodes.push_back(node); 
            node->refreshTransform(glm::mat4{1.f});
        }
    }

    return scene;
    //< load_graph
}

void LoadedGLTF::draw(const glm::mat4& topMatrix, DrawContext& ctx)
{
    // create renderables from the scenenodes
    for (auto& n : topNodes)
    {
        n->draw(topMatrix, ctx);
    }
}

void LoadedGLTF::clearAll()
{
    VkDevice dv = creator->_context->getDevice();

    for (auto& [k, v] : meshes)
    {
        creator->destroy_buffer(v->meshBuffers.indexBuffer);
        creator->destroy_buffer(v->meshBuffers.vertexBuffer);
    }

    for (auto& [k, v] : images)
    {
        if (v.image == creator->_errorCheckerboardImage.image)
        {
            // dont destroy the default images
            continue;
        }
        creator->destroy_image(v);
    }

    for (auto& sampler : samplers)
    {
        vkDestroySampler(dv, sampler, nullptr);
    }

    auto materialBuffer    = materialDataBuffer;
    auto samplersToDestroy = samplers;

    descriptorPool.destroy_pools(dv);

    creator->destroy_buffer(materialBuffer);
}
} // dk

namespace vkutil {
std::filesystem::path get_model_path(const nlohmann::json& j, const std::string& model_name)
{
    // 遍历 file_paths 数组，查找 name 匹配 model_name 的项
    const auto it = std::ranges::find_if(j["file_paths"],
                                         [&](const nlohmann::json& item)
                                         {
                                             return item["name"] == model_name;
                                         });

    // 如果找到了，返回对应的路径
    if (it != j["file_paths"].end())
    {
        return (*it)["path"].get<std::string>();
    }

    // 否则返回空路径
    return {};
}

bool readShaderFile(const std::string& file_path, std::string& code)
{
    std::ifstream fp(file_path);

    if (!fp.is_open())
    {
        fmt::print("can't open: {}\n", file_path);
        return false;
    }

    std::stringstream ss;
    ss << fp.rdbuf();

    fp.close();

    code = ss.str();

    return true;
}
}
