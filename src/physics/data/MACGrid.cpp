// data/MacGrid.cpp
#include "data/MacGrid.h"
#include <cmath>

namespace dk {

    // --- 通用帮助：三线性插值 ---
    static inline float lerp(float a, float b, float t) { return a + (b - a) * t; }

    static inline float trilerp(float c000, float c100, float c010, float c110,
        float c001, float c101, float c011, float c111,
        float fx, float fy, float fz)
    {
        float c00 = lerp(c000, c100, fx);
        float c10 = lerp(c010, c110, fx);
        float c01 = lerp(c001, c101, fx);
        float c11 = lerp(c011, c111, fx);
        float c0 = lerp(c00, c10, fy);
        float c1 = lerp(c01, c11, fy);
        return lerp(c0, c1, fz);
    }

    // 将世界坐标映射到 cell index + frac（中心网格）
    static inline void worldToCell(const vec3& origin, float h, const vec3& x,
        int& i, int& j, int& k, float& fx, float& fy, float& fz)
    {
        vec3 p = (x - origin) / h - vec3(0.5f);
        float xi = glm::floor(p.x);
        float yj = glm::floor(p.y);
        float zk = glm::floor(p.z);
        i = (int)xi; j = (int)yj; k = (int)zk;
        fx = p.x - xi; fy = p.y - yj; fz = p.z - zk;
    }

    // 将世界坐标映射到 U/V/W 网格 index + frac
    static inline void worldToU(const vec3& origin, float h, const vec3& x,
        int& i, int& j, int& k, float& fx, float& fy, float& fz)
    {
        vec3 p = (x - origin) / h - vec3(0.0f, 0.5f, 0.5f);
        float xi = glm::floor(p.x);
        float yj = glm::floor(p.y);
        float zk = glm::floor(p.z);
        i = (int)xi; j = (int)yj; k = (int)zk;
        fx = p.x - xi; fy = p.y - yj; fz = p.z - zk;
    }

    static inline void worldToV(const vec3& origin, float h, const vec3& x,
        int& i, int& j, int& k, float& fx, float& fy, float& fz)
    {
        vec3 p = (x - origin) / h - vec3(0.5f, 0.0f, 0.5f);
        float xi = glm::floor(p.x);
        float yj = glm::floor(p.y);
        float zk = glm::floor(p.z);
        i = (int)xi; j = (int)yj; k = (int)zk;
        fx = p.x - xi; fy = p.y - yj; fz = p.z - zk;
    }

    static inline void worldToW(const vec3& origin, float h, const vec3& x,
        int& i, int& j, int& k, float& fx, float& fy, float& fz)
    {
        vec3 p = (x - origin) / h - vec3(0.5f, 0.5f, 0.0f);
        float xi = glm::floor(p.x);
        float yj = glm::floor(p.y);
        float zk = glm::floor(p.z);
        i = (int)xi; j = (int)yj; k = (int)zk;
        fx = p.x - xi; fy = p.y - yj; fz = p.z - zk;
    }

    float MacGrid::sampleU(const vec3& x) const
    {
        int i, j, k; float fx, fy, fz;
        worldToU(origin_, h_, clampToDomain(x), i, j, k, fx, fy, fz);

        // U 网格尺寸：(nx+1, ny, nz)
        auto C = [&](int ii, int jj, int kk) -> float {
            ii = std::clamp(ii, 0, nx_);
            jj = std::clamp(jj, 0, ny_ - 1);
            kk = std::clamp(kk, 0, nz_ - 1);
            return U(ii, jj, kk);
            };

        float c000 = C(i, j, k);
        float c100 = C(i + 1, j, k);
        float c010 = C(i, j + 1, k);
        float c110 = C(i + 1, j + 1, k);
        float c001 = C(i, j, k + 1);
        float c101 = C(i + 1, j, k + 1);
        float c011 = C(i, j + 1, k + 1);
        float c111 = C(i + 1, j + 1, k + 1);
        return trilerp(c000, c100, c010, c110, c001, c101, c011, c111, fx, fy, fz);
    }

    float MacGrid::sampleV(const vec3& x) const
    {
        int i, j, k; float fx, fy, fz;
        worldToV(origin_, h_, clampToDomain(x), i, j, k, fx, fy, fz);

        auto C = [&](int ii, int jj, int kk) -> float {
            ii = std::clamp(ii, 0, nx_ - 1);
            jj = std::clamp(jj, 0, ny_);
            kk = std::clamp(kk, 0, nz_ - 1);
            return V(ii, jj, kk);
            };

        float c000 = C(i, j, k);
        float c100 = C(i + 1, j, k);
        float c010 = C(i, j + 1, k);
        float c110 = C(i + 1, j + 1, k);
        float c001 = C(i, j, k + 1);
        float c101 = C(i + 1, j, k + 1);
        float c011 = C(i, j + 1, k + 1);
        float c111 = C(i + 1, j + 1, k + 1);
        return trilerp(c000, c100, c010, c110, c001, c101, c011, c111, fx, fy, fz);
    }

    float MacGrid::sampleW(const vec3& x) const
    {
        int i, j, k; float fx, fy, fz;
        worldToW(origin_, h_, clampToDomain(x), i, j, k, fx, fy, fz);

        auto C = [&](int ii, int jj, int kk) -> float {
            ii = std::clamp(ii, 0, nx_ - 1);
            jj = std::clamp(jj, 0, ny_ - 1);
            kk = std::clamp(kk, 0, nz_);
            return W(ii, jj, kk);
            };

        float c000 = C(i, j, k);
        float c100 = C(i + 1, j, k);
        float c010 = C(i, j + 1, k);
        float c110 = C(i + 1, j + 1, k);
        float c001 = C(i, j, k + 1);
        float c101 = C(i + 1, j, k + 1);
        float c011 = C(i, j + 1, k + 1);
        float c111 = C(i + 1, j + 1, k + 1);
        return trilerp(c000, c100, c010, c110, c001, c101, c011, c111, fx, fy, fz);
    }

    vec3 MacGrid::sampleVelocity(const vec3& x) const
    {
        float ux = sampleU(x);
        float vy = sampleV(x);
        float wz = sampleW(x);
        return vec3(ux, vy, wz);
    }

    float MacGrid::sampleCellScalar(const std::vector<float>& s, const vec3& x) const
    {
        int i, j, k; float fx, fy, fz;
        worldToCell(origin_, h_, clampToDomain(x), i, j, k, fx, fy, fz);
        auto C = [&](int ii, int jj, int kk) -> float {
            ii = std::clamp(ii, 0, nx_ - 1);
            jj = std::clamp(jj, 0, ny_ - 1);
            kk = std::clamp(kk, 0, nz_ - 1);
            return s[idxP(ii, jj, kk)];
            };

        float c000 = C(i, j, k);
        float c100 = C(i + 1, j, k);
        float c010 = C(i, j + 1, k);
        float c110 = C(i + 1, j + 1, k);
        float c001 = C(i, j, k + 1);
        float c101 = C(i + 1, j, k + 1);
        float c011 = C(i, j + 1, k + 1);
        float c111 = C(i + 1, j + 1, k + 1);
        return trilerp(c000, c100, c010, c110, c001, c101, c011, c111, fx, fy, fz);
    }

} // namespace dk
