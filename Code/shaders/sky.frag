#version 450

layout(location = 0) in vec3 fragDir;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 dir = normalize(fragDir);

    vec3 groundColor  = vec3(0.15, 0.15, 0.2);
    vec3 horizonColor = vec3(0.75, 0.8, 0.9);
    vec3 skyColor     = vec3(0.3, 0.5, 0.95);

    float h = smoothstep(-0.2, 0.6, dir.y);
    float g = smoothstep(-1.0, -0.2, dir.y);
    vec3 color = mix(mix(groundColor, horizonColor, g), skyColor, h);

    outColor = vec4(color, 1.0);
}
