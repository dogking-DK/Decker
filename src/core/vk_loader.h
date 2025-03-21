// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vk_types.h>

#include "vk_descriptors.h"
#include <unordered_map>
#include <filesystem>
#include <nlohmann/json_fwd.hpp>

#include "Macros.h"
#include "Scene/Node.h"

namespace dk {
class VulkanEngine;

struct GLTFMaterial
{
    MaterialInstance data;
};

struct GeoSurface
{
    uint32_t                      startIndex;
    uint32_t                      count;
    Bounds                        bounds;
    std::shared_ptr<GLTFMaterial> material;
};

struct MeshAsset
{
    std::string name;

    std::vector<GeoSurface> surfaces;
    GPUMeshBuffers          meshBuffers;
};

struct LoadedGLTF : IRenderable
{
    // storage for all the data on a given gltf file
    std::unordered_map<std::string, std::shared_ptr<MeshAsset>>    meshes;
    std::unordered_map<std::string, std::shared_ptr<Node>>         nodes;
    std::unordered_map<std::string, AllocatedImage>                images;
    std::unordered_map<std::string, std::shared_ptr<GLTFMaterial>> materials;

    // nodes that dont have a parent, for iterating through the file in tree order
    std::vector<std::shared_ptr<Node>> topNodes;

    std::vector<VkSampler> samplers;

    DescriptorAllocatorGrowable descriptorPool;

    AllocatedBuffer materialDataBuffer;

    VulkanEngine* creator;

    ~LoadedGLTF() override { clearAll(); }

    void draw(const glm::mat4& topMatrix, DrawContext& ctx) override;

private:
    void clearAll();
};

std::optional<std::shared_ptr<LoadedGLTF>> loadGltf(VulkanEngine* engine, std::string_view filePath);
} // dk

namespace vkutil {
std::filesystem::path get_model_path(const nlohmann::json& j, const std::string& model_name);

bool readShaderFile(const std::string& file_path, std::string& code);
} // vkutil
