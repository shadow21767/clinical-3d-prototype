#version 450

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec4 fragColor;
layout(location = 2) in vec2 fragUV;
layout(location = 3) in vec3 fragWorld;

layout(location = 0) out vec4 outColor;

const vec3 KEY_DIR = normalize(vec3(3.2, 5.4, 3.6));
const vec3 FILL_DIR = normalize(vec3(-3.5, 2.4, -2.5));
const vec3 SKY = vec3(0.62, 0.77, 0.88);
const vec3 GROUND = vec3(0.10, 0.13, 0.17);

void main() {
    vec3 n = normalize(fragNormal);
    // Palette colors are authored as sRGB; light them in linear space and let
    // the sRGB swapchain re-encode on write.
    vec3 base = pow(fragColor.rgb, vec3(2.2));

    float key = max(dot(n, KEY_DIR), 0.0);
    float fill = max(dot(n, FILL_DIR), 0.0) * 0.28;
    // Hemispheric ambient so downward faces stay readable instead of going black.
    vec3 ambient = mix(GROUND, SKY, n.y * 0.5 + 0.5) * 0.42;

    vec3 lit = base * (ambient + key * 1.25 + fill);

    // Specular sheen keeps the equipment from looking like matte clay.
    vec3 viewDir = normalize(vec3(0.0, 1.5, 4.0) - fragWorld);
    vec3 halfDir = normalize(KEY_DIR + viewDir);
    lit += vec3(1.0) * pow(max(dot(n, halfDir), 0.0), 42.0) * 0.10;

    outColor = vec4(lit, fragColor.a);
}
