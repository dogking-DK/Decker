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

// -------------------- 预设场景 --------------------

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

    // 初始速度为0，投影后即可在重力下自然坍塌
}

void Scene_FallingWaterColumn(MacGrid& g, float              columnRadiusW,
                              float    topHeightRatio, float initVy, float dyeValue)
{
    ClearAll(g);
    const vec3 domMin = g.origin();
    const vec3 domMax = g.origin() + vec3(g.nx() * g.h(), g.ny() * g.h(), g.nz() * g.h());

    const float yTop     = domMin.y + topHeightRatio * (domMax.y - domMin.y);
    const float yBottom  = std::max(domMin.y + 0.7f * (domMax.y - domMin.y), yTop - 3.0f * g.h()); // 短柱段即可
    auto        centerXZ = vec3(0.5f * (domMin.x + domMax.x), 0.0f, 0.5f * (domMin.z + domMax.z));

    // 顶部一段垂直圆柱
    FillDyeCylinder_World(g, centerXZ, columnRadiusW, yBottom, yTop, dyeValue);

    // 给一个向下初速度（只设 V 分量为负）
    SetVelocityBox_World(g,
                         vec3(centerXZ.x - columnRadiusW, yBottom, centerXZ.z - columnRadiusW),
                         vec3(centerXZ.x + columnRadiusW, yTop, centerXZ.z + columnRadiusW),
                         vec3(0.0f, initVy, 0.0f));
}

void Scene_FaucetInflowOnce(MacGrid&    g, const vec3& holeMinW, const vec3& holeMaxW,
                            const vec3& vel, float     dyeValue)
{
    // 仅初始化一次入流团（若要持续入流，可在每帧 step 前再次调用）
    FillDyeBox_World(g, holeMinW, holeMaxW, dyeValue);
    SetVelocityBox_World(g, holeMinW, holeMaxW, vel);
}

void Scene_ShearLayer(MacGrid& g, float vTop, float vBottom, float transitionThicknessW)
{
    ClearAll(g);
    const float yMid = g.origin().y + 0.5f * g.ny() * g.h();

    // U 分量：上半域 +vTop，下半域 -vBottom，中间可做过渡
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
    // 其余分量为 0；dye 可按需涂抹上下两层颜色以便可视化
}

void Scene_VortexSpin(MacGrid& g, const vec3& centerW, float omega, float maxRadiusW)
{
    ClearAll(g);
    const float rMax2 = maxRadiusW * maxRadiusW;

    // 在水平面内制造绕 centerW 的匀角速度场：u = (-ω (y-?),  ω (x-?), 0)
    // 注意这里是 3D：我们让速度只在 XZ 平面自转（绕 Y 轴）
    // U faces（采样在 (i, j+1/2, k+1/2)）
    for (int k = 0; k < g.nz(); ++k)
        for (int j = 0; j < g.ny(); ++j)
            for (int i = 0; i < g.nx() + 1; ++i)
            {
                vec3  p  = g.uFacePos(i, j, k);
                auto  d  = vec2(p.x - centerW.x, p.z - centerW.z);
                float r2 = dot(d, d);
                if (r2 <= rMax2)
                {
                    // 理想刚体旋转：u = -ω (z - zc)
                    // 但 U 存的是 x 分量，绕 y 旋转速度 x = -ω (z - zc)
                    g.U(i, j, k) = -omega * (p.z - centerW.z);
                }
            }
    // V faces：绕 y 轴旋转对 y 分量为 0
    // 保持默认 0

    // W faces（z 分量 = ω (x - xc)）
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
    // 可选：在旋涡半径内填充一点染料，便于可视化
    PaintDyeByPredicate(g, [&](const vec3& c)
    {
        auto d = vec2(c.x - centerW.x, c.z - centerW.z);
        return dot(d, d) <= 0.9f * rMax2;
    }, 1.0f);
}
} // namespace dk::gridinit
