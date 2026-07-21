#version 450

layout(binding = 0, set = 0) uniform sampler2D colorTex;
layout(binding = 1, set = 0) uniform sampler2D bloomTex;

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PostPush {
    float exposure;
    float gamma;
    float bloomStrength;
    float bloomThreshold;
    float vignetteStrength;
    float filmGrain;
    float time;
    float padding[2];
} push;

vec3 tonemap(vec3 color) {
    return color / (color + vec3(1.0));
}

vec3 bloom(vec2 uv) {
    vec3 bloomColor = texture(bloomTex, uv).rgb;
    return bloomColor * push.bloomStrength;
}

vec3 vignette(vec2 uv) {
    float dist = length(uv - 0.5) * 1.414;
    float vig = 1.0 - smoothstep(0.4, 0.9, dist) * push.vignetteStrength;
    return vec3(vig);
}

float filmGrain(float time, vec2 uv) {
    float grain = fract(sin(dot(uv, vec2(12.9898, 78.233)) + push.time * 10.0) * 43758.5453);
    return grain * 2.0 - 1.0;
}

void main() {
    vec3 color = texture(colorTex, fragUV).rgb;
    vec3 bloomColor = bloom(fragUV);
    color += bloomColor;

    // Tonemap
    color = tonemap(color * push.exposure);

    // Vignette
    color *= vignette(fragUV);

    // Film grain
    color += vec3(filmGrain(push.time, fragUV)) * push.filmGrain * 0.02;

    // Gamma correction
    color = pow(max(color, vec3(0.0)), vec3(1.0 / push.gamma));

    outColor = vec4(color, 1.0);
}