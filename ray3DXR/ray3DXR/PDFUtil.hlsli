#ifndef PDF_HLSL
#define PDF_HLSL

#include "Common.hlsli"
#include "Util.hlsli"
#include "RaytracingUtil.hlsli"

ONB InitializeONB(in float3 n)
{
    ONB retONB;
    
    retONB.Axis[2] = normalize(n);
    
    float3 a = (retONB.Axis[2].x > 0.9f ? float3(0.0f, 1.0f, 0.0f) : float3(1.0f, 0.0f, 0.0f));
    retONB.Axis[1] = normalize(cross(retONB.Axis[2], a));
    
    retONB.Axis[0] = cross(retONB.Axis[2], retONB.Axis[1]);
    
    return retONB;
}

float GetLightSourcesPDFValue(in float3 P0Origin, in float3 direction)
{
    float sum = 0.0f;
    float weight = 1.0f / (float) g_SceneCB.LightCount;
    
    Ray ray;
    ray.Origin = P0Origin;
    ray.Direction = direction;
    
    [roll]
    for (uint i = 0; i < g_SceneCB.LightCount; ++i)
    {
        HitFoundPayload payload = TracePDFRay(ray, 0.1f, 10000.0f);
        if(payload.RayTime <= 0.0f)
        {
            continue;
        }
        
        if (g_LightSources[i].LightGeomType == LIGHT_GEOM_TYPE_QUAD)
        {
            float directionLength = length(direction);
            float distanceSquared = payload.RayTime * payload.RayTime * directionLength;
            float cosine = dot(direction, payload.HitNormal) / directionLength;
            sum += distanceSquared / (cosine * g_LightSources[i].Width * g_LightSources[i].Height);
        }
        else
        {
            float distSquared = pow(length(g_LightSources[i].Center - P0Origin), 2.0f);
            float cosThetaMax = sqrt(1.0f - g_LightSources[i].Radius * g_LightSources[i].Radius / distSquared);
            float solidAngle = 2.0f * PI * (1.0f - cosThetaMax);

            sum += 1.0f / solidAngle;
        }
    }

    return sum;
}

float GetMixedPDFValue(in float3 P0Origin, in ONB P1ONB, in float3 direction)
{
    // p0->hittable_list pdf
    // p1->cosine pdf
    
    // p0
    float p0Value = GetLightSourcesPDFValue(P0Origin, direction);
   
    // p1
    float consineTheta = dot(normalize(direction), P1ONB.Axis[2]);
    float p1Value = max(0.0f, consineTheta / PI);
    
    return (0.5f * (p0Value + p1Value));
}

float3 GetMixedPDFGeneration(in float3 origin, in float3 normal, in float3 tangent, in uint2 dtID)
{
    int randomIndex = GetRandomIntValue(dtID) % g_SceneCB.LightCount;
    float3 ret = float3(0.0f, 0.0f, 0.0f);
    
    if (g_LightSources[randomIndex].LightGeomType == LIGHT_GEOM_TYPE_QUAD)
    {
        normal = normalize(normal);
        tangent = normalize(tangent);
        
        float3 biTangent = normalize(cross(normal, tangent));
        float3 p = g_LightSources[randomIndex].Center + (GetRandomFloatValue(dtID) * tangent) + (GetRandomFloatValue(dtID) * biTangent);
        ret =  p - origin;
    }
    else
    {
        float3 direction = g_LightSources[randomIndex].Center - origin;
        float distanceSquared = length(direction) * length(direction);
        ONB uvw = InitializeONB(direction);
        ret = Transform(GetRandomVectorInSphere(g_LightSources[randomIndex].Radius, distanceSquared, dtID), uvw);
    }

    return ret;
}

float GetScatterringPDF(in Material mat, in Ray ray, in Payload payload, in Ray scatterdRay)
{
    float cosTheta = dot(payload.HitNormal, normalize(scatterdRay.Direction));
    return (cosTheta < 0.0f ? 0.0f : cosTheta / PI);
}

#endif
