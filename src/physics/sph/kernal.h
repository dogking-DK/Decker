#pragma once
#include <glm/glm.hpp>
#include <cmath>


namespace dk::sphk {
// Poly6 / Spiky / Viscosity kernels (3D)
struct Kernels
{
    float h{}, W_poly6_norm{}, W_spiky_norm{}, W_visc_laplace_norm{};

    explicit Kernels(float h_) : h(h_)
    {
        float h9            = std::pow(h, 9);
        float h6            = std::pow(h, 6);
        float h3            = std::pow(h, 3);
        W_poly6_norm        = 315.0f / (64.0f * static_cast<float>(M_PI) * h9);
        W_spiky_norm        = -45.0f / (static_cast<float>(M_PI) * h6); // gradient uses negative norm
        W_visc_laplace_norm = 45.0f / (static_cast<float>(M_PI) * h6);
    }

    float poly6(float r) const
    {
        if (r >= 0 && r <= h)
        {
            float x = h * h - r * r;
            return W_poly6_norm * x * x * x;
        }
        return 0.f;
    }

    glm::vec3 grad_spiky(const glm::vec3& rij, float r) const
    {
        if (r > 0 && r <= h)
        {
            float x     = (h - r);
            float coeff = W_spiky_norm * x * x / r; // negative already in norm
            return coeff * rij; // points from i->j contribution for ∇W_i
        }
        return glm::vec3(0);
    }

    float laplace_visc(float r) const
    {
        if (r >= 0 && r <= h) return W_visc_laplace_norm * (h - r);
        return 0.f;
    }
};
}
