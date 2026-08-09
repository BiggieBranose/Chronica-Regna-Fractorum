#version 450

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) in vec2 fragTexCoord;

layout(location = 1) in vec3 fragNormal;

layout(location = 0) out vec4 outColor;

void main() {
    float brightness = max(dot(normalize(fragNormal), vec3(0.0, 1.0, 0.0)), 0.0) + 0.05;
    outColor = texture(texSampler, fragTexCoord) * brightness;
}