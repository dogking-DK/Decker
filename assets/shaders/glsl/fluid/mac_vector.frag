#version 460
#extension GL_EXT_mesh_shader : require

layout(location = 0) perprimitiveEXT in vec3 pColor; // ✅ 非数组
layout(location = 1) in vec3 vColor;

layout(location = 0) out vec4 outColor;

void main() {
    // 任选其一，这里用 vColor
    outColor = vec4(vColor, 1.0);
}
