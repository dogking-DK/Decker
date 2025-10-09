#pragma once
#define GLM_ENABLE_EXPERIMENTAL

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cstdint>
#include <vector>
#include <memory>
#include <unordered_map>
#include <limits>


namespace dk {
using u32 = std::uint32_t;
using i64 = std::int64_t;

using vec4 = glm::vec4;
using vec3 = glm::vec3;
using vec2 = glm::vec2;

constexpr float kEps = 1e-6f;
}
