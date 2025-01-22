#ifndef UTIL_HLSL
#define UTIL_HLSL

#include "Common.hlsli"

const float PHI = 1.61803398874989484820459f; // ¥Õ = Golden Ratio 

float GoldNoise(in float2 xy, in float seed)
{
    return frac(tan(distance(xy * PHI, xy) * seed) * xy.x);
}

float3 GetRandomUnitVector()
{
    return float3(GoldNoise(, g_SceneCB.SceneTime + 0.1f), GoldNoise(, g_SceneCB.SceneTime + 0.2f), GoldNoise(, g_SceneCB.SceneTime + 0.3f));
}

float GetRandomFloatValue()
{
    return GoldNoise(, g_SceneCB.SceneTime + 0.1f);
}

#endif
