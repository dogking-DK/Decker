#version 450

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec4 inColor;
layout (location = 2) in vec2 inTexcoord0;

layout (location = 0) out vec4 outColor;
layout (location = 1) out vec2 outUV;

layout (push_constant) uniform PushConstants
{
    mat4 viewproj;
    mat4 model;
} pc;

void main()
{
    gl_Position = pc.viewproj * pc.model * vec4(inPosition, 1.0);
    outColor = inColor;
    outUV = inTexcoord0;
}
