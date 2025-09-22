#pragma once
#include <vector>
#include <memory>


namespace dk {
struct WorldSettings
{
    float fixed_dt{1.0f / 60.0f}; // 时间步长
    int   substeps{ 1 }; // 每帧的子步数
};


class ISystem
{
public:
    virtual      ~ISystem() = default;
    virtual void step(float dt) = 0; // one substep
};


class World
{
public:
    explicit World(const WorldSettings& s);

    template <class T, class... Args>
    T* addSystem(Args&&... args)
    {
        auto ptr = std::make_unique<T>(std::forward<Args>(args)...);
        T*   raw = ptr.get();
        systems_.emplace_back(std::move(ptr));
        return raw;
    }

    void                 tick(float real_dt); // accumulate and run fixed steps
    const WorldSettings& settings() const { return settings_; }

private:
    WorldSettings                         settings_{};
    std::vector<std::unique_ptr<ISystem>> systems_;
    float                                 acc_{0.f};
};
}
