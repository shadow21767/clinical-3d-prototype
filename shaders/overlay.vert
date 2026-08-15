#version 450

// Screen-filling quad generated from the vertex index; no vertex buffer bound.

layout(location = 0) out vec2 fragUV;

vec2 positions[6] = vec2[](
    vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(1.0, 1.0),
    vec2(1.0, 1.0), vec2(-1.0, 1.0), vec2(-1.0, -1.0)
);

void main() {
    vec2 p = positions[gl_VertexIndex];
    gl_Position = vec4(p, 0.0, 1.0);
    fragUV = p * 0.5 + 0.5;
}
