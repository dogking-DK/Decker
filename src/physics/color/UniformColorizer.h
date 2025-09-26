// VelocityColorizer.h
#pragma once
#include "IParticleColorizer.h"
#include <algorithm>
#include <limits>

namespace dk {
class VelocityColorizer : public IParticleColorizer
{
public:
    // ����ʱ����һ����ɫ�ݶ�
    VelocityColorizer(const vec4& minColor = vec4(0, 0, 1, 1), const vec4& maxColor = vec4(1, 0, 0, 1))
        : m_minColor(minColor), m_maxColor(maxColor)
    {
    }

    void colorize(ParticleData& data) override
    {
        const size_t count = data.size();
        if (count == 0) return;

        // 1. �ҵ���ǰ֡�ٶȴ�С����Сֵ�����ֵ
        float minMag = std::numeric_limits<float>::max();
        float maxMag = std::numeric_limits<float>::min();
        for (size_t i = 0; i < count; ++i)
        {
            float mag = length(data.velocity[i]);
            minMag    = std::min(minMag, mag);
            maxMag    = std::max(maxMag, mag);
        }

        // 2. ��һ����ӳ�䵽��ɫ�ݶ�
        for (size_t i = 0; i < count; ++i)
        {
            float mag = length(data.velocity[i]);
            float t   = 0.0f;
            // ���������
            if (maxMag > minMag)
            {
                t = (mag - minMag) / (maxMag - minMag);
            }
            // ʹ�� glm::mix �������Բ�ֵ
            data.color[i] = mix(m_minColor, m_maxColor, t);
        }
    }

private:
    vec4 m_minColor; // �ٶ�����ʱ����ɫ
    vec4 m_maxColor; // �ٶ����ʱ����ɫ
};
}
