#version 450

layout(binding = 0) uniform Camera {
    mat4 view;
    mat4 proj;
    vec4 lightDir;
} cam;

layout(location = 0) in vec2 inPos;
layout(location = 1) in vec2 inUV;
layout(location = 2) in uint inTexIndex;

layout(location = 0) out vec2 fragUV;
layout(location = 1) out flat uint fragTexIndex;
layout(location = 2) out vec4 fragColor;

void main() {
    gl_Position = vec4(inPos, 0.0, 1.0);
    fragUV = inUV;
    fragTexIndex = inTexIndex;
    fragColor = vec4(1.0);
}