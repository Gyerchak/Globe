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
    float u_shift = fract(0.5 - vUV.x);
    float u_scaled = u_shift * 18.0;
    uint col_idx = uint(min(u_scaled, 17.99999));
    float tileUV_x = clamp(u_scaled - float(col_idx), 0.008, 0.992);

    float v_scaled = min(vUV.y * 9.0, 8.99999);
    uint row = uint(v_scaled);
    float tileUV_y = clamp(v_scaled - float(row), 0.008, 0.992);

    uint tileIndex = row * 18u + col_idx;
    vec2 tileUV = vec2(tileUV_x, tileUV_y);

    ivec2 texSize = textureSize(tileTextures[tileIndex], 0);
    vec2 texelSize = 1.0 / vec2(texSize);

    // Analytical sphere tangent frame
    vec3 sphereN = normalize(vWorldPos);
    vec3 T = normalize(cross(sphereN, vec3(0, 1, 0)));
    if (length(T) < 0.001) T = normalize(cross(sphereN, vec3(1, 0, 0)));
    vec3 B = cross(sphereN, T);

    // View direction in world space → tangent space for parallax offset
    vec3 V = normalize(camera.cameraPos.xyz - vWorldPos);
    vec3 V_ts = vec3(dot(V, T), dot(V, B), dot(V, sphereN));

    // Sample height at original UV for parallax depth
    float h_center = textureLod(tileTextures[tileIndex], tileUV, 0.0).r;

    // Parallax offset: shift UV toward viewer based on height
    float parallaxScale = 0.03;
    vec2 parallaxOffset = V_ts.xy / max(V_ts.z, 0.05) * h_center * parallaxScale;
    parallaxOffset = clamp(parallaxOffset, -texelSize * 20, texelSize * 20);
    vec2 offsetUV = clamp(tileUV + parallaxOffset, 0.008, 0.992);

    // Sample height at offset UV (pixel-level displacement)
    float hC = textureLod(tileTextures[tileIndex], offsetUV, 0.0).r;
    float hL = textureLod(tileTextures[tileIndex], offsetUV + vec2(-texelSize.x, 0), 0.0).r;
    float hR = textureLod(tileTextures[tileIndex], offsetUV + vec2(texelSize.x, 0), 0.0).r;
    float hD = textureLod(tileTextures[tileIndex], offsetUV + vec2(0, -texelSize.y), 0.0).r;
    float hU = textureLod(tileTextures[tileIndex], offsetUV + vec2(0, texelSize.y), 0.0).r;

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

    // Geometry normal from screen-space derivatives
    vec3 worldPos = vWorldPos;
    vec3 fdx = dFdx(worldPos);
    vec3 fdy = dFdy(worldPos);
    vec3 geoN = normalize(cross(fdx, fdy));
    if (dot(geoN, worldPos) < 0.0) geoN = -geoN;

    // Perturb geometry normal with per-pixel height detail
    vec3 N = normalize(geoN - dhdu * T * 0.07 - dhdv * B * 0.07);

    // Directional lighting
    vec3 L = normalize(vec3(0.5, 0.8, 0.6));
    float NdotL = max(dot(N, L), 0.0);
    float ambient = 0.3;
    float diffuse = ambient + (1.0 - ambient) * NdotL;

    // Specular (Blinn-Phong)
    vec3 H = normalize(L + V);
    float NdotH = max(dot(N, H), 0.0);
    float specular = pow(NdotH, 64.0) * 0.15;

    // Rim light (atmospheric edge glow)
    float NdotV = max(dot(N, V), 0.0);
    float rim = pow(1.0 - NdotV, 3.0) * 0.4;

    vec3 lighting = vec3(diffuse + specular + rim);
    col.rgb *= lighting;

    // Distance fog
    float dist = length(worldPos - camera.cameraPos.xyz);
    float fog = 1.0 - exp(-dist * dist * 0.003);
    vec3 fogColor = vec3(0.35, 0.55, 0.75);
    col.rgb = mix(col.rgb, fogColor, fog);

    if (pc.waterOverlay == 1) {
        col = mix(col, vec4(0.0, 0.3, 0.8, 1.0), 0.4);
    }

    outColor = col;
}
