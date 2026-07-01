#version 450

layout(set=0, binding=0) uniform sampler2D tileTextures[162];

layout(set=0, binding=1) uniform CameraUBO {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec4 cameraPos;
} camera;

layout(set=0, binding=2, std430) buffer HeightLUT {
    float heights[];
} lut;

layout(location=0) in vec3 inPos;
layout(location=1) in vec2 inUV;

layout(location=0) out float vHeight;
layout(location=1) out vec2 vUV;
layout(location=2) out vec3 vWorldPos;

layout(push_constant) uniform PushConstants {
    uint waterOverlay;
} pc;

void main() {
    float u_shift = fract(0.5 - inUV.x);
    float u_scaled = u_shift * 18.0;
    uint col = uint(min(u_scaled, 17.99999));
    float tileUV_x = clamp(u_scaled - float(col), 0.008, 0.992);

    float v_scaled = min(inUV.y * 9.0, 8.99999);
    uint row = uint(v_scaled);
    float tileUV_y = clamp(v_scaled - float(row), 0.008, 0.992);

    uint tileIndex = row * 18u + col;
    vec2 tileUV = vec2(tileUV_x, tileUV_y);
    float h = textureLod(tileTextures[tileIndex], tileUV, 0.0).r;
    uint idx = uint(h * 255.0 + 0.5);
    idx = clamp(idx, 0u, 255u);
    float elevation = lut.heights[idx];

    vec3 normal = normalize(inPos);
    vec3 pos = inPos + normal * elevation * 0.000008;

    vHeight = elevation;
    vUV = inUV;
    vWorldPos = pos;

    gl_Position = camera.proj * camera.view * camera.model * vec4(pos, 1.0);
}
