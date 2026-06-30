#version 450
layout(set=0, binding=1) uniform Camera { mat4 view; mat4 proj; };

layout(location=0) in vec3 inPos;
layout(location=1) in vec2 inUV;

layout(location=0) out float vHeight;

void main() {
    gl_Position = proj * view * vec4(inPos, 1.0);
    vHeight = 0.0;  // not used, but must be defined
}