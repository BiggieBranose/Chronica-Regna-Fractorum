#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inTexCoord;

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec3 fragWorldPos;

void main() {
    vec3 right = vec3(ubo.view[0][0], ubo.view[1][0], ubo.view[2][0]);
    vec3 up = vec3(ubo.view[0][1], ubo.view[1][1], ubo.view[2][1]);

    vec3 center = vec3(ubo.model[3][0], ubo.model[3][1], ubo.model[3][2]);

    float sx = length(vec3(ubo.model[0][0], ubo.model[0][1], ubo.model[0][2]));
    float sy = length(vec3(ubo.model[1][0], ubo.model[1][1], ubo.model[1][2]));

    vec3 worldPos = center + right * inPosition.x * sx + up * inPosition.y * sy;

    gl_Position = ubo.proj * ubo.view * vec4(worldPos, 1.0);
    fragTexCoord = inTexCoord;
    fragWorldPos = worldPos;
}
