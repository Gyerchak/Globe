#version 450

layout(set=0, binding=0) uniform sampler2D tileTextures[162];

layout(set=0, binding=1) uniform CameraUBO {
    mat4 model;
    mat4 view;
    mat4 proj;
} camera;

layout(set=0, binding=2, std430) buffer HeightLUT {
    float heights[];
} lut;

layout(location=0) in vec3 inPos;
layout(location=1) in vec2 inUV;

layout(location=0) out float vHeight;
layout(location=1) out vec2 vUV;

layout(push_constant) uniform PushConstants {
    uint waterOverlay;
} pc;

void main() {
    float u_shift = fract(inUV.x + 0.5);
    float u_scaled = u_shift * 18.0;
    uint col = uint(min(u_scaled, 17.99999));
    float tileUV_x = u_scaled - float(col);

    float v_scaled = min(inUV.y * 9.0, 8.99999);
    uint row = uint(v_scaled);
    float tileUV_y = v_scaled - float(row);

    uint tileIndex = row * 18u + col;
    vec2 tileUV = vec2(tileUV_x, tileUV_y);
    float h = texture(tileTextures[tileIndex], tileUV).r;
    uint idx = uint(h * 255.0 + 0.5);
    idx = clamp(idx, 0u, 255u);
    float elevation = lut.heights[idx];

    vec3 normal = normalize(inPos);
    vec3 pos = inPos + normal * elevation * 0.00001;

    vHeight = elevation;
    vUV = inUV;

    gl_Position = camera.proj * camera.view * camera.model * vec4(pos, 1.0);
}
