#version 450

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec4 inTangent;
layout (location = 3) in vec2 inTexcoord0;
layout (location = 4) in vec4 inColor;

layout (location = 0) out vec4 outColor;

layout (push_constant) uniform PushConstants
{
    mat4 viewproj;
    mat4 model;
    vec4 params; // x = outline thickness
} pc;

void main()
{
    vec3 worldNormal = normalize(mat3(pc.model) * inNormal);
    vec3 worldPos = vec3(pc.model * vec4(inPosition, 1.0));
    float thickness = pc.params.x;
    vec3 extrudedPos = worldPos + worldNormal * thickness;
    gl_Position = pc.viewproj * vec4(extrudedPos, 1.0);
    outColor = inColor;
}
