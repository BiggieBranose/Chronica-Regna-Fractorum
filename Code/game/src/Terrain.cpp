#include "../header/Terrain.hpp"

namespace game {

float terrainHeight(float x, float z) {
    float base = -0.5f;
    float h = 0.0f;

    auto smoothstep = [](float edge0, float edge1, float val) -> float {
        float t = (val - edge0) / (edge1 - edge0);
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        return t * t * (3.0f - 2.0f * t);
    };

    if (x > 1.5f && x < 6.5f && z > -2.5f && z < 2.5f) {
        float blend = smoothstep(1.5f, 2.5f, x) * smoothstep(1.5f, 2.5f, z)
                    * smoothstep(1.5f, 2.5f, 6.5f - x) * smoothstep(1.5f, 2.5f, 2.5f - z);
        h += 0.5f * blend;
    }

    if (x > -6.0f && x < -1.0f && z > 1.0f && z < 5.5f) {
        float blend = smoothstep(1.0f, 2.0f, x - (-6.0f)) * smoothstep(1.0f, 2.0f, z - 1.0f)
                    * smoothstep(1.0f, 2.0f, -1.0f - x) * smoothstep(1.0f, 2.0f, 5.5f - z);
        h += 1.0f * blend;
    }

    if (x > -3.5f && x < 1.5f && z > -6.0f && z < -2.0f) {
        float blend = smoothstep(1.0f, 2.0f, x - (-3.5f)) * smoothstep(1.0f, 2.0f, z - (-6.0f))
                    * smoothstep(1.0f, 2.0f, 1.5f - x) * smoothstep(1.0f, 2.0f, -2.0f - z);
        h += 0.3f * blend;
    }

    if (z > 3.0f) {
        float blend = smoothstep(3.0f, 4.0f, z);
        h += 0.25f * blend;
    }

    if (x < -4.5f && z < -0.5f) {
        float blend = smoothstep(-0.5f, -1.5f, z) * smoothstep(-4.5f, -5.5f, x);
        h += 0.7f * blend;
    }

    return base + h;
}

std::array<float, 3> terrainColor(float h) {
    float dh = h - (-0.5f);
    if (dh < 0.05f)  return {{0.30f, 0.52f, 0.22f}};
    if (dh < 0.15f)  return {{0.28f, 0.48f, 0.20f}};
    if (dh < 0.35f)  return {{0.45f, 0.38f, 0.25f}};
    if (dh < 0.6f)   return {{0.50f, 0.44f, 0.30f}};
    if (dh < 0.8f)   return {{0.42f, 0.38f, 0.32f}};
    return {{0.52f, 0.48f, 0.42f}};
}

}
