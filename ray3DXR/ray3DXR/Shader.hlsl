#ifndef HLSL
#define HLSL

#include "CommonDefine.hlsli"
#include "Common.hlsli"
#include "MaterialUtil.hlsli"
#include "RaytracingUtil.hlsli"
#include "PDFUtil.hlsli"

//RaytracingAccelerationStructure g_RtScene : register(t0);
//RWTexture2D<float4> g_Output : register(u0);

//float3 LinearToSRGB(float3 c)
//{
//    float3 sq1 = sqrt(c);
//    float3 sq2 = sqrt(sq1);
//    float3 sq3 = sqrt(sq2);
//    float3 srgb = 0.662002687f * sq1 + 0.684122060f * sq2 - 0.323583601f * sq3 - 0.0225411470f * c;
//    return srgb;
//}

//struct RayPayload
//{
//    float3 Color;
//};

//[shader("raygeneration")]
//void RayGen()
//{
//    uint3 launchIndex = DispatchRaysIndex();
//    uint3 launchDim = DispatchRaysDimensions();

//    float2 crd = float2(launchIndex.xy);
//    float2 dims = float2(launchDim.xy);

//    float2 d = ((crd / dims) * 2.0f - 1.0f);
//    float aspectRatio = dims.x / dims.y;

//    RayDesc ray;
//    ray.Origin = float3(0, 0, -2);
//    ray.Direction = normalize(float3(d.x * aspectRatio, -d.y, 1.0f));

//    ray.TMin = 0;
//    ray.TMax = 100000;

//    RayPayload payload;
//    TraceRay(g_RtScene, 0 /*rayFlags*/, 0xFF, 0 /* ray index*/, 0, 0, ray, payload);
//    float3 col = LinearToSRGB(payload.Color);
//    g_Output[launchIndex.xy] = float4(col, 1.0f);
//}

//[shader("miss")]
//void Miss(inout RayPayload payload)
//{
//    payload.Color = float3(0.4f, 0.6f, 0.2f);
//}

//[shader("closesthit")]
//void ClosestHit(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
//{   
//    float3 barycentrics = float3(1.0 - attribs.barycentrics.x - attribs.barycentrics.y, attribs.barycentrics.x, attribs.barycentrics.y);

//    const float3 A = float3(1, 0, 0);
//    const float3 B = float3(0, 1, 0);
//    const float3 C = float3(0, 0, 1);

//    payload.Color = A * barycentrics.x + B * barycentrics.y + C * barycentrics.z;
//}


[shader("raygeneration")]
void RayGeneration()
{
    uint2 dtID = DispatchRaysIndex().xy;
    
    Ray ray = GenerateCameraRay(dtID, g_SceneCB.CameraPos, g_SceneCB.InverseProjection);

    uint currentRayRecursionDepth = 0;
    Payload payload = TraceRadianceRay(ray, currentRayRecursionDepth, 0.1f, 10000.0f, dtID);
    
    g_Output[dtID.xy] = payload.Color;
}

[shader("closesthit")]
void ClosestHitForRadianceRay(inout Payload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    uint startIndex = PrimitiveIndex() * 3;
    const uint3 INDICES = uint3(l_Indices[startIndex], l_Indices[startIndex + 1], l_Indices[startIndex + 2]);
    
    Vertex vertices[3] = { l_Vertices[INDICES.x], l_Vertices[INDICES.y], l_Vertices[INDICES.z] };
    float3 positions[3] = { vertices[0].Position, vertices[1].Position, vertices[2].Position };
    float3 normals[3] = { vertices[0].Normal, vertices[1].Normal, vertices[2].Normal };
    float3 tangents[3] = { vertices[0].Tangent, vertices[1].Tangent, vertices[2].Tangent };
    
    float3 hitPosition = HitAttribute(positions, attr);
    float3 hitNormal = HitAttribute(normals, attr);
    float3 hitTangent = HitAttribute(tangents, attr);
    uint hitKind = HitKind();
    
    payload.HitPoint = hitPosition;
    payload.HitNormal = hitNormal;
    payload.HitTangent = hitTangent;
    payload.RayTime = RayTCurrent();
    
    // get material
    uint materialID = l_MaterialCB.MaterialID;
    Material material = g_Materials[materialID];
    
    // emmitted
    float3 emissionColor = CalculateEmissionColor(material);
    
    ScatterPayload scatterPayload;
    Ray ray;
    ray.Origin = WorldRayOrigin();
    ray.Direction = WorldRayDirection();
    // scatter check
    if (!IsScatter(material, ray, payload, hitKind, scatterPayload))
    {
        payload.Color = emissionColor;
        return;
    }
    
    // if pdf skip, return atta * RadianceCalc
    if (scatterPayload.bSkipPDF)
    {
        float3 skipScatterColor = TraceRadianceRay(scatterPayload.SkipPDFRay, payload.RayRecursionDepth + 1, 0.1f, 10000.0f, payload.DTID).Color;
        payload.Color = scatterPayload.Attenuation * skipScatterColor;
        return;
    }
    
    // pdf
    float3 lightPDFOrigin = hitPosition;
    
    // gen scatter ray
    // calc pdf val
    // calc scatter pdf val
    Ray scatteredRay;
    scatteredRay.Origin = hitPosition;
    scatteredRay.Direction = GetMixedPDFGeneration(lightPDFOrigin, hitNormal, hitTangent, payload.DTID);
    
    float pdfValue = GetMixedPDFValue(lightPDFOrigin, scatterPayload.PDF, scatteredRay.Direction);
    float scatteringPDF = GetScatterringPDF(material, ray, payload, scatteredRay);
    
    // trace ray
    float3 sampleColor = TraceRadianceRay(scatteredRay, payload.RayRecursionDepth + 1, 0.1f, 10000.0f, payload.DTID).Color;
    
    // calc scatter color
    float3 scatterColor = (scatterPayload.Attenuation * scatteringPDF * sampleColor) / pdfValue;
    
    // final color(emission + scatter)
    payload.Color = emissionColor + scatterColor;
}

[shader("closesthit")]
void ClosestHitForPDFRay(inout HitFoundPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    uint startIndex = PrimitiveIndex() * 3;
    const uint3 INDICES = uint3(l_Indices[startIndex], l_Indices[startIndex + 1], l_Indices[startIndex + 2]);
    
    Vertex vertices[3] = { l_Vertices[INDICES.x], l_Vertices[INDICES.y], l_Vertices[INDICES.z] };
    float3 positions[3] = { vertices[0].Position, vertices[1].Position, vertices[2].Position };
    float3 normals[3] = { vertices[0].Normal, vertices[1].Normal, vertices[2].Normal };
    float3 tangents[3] = { vertices[0].Tangent, vertices[1].Tangent, vertices[2].Tangent };
    
    float3 hitPosition = HitAttribute(positions, attr);
    float3 hitNormal = HitAttribute(normals, attr);
    float3 hitTangent = HitAttribute(tangents, attr);
    uint hitKind = HitKind();
    
    payload.HitPoint = hitPosition;
    payload.HitNormal = hitNormal;
    payload.HitTangent = hitTangent;
    payload.RayTime = RayTCurrent();
}

[shader("miss")]
void MissRadiance(inout Payload payload)
{
    payload.Color = float3(0.0f, 0.0f, 0.0f);
    payload.RayTime = 0.0f;
}

[shader("miss")]
void MissPDF(inout HitFoundPayload payload)
{
    payload.RayTime = 0.0f;
}

#endif
