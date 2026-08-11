#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec3 fragDir;

void main() {
    fragDir = inPosition;
    vec4 pos = ubo.proj * mat4(mat3(ubo.view)) * vec4(inPosition, 1.0);
    gl_Position = pos.xyww;
}
