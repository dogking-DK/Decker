#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <span>
#include <array>
#include <functional>
#include <deque>
#include <limits>
#include <memory>

//#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1

#include <filesystem>
#include <vulkan/vulkan.hpp>
#include <vulkan/vk_enum_string_helper.h>
#include <vk_mem_alloc.h>



#include <magic_enum/magic_enum.hpp>

#include <tracy/Tracy.hpp>

#include <fmt/core.h>
#include <fmt/color.h>

#include <glm/glm.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>
//< intro

constexpr unsigned int FRAME_OVERLAP = 2;

static std::filesystem::path get_exe_dir()
{
#if defined(_WIN32)
    wchar_t buf[MAX_PATH];
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    return std::filesystem::path(buf).parent_path();
#elif defined(__APPLE__)
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::string buf(size, '\0');
    _NSGetExecutablePath(buf.data(), &size);
    return std::filesystem::weakly_canonical(std::filesystem::path(buf)).parent_path();
#else
    std::string buf(4096, '\0');
    ssize_t len = readlink("/proc/self/exe", buf.data(), buf.size() - 1);
    buf.resize((len > 0) ? len : 0);
    return std::filesystem::path(buf).parent_path();
#endif
}


// Camera UBO
struct CameraData
{
    glm::mat4 viewProj;
    glm::mat4 view;
    glm::mat4 proj;
};

// Push Constants
struct PushConstantData
{
    glm::mat4 modelMatrix;
    float     size; // 点或者线的大小
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

// push constants for our mesh object draws
struct GPUDrawPushConstants
{
    glm::mat4 worldMatrix;
    VkDeviceAddress vertexBuffer;
};

//< vbuf_types
inline void VK_CHECK(VkResult err)
{
    if (err != VK_SUCCESS) 
    {
        fmt::print(fmt::fg(fmt::color::orange_red),
            "Detected Vulkan error: {}\n", string_VkResult(err));
        abort();
    }
}
