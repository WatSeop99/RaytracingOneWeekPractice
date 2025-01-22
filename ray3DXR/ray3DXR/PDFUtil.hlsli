#ifndef PDF_HLSL
#define PDF_HLSL

#include "Common.hlsli"
#include "RaytracingUtil.hlsli"

#define PI 3.1415926535f

ONB InitializeONB(in float3 n)
{
    ONB retONB;
    
    retONB.Axis[2] = normalize(n);
    
    float3 a = (retONB.Axis[2].x > 0.9f ? float3(0.0f, 1.0f, 0.0f) : float3(1.0f, 0.0f, 0.0f));
    retONB.Axis[1] = normalize(cross(retONB.Axis[2], a));
    
    retONB.Axis[0] = cross(retONB.Axis[2], retONB.Axis[1]);
    
    return retONB;
}

float GetMixedPDFValue(in float3 P0Origin, in ONB P1ONB, in float3 direction)
{
    // p0->hittable pdf
    // p1->cosine pdf
    
    // p0
    Ray ray;
    ray.Origin = P0Origin;
    ray.Direction = direction;
    
    HitFoundPayload payload = TraceNormalHit(ray, 0.1f, 10000.0f);
    if (payload.RayTime == 0.0f)
    {
        return 0.0f;
    }
    
    float directionLength = length(direction);
    float distanceSquared = payload.RayTime * payload.RayTime * directionLength;
    float cosine = dot(direction, payload.HitNormal) / directionLength;
    float p0Value = distanceSquared / (cosine * g_SceneCB.LightArea);
   
    // p1
    float consineTheta = dot(normalize(direction), P1ONB.Axis[2]);
    float p1Value = max(0.0f, consineTheta / PI);
    
    return (0.5f * (p0Value + p1Value));
}

float GetScatterringPDF(in Material mat, in Ray ray, in Payload payload, in Ray scatterdRay)
{
    float cosTheta = dot(payload.HitNormal, normalize(scatterdRay.Direction));
    return (cosTheta < 0.0f ? 0.0f : cosTheta / PI);
}

#endif
