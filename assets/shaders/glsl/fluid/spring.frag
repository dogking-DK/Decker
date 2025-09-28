#version 460

layout(location = 0) in  vec4 inColor;
layout(location = 0) out vec4 outColor;

void main() {
    outColor = inColor; // 直接输出插值后的端点颜色
}
