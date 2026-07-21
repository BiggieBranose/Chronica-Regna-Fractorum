#version 450

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D bloomTex;

layout(push_constant) uniform PushConstants {
    int blurDirection; // 0 = horizontal, 1 = vertical
    int blurRadius;
    float blurSigma;
    float pad;
} push;

void main() {
    vec2 texelSize = 1.0 / textureSize(bloomTex, 0);
    vec2 offset = (push.blurDirection == 0) ? vec2(texelSize.x, 0.0) : vec2(0.0, texelSize.y);

    vec3 result = texture(bloomTex, fragUV).rgb * 0.2270270270;
    result += texture(bloomTex, fragUV + offset * 1.0).rgb * 0.1945945946;
    result += texture(bloomTex, fragUV + offset * 2.0).rgb * 0.1216216216;
    result += texture(bloomTex, fragUV + offset * 3.0).rgb * 0.0540540541;
    result += texture(bloomTex, fragUV + offset * 4.0).rgb * 0.0162162162;

    outColor = vec4(result, 1.0);
}