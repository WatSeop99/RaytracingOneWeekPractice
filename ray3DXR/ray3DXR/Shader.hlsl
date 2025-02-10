#ifndef HLSL
#define HLSL

#include "Common.hlsli"
#include "MaterialUtil.hlsli"
#include "RaytracingUtil.hlsli"
#include "PDFUtil.hlsli"

[shader("raygeneration")]
void RayGeneration()
{
    uint2 dtID = DispatchRaysIndex().xy;
    
    Ray ray = GenerateCameraRay(dtID, g_SceneCB.CameraPos, g_SceneCB.InverseProjection);

    uint currentRayRecursionDepth = 0;
    Payload payload = TraceRadianceRay(ray, currentRayRecursionDepth, 0.1f, 10000.0f, dtID);
    
    g_Output[dtID.xy] = float4(payload.Color, 1.0f);
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

//#include "Random.hlsl"

//struct Vertex
//{
//    float3 Position;
//    float pad1;
//    float3 Normal;
//    float pad2;
//};
//struct MeshBuffer
//{
//    float4 Albedo;
//    int MeshID;
//    int MaterialID;
//    int VerticesOffset;
//    int IndicesOffset;
//};
//struct FrameBuffer
//{
    
//};

//float Schlick(const float cosine, const float refractionIndex)
//{
//    float r0 = (1 - refractionIndex) / (1 + refractionIndex);
//    r0 *= r0;
//    return r0 + (1 - r0) * pow(1 - cosine, 5);
//}

//RaytracingAccelerationStructure Scene : register(t0, space0);
//RWTexture2D<float4> RenderTarget : register(u0);
//StructuredBuffer<int> Indices : register(t1, space0);
//StructuredBuffer<Vertex> Vertices : register(t2, space0);

//ConstantBuffer<FrameBuffer> frameBuffer : register(b0);
//ConstantBuffer<MeshBuffer> meshBuffer : register(b1);

//typedef BuiltInTriangleIntersectionAttributes MyAttributes;
//struct RayPayload
//{
//    float4 colorAndDistance;
//    float4 scatterDirection;
//    uint seed;
//};

//// Retrieve hit world position.
//float3 HitWorldPosition()
//{
//    return WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
//}

//// Retrieve attribute at a hit position interpolated from vertex attributes using the hit's barycentrics.
//float3 HitAttribute(float3 vertexAttribute[3], BuiltInTriangleIntersectionAttributes attr)
//{
//    return vertexAttribute[0] +
//        attr.barycentrics.x * (vertexAttribute[1] - vertexAttribute[0]) +
//        attr.barycentrics.y * (vertexAttribute[2] - vertexAttribute[0]);
//}

//RayPayload ScatterLambertian(in float4 albedo, in float3 worldRayDirection, in float3 normal, in float t, in uint seed)
//{
//    bool isScattered = dot(worldRayDirection, normal) < 0;
//    float4 colorAndDistance = float4(albedo.rgb, t);
//    float4 scatter = float4(normal + RandomInUnitSphere(seed), isScattered ? 1 : 0);

//    RayPayload payload = (RayPayload)0;
//    payload.colorAndDistance = colorAndDistance;
//    payload.scatterDirection = scatter;
//    payload.seed = seed;
//    return payload;
//}

//RayPayload ScatterMetal(in float4 albedo, in float3 worldRayDirection, in float3 normal, in float t, in uint seed)
//{
//    const float3 reflected = reflect(worldRayDirection, normal);
//    const bool isScattered = dot(reflected, normal) > 0;
    
//    const float4 colorAndDistance = float4(albedo.rgb, t);
//    const float4 scatter = float4(reflected + albedo.w * RandomInUnitSphere(seed), isScattered ? 1 : 0);
    
//    RayPayload payload = (RayPayload)0;
//    payload.colorAndDistance = colorAndDistance;
//    payload.scatterDirection = scatter;
//    payload.seed = seed;
//    return payload;
//}

//RayPayload ScatterDielectric(in float4 albedo, in float3 worldRayDirection, in float3 normal, in float t, in uint seed, float refractionIndex)
//{
//    //
//    const float DoN = dot(worldRayDirection, normal);
//    const float3 outwardNormal = DoN > 0 ? -normal : normal;
//    const float niOverNt = DoN > 0 ? refractionIndex : 1 / refractionIndex;
//    const float cosine = DoN > 0 ? refractionIndex * DoN : -DoN;

//    const float3 refracted = refract(worldRayDirection, outwardNormal, niOverNt);
//    const float reflectProb = select(refracted != float3(0, 0, 0), Schlick(cosine, refractionIndex), 1.0f);

//    const float4 color = float4(1.0, 1.0, 1.0, 1.0);
	
//    RayPayload payload = (RayPayload)0;
//    payload.colorAndDistance = float4(color.rgb, t);
//    payload.scatterDirection = RandomFloat(seed) < reflectProb ? float4(reflect(worldRayDirection, normal), 1) : float4(refracted, 1);
//    payload.seed = seed;
//    return payload;
//}

//// Inspired with:
//// https://github.com/GPSnoopy/RayTracingInVulkan
//[shader("raygeneration")]
//void MyRaygenShader()
//{
//    uint TotalNumberOfSamples = 4; // TODO: should be in camera
//    uint randomSeed = InitRandomSeed(InitRandomSeed(DispatchRaysIndex().x, DispatchRaysIndex().y), TotalNumberOfSamples);
//    uint pixelRandomSeed = 1; // TODO: pass with raypayload
//    // Generate a ray for a camera pixel corresponding to an index from the dispatched 2D grid.
//    uint bounces = 8;
//    float aperture = 0.1;
//    float focusDistance = 10.4;
//    float3 color = float3(0.0, 0.0, 0.0);
    
//    for (int sample = 0; sample < TotalNumberOfSamples; sample++)
//    {
//        float3 rayColor = float3(1.0, 1.0, 1.0);
       
//        const float2 pixel = float2(DispatchRaysIndex().x + RandomFloat(pixelRandomSeed), DispatchRaysIndex().y + RandomFloat(pixelRandomSeed));
//        float2 uv = (pixel / DispatchRaysDimensions().xy) * 2.0 - 1.0;
//        uv.y *= -1; // directx 
 
//        float2 offset = aperture / 2 * RandomInUnitDisk(randomSeed);
//        float4 origin = mul(float4(offset, 0, 1), frameBuffer.modelViewInverse);
//        float4 target = mul((float4(uv.x, uv.y, 1, 1)), frameBuffer.projectionToWorld);
//        float4 direction = mul(float4(normalize(target.xyz * focusDistance - float3(offset, 0)), 0), frameBuffer.modelViewInverse);
       
//        for (int i = 0; i <= bounces; ++i)
//        {
//            if (i == bounces)
//            {
//                rayColor = float3(0.0, 0.0, 0.0);
//                break;
//            }
            
//            RayDesc ray;
//            ray.Origin = origin;
//            ray.Direction = direction;
//            ray.TMin = 0.001;
//            ray.TMax = 10000.0;
        
//            RayPayload payload = (RayPayload)0;
//            payload.seed = randomSeed;

//            TraceRay(Scene, RAY_FLAG_FORCE_OPAQUE, 0xFF, 0, 0, 0, ray, payload);
            
//            const float3 hitColor = payload.colorAndDistance.rgb;
//            const float t = payload.colorAndDistance.w;
//            const bool isScattered = payload.scatterDirection.w > 0;
   
//            rayColor *= hitColor;
            
//            if (t < 0 || !isScattered)
//            {
//                break;
//            }
            
//            origin = origin + t * direction;
//            direction = float4(payload.scatterDirection.xyz, 0);
     
//        }
//        color += rayColor;
//    }
    
//    color /= TotalNumberOfSamples;
//    color = sqrt(color);

//    // Write the raytraced color to the output texture.
//    RenderTarget[DispatchRaysIndex().xy].rgb = color;
//    RenderTarget[DispatchRaysIndex().xy].a = 1.0;
//}

//[shader("closesthit")]
//void MyClosestHitShader(inout RayPayload payload, in MyAttributes attr)
//{
//    float3 hitPosition = HitWorldPosition();
//    float3 worlRayDirection = WorldRayDirection();
 
//    uint indicesPerTriangle = 3;
//    uint baseIdx = indicesPerTriangle * PrimitiveIndex();
//    uint indexWithOffset = baseIdx + meshBuffer.IndicesOffset;
    
//    int i0 = Indices[indexWithOffset + 0];
//    int i1 = Indices[indexWithOffset + 1];
//    int i2 = Indices[indexWithOffset + 2];

//    // Retrieve corresponding vertex normals for the triangle vertices.
//    float3 vertexNormals[3] =
//    {
//        Vertices[i0 + meshBuffer.VerticesOffset].Normal,
//        Vertices[i1 + meshBuffer.VerticesOffset].Normal,
//        Vertices[i2 + meshBuffer.VerticesOffset].Normal 
//    };

//    // Compute the triangle's normal.
//    // This is redundant and done for illustration purposes 
//    // as all the per-vertex normals are the same and match triangle's normal in this sample. 
//    float3 triangleNormal = HitAttribute(vertexNormals, attr);
//    float t = RayTCurrent();
   
//    int materialId = meshBuffer.MaterialID;
//    switch (materialId)
//    {
//        case 0:
//            payload = ScatterLambertian(meshBuffer.Albedo, worlRayDirection, triangleNormal, t, payload.seed);
//            break;
//        case 1:
//            payload = ScatterMetal(meshBuffer.Albedo, worlRayDirection, triangleNormal, t, payload.seed);
//            break;
//        case 2:
//            float refractionIndex = meshBuffer.Albedo.x;
//            payload = ScatterDielectric(meshBuffer.Albedo, worlRayDirection, triangleNormal, t, payload.seed, refractionIndex);
//            break;
//    }
//}

//[shader("miss")]
//void MyMissShader(inout RayPayload payload)
//{
//    float a = 0.5 * (normalize(WorldRayDirection().y + 1.0));
//    payload.colorAndDistance.rgb = (1.0 - a) * float3(1.0, 1.0, 1.0) + a * float3(0.5, 0.7, 1.0);
//    payload.colorAndDistance.w = -1;
//}

#endif
