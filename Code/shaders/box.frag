#version 460
#extension GL_KHR_ray_query : enable

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragWorldPos;

layout(binding = 0, set = 0) uniform accelerationStructureKHR topLevelAS;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 normal = normalize(cross(dFdx(fragWorldPos), dFdy(fragWorldPos)));
    vec3 lightPos = vec3(5.0, 5.0, 5.0);
    vec3 lightColor = vec3(1.0);
    
    vec3 L = normalize(lightPos - fragWorldPos);
    float ambient = 0.1;
    
    // Ray query for shadows
    rayQueryKHR rq;
    rayQueryInitializeKHR(rq, topLevelAS, gl_RayFlagsOpaqueKHR, 0xff, 0, 0, 0, fragWorldPos, 0.001, L, 100.0);
    
    bool inShadow = false;
    while (rayQueryProceedKHR(rq)) {
        if (rayQueryGetIntersectionTypeKHR(rq, true) == gl_RayQueryCommittedIntersectionKHR) {
            inShadow = true;
            break;
        }
    }
    
    float diffuse = inShadow ? 0.0 : max(dot(normal, L), 0.0);
    float lighting = ambient + diffuse;
    
    outColor = vec4(fragColor * lighting, 1.0);
}