#pragma once

#include <vector>
#include <memory>
#include <execution>
#include "glm/glm.hpp"

#include "vk_engine.h"
#include "Vulkan/Context.h"
#include "Vulkan/Buffer.h" // 假设有 BufferResource 类
#include "Vulkan/Image.h"  // 假设有 ImageResource 类
#include "Vulkan/CommandBuffer.h"
#include "Vulkan/DescriptorSetLayout.h"
#include "Vulkan/DescriptorSet.h"
#include "Vulkan/DescriptorSetPool.h"
#include "Vulkan/DescriptorWriter.h"
#include "Vulkan/Pipeline.h"
#include "Vulkan/PipelineLayout.h"

namespace dk {
// --- GLSL/C++ 共享数据结构 (必须与 Slang 中的定义匹配) ---

inline void translate_points(std::vector<PointData>& points, const glm::vec4 d)
{
    ZoneScopedN("point cloud translate");
    std::for_each(std::execution::par_unseq, points.begin(), points.end(), [d](PointData& p)
    {
        p.position += d;
    });
}

// 立方体/长方体内均匀分布
inline std::vector<PointData>
makeRandomPointCloudCube(size_t count, glm::vec3 minP = {-1.f, -1.f, -1.f}, glm::vec3  maxP = {1.f, 1.f, 1.f},
                         bool   randomColors          = false, std::optional<uint32_t> seed = std::nullopt)
{
    std::mt19937                          gen(seed ? *seed : std::random_device{}());
    std::uniform_real_distribution<float> dx(minP.x, maxP.x);
    std::uniform_real_distribution<float> dy(minP.y, maxP.y);
    std::uniform_real_distribution<float> dz(minP.z, maxP.z);
    std::uniform_real_distribution<float> dc(0.f, 1.f);

    glm::vec3              size = maxP - minP;
    std::vector<PointData> pts;
    pts.reserve(count);

    for (size_t i = 0; i < count; ++i)
    {
        glm::vec3 p{dx(gen), dy(gen), dz(gen)};
        // 位置映射到 [0,1]^3 用作颜色（可选）
        auto t = glm::vec3(
            size.x != 0 ? (p.x - minP.x) / size.x : 0.5f,
            size.y != 0 ? (p.y - minP.y) / size.y : 0.5f,
            size.z != 0 ? (p.z - minP.z) / size.z : 0.5f
        );
        glm::vec4 col = randomColors
                            ? glm::vec4(dc(gen), dc(gen), dc(gen), 1.f)
                            : glm::vec4(t, 1.f);

        pts.push_back({glm::vec4(p, 1.f), col});
    }
    return pts;
}

// 球体内/表面均匀分布（shell=false 为体积均匀；true 为表面均匀）
inline std::vector<PointData>
makeRandomPointCloudSphere(size_t count, glm::vec3 center = {0, 0, 0}, float radius = 1.f, bool shell = false,
                           std::optional<uint32_t> seed = std::nullopt)
{
    ZoneScopedN("point cloud radom generate");
    std::mt19937                          gen(seed ? *seed : std::random_device{}());
    std::normal_distribution<float>       gauss(0.f, 1.f);
    std::uniform_real_distribution<float> u01(0.f, 1.f);

    std::vector<PointData> pts;
    pts.reserve(count);

    for (size_t i = 0; i < count; ++i)
    {
        // 随机方向（高斯→归一化）
        glm::vec3 dir{gauss(gen), gauss(gen), gauss(gen)};
        float     len = length(dir);
        if (len == 0.f)
        {
            --i;
            continue;
        }
        dir /= len;

        // 半径：表面均匀=固定 r；体积均匀=cbrt(u)*R
        float     r = shell ? radius : radius * std::cbrt(u01(gen));
        glm::vec3 p = center + dir * r;

        // 简单颜色：用方向映射到 [0,1]，或你也可改成随机色
        glm::vec3 t = dir * 0.5f + glm::vec3(0.5f);
        pts.push_back({glm::vec4(p, 1.f), glm::vec4(t, 1.f)});
    }
    return pts;
}

class PointCloudRenderer
{
public:
    PointCloudRenderer(vkcore::VulkanContext* context);
    ~PointCloudRenderer() = default;

    // 初始化渲染器所需的所有 Vulkan 对象
    void init(vk::Format color_format, vk::Format depth_format);

    // 每帧更新点云数据
    void updatePoints();

    // 在命令缓冲区中录制绘制命令
    void draw(vkcore::CommandBuffer& cmd, const CameraData& camera_data, VkRenderingInfo& render_info);

    std::vector<PointData>& getPointData() { return _point_data; }

    // 清理所有资源
    void cleanup();

private:
    void createBuffers();
    void createDescriptors();
    void createPipeline(vk::Format color_format, vk::Format depth_format);

    // --- 成员变量 ---
    vkcore::VulkanContext* _context;
    uint32_t               _point_count     = 0;
    uint32_t               _max_point_count = 0; // 缓冲区的最大容量

    std::vector<PointData> _point_data; // CPU 端的点云数据

    // 资源
    std::unique_ptr<vkcore::BufferResource> _point_cloud_ssbo;
    std::unique_ptr<vkcore::BufferResource> _camera_ubo;

    // 描述符相关
    std::unique_ptr<vkcore::DescriptorSetLayout>         _layout;
    std::unique_ptr<vkcore::GrowableDescriptorAllocator> _frame_allocator;

    // 管线相关
    std::unique_ptr<vkcore::PipelineLayout> _pipeline_layout;
    std::unique_ptr<vkcore::Pipeline>       _pipeline;
};
}
