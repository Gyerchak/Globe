#version 450

layout(set=0, binding=3) uniform sampler2D colorRamp;

layout(location=0) in float vHeight;
layout(location=1) in vec2 vUV;

layout(location=0) out vec4 outColor;

layout(push_constant) uniform PushConstants {
    uint waterOverlay;
} pc;

void main() {
    vec4 col;
    if (vHeight <= 0.0) {
        col = vec4(0.0, 0.1, 0.5, 1.0);
    } else {
        float t = clamp(vHeight / 8846.0, 0.0, 1.0);
        float ramp_u = t * (254.0 / 256.0) + (1.5 / 256.0);
        col = texture(colorRamp, vec2(ramp_u, 0.5));
    }

    if (pc.waterOverlay == 1) {
        col = mix(col, vec4(0.0, 0.3, 0.8, 1.0), 0.4);
    }

    outColor = col;
}
