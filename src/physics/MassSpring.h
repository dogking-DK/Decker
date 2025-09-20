// MassSpringSystem.h
#pragma once
#include <vector>
#include <cmath>

// 包含GLM主头文件
#include <glm/glm.hpp>

class MassSpringSystem
{
public:
    // Particle结构现在使用 glm::vec2
    struct Particle
    {
        glm::vec2 position;    // 位置
        glm::vec2 velocity;    // 速度
        glm::vec2 force;       // 力
        float     mass;        //质量
        bool      is_pinned = false; // 是否固定
    };

    struct Spring
    {
        int   p1_idx;       // 连接的粒子索引
        int   p2_idx;       // 连接的粒子索引   
        float rest_length;  // 自然长度
        float stiffness;    // 刚度
    };

    // 构造函数的参数也更新为 glm::vec2
    MassSpringSystem(glm::vec2 gravity = glm::vec2(0.0f, -9.8f), float damping = 0.1f);

    // 接口使用 glm::vec2
    int  addParticle(float mass, glm::vec2 position, bool is_pinned = false);
    void addSpring(int p1_idx, int p2_idx, float stiffness);

    void update(float dt);

    const std::vector<Particle>& getParticles() const { return _particles; }
    const std::vector<Spring>&   getSprings() const { return _springs; }

private:
    void computeForces();
    void integrate(float dt);
    void handleCollisions();

    std::vector<Particle> _particles;
    std::vector<Spring>   _springs;
    glm::vec2             _gravity;              // 重力向量
    float                 _damping_coefficient;  // 阻尼系数
};
