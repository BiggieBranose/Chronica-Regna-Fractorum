#version 450
#extension GL_EXT_nonuniform_qualifier : enable

layout(binding = 0, set = 0) uniform sampler2DArray shadowMaps;

layout(location = 0) in vec2 fragUV;
layout(location = 1) flat in uint fragTexIndex;
layout(location = 2) in vec4 fragColor;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform SpritePush {
    mat4 view;
    mat4 proj;
    vec4 lightDir;
    vec4 cameraPos;
    float cascadeSplits[4];
    mat4 shadowViewProj[4];
} push;

layout(set = 1, binding = 0) uniform sampler2D spriteTextures[];

float sampleShadow(vec3 fragPos) {
    vec4 shadowCoord = push.shadowViewProj[0] * vec4(fragPos, 1.0);
    shadowCoord.xyz /= shadowCoord.w;
    shadowCoord.xyz = shadowCoord.xyz * 0.5 + 0.5;

    if (shadowCoord.z > 1.0) return 1.0;

    float shadow = 1.0;
    float bias = max(0.005 * (1.0 - dot(normalize(push.lightDir.xyz), vec3(0.0, 0.0, 1.0))), 0.005);
    float depth = texture(shadowMaps, vec3(shadowCoord.xy, 0)).r;
    if (shadowCoord.z - bias > depth) shadow = 0.3;
    return shadow;
}

void main() {
    vec4 texColor = texture(spriteTextures[fragTexIndex], fragUV);
    if (texColor.a < 0.01) discard;

    vec3 normal = vec3(0.0, 0.0, 1.0);
    vec3 lightDir = normalize(push.lightDir.xyz);
    float NdotL = max(dot(normal, lightDir), 0.0);

    float shadow = 1.0;

    vec3 worldPos = vec3(push.view * vec4(0.0, 0.0, 0.0, 1.0));
    shadow = sampleShadow(worldPos);

    vec3 lighting = vec3(0.2 + 0.8 * NdotL) * (0.3 + 0.7 * shadow);
    outColor = texColor * vec4(lighting, 1.0) * fragColor;
}