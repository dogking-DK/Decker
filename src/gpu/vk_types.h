// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.
//> intro
#pragma once
#define GLM_ENABLE_EXPERIMENTAL


#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <span>
#include <array>
#include <functional>
#include <deque>

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vk_mem_alloc.h>

#include <fmt/core.h>
#include <fmt/color.h>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
//< intro 


// we will add our main reusable types here
struct AllocatedImage
{
    VkImage image;
    VkImageView imageView;
    VmaAllocation allocation;
    VkExtent3D imageExtent;
    VkFormat imageFormat;
};

struct AllocatedBuffer
{
    VkBuffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo info;
};

struct GPUGLTFMaterial
{
    glm::vec4 colorFactors;
    glm::vec4 metal_rough_factors;
    glm::vec4 extra[14];
};

static_assert(sizeof(GPUGLTFMaterial) == 256);

struct GPUSceneData
{
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 viewproj;
    glm::vec4 ambientColor;
    glm::vec4 sunlightDirection; // w for sun power
    glm::vec4 sunlightColor;
};

//> mat_types
enum class MaterialPass :uint8_t
{
    MainColor,
    Transparent,
    Other
};

struct MaterialPipeline
{
    VkPipeline pipeline;
    VkPipelineLayout layout;
};

struct MaterialInstance
{
    MaterialPipeline* pipeline;
    VkDescriptorSet materialSet;
    MaterialPass passType;
};

//< mat_types
//> vbuf_types
struct Vertex
{
    glm::vec3 position;
    float uv_x;
    glm::vec3 normal;
    float uv_y;
    glm::vec4 color;
};

// holds the resources needed for a mesh
struct GPUMeshBuffers
{
    AllocatedBuffer indexBuffer;
    AllocatedBuffer vertexBuffer;
    VkDeviceAddress vertexBufferAddress;
};

// push constants for our mesh object draws
struct GPUDrawPushConstants
{
    glm::mat4 worldMatrix;
    VkDeviceAddress vertexBuffer;
};

//< vbuf_types

#define VK_CHECK(x)                                                         \
    do {                                                                    \
        VkResult err = x;                                                   \
        if (err) {                                                          \
             fmt::print("Detected Vulkan error: {}", string_VkResult(err)); \
            abort();                                                        \
        }                                                                   \
    } while (0)

#define VK_DEBUGNAME(device, objType, objHandle, name)                       \
    do {                                                                     \
        VkDebugUtilsObjectNameInfoEXT nameInfo{};                            \
        nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT; \
        nameInfo.objectType = objType;                                       \
        nameInfo.objectHandle = reinterpret_cast<uint64_t>(objHandle);       \
        nameInfo.pObjectName = name;                                         \
        pfnSetDebugUtilsObjectNameEXT(device, &imageNameInfo);               \
    } while (0)
