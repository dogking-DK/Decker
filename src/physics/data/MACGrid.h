// data/MacGrid.h
#pragma once
#include "Base.h"
#include <algorithm>
#include <cassert>

namespace dk {
/**
 * 3D MAC Staggered Grid (�̳� ISimulationState���� ISolver::solve �ӿ�ʹ��)
 * ��׼���֣�
 *  - u: (nx+1) * ny     * nz     �� x-����������λ�� x-����������
 *  - v: nx     * (ny+1) * nz     �� y-����������λ�� y-����������
 *  - w: nx     * ny     * (nz+1) �� z-����������λ�� z-����������
 *  - p, div, dye: nx * ny * nz   ����������ģ�
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

    // �ߴ�/����/ԭ��
    int   nx() const { return nx_; }
    int   ny() const { return ny_; }
    int   nz() const { return nz_; }
    float h() const { return h_; }
    vec3  origin() const { return origin_; }

    // --- �������ߣ�������: x ��죩 ---
    int idxP(int i, int j, int k) const { return (k * ny_ + j) * nx_ + i; }
    int idxU(int i, int j, int k) const { return (k * ny_ + j) * (nx_ + 1) + i; }
    int idxV(int i, int j, int k) const { return (k * (ny_ + 1) + j) * nx_ + i; }
    int idxW(int i, int j, int k) const { return ((k) * ny_ + j) * nx_ + i; }

    // --- λ�ã��������꣩ ---
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

    // --- clamp index ���Ϸ���Χ ---
    int clampI(int i) const { return std::clamp(i, 0, nx_ - 1); }
    int clampJ(int j) const { return std::clamp(j, 0, ny_ - 1); }
    int clampK(int k) const { return std::clamp(k, 0, nz_ - 1); }

    // --- ������ ---
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

    // --- �ٶȲ����������������ã� ---
    // ע�⣺�������ڸ����������������Բ�ֵ
    vec3 sampleVelocity(const vec3& x) const;

    // --- ������ cell center �ϵ������Բ���������Ⱦ�ϡ�ѹ���ȿ�ѡ�� ---
    float sampleCellScalar(const std::vector<float>& s, const vec3& x) const;

    // --- clamp �������굽�ɲ����� ---
    vec3 clampToDomain(const vec3& x) const
    {
        // �� 1.5h �� margin������Խ���ֵ
        const float eps  = 1.5f * h_;
        vec3        minp = origin_ + vec3(eps);
        vec3        maxp = origin_ + vec3(nx_ * h_ - eps, ny_ * h_ - eps, nz_ * h_ - eps);
        return clamp(x, minp, maxp);
    }

    // --- ���Ⱪ¶���ݣ��� solver �ã� ---
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

    // ���������ԣ��ڲ����ߣ�
    float sampleU(const vec3& x) const;
    float sampleV(const vec3& x) const;
    float sampleW(const vec3& x) const;

private:

    int   nx_, ny_, nz_;
    float h_;
    vec3  origin_;

    // ���ֶ�
    std::vector<float> u_, v_, w_;
    std::vector<float> p_, div_;
    std::vector<float> dye_;

    // ��ʱ����
    std::vector<float> u_tmp_, v_tmp_, w_tmp_, p_tmp_;
};
} // namespace dk
