#version 460
#extension GL_KHR_ray_tracing : enable

hitAttributeKHR vec2 attribs;

layout(location = 0) rayPayloadInKHR vec3 hitValue;
layout(binding = 2, set = 0) buffer VertexBuffer { vec4 vertices[]; };

struct Vertex {
    vec3 pos;
    vec3 color;
    vec2 uv;
};

Vertex unpackVertex(uint index)
{
    Vertex v;
    v.pos = vertices[3 * index + 0].xyz;
    v.color = vertices[3 * index + 1].xyz;
    v.uv = vertices[3 * index + 2].xy;
    return v;
}

void main()
{
    Vertex v0 = unpackVertex(gl_PrimitiveID * 3u + 0u);
    Vertex v1 = unpackVertex(gl_PrimitiveID * 3u + 1u);
    Vertex v2 = unpackVertex(gl_PrimitiveID * 3u + 2u);

    const vec3 barycentrics = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);

    vec3 normal = normalize(cross(v1.pos - v0.pos, v2.pos - v0.pos));

    const vec3 lightPosition = vec3(5.0, 5.0, 5.0);
    const vec3 lightColor = vec3(1.0);

    vec3 objectColor = mix(v0.color, v1.color, barycentrics.y);
    objectColor = mix(objectColor, v2.color, barycentrics.z);

    vec3 worldPos = v0.pos * barycentrics.x + v1.pos * barycentrics.y + v2.pos * barycentrics.z;

    vec3 L = normalize(lightPosition - worldPos);
    float ambient = 0.1;

    float lighting = ambient + max(dot(normal, L), 0.0);

    hitValue = lightColor * lighting * objectColor;
}