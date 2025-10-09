// VelocityColorizer.h
#pragma once
#include "IParticleColorizer.h"
#include <algorithm>
#include <limits>

namespace dk {
class VelocityColorizer : public IParticleColorizer
{
public:
    // 构造时定义一个颜色梯度
    VelocityColorizer(const vec4& minColor = vec4(0, 0, 1, 1), const vec4& maxColor = vec4(1, 0, 0, 1))
        : m_minColor(minColor), m_maxColor(maxColor)
    {
    }

    void colorize(ParticleData& data) override
    {
        const size_t count = data.size();
        if (count == 0) return;

        // 1. 找到当前帧速度大小的最小值和最大值
        float minMag = std::numeric_limits<float>::max();
        float maxMag = std::numeric_limits<float>::min();
        for (size_t i = 0; i < count; ++i)
        {
            float mag = length(data.velocity[i]);
            minMag    = std::min(minMag, mag);
            maxMag    = std::max(maxMag, mag);
        }

        // 2. 归一化并映射到颜色梯度
        for (size_t i = 0; i < count; ++i)
        {
            float mag = length(data.velocity[i]);
            float t   = 0.0f;
            // 避免除以零
            if (maxMag > minMag)
            {
                t = (mag - minMag) / (maxMag - minMag);
            }
            // 使用 glm::mix 进行线性插值
            data.color[i] = mix(m_minColor, m_maxColor, t);
        }
    }

private:
    vec4 m_minColor; // 速度最慢时的颜色
    vec4 m_maxColor; // 速度最快时的颜色
};
}
