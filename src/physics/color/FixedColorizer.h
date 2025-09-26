// FixedColorizer.h
#pragma once
#include "IParticleColorizer.h"

namespace dk {
class FixedColorizer : public IParticleColorizer
{
public:
    FixedColorizer(const vec4& color) : m_color(color)
    {
    }

    void colorize(ParticleData& data) override
    {
        std::fill(data.colors.begin(), data.colors.end(), m_color);
    }

private:
    vec4 m_color;
};
}
