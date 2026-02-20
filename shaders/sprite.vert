#version 450

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 fragColor;

layout(push_constant) uniform PushConstants {
    vec2 position;
    vec2 scale;
} pc;

void main() {
    // Simple 2D orthographic projection
    vec2 pos = inPosition * pc.scale + pc.position;

    // Convert to NDC (normalized device coordinates)
    // Assuming 1280x720 resolution
    vec2 ndc = vec2(
        (pos.x / 640.0) - 1.0,
        (pos.y / 360.0) - 1.0
    );

    gl_Position = vec4(ndc, 0.0, 1.0);
    fragColor = inColor;
}
