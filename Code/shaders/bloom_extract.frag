#version 450

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D colorTex;

layout(push_constant) uniform PushConstants {
    float exposure;
    float gamma;
    int tonemapMode;
    float bloomThreshold;
    float bloomStrength;
    float vignetteStrength;
    float filmGrain;
    float time;
    vec2 pad;
} push;

void main() {
    vec3 color = texture(colorTex, fragUV).rgb;
    float brightness = max(max(outColor.r, outColor.g), outColor.b);
    outColor = vec4(color * step(push.bloomThreshold, brightness), 1.0);
}