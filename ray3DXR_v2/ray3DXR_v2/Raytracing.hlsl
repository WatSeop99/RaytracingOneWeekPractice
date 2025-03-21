#ifndef RAYTRACING_HLSL
#define RAYTRACING_HLSL

#define HLSL
#include "RaytracingHLSLCompat.h"
#include "Random.hlsl"

// Global resources.

ConstantBuffer<FrameBuffer> g_FrameBuffer : register(b0);

RaytracingAccelerationStructure g_Scene : register(t0, space0);
StructuredBuffer<int> g_Indices : register(t1, space0);
StructuredBuffer<Vertex> g_Vertices : register(t2, space0);

RWTexture2D<float4> g_RenderTarget : register(u0);


// Local resources.

ConstantBuffer<MeshBuffer> l_MeshBuffer : register(b1);

typedef BuiltInTriangleIntersectionAttributes MyAttributes;
struct RayPayload
{
    float4 ColorAndDistance;
    float4 ScatterDirection;
    uint Seed;
};

float Schlick(in const float COSINE, in const float REFRACTIION_INDEX)
{
    float r0 = (1.0f - REFRACTIION_INDEX) / (1.0f + REFRACTIION_INDEX);
    r0 *= r0;
    return r0 + (1.0f - r0) * pow(1.0f - COSINE, 5.0f);
}

// Retrieve hit world position.
float3 HitWorldPosition()
{
    return WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
}

// Retrieve attribute at a hit position interpolated from vertex attributes using the hit's barycentrics.
float3 HitAttribute(float3 vertexAttribute[3], BuiltInTriangleIntersectionAttributes attr)
{
    return vertexAttribute[0] +
        attr.barycentrics.x * (vertexAttribute[1] - vertexAttribute[0]) +
        attr.barycentrics.y * (vertexAttribute[2] - vertexAttribute[0]);
}

RayPayload ScatterLambertian(in float4 albedo, in float3 worldRayDirection, in float3 normal, in float t, in uint seed)
{
    bool isScattered = (dot(worldRayDirection, normal) < 0.0f);
    float4 colorAndDistance = float4(albedo.rgb, t);
    float4 scatter = float4(normal + RandomInUnitSphere(seed), isScattered ? 1.0f : 0.0f);

    RayPayload payload = (RayPayload) 0;
    payload.ColorAndDistance = colorAndDistance;
    payload.ScatterDirection = scatter;
    payload.Seed = seed;
    return payload;
}

RayPayload ScatterMetal(in float4 albedo, in float3 worldRayDirection, in float3 normal, in float t, in uint seed)
{
    const float3 REFLECTED = reflect(worldRayDirection, normal);
    const bool bIS_SCATTRED = (dot(REFLECTED, normal) > 0.0f);
    
    const float4 COLOR_AND_DISTANCE = float4(albedo.rgb, t);
    const float4 SCATTER = float4(REFLECTED + albedo.w * RandomInUnitSphere(seed), bIS_SCATTRED ? 1.0f : 0.0f);
    
    RayPayload payload = (RayPayload) 0;
    payload.ColorAndDistance = COLOR_AND_DISTANCE;
    payload.ScatterDirection = SCATTER;
    payload.Seed = seed;
    return payload;
}

RayPayload ScatterDielectric(in float4 albedo, in float3 worldRayDirection, in float3 normal, in float t, in uint seed, in float refractionIndex)
{
    const float DoN = dot(worldRayDirection, normal);
    const float3 OUTWARD_NORMAL = (DoN > 0.0f ? -normal : normal);
    const float NI_OVER_NT = (DoN > 0.0f ? refractionIndex : 1.0f / refractionIndex);
    const float COSINE = (DoN > 0.0f ? refractionIndex * DoN : -DoN);

    const float3 REFRACTED = refract(worldRayDirection, OUTWARD_NORMAL, NI_OVER_NT);
    const float REFLECT_PROB = select(REFRACTED != float3(0.0f, 0.0f, 0.0f), Schlick(COSINE, refractionIndex), 1.0f);

    const float4 COLOR = float4(1.0f, 1.0f, 1.0f, 1.0f);
	
    RayPayload payload = (RayPayload) 0;
    payload.ColorAndDistance = float4(COLOR.rgb, t);
    payload.ScatterDirection = (RandomFloat(seed) < REFLECT_PROB ? float4(reflect(worldRayDirection, normal), 1.0f) : float4(REFRACTED, 1.0f));
    payload.Seed = seed;
    return payload;
}

