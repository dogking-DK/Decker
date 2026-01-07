#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace dk {

    /// @brief 基础位姿结构，负责描述节点的局部变换。
    struct Transform
    {
        glm::vec3 translation{ 0.0f };
        glm::quat rotation{ 1.0f, 0.0f, 0.0f, 0.0f };
        glm::vec3 scale{ 1.0f };
    };

} // namespace core