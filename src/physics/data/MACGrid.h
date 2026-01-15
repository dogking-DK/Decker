// data/MacGrid.h
#pragma once
#include "Base.h"
#include <algorithm>
#include <cassert>

namespace dk {
/**
 * 3D MAC Staggered Grid (继承 ISimulationState，供 ISolver::solve 接口使用)
 * 标准布局：
 *  - u: (nx+1) * ny     * nz     存 x-向量分量，位于 x-法向面中心
 *  - v: nx     * (ny+1) * nz     存 y-向量分量，位于 y-法向面中心
 *  - w: nx     * ny     * (nz+1) 存 z-向量分量，位于 z-法向面中心
 *  - p, div, dye: nx * ny * nz   存标量（中心）
 */
class MacGrid : public ISimulationState
{
public:
    MacGrid(int nx, int ny, int nz, float h, const vec3& origin = vec3(0))
        : nx_(nx), ny_(ny), nz_(nz), h_(h), origin_(origin)
    {
        assert(nx_ > 0 && ny_ > 0 && nz_ > 0 && h_ > 0);
        u_.assign((nx_ + 1) * ny_ * nz_, 0.0f);
        v_.assign(nx_ * (ny_ + 1) * nz_, 0.0f);
        w_.assign(nx_ * ny_ * (nz_ + 1), 0.0f);

        p_.assign(nx_ * ny_ * nz_, 0.0f);
        div_.assign(nx_ * ny_ * nz_, 0.0f);
        dye_.assign(nx_ * ny_ * nz_, 0.0f);

        // scratch
        u_tmp_.assign(u_.size(), 0.0f);
        v_tmp_.assign(v_.size(), 0.0f);
        w_tmp_.assign(w_.size(), 0.0f);
        p_tmp_.assign(p_.size(), 0.0f);
    }

    // 尺寸/步长/原点
    int   nx() const { return nx_; }
    int   ny() const { return ny_; }
    int   nz() const { return nz_; }
    float h() const { return h_; }
    vec3  origin() const { return origin_; }

    // --- 索引工具（行优先: x 最快） ---
    int idxP(int i, int j, int k) const { return (k * ny_ + j) * nx_ + i; }
    int idxU(int i, int j, int k) const { return (k * ny_ + j) * (nx_ + 1) + i; }
    int idxV(int i, int j, int k) const { return (k * (ny_ + 1) + j) * nx_ + i; }
    int idxW(int i, int j, int k) const { return ((k) * ny_ + j) * nx_ + i; }

    // --- 位置（世界坐标） ---
    vec3 cellCenter(int i, int j, int k) const
    {
        return origin_ + h_ * vec3(i + 0.5f, j + 0.5f, k + 0.5f);
    }

    vec3 uFacePos(int i, int j, int k) const
    {
        return origin_ + h_ * vec3(i, j + 0.5f, k + 0.5f);
    }

    vec3 vFacePos(int i, int j, int k) const
    {
        return origin_ + h_ * vec3(i + 0.5f, j, k + 0.5f);
    }

    vec3 wFacePos(int i, int j, int k) const
    {
        return origin_ + h_ * vec3(i + 0.5f, j + 0.5f, k);
    }

    // --- clamp index 到合法范围 ---
    int clampI(int i) const { return std::clamp(i, 0, nx_ - 1); }
    int clampJ(int j) const { return std::clamp(j, 0, ny_ - 1); }
    int clampK(int k) const { return std::clamp(k, 0, nz_ - 1); }

    // --- 访问器 ---
    float& U(int i, int j, int k) { return u_[idxU(i, j, k)]; }
    float& V(int i, int j, int k) { return v_[idxV(i, j, k)]; }
    float& W(int i, int j, int k) { return w_[idxW(i, j, k)]; }

    float U(int i, int j, int k) const { return u_[idxU(i, j, k)]; }
    float V(int i, int j, int k) const { return v_[idxV(i, j, k)]; }
    float W(int i, int j, int k) const { return w_[idxW(i, j, k)]; }

    float& P(int i, int j, int k) { return p_[idxP(i, j, k)]; }
    float  P(int i, int j, int k) const { return p_[idxP(i, j, k)]; }

    float& Div(int i, int j, int k) { return div_[idxP(i, j, k)]; }
    float  Div(int i, int j, int k) const { return div_[idxP(i, j, k)]; }

    float& Dye(int i, int j, int k) { return dye_[idxP(i, j, k)]; }
    float  Dye(int i, int j, int k) const { return dye_[idxP(i, j, k)]; }

    // --- 速度采样（半拉格朗日用） ---
    // 注意：各分量在各自网格上做三线性插值
    vec3 sampleVelocity(const vec3& x) const;

    // --- 标量在 cell center 上的三线性采样（用于染料、压力等可选） ---
    float sampleCellScalar(const std::vector<float>& s, const vec3& x) const;

    // --- clamp 物理坐标到可采样域 ---
    vec3 clampToDomain(const vec3& x) const
    {
        // 留 1.5h 的 margin，避免越界插值
        const float eps  = 1.5f * h_;
        vec3        minp = origin_ + vec3(eps);
        vec3        maxp = origin_ + vec3(nx_ * h_ - eps, ny_ * h_ - eps, nz_ * h_ - eps);
        return clamp(x, minp, maxp);
    }

    // --- 对外暴露数据（给 solver 用） ---
    std::vector<float>& u() { return u_; }
    std::vector<float>& v() { return v_; }
    std::vector<float>& w() { return w_; }
    std::vector<float>& p() { return p_; }
    std::vector<float>& div() { return div_; }
    std::vector<float>& dye() { return dye_; }

    const std::vector<float>& u() const { return u_; }
    const std::vector<float>& v() const { return v_; }
    const std::vector<float>& w() const { return w_; }
    const std::vector<float>& p() const { return p_; }
    const std::vector<float>& div() const { return div_; }
    const std::vector<float>& dye() const { return dye_; }

    // scratch
    std::vector<float>& u_tmp() { return u_tmp_; }
    std::vector<float>& v_tmp() { return v_tmp_; }
    std::vector<float>& w_tmp() { return w_tmp_; }
    std::vector<float>& p_tmp() { return p_tmp_; }

    // 分量三线性（内部工具）
    float sampleU(const vec3& x) const;
    float sampleV(const vec3& x) const;
    float sampleW(const vec3& x) const;

private:

    int   nx_, ny_, nz_;
    float h_;
    vec3  origin_;

    // 主字段
    std::vector<float> u_, v_, w_;
    std::vector<float> p_, div_;
    std::vector<float> dye_;

    // 临时缓存
    std::vector<float> u_tmp_, v_tmp_, w_tmp_, p_tmp_;
};
} // namespace dk