// Inspired with:
// https://github.com/GPSnoopy/RayTracingInVulkan
[shader("raygeneration")]
void MyRaygenShader()
{
    uint TotalNumberOfSamples = 4; // TODO: should be in camera
    uint randomSeed = InitRandomSeed(InitRandomSeed(DispatchRaysIndex().x, DispatchRaysIndex().y), TotalNumberOfSamples);
    uint pixelRandomSeed = 1; // TODO: pass with raypayload
    // Generate a ray for a camera pixel corresponding to an index from the dispatched 2D grid.
    uint bounces = 8;
    float aperture = 0.1f;
    float focusDistance = 10.4f;
    float3 color = float3(0.0f, 0.0f, 0.0f);
    uint3 rayIndex = DispatchRaysIndex();
    
    for (int sample = 0; sample < TotalNumberOfSamples; sample++)
    {
        float3 rayColor = float3(1.0f, 1.0f, 1.0f);
       
        const float2 PIXEL = float2(DispatchRaysIndex().x + RandomFloat(pixelRandomSeed), DispatchRaysIndex().y + RandomFloat(pixelRandomSeed));
        float2 uv = (PIXEL / DispatchRaysDimensions().xy) * 2.0f - 1.0f;
        uv.y *= -1.0f; // directx 
 
        float2 offset = aperture / 2.0f * RandomInUnitDisk(randomSeed);
        float4 origin = mul(float4(offset, 0.0f, 1.0f), g_FrameBuffer.ModelViewInverse);
        float4 target = mul((float4(uv.x, uv.y, 1.0f, 1.0f)), g_FrameBuffer.ProjectionToWorld);
        float4 direction = mul(float4(normalize(target.xyz * focusDistance - float3(offset, 0.0f)), 0.0f), g_FrameBuffer.ModelViewInverse);
       
        for (int i = 0; i <= bounces; ++i)
        {
            if (i == bounces)
            {
                rayColor = float3(0.0f, 0.0f, 0.0f);
                break;
            }
            
            RayDesc ray;
            ray.Origin = origin.xyz;
            ray.Direction = direction.xyz;
            ray.TMin = 0.001f;
            ray.TMax = 10000.0f;
        
            RayPayload payload = (RayPayload) 0;
            payload.Seed = randomSeed;

            TraceRay(g_Scene, RAY_FLAG_FORCE_OPAQUE, 0xFF, 0, 0, 0, ray, payload);
            
            const float3 HIT_COLOR = payload.ColorAndDistance.rgb;
            const float T = payload.ColorAndDistance.w;
            const bool bIS_SCATTERED = (payload.ScatterDirection.w > 0.0f);
   
            rayColor *= HIT_COLOR;
            
            if (T < 0.0f || !bIS_SCATTERED)
            {
                break;
            }
            
            origin = origin + T * direction;
            direction = float4(payload.ScatterDirection.xyz, 0.0f);
     
        }
        color += rayColor;
    }
    
    color /= TotalNumberOfSamples;
    color = sqrt(color);

    // Write the raytraced color to the output texture.
    g_RenderTarget[rayIndex.xy].rgb = color;
    g_RenderTarget[rayIndex.xy].a = 1.0f;
}

[shader("closesthit")]
void MyClosestHitShader(inout RayPayload payload, in MyAttributes attr)
{
    float3 hitPosition = HitWorldPosition();
    float3 worlRayDirection = WorldRayDirection();
 
    uint indicesPerTriangle = 3;
    uint baseIdx = indicesPerTriangle * PrimitiveIndex();
    uint indexWithOffset = baseIdx + l_MeshBuffer.IndicesOffset;
    
    int i0 = g_Indices[indexWithOffset + 0];
    int i1 = g_Indices[indexWithOffset + 1];
    int i2 = g_Indices[indexWithOffset + 2];

    // Retrieve corresponding vertex normals for the triangle vertices.
    float3 vertexNormals[3] =
    {
        g_Vertices[i0 + l_MeshBuffer.VerticesOffset].Normal,
        g_Vertices[i1 + l_MeshBuffer.VerticesOffset].Normal,
        g_Vertices[i2 + l_MeshBuffer.VerticesOffset].Normal 
    };

    // Compute the triangle's normal.
    // This is redundant and done for illustration purposes 
    // as all the per-vertex normals are the same and match triangle's normal in this sample. 
    float3 triangleNormal = HitAttribute(vertexNormals, attr);
    float t = RayTCurrent();
   
    int materialId = l_MeshBuffer.MaterialID;
    switch (materialId)
    {
        case 0:
            payload = ScatterLambertian(l_MeshBuffer.Albedo, worlRayDirection, triangleNormal, t, payload.Seed);
            break;
        
        case 1:
            payload = ScatterMetal(l_MeshBuffer.Albedo, worlRayDirection, triangleNormal, t, payload.Seed);
            break;
        
        case 2:
            float refractionIndex = l_MeshBuffer.Albedo.x;
            payload = ScatterDielectric(l_MeshBuffer.Albedo, worlRayDirection, triangleNormal, t, payload.Seed, refractionIndex);
            break;
    }
}

[shader("miss")]
void MyMissShader(inout RayPayload payload)
{
    float a = 0.5f * (normalize(WorldRayDirection().y + 1.0f));
    payload.ColorAndDistance.rgb = (1.0f - a) * float3(1.0f, 1.0f, 1.0f) + a * float3(0.5f, 0.7f, 1.0f);
    payload.ColorAndDistance.w = -1.0f;
}

#endif // RAYTRACING_HLSL