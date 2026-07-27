#version 450

// Samples the sprite atlas and tints by the per-instance colour. Both the alpha
// and the additive pipeline use this shader; only the blend state differs.

layout(set = 0, binding = 0) uniform sampler2D uAtlas;

layout(location = 0) in vec2 fragUv;
layout(location = 1) in vec4 fragColor;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = texture(uAtlas, fragUv) * fragColor;
}
