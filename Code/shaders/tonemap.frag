#version 450

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D colorTex;

layout(push_constant) uniform PushConstants {
    float exposure;
    float gamma;
    int tonemapMode;
    float pad;
} push;

vec3 acesFilm(vec3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

vec3 reinhard(vec3 x) {
    return x / (x + vec3(1.0));
}

vec3 uncharted2(vec3 x) {
    float A = 0.15;
    float B = 0.50;
    float C = 0.10;
    float D = 0.20;
    float E = 0.02;
    float F = 0.30;
    return ((x * (A * x + B * vec3(1.0)) + C * vec3(1.0)) / (x * (A * x + B) + D * vec3(1.0)) - E * vec3(1.0)) / F;
}

void main() {
    vec3 color = texture(colorTex, fragUV).rgb;
    color *= push.exposure;

    vec3 tonemapped;
    switch (push.tonemapMode) {
        case 0: tonemapped = acesFilm(color); break;
        case 1: tonemapped = reinhard(color); break;
        case 2: tonemapped = uncharted2(color); break;
        default: tonemapped = color;
    }

    vec3 gammaCorrected = pow(tonemapped, vec3(1.0 / push.gamma));
    outColor = vec4(gammaCorrected, 1.0);
}