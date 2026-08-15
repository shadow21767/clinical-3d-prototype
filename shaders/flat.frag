#version 450

// Unlit pass for markers, rings, and the airway flow cone.

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec4 fragColor;
layout(location = 2) in vec2 fragUV;
layout(location = 3) in vec3 fragWorld;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(pow(fragColor.rgb, vec3(2.2)), fragColor.a);
}
