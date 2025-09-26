#pragma once
#include "Base.h"
#include <glm/gtx/hash.hpp>
#include <unordered_map>



namespace dk {
// Integer 3D to 64-bit key (Morton-free simple hash)
inline i64 makeKey(int ix, int iy, int iz)
{
    // pack with large primes to reduce collisions
    constexpr constexpr constexpr i64 p1 = 73856093, p2 = 19349663, p3 = 83492791;
    return (ix * p1) ^ (iy * p2) ^ (iz * p3);
}


struct HashGrid
{
    float                                     cell{0.1f};
    std::unordered_map<i64, std::vector<u32>> buckets;


    glm::ivec3 cellCoord(const glm::vec3& x) const
    {
        return {floor(x.x / cell), floor(x.y / cell), floor(x.z / cell)};
    }


    void clear() { buckets.clear(); }


    void build(const std::vector<glm::vec3>& pos)
    {
        buckets.clear();
        buckets.reserve(pos.size() * 2);
        for (u32 i = 0; i < pos.size(); ++i)
        {
            auto c = cellCoord(pos[i]);
            buckets[makeKey(c.x, c.y, c.z)].push_back(i);
        }
    }


    void query(const glm::vec3& x, float radius, std::vector<u32>& out) const;
};
} // namespace dk
