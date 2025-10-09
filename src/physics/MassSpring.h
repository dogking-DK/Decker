// SpringMassSystem.h
#pragma once
#include <execution>
#include <vector>
#include <memory>

#include "Base.h"
#include "World.h"
#include "color/IParticleColorizer.h"
#include "force/IForce.h"
#include "solver/ISolver.h"
#include "data/Particle.h"

namespace dk {
class SpringMassSystem : public ISystem
{
public:
    // 构造时确定求解器
    SpringMassSystem(std::unique_ptr<ISolver> solver)
        : _solver(std::move(solver))
    {
        _data = std::make_unique<ParticleData>();
    }

    // 添加具体的力模型
    void addForce(std::unique_ptr<IForce> force)
    {
        _force.push_back(std::move(force));
    }

    void setColorizer(std::unique_ptr<IParticleColorizer> colorizer)
    {
        _colorizer = std::move(colorizer);
    }

    // 实现仿真循环的核心逻辑
    void step(float dt) override;

    // 提供对数据的访问
    ParticleData&       getParticles_mut() { return *_data; }
    Spring&             getTopology_mut() { return _topology; }
    const ParticleData& getParticleData() const { return *_data; }

    void getRenderData(std::vector<PointData>& out_data) const override
    {
        if (_colorizer)
        {
            _colorizer->colorize(*_data);
        }

        const size_t count = _data->size();

        // 清空并预留空间，确保只有在需要时才发生一次内存分配
        out_data.clear();
        out_data.resize(count);

        for (size_t i = 0; i < count; ++i)
        {
            out_data[i].position = vec4(_data->position[i], 1.0f);
            out_data[i].color    = _data->color[i];
        }
    }

private:
    std::unique_ptr<ParticleData>        _data;
    Spring                               _topology;
    std::vector<std::unique_ptr<IForce>> _force;
    std::unique_ptr<ISolver>             _solver;
    std::unique_ptr<IParticleColorizer>  _colorizer;
};
}
