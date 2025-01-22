#ifndef UTIL_HLSL
#define UTIL_HLSL

#include "Common.hlsli"

float3 HitAttribute(in float3 vertexAttribute[3], in BuiltInTriangleIntersectionAttributes attr)
{
    return vertexAttribute[0] +
            attr.barycentrics.x * (vertexAttribute[1] - vertexAttribute[0]) +
            attr.barycentrics.y * (vertexAttribute[2] - vertexAttribute[0]);
}

Ray GenerateCameraRay(in uint2 index, in float3 cameraPosition, in float4x4 inverseCamProjection, in float2 jitter = float2(0.0f, 0.0f))
{
    float2 xy = index + 0.5f; // center in the middle of the pixel.
    xy += jitter;
    float2 screenPos = xy / DispatchRaysDimensions().xy * 2.0f - 1.0f;

    // Invert Y for DirectX-style coordinates.
    screenPos.y = -screenPos.y;

    // Unproject the pixel coordinate into a world positon.
    float4 world = mul(float4(screenPos, 0.0f, 1.0f), inverseCamProjection);

    Ray ray;
    ray.Origin = cameraPosition;
	// Since the camera's eye was at 0,0,0 in inverseCamProjection 
	// the world.xyz is the direction.
    ray.Direction = normalize(world.xyz);

    return ray;
}

Payload TraceRadianceRay(in Ray ray, in uint curRayRecursion, in float tMin, in float tMax)
{
    Payload payload;
    payload.Color = float3(0.0f, 0.0f, 0.0f);
    payload.RayRecursionDepth = curRayRecursion;
    payload.RayTime = 0.0f;
    
    if (curRayRecursion >= g_SceneCB.MaxRayRecursionDepth)
    {
        return payload;
    }
    
    RayDesc rayDesc;
    rayDesc.Origin = ray.Origin;
    rayDesc.Direction = ray.Direction;
    rayDesc.TMin = tMin;
    rayDesc.TMax = tMax;

    TraceRay(g_RtScene, 0, 0xFF, 0, 0, 0, rayDesc, payload);
    
    return payload;
}

HitFoundPayload TraceNormalHit(in Ray ray, in float tMin, in float tMax)
{
    HitFoundPayload payload;
    payload.HitPoint = float3(0.0f, 0.0f, 0.0f);
    payload.HitNormal = float3(0.0f, 0.0f, 0.0f);
    payload.HitTangent = float3(0.0f, 0.0f, 0.0f);
    
    RayDesc rayDesc;
    rayDesc.Origin = ray.Origin;
    rayDesc.Direction = ray.Direction;
    rayDesc.TMin = tMin;
    rayDesc.TMax = tMax;
    
    TraceRay(g_RtScene, 0, 0xFF, 1, 0, 1, rayDesc, payload);
    
    return payload;
}

#endif
