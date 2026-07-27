#version 450

// Instanced sprite vertex shader.
//
// Binding 0 is a shared unit quad spanning -0.5..0.5 on both axes. Binding 1 is
// the per-instance stream written by SpriteBatch each frame. The quad is scaled,
// rotated and translated here so the CPU never touches vertex data.

// Per-vertex (binding 0)
layout(location = 0) in vec2 inQuadPosition;

// Per-instance (binding 1)
layout(location = 1) in vec2 inCenter;
layout(location = 2) in vec2 inSize;
layout(location = 3) in float inRotation;
layout(location = 4) in vec4 inColor;
layout(location = 5) in vec4 inUvRect;   // u0, v0, u1, v1

// The projection extent is supplied by the host so the same shader serves the
// game window and the editor viewport at any size.
layout(push_constant) uniform PushConstants {
    vec2 viewportSize;
    vec2 padding;
} pc;

layout(location = 0) out vec2 fragUv;
layout(location = 1) out vec4 fragColor;

void main() {
    vec2 local = inQuadPosition * inSize;

    float s = sin(inRotation);
    float c = cos(inRotation);
    vec2 rotated = vec2(local.x * c - local.y * s,
                        local.x * s + local.y * c);

    vec2 world = inCenter + rotated;

    // World pixels -> NDC. Y grows downwards, matching screen-space gameplay
    // coordinates.
    vec2 halfExtent = max(pc.viewportSize, vec2(1.0)) * 0.5;
    gl_Position = vec4(world / halfExtent - vec2(1.0), 0.0, 1.0);

    fragUv = mix(inUvRect.xy, inUvRect.zw, inQuadPosition + vec2(0.5));
    fragColor = inColor;
}
