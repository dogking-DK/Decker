// IParticleColorizer.h
#pragma once
#include "data/Particle.h"

namespace dk {
class IParticleColorizer
{
public:
    virtual ~IParticleColorizer() = default;

    /**
     * @brief �����ڲ��߼����� ParticleData �е���ɫ.
     * @param data Ҫ����ɫ����������.
     */
    virtual void colorize(ParticleData& data) = 0;
};
}
