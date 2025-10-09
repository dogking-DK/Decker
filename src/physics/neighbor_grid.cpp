#include "neighbor_grid.h"
#include <cmath>


namespace dk {
void HashGrid::query(const glm::vec3& x, float radius, std::vector<u32>& out) const
{
    out.clear();
    const int r = static_cast<int>(std::ceil(radius / cell));
    auto      c = cellCoord(x);
    for (int dz = -r; dz <= r; ++dz)
        for (int dy = -r; dy <= r; ++dy)
            for (int dx = -r; dx <= r; ++dx)
            {
                auto it = buckets.find(makeKey(c.x + dx, c.y + dy, c.z + dz));
                if (it == buckets.end()) continue;
                const auto& vec = it->second;
                out.insert(out.end(), vec.begin(), vec.end());
            }
}
} // namespace dk
