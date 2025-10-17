#pragma once
#include "data/MacGrid.h"
#include <functional>

namespace dk::gridinit {
// ��������
void ClearAll(MacGrid& g);                                       // ��������ֶ�
void FillDyeBox_World(MacGrid& g, const vec3& minW, const vec3& maxW, float value);
void FillDyeSphere_World(MacGrid& g, const vec3& centerW, float radiusW, float value);
void FillDyeCylinder_World(MacGrid& g, const vec3& baseCenterW, float radiusW,
                           float    yMinW, float   yMaxW, float       value);

// ������AABB�����þ����ٶȣ��ֱ����� U/V/W �����������ϵ�ֵ��
void SetVelocityBox_World(MacGrid& g, const vec3& minW, const vec3& maxW, const vec3& vel);

// ���ó���
// 1) Dam Break���������/�ײ���䡰ˮ������ dye ��ʾ�����ٶȳ�ʼΪ 0
void Scene_DamBreak(MacGrid& g, float fillRatioX = 0.35f,
                    float    fillHeightRatio     = 0.6f, float dyeValue = 1.0f);

// 2) �ߴ����µ�ˮ�����ڶ�����һ�δ�ֱԲ����dye=1������������ٶ� Vy
void Scene_FallingWaterColumn(MacGrid& g, float              columnRadiusW,
                              float    topHeightRatio, float initVy = -3.0f, float dyeValue = 1.0f);

// 3) ������ˮ��ͷ���������ڶ���һ���׶��������������ٶȣ������ dye
void Scene_FaucetInflowOnce(MacGrid&    g, const vec3& holeMinW, const vec3& holeMaxW,
                            const vec3& vel, float     dyeValue = 1.0f);

// 4) ���в㣺�ϰ��� +Ux���°��� -Ux������ Kelvin�CHelmholtz ��ʼ����
void Scene_ShearLayer(MacGrid& g, float vTop = 1.0f, float vBottom = -1.0f, float transitionThicknessW = 0.0f);

// 5) ˮƽƽ���ڵĹ̶�λ�����г����� centerW����ת���ٶ� omega��
void Scene_VortexSpin(MacGrid& g, const vec3& centerW, float omega, float maxRadiusW);

// С���ߣ�����ν�ʣ�������������λ����������/�����ٶȻ�Ⱦ��
void PaintVelocityByPredicate(MacGrid& g, const std::function<bool(const vec3&)>& inside, const vec3& velUVW);
void PaintDyeByPredicate(MacGrid& g, const std::function<bool(const vec3&)>& inside, float value);
}
