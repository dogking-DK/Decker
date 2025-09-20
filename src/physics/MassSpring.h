// MassSpringSystem.h
#pragma once
#include <vector>
#include <cmath>

// ����GLM��ͷ�ļ�
#include <glm/glm.hpp>

class MassSpringSystem
{
public:
    // Particle�ṹ����ʹ�� glm::vec2
    struct Particle
    {
        glm::vec2 position;    // λ��
        glm::vec2 velocity;    // �ٶ�
        glm::vec2 force;       // ��
        float     mass;        //����
        bool      is_pinned = false; // �Ƿ�̶�
    };

    struct Spring
    {
        int   p1_idx;       // ���ӵ���������
        int   p2_idx;       // ���ӵ���������   
        float rest_length;  // ��Ȼ����
        float stiffness;    // �ն�
    };

    // ���캯���Ĳ���Ҳ����Ϊ glm::vec2
    MassSpringSystem(glm::vec2 gravity = glm::vec2(0.0f, -9.8f), float damping = 0.1f);

    // �ӿ�ʹ�� glm::vec2
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
    glm::vec2             _gravity;              // ��������
    float                 _damping_coefficient;  // ����ϵ��
};
