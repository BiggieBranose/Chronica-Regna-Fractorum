#version 460
#extension GL_KHR_ray_tracing : enable

layout(location = 0) rayPayloadInKHR vec3 hitValue;

void main()
{
    hitValue = vec3(0.5, 0.7, 1.0); // Sky blue
}