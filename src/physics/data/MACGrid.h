// MACGrid.h
#pragma once
#include "Base.h" // ��Ļ���ͷ�ļ������� glm ��
#include <vector>

namespace dk {
// ��������Ԫ�������
enum class CellType : uint8_t
{
    AIR,    // ����
    FLUID,  // ����
    SOLID   // ����/�߽�
};

class MACGrid
{
public:
    MACGrid(const glm::ivec3& dimensions, float cell_size)
        : m_dims(dimensions), m_cellSize(cell_size)
    {
        // �ٶȷ��� U �洢�� (i+1/2, j, k) �������� X ά�ȶ�һ��
        m_u.resize((m_dims.x + 1) * m_dims.y * m_dims.z, 0.0f);
        // �ٶȷ��� V �洢�� (i, j+1/2, k) �������� Y ά�ȶ�һ��
        m_v.resize(m_dims.x * (m_dims.y + 1) * m_dims.z, 0.0f);
        // �ٶȷ��� W �洢�� (i, j, k+1/2) �������� Z ά�ȶ�һ��
        m_w.resize(m_dims.x * m_dims.y * (m_dims.z + 1), 0.0f);

        // ѹ�������ʹ洢�ڵ�Ԫ������
        m_pressure.resize(m_dims.x * m_dims.y * m_dims.z, 0.0f);
        m_cellType.resize(m_dims.x * m_dims.y * m_dims.z, CellType::AIR);
    }

    // --- �����ӿ� ---

    // ������ռ��е�����λ�ö��ٶȳ����������Բ�ֵ����
    // ����ƽ������Ĺؼ�
    glm::vec3 sampleVelocity(const glm::vec3& world_pos) const
    {
        // ʵ��ϸ�ڣ�
        // 1. �� world_pos ת��Ϊ�������� (������)
        // 2. �ֱ�� U, V, W �ٶȷ������������Բ�ֵ
        //    - ���� U ��Ҫ���佻�������8���ھӵ��ϲ�ֵ
        //    - ���� V, W ͬ��
        // 3. ���ز�ֵ�õ��� (u, v, w) ����
        return glm::vec3(0.0f); // ��ʵ��
    }

    const glm::ivec3& getDimensions() const { return m_dims; }
    float             getCellSize() const { return m_cellSize; }

    // --- ���ݳ�Ա (Ϊ���������Ϊ public) ---

    glm::ivec3 m_dims;
    float      m_cellSize;

    // �ٶȳ� (����洢)
    std::vector<float> m_u, m_v, m_w;

    // ��Ԫ����������
    std::vector<float>    m_pressure;
    std::vector<CellType> m_cellType;
};
}
