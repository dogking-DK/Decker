#version 450

layout (location = 0) in vec4 inColor;
layout (location = 0) out vec4 outFragColor;

void main()
{
    // 简单描边颜色：使用顶点色并增强对比度
    vec3 tint = mix(inColor.rgb, vec3(1.0, 0.8, 0.2), 0.7);
    outFragColor = vec4(tint, 1.0);
}
