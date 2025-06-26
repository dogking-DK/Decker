// input_structures.slang

// 1. 场景数据常量缓冲区（set=0, binding=0）
struct SceneData {
    float4x4 view;
    float4x4 proj;
    float4x4 viewproj;
    float4   ambientColor;
    float4   sunlightDirection; // w = sun power
    float4   sunlightColor;
};
[[vk::binding(0), vk::set(0)]]
ConstantBuffer<SceneData> sceneData;

// 2. 贴图采样（bindless 或固定绑定）
#ifdef USE_BINDLESS
[[vk::binding(1), vk::set(0)]]
Texture2D<float4> allTextures[];
SamplerState      samplerAllTextures[];      // 需要与 allTextures 一一对应
#else
[[vk::binding(1), vk::set(1)]]
Texture2D<float4> colorTex;
[[vk::binding(2), vk::set(1)]]
Texture2D<float4> metalRoughTex;
SamplerState      texSampler;                // 通用采样器
#endif

// 3. 材质数据常量缓冲区（set=1, binding=0）
struct GLTFMaterialData {
    float4 colorFactors;
    float4 metal_rough_factors;
    int    colorTexID;
    int    metalRoughTexID;
    // 若有对齐需求，可插入 padding
};
[[vk::binding(0), vk::set(1)]]
ConstantBuffer<GLTFMaterialData> materialData;
