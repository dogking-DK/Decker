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
layout(binding = 0, set = 0) ConstantBuffer<SceneData> sceneData;


// 2. 贴图采样（bindless 或固定绑定）
#ifdef USE_BINDLESS
layout(binding = 1, set = 0) Sampler2D allTextures[];
#else
layout(binding = 0, set = 0) Sampler2D colorTex;
layout(binding = 2, set = 1) Sampler2D metalRoughTex;
#endif

// 3. 材质数据常量缓冲区（set=1, binding=0）
struct GLTFMaterialData 
{
    float4 colorFactors;
    float4 metal_rough_factors;
    int    colorTexID;
    int    metalRoughTexID;
    // 若有对齐需求，可插入 padding
};
layout(binding = 0, set = 1) ConstantBuffer<GLTFMaterialData> materialData;
