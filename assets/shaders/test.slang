// 顶点数据结构定义
struct Vertex {
    float3 position;
    float3 normal;
    float2 uv;
};

// 网格参数常量
cbuffer MeshConstants : register(b0) {
    uint vertexCount;
    uint triangleCount;
    float4x4 worldMatrix;
};

// 输入顶点数据存储缓冲区
StructuredBuffer<Vertex> vertexBuffer : register(t1);
StructuredBuffer<uint> indexBuffer : register(t2);

// 输出拓扑结构
struct MeshOutput {
    float4 position : SV_Position;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

// 线程组配置（每个线程处理一个三角形）
[outputtopology("triangle")]
[numthreads(128, 1, 1)]
void MainMesh(
    // 系统值参数
    uint gtid : SV_GroupThreadID,
    uint gid : SV_GroupID,
    // 输出声明
    out vertices MeshOutput verts[128],
    out indices uint3 tris[256]
)
{
    // 设置网格输出参数
    SetMeshOutputCounts(vertexCount, triangleCount);

    // 顶点处理
    if (gtid < vertexCount) {
        Vertex v = vertexBuffer[gtid];
        
        // 转换到齐次裁剪空间
        verts[gtid].position = mul(worldMatrix, float4(v.position, 1.0));
        verts[gtid].normal = mul((float3x3)worldMatrix, v.normal);
        verts[gtid].uv = v.uv;
    }

    // 索引处理（每3个索引组成一个三角形）
    uint triIndex = gtid * 3;
    if (triIndex + 2 < triangleCount * 3) {
        tris[gtid] = uint3(
            indexBuffer[triIndex],
            indexBuffer[triIndex + 1],
            indexBuffer[triIndex + 2]
        );
    }
}