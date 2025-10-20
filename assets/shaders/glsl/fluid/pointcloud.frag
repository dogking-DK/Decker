#version 460
layout(location=0) in  vec3 inColor;
layout(location=0) out vec4 outColor;

// 如需“圆点”，可打开下面的 discard：
// layout(origin_upper_left) in vec4 gl_FragCoord; // 可选
// in vec2 gl_PointCoord; // Mesh Shader 输出 points 同样可用
void main(){
    // 圆形点（可选）：
//  vec2 q = gl_PointCoord * 2.0 - 1.0;
//  if (dot(q,q) > 1.0) discard;

    outColor = vec4(inColor, 1.0);
}
