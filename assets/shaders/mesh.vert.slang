// mesh.vert.slang

#include "input_structures.slang"

// 顶点结构体
struct Vertex 
{
    float3 position;
    float  uv_x;
    float3 normal;
    float  uv_y;
    float4 color;
};

// 结构化缓冲区，原 buffer_reference 改为 StructuredBuffer
[[vk::binding(0), vk::set(2)]]
StructuredBuffer<Vertex> vertexBuffer;

// Push Constants 只保留 render_matrix
[[vk::push_constant]]
struct PushConstants {
    float4x4 render_matrix;
};
[[vk::push_constant]]
PushConstants pushConstants;

// VS 输出布局
struct VSOutput {
    float4 position : SV_Position;
    float3 normal   : NORMAL;
    float3 color    : COLOR;
    float2 uv       : TEXCOORD;
};

VSOutput VSMain(uint vertexIndex : SV_VertexID)
{
    VSOutput out;

    // 从结构化缓冲区读取
    Vertex v = vertexBuffer[vertexIndex];
    float4 pos = float4(v.position, 1.0);

    // 计算裁剪空间位置
    out.position = mul(mul(sceneData.viewproj, pushConstants.render_matrix), pos);

    // 计算法线（假设 render_matrix 无缩放或已正交化）
    out.normal = mul(pushConstants.render_matrix, float4(v.normal, 0.0)).xyz;

    // 顶点颜色与材质色系数相乘
    out.color = v.color.xyz * materialData.colorFactors.xyz;

    // UV 坐标
    out.uv = float2(v.uv_x, v.uv_y);

    return out;
}
