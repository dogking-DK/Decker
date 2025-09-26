#include "Base.h"
#include "World.h"

#include <ranges>


namespace dk {
World::World(const WorldSettings& s): settings_(s)
{
}


void World::tick(float real_dt)
{
    acc_ += real_dt;
    const float h = settings_.fixed_dt;
    while (acc_ + kEps >= h)
    {
        for (int s = 0; s < settings_.substeps; ++s)
        {
            for (const auto& val : systems_ | std::views::values) val->step(h / static_cast<float>(settings_.substeps));
        }
        acc_ -= h;
    }
}
} // namespace dk
