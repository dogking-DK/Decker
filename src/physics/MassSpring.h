#pragma once
#include "Base.h"
#include "World.h"

namespace dk {
struct MSParticle
{
    glm::vec3 x{0}, v{0};
    float     inv_mass{1.f};
};

struct MSSpring
{
    u32   i,        j;
    float rest_len, k, damping;
};


class MassSpringSystem final : public ISystem
{
public:
    void setParticles(std::vector<MSParticle> p) { p_ = std::move(p); }
    void setSprings(std::vector<MSSpring> s) { s_ = std::move(s); }
    void setGravity(glm::vec3 g) { g_ = g; }
    void setGlobalDamping(float c) { global_damping_ = c; }
    void setFloor(float y) { floor_y_ = y; }


    const std::vector<MSParticle>& particles() const { return p_; }
    std::vector<MSParticle>&       particles() { return p_; }


    void step(float dt) override;

private:
    std::vector<MSParticle> p_;
    std::vector<MSSpring>   s_;
    glm::vec3               g_{0.f, -9.81f, 0.f};
    float                   global_damping_{0.01f};
    float                   floor_y_{-std::numeric_limits<float>::infinity()};
};
} // namespace dk
