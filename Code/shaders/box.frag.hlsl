struct PSInput {
    float3 color : COLOR0;
    float3 worldPos : TEXCOORD1;
};

[[vk::binding(1, 0)]] RaytracingAccelerationStructure topLevelAS : register(t0);

float4 main(PSInput input) : SV_Target {
    float3 normal = normalize(cross(ddx(input.worldPos), ddy(input.worldPos)));
    float3 sunDir = normalize(float3(2.0, 4.0, 1.0));
    float NdotL = max(dot(normal, sunDir), 0.0);

    RayQuery<RAY_FLAG_FORCE_OPAQUE> rq;

    RayDesc ray;
    ray.Origin = input.worldPos + normal * 0.05;
    ray.TMin = 0.001;
    ray.Direction = sunDir;
    ray.TMax = 50.0;
    rq.TraceRayInline(topLevelAS, RAY_FLAG_FORCE_OPAQUE, 0xFF, ray);

    float shadow = 1.0;
    while (rq.Proceed()) {
    }

    if (rq.CommittedStatus() == COMMITTED_TRIANGLE_HIT) {
        float tHit = rq.CommittedRayT();
        shadow = lerp(1.0, 0.5, smoothstep(2.0, 8.0, tHit));
    }

    float ambient = 0.4;
    float lighting = ambient + (1.0 - ambient) * NdotL * shadow;

    return float4(input.color * lighting, 1.0);
}