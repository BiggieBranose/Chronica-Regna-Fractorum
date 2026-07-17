#version 450

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec3 fragWorldPos;

layout(location = 0) out vec4 outColor;

layout(binding = 1) uniform sampler2D spriteTex;

void main() {
    vec4 texColor = texture(spriteTex, fragTexCoord);
    if (texColor.a < 0.1) discard;

    vec3 sunDir = normalize(vec3(2.0, 4.0, 1.0));
    vec3 normal = vec3(0.0, 1.0, 0.0);
    float NdotL = max(dot(normal, sunDir), 0.0);
    float lighting = 0.45 + 0.55 * NdotL;

    outColor = vec4(texColor.rgb * lighting, texColor.a);
}
