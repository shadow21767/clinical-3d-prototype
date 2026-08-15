#version 450

// Composites the CPU-painted teaching panel over the rendered scene.

layout(set = 0, binding = 0) uniform sampler2D uiTex;

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

void main() {
    outColor = texture(uiTex, fragUV);
}
