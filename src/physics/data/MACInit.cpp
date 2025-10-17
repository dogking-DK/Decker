#include "MACInit.h"
#include <algorithm>
#include <cmath>
#include <glm/gtx/norm.hpp>

namespace dk::gridinit {
static inline bool insideAABB(const vec3& p, const vec3& a, const vec3& b)
{
    return (p.x >= a.x && p.y >= a.y && p.z >= a.z &&
            p.x <= b.x && p.y <= b.y && p.z <= b.z);
}

void ClearAll(MacGrid& g)
{
    std::fill(g.u().begin(), g.u().end(), 0.0f);
    std::fill(g.v().begin(), g.v().end(), 0.0f);
    std::fill(g.w().begin(), g.w().end(), 0.0f);
    std::fill(g.p().begin(), g.p().end(), 0.0f);
    std::fill(g.div().begin(), g.div().end(), 0.0f);
    std::fill(g.dye().begin(), g.dye().end(), 0.0f);
}

void FillDyeBox_World(MacGrid& g, const vec3& minW, const vec3& maxW, float value)
{
    for (int k = 0; k < g.nz(); ++k)
        for (int j = 0; j < g.ny(); ++j)
            for (int i = 0; i < g.nx(); ++i)
            {
                vec3 c = g.cellCenter(i, j, k);
                if (insideAABB(c, minW, maxW))
                {
                    g.Dye(i, j, k) = value;
                }
            }
}

void FillDyeSphere_World(MacGrid& g, const vec3& centerW, float radiusW, float value)
{
    const float r2 = radiusW * radiusW;
    for (int k = 0; k < g.nz(); ++k)
        for (int j = 0; j < g.ny(); ++j)
            for (int i = 0; i < g.nx(); ++i)
            {
                vec3 c = g.cellCenter(i, j, k);
                if (length2(c - centerW) <= r2)
                {
                    g.Dye(i, j, k) = value;
                }
            }
}

void FillDyeCylinder_World(MacGrid& g, const vec3& baseCenterW, float radiusW,
                           float    yMinW, float   yMaxW, float       value)
{
    const float r2 = radiusW * radiusW;
    for (int k = 0; k < g.nz(); ++k)
        for (int j = 0; j < g.ny(); ++j)
            for (int i = 0; i < g.nx(); ++i)
            {
                vec3 c = g.cellCenter(i, j, k);
                if (c.y < yMinW || c.y > yMaxW) continue;
                auto dxz = vec2(c.x - baseCenterW.x, c.z - baseCenterW.z);
                if (dot(dxz, dxz) <= r2)
                {
                    g.Dye(i, j, k) = value;
                }
            }
}

void SetVelocityBox_World(MacGrid& g, const vec3& minW, const vec3& maxW, const vec3& vel)
{
    // U faces
    for (int k = 0; k < g.nz(); ++k)
        for (int j = 0; j < g.ny(); ++j)
            for (int i = 0; i < g.nx() + 1; ++i)
            {
                vec3 p = g.uFacePos(i, j, k);
                if (insideAABB(p, minW, maxW)) g.U(i, j, k) = vel.x;
            }
    // V faces
    for (int k = 0; k < g.nz(); ++k)
        for (int j = 0; j < g.ny() + 1; ++j)
            for (int i = 0; i < g.nx(); ++i)
            {
                vec3 p = g.vFacePos(i, j, k);
                if (insideAABB(p, minW, maxW)) g.V(i, j, k) = vel.y;
            }
    // W faces
    for (int k = 0; k < g.nz() + 1; ++k)
        for (int j = 0; j < g.ny(); ++j)
            for (int i = 0; i < g.nx(); ++i)
            {
                vec3 p = g.wFacePos(i, j, k);
                if (insideAABB(p, minW, maxW)) g.W(i, j, k) = vel.z;
            }
}

void PaintVelocityByPredicate(MacGrid& g, const std::function<bool(const vec3&)>& inside, const vec3& velUVW)
{
    // U
    for (int k = 0; k < g.nz(); ++k)
        for (int j = 0; j < g.ny(); ++j)
            for (int i = 0; i < g.nx() + 1; ++i)
            {
                vec3 p = g.uFacePos(i, j, k);
                if (inside(p)) g.U(i, j, k) = velUVW.x;
            }
    // V
    for (int k = 0; k < g.nz(); ++k)
        for (int j = 0; j < g.ny() + 1; ++j)
            for (int i = 0; i < g.nx(); ++i)
            {
                vec3 p = g.vFacePos(i, j, k);
                if (inside(p)) g.V(i, j, k) = velUVW.y;
            }
    // W
    for (int k = 0; k < g.nz() + 1; ++k)
        for (int j = 0; j < g.ny(); ++j)
            for (int i = 0; i < g.nx(); ++i)
            {
                vec3 p = g.wFacePos(i, j, k);
                if (inside(p)) g.W(i, j, k) = velUVW.z;
            }
}

void PaintDyeByPredicate(MacGrid& g, const std::function<bool(const vec3&)>& inside, float value)
{
    for (int k = 0; k < g.nz(); ++k)
        for (int j = 0; j < g.ny(); ++j)
            for (int i = 0; i < g.nx(); ++i)
            {
                vec3 c = g.cellCenter(i, j, k);
                if (inside(c)) g.Dye(i, j, k) = value;
            }
}

// -------------------- Ԥ�賡�� --------------------

void Scene_DamBreak(MacGrid& g, float fillRatioX, float fillHeightRatio, float dyeValue)
{
    ClearAll(g);
    const vec3 domMin = g.origin();
    const vec3 domMax = g.origin() + vec3(g.nx() * g.h(), g.ny() * g.h(), g.nz() * g.h());

    vec3 boxMin = domMin + vec3(0.0f, 0.0f, 0.0f);
    vec3 boxMax = domMin + vec3(fillRatioX * (domMax.x - domMin.x),
                                fillHeightRatio * (domMax.y - domMin.y),
                                0.8f * (domMax.z - domMin.z));
    FillDyeBox_World(g, boxMin, boxMax, dyeValue);

    // ��ʼ�ٶ�Ϊ0��ͶӰ�󼴿�����������Ȼ̮��
}

void Scene_FallingWaterColumn(MacGrid& g, float              columnRadiusW,
                              float    topHeightRatio, float initVy, float dyeValue)
{
    ClearAll(g);
    const vec3 domMin = g.origin();
    const vec3 domMax = g.origin() + vec3(g.nx() * g.h(), g.ny() * g.h(), g.nz() * g.h());

    const float yTop     = domMin.y + topHeightRatio * (domMax.y - domMin.y);
    const float yBottom  = std::max(domMin.y + 0.7f * (domMax.y - domMin.y), yTop - 3.0f * g.h()); // �����μ���
    auto        centerXZ = vec3(0.5f * (domMin.x + domMax.x), 0.0f, 0.5f * (domMin.z + domMax.z));

    // ����һ�δ�ֱԲ��
    FillDyeCylinder_World(g, centerXZ, columnRadiusW, yBottom, yTop, dyeValue);

    // ��һ�����³��ٶȣ�ֻ�� V ����Ϊ����
    SetVelocityBox_World(g,
                         vec3(centerXZ.x - columnRadiusW, yBottom, centerXZ.z - columnRadiusW),
                         vec3(centerXZ.x + columnRadiusW, yTop, centerXZ.z + columnRadiusW),
                         vec3(0.0f, initVy, 0.0f));
}

void Scene_FaucetInflowOnce(MacGrid&    g, const vec3& holeMinW, const vec3& holeMaxW,
                            const vec3& vel, float     dyeValue)
{
    // ����ʼ��һ�������ţ���Ҫ��������������ÿ֡ step ǰ�ٴε��ã�
    FillDyeBox_World(g, holeMinW, holeMaxW, dyeValue);
    SetVelocityBox_World(g, holeMinW, holeMaxW, vel);
}

void Scene_ShearLayer(MacGrid& g, float vTop, float vBottom, float transitionThicknessW)
{
    ClearAll(g);
    const float yMid = g.origin().y + 0.5f * g.ny() * g.h();

    // U �������ϰ��� +vTop���°��� -vBottom���м��������
    for (int k = 0; k < g.nz(); ++k)
        for (int j = 0; j < g.ny(); ++j)
            for (int i = 0; i < g.nx() + 1; ++i)
            {
                vec3  p = g.uFacePos(i, j, k);
                float v = (p.y > yMid) ? vTop : vBottom;
                if (transitionThicknessW > 0.0f)
                {
                    float t = std::clamp((p.y - (yMid - 0.5f * transitionThicknessW)) / transitionThicknessW, 0.0f,
                                         1.0f);
                    v = glm::mix(vBottom, vTop, t);
                }
                g.U(i, j, k) = v;
            }
    // �������Ϊ 0��dye �ɰ���ͿĨ����������ɫ�Ա���ӻ�
}

void Scene_VortexSpin(MacGrid& g, const vec3& centerW, float omega, float maxRadiusW)
{
    ClearAll(g);
    const float rMax2 = maxRadiusW * maxRadiusW;

    // ��ˮƽ���������� centerW ���Ƚ��ٶȳ���u = (-�� (y-?),  �� (x-?), 0)
    // ע�������� 3D���������ٶ�ֻ�� XZ ƽ����ת���� Y �ᣩ
    // U faces�������� (i, j+1/2, k+1/2)��
    for (int k = 0; k < g.nz(); ++k)
        for (int j = 0; j < g.ny(); ++j)
            for (int i = 0; i < g.nx() + 1; ++i)
            {
                vec3  p  = g.uFacePos(i, j, k);
                auto  d  = vec2(p.x - centerW.x, p.z - centerW.z);
                float r2 = dot(d, d);
                if (r2 <= rMax2)
                {
                    // ���������ת��u = -�� (z - zc)
                    // �� U ����� x �������� y ��ת�ٶ� x = -�� (z - zc)
                    g.U(i, j, k) = -omega * (p.z - centerW.z);
                }
            }
    // V faces���� y ����ת�� y ����Ϊ 0
    // ����Ĭ�� 0

    // W faces��z ���� = �� (x - xc)��
    for (int k = 0; k < g.nz() + 1; ++k)
        for (int j = 0; j < g.ny(); ++j)
            for (int i = 0; i < g.nx(); ++i)
            {
                vec3  p  = g.wFacePos(i, j, k);
                auto  d  = vec2(p.x - centerW.x, p.z - centerW.z);
                float r2 = dot(d, d);
                if (r2 <= rMax2)
                {
                    g.W(i, j, k) = omega * (p.x - centerW.x);
                }
            }
    // ��ѡ�������а뾶�����һ��Ⱦ�ϣ����ڿ��ӻ�
    PaintDyeByPredicate(g, [&](const vec3& c)
    {
        auto d = vec2(c.x - centerW.x, c.z - centerW.z);
        return dot(d, d) <= 0.9f * rMax2;
    }, 1.0f);
}
} // namespace dk::gridinit
