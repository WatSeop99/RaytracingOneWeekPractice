#ifndef UTIL_HLSL
#define UTIL_HLSL

#include "Common.hlsli"

static const float PI = 3.1415926535f;
static const float PHI = 1.61803398874989484820459f; // ¥Õ = Golden Ratio 

float GoldNoise(in float2 xy, in float seed)
{
    return frac(tan(distance(xy * PHI, xy) * seed) * xy.x);
}

float3 GetRandomUnitVector(in uint2 dtID)
{
    return float3(GoldNoise(dtID, g_SceneCB.SceneTime + 0.1f), GoldNoise(dtID, g_SceneCB.SceneTime + 0.2f), GoldNoise(dtID, g_SceneCB.SceneTime + 0.3f));
}

float GetRandomFloatValue(in uint2 dtID)
{
    return GoldNoise(dtID, g_SceneCB.SceneTime + 0.1f);
}

int GetRandomIntValue(in uint2 dtID)
{
    return (int) GoldNoise(dtID, g_SceneCB.SceneTime + 0.1f);
}

float3 GetRandomVectorInSphere(in float radius, in float distanceSquared, in uint2 dtID)
{
    float r1 = GetRandomFloatValue(dtID);
    float r2 = GetRandomFloatValue(dtID);
    float z = 1.0f + r2 * (sqrt(1.0f - radius * radius / distanceSquared) - 1.0f);

    float phi = 2.0f * PI * r1;
    float x = cos(phi) * sqrt(1.0f - z * z);
    float y = sin(phi) * sqrt(1.0f - z * z);

    return float3(x, y, z);
}

float3 Transform(in float3 vec, in ONB transformer)
{
    return (vec.x * transformer.Axis[0]) + (vec.y * transformer.Axis[1]) + (vec.z * transformer.Axis[2]);
}

#endif
