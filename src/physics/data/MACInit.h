#pragma once
#include "data/MacGrid.h"
#include <functional>

namespace dk::gridinit {
// 基础操作
void ClearAll(MacGrid& g);                                       // 清空所有字段
void FillDyeBox_World(MacGrid& g, const vec3& minW, const vec3& maxW, float value);
void FillDyeSphere_World(MacGrid& g, const vec3& centerW, float radiusW, float value);
void FillDyeCylinder_World(MacGrid& g, const vec3& baseCenterW, float radiusW,
                           float    yMinW, float   yMaxW, float       value);

// 在世界AABB内设置均匀速度（分别设置 U/V/W 三个面中心上的值）
void SetVelocityBox_World(MacGrid& g, const vec3& minW, const vec3& maxW, const vec3& vel);

// 常用场景
// 1) Dam Break：盒子左侧/底部填充“水”（用 dye 表示），速度初始为 0
void Scene_DamBreak(MacGrid& g, float fillRatioX = 0.35f,
                    float    fillHeightRatio     = 0.6f, float dyeValue = 1.0f);

// 2) 高处落下的水柱：在顶部放一段垂直圆柱（dye=1），给负向初速度 Vy
void Scene_FallingWaterColumn(MacGrid& g, float              columnRadiusW,
                              float    topHeightRatio, float initVy = -3.0f, float dyeValue = 1.0f);

// 3) 顶部“水龙头”入流：在顶面一个孔洞区域设置向下速度，并填充 dye
void Scene_FaucetInflowOnce(MacGrid&    g, const vec3& holeMinW, const vec3& holeMaxW,
                            const vec3& vel, float     dyeValue = 1.0f);

// 4) 剪切层：上半域 +Ux，下半域 -Ux，制造 Kelvin–Helmholtz 初始条件
void Scene_ShearLayer(MacGrid& g, float vTop = 1.0f, float vBottom = -1.0f, float transitionThicknessW = 0.0f);

// 5) 水平平面内的固定位形旋涡场（绕 centerW，自转角速度 omega）
void Scene_VortexSpin(MacGrid& g, const vec3& centerW, float omega, float maxRadiusW);

// 小工具：给定谓词，在满足条件的位置上设置面/中心速度或染料
void PaintVelocityByPredicate(MacGrid& g, const std::function<bool(const vec3&)>& inside, const vec3& velUVW);
void PaintDyeByPredicate(MacGrid& g, const std::function<bool(const vec3&)>& inside, float value);
}
