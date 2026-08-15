#version 450

layout(binding = 0) uniform ViewProj {
    mat4 view;
    mat4 proj;
} vp;

layout(push_constant) uniform PushConsts {
    mat4 model;
    vec4 color;
} pc;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec4 fragColor;
layout(location = 2) out vec2 fragUV;
layout(location = 3) out vec3 fragWorld;

void main() {
    vec4 world = pc.model * vec4(inPosition, 1.0);
    gl_Position = vp.proj * vp.view * world;

    // Bones are scaled non-uniformly, so normals need the inverse transpose.
    fragNormal = normalize(transpose(inverse(mat3(pc.model))) * inNormal);
    fragColor = pc.color;
    fragUV = inUV;
    fragWorld = world.xyz;
}
