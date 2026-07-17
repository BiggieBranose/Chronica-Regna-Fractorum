#version 460
#extension GL_EXT_ray_query : enable

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragWorldPos;

layout(binding = 1, set = 0) uniform accelerationStructureEXT topLevelAS;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 normal = normalize(cross(dFdx(fragWorldPos), dFdy(fragWorldPos)));
    vec3 sunDir = normalize(vec3(2.0, 4.0, 1.0));
    float NdotL = max(dot(normal, sunDir), 0.0);

    rayQueryEXT rq;
    rayQueryInitializeEXT(rq, topLevelAS, 0x10u, 0xFFu, 0u, 0u, 0u, fragWorldPos + normal * 0.05, 0.001, sunDir, 50.0);

    float shadow = 1.0;
    while (rayQueryProceedEXT(rq)) {
        if (rayQueryGetIntersectionTypeEXT(rq, true) == 0x700003u) {
            float tHit = rayQueryGetIntersectionTEXT(rq, true);
            shadow = mix(1.0, 0.5, smoothstep(2.0, 8.0, tHit));
            break;
        }
    }

    float ambient = 0.4;
    float lighting = ambient + (1.0 - ambient) * NdotL * shadow;

    outColor = vec4(fragColor * lighting, 1.0);
}