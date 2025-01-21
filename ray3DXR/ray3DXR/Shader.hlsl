#ifndef HLSL
#define HLSL

#include "Common.hlsli"
#include "MaterialUtil.hlsl"

RaytracingAccelerationStructure g_RtScene : register(t0);
RWTexture2D<float4> g_Output : register(u0);

float3 LinearToSRGB(float3 c)
{
    float3 sq1 = sqrt(c);
    float3 sq2 = sqrt(sq1);
    float3 sq3 = sqrt(sq2);
    float3 srgb = 0.662002687f * sq1 + 0.684122060f * sq2 - 0.323583601f * sq3 - 0.0225411470f * c;
    return srgb;
}

struct RayPayload
{
    float3 Color;
};

[shader("raygeneration")]
void RayGen()
{
    uint3 launchIndex = DispatchRaysIndex();
    uint3 launchDim = DispatchRaysDimensions();

    float2 crd = float2(launchIndex.xy);
    float2 dims = float2(launchDim.xy);

    float2 d = ((crd / dims) * 2.0f - 1.0f);
    float aspectRatio = dims.x / dims.y;

    RayDesc ray;
    ray.Origin = float3(0, 0, -2);
    ray.Direction = normalize(float3(d.x * aspectRatio, -d.y, 1.0f));

    ray.TMin = 0;
    ray.TMax = 100000;

    RayPayload payload;
    TraceRay(g_RtScene, 0 /*rayFlags*/, 0xFF, 0 /* ray index*/, 0, 0, ray, payload);
    float3 col = LinearToSRGB(payload.Color);
    g_Output[launchIndex.xy] = float4(col, 1.0f);
}

[shader("miss")]
void Miss(inout RayPayload payload)
{
    payload.Color = float3(0.4f, 0.6f, 0.2f);
}

[shader("closesthit")]
void ClosestHit(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{   
    float3 barycentrics = float3(1.0 - attribs.barycentrics.x - attribs.barycentrics.y, attribs.barycentrics.x, attribs.barycentrics.y);

    const float3 A = float3(1, 0, 0);
    const float3 B = float3(0, 1, 0);
    const float3 C = float3(0, 0, 1);

    payload.Color = A * barycentrics.x + B * barycentrics.y + C * barycentrics.z;
}

StructuredBuffer<Material> g_Materials : register(t3);

ConstantBuffer<MaterialCB> l_MaterialCB : register(c0);

StructuredBuffer<uint> l_Indices : register(t1);
StructuredBuffer<Vertex> l_Vertices : register(t2);

uint3 Load3x32BitIndices(uint offsetBytes)
{
    uint3 indices;
    
    // ByteAddressBuffer는 4byte 단위로 읽어올 수 있다.
    // 어차피 인덱스로 32비트 정수형 데이터를 쓰기 때문에
    // 인자로 들어온 오프셋 바이트 포함 총 3개의 인덱스를 읽어오면 된다.
    indices = g_Indices.Load3(offsetBytes);
    
    return indices;
}

Payload TraceRadianceRay(in Ray ray, in uint curRayRecursion, float tMin, float tMax)
{
    Payload payload;
    payload.Color = float3(0.0f, 0.0f, 0.0f);
    payload.RayRecursionDepth = curRayRecursion;
    payload.tHit = 0.0f;
    
    if (curRayRecursion >= g_SceneCB.MaxRayRecursionDepth)
    {
        payload.Color = float3(0.0f, 0.0f, 0.0f);
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

[shader("raygeneration")]
void RayGeneration()
{
    uint2 dtID = DispatchRaysIndex().xy;

    
    
    g_Output[dtID.xy];
}

[shader("closesthit")]
void ClosestHitForRadianceRay(inout Payload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    uint startIndex = PrimitiveIndex() * 3;
    const uint3 INDICES = uint3(l_Indices[startIndex], l_Indices[startIndex + 1], l_Indices[startIndex + 2]);
    
    Vertex vertices[3] = { l_Vertices[INDICES.x], l_Vertices[INDICES.y], l_Vertices[INDICES.z] };
    // calcuate coord
    
    float3 hitPosition = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    uint hitKind = HitKind();
    payload.HitPoint = hitPosition;
    
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
    if (!IsScatter(material, ray, payload, normal, tangent, hitKind, scatterPayload))
    {
        payload.Color = emissionColor;
        return;
    }
    
    // if pdf skip, return atta * RadianceCalc
    if (scatterPayload.bSkipPDF)
    {
        float3 skipScatterColor = TraceRadianceRay(scatterPayload.SkipPDFRay, payload.RayRecursionDepth + 1, 0.1f, 10000.0f).Color;
        payload.Color = scatterPayload.Attenuation * skipScatterColor;
        return;
    }
    
    // pdf
    float lightPDF;
    
    // gen scatter ray
    // calc pdf val
    // calc scatter pdf val
    Ray scatteredRay;
    scatteredRay.Origin = hitPosition;
    scatteredRay.Direction;
    
    
    
    // trace ray
    
    // calc scatter color
    
    // final color(emission + scatter)
    payload.Color = emissionColor + scatterColor;
}

[shader("closesthit")]
void ClosestHitForShadowRay(inout Payload record, in BuiltInTriangleIntersectionAttributes attr)
{
    record.tHit = RayTCurrent();
}

[shader("miss")]
void MissRadiance(inout Payload record)
{
    record.Color = float3(0.0f, 0.0f, 0.0f);
}

[shader("miss")]
void MissShadow(inout Payload record)
{
    record.tHit = 0.0f;
}

#endif
