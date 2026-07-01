#version 450

layout(set=0, binding=0) uniform sampler2D tileTextures[162];
layout(set=0, binding=1) uniform CameraUBO {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec4 cameraPos;
} camera;
layout(set=0, binding=3) uniform sampler2D colorRamp;

layout(location=0) in float vHeight;
layout(location=1) in vec2 vUV;
layout(location=2) in vec3 vWorldPos;

layout(location=0) out vec4 outColor;

layout(push_constant) uniform PushConstants {
    uint waterOverlay;
} pc;

void main() {
    // Recompute tile coordinates per-pixel (exact same math as vertex shader)
    float u_shift = fract(0.5 - vUV.x);
    float u_scaled = u_shift * 18.0;
    uint col_idx = uint(min(u_scaled, 17.99999));
    float tileUV_x = u_scaled - float(col_idx);

    float v_scaled = min(vUV.y * 9.0, 8.99999);
    uint row = uint(v_scaled);
    float tileUV_y = v_scaled - float(row);

    uint tileIndex = row * 18u + col_idx;
    vec2 tileUV = vec2(tileUV_x, tileUV_y);

    // 1:1 pixel-level height sampling from the tile
    ivec2 texSize = textureSize(tileTextures[tileIndex], 0);
    vec2 texelSize = 1.0 / vec2(texSize);

    float hC = textureLod(tileTextures[tileIndex], tileUV, 0.0).r;
    float hL = textureLod(tileTextures[tileIndex], tileUV + vec2(-texelSize.x, 0), 0.0).r;
    float hR = textureLod(tileTextures[tileIndex], tileUV + vec2(texelSize.x, 0), 0.0).r;
    float hD = textureLod(tileTextures[tileIndex], tileUV + vec2(0, -texelSize.y), 0.0).r;
    float hU = textureLod(tileTextures[tileIndex], tileUV + vec2(0, texelSize.y), 0.0).r;

    float elevation = hC * 8846.0;
    float dhdu = (hR - hL) * 0.5 / texelSize.x;
    float dhdv = (hU - hD) * 0.5 / texelSize.y;

    vec4 col;
    if (elevation <= 0.0) {
        col = vec4(0.0, 0.1, 0.5, 1.0);
    } else {
        float t = clamp(elevation / 8846.0, 0.0, 1.0);
        float ramp_u = t * (254.0 / 256.0) + (1.5 / 256.0);
        col = texture(colorRamp, vec2(ramp_u, 0.5));
    }

    // Base normal from screen-space derivatives of displaced position
    vec3 worldPos = vWorldPos;
    vec3 fdx = dFdx(worldPos);
    vec3 fdy = dFdy(worldPos);
    vec3 normal = normalize(cross(fdx, fdy));
    if (dot(normal, worldPos) < 0.0) normal = -normal;

    // Sphere tangent/bitangent for UV-space gradient → world-space tilt
    vec3 tangent = normalize(cross(vec3(0, 1, 0), normal));
    if (length(tangent) < 0.001) tangent = vec3(1, 0, 0);
    vec3 bitangent = cross(normal, tangent);

    // Perturb normal with pixel-level height detail
    normal = normalize(normal - dhdu * tangent * 0.00003 - dhdv * bitangent * 0.00003);

    // Directional lighting (sun from upper-right-front)
    vec3 lightDir = normalize(vec3(0.5, 0.8, 0.6));
    float diffuse = max(dot(normal, lightDir), 0.0);
    float ambient = 0.3;
    float lighting = ambient + (1.0 - ambient) * diffuse;
    col.rgb *= lighting;

    // Distance fog
    float dist = length(worldPos - camera.cameraPos.xyz);
    float fog = 1.0 - exp(-dist * dist * 0.005);
    vec3 fogColor = vec3(0.35, 0.55, 0.75);
    col.rgb = mix(col.rgb, fogColor, fog);

    if (pc.waterOverlay == 1) {
        col = mix(col, vec4(0.0, 0.3, 0.8, 1.0), 0.4);
    }

    outColor = col;
}
