#version 450

// The vitals monitor screen: a CPU-painted texture shown unlit so the trace and
// readouts stay their true colors.

layout(set = 1, binding = 0) uniform sampler2D screenTex;

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec4 fragColor;
layout(location = 2) in vec2 fragUV;
layout(location = 3) in vec3 fragWorld;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(texture(screenTex, fragUV).rgb, 1.0);
}
