#version 450

layout(push_constant) uniform ShadowPushConstants {
    mat4 cascadeViewProj[4];
    float cascadeSplits[4];
} shadow;

layout(location = 0) in vec3 inPosition;

void main() {
    gl_Position = shadow.cascadeViewProj[gl_InstanceIndex % 4] * vec4(inPosition, 1.0);
}