#include <algorithm>

#include "MassSpring.h"
#include "integrators.h"


namespace dk {
void MassSpringSystem::step(float dt)
{
    if (p_.empty()) return;
    std::vector<glm::vec3> force(p_.size(), glm::vec3(0));


    // gravity + global damping
    for (size_t i = 0; i < p_.size(); ++i)
    {
        if (p_[i].inv_mass <= 0) continue;
        force[i] += (1.f / p_[i].inv_mass) * g_; // F = m g, where m=1/inv_mass
        force[i] += -global_damping_ * p_[i].v; // viscous drag
    }


    // springs
    for (const auto& s : s_)
    {
        auto&     a = p_[s.i];
        auto&     b = p_[s.j];
        glm::vec3 d = b.x - a.x;
        float     L = glm::length(d);
        if (L < 1e-7f) continue;
        glm::vec3 n   = d / L;
        float     ext = L - s.rest_len;
        // spring force + dashpot along n
        float     rel_v = glm::dot(b.v - a.v, n);
        glm::vec3 Fs    = -s.k * ext * n - s.damping * rel_v * n;
        if (a.inv_mass > 0) force[s.i] += Fs;
        if (b.inv_mass > 0) force[s.j] -= Fs;
    }


    // integrate + simple floor collision (inelastic bounce)
    for (auto& pt : p_)
    {
        if (pt.inv_mass <= 0) continue;
        glm::vec3 a = force[&pt - p_.data()] * pt.inv_mass; // a=F/m
        semiImplicitEuler(pt.x, pt.v, a, dt);
        if (pt.x.y < floor_y_)
        {
            pt.x.y = floor_y_;
            if (pt.v.y < 0) pt.v.y *= -0.3f;
        }
    }
}
} // namespace dk
