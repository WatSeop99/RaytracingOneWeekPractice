#ifndef RAYTRACING_COMMON_HLSL
#define RAYTRACING_COMMON_HLSL


#include "Raytracing_typedef.hlsl"

SamplerState samplerWrap : register(s0);
SamplerState samplerClamp : register(s1);
SamplerState samplerPoint : register(s2);
SamplerState samplerMirror : register(s3);

// Global Root Parameter
RWTexture2D<float4> g_OutputDiffuse : register(u0);
RWTexture2D<float4> g_OutputDepth : register(u1);
TextureCube		g_texSky : register(t10);
RaytracingAccelerationStructure Scene : register(t0, space0);

// Local Root Parameter
ConstantBuffer<CONSTANT_BUFFER_RT_TRIGROUP> l_rayGeomCB : register(b0, space1);
StructuredBuffer<BasicVertex> l_Vertices : register(t0, space1); // for triangle
ByteAddressBuffer l_Indices : register(t1, space1);
Texture2D<float4> l_texDiffuse : register(t2, space1);
Texture2D<float4> l_texNormal : register(t3, space1);

cbuffer CONSTANT_BUFFER_RAY_TRACING : register(b0, space0)
{
    matrix g_ViewProj;
    matrix g_ViewInv;
    DECOMP_PROJ g_DecompProj;
    float4 g_CameraPosition;
    float g_Near;
    float g_Far;
    uint g_MaxRadianceRayRecursionDepth;
    uint g_MaxShadowRayRecursionDepth;
    uint g_LightCount;
    uint Reserved0;
    uint Reserved1;
    uint Reserved2;
	RT_LIGHT g_LightList[MAX_RT_LIGHT_COUNT];
};

float4 HitAttribute(float4 vertexAttribute[3], BuiltInTriangleIntersectionAttributes attr)
{
    return vertexAttribute[0] +
		attr.barycentrics.x * (vertexAttribute[1] - vertexAttribute[0]) +
		attr.barycentrics.y * (vertexAttribute[2] - vertexAttribute[0]);
}
float3 HitAttribute(float3 vertexAttribute[3], BuiltInTriangleIntersectionAttributes attr)
{
    return vertexAttribute[0] +
		attr.barycentrics.x * (vertexAttribute[1] - vertexAttribute[0]) +
		attr.barycentrics.y * (vertexAttribute[2] - vertexAttribute[0]);
}
float2 HitAttribute(float2 vertexAttribute[3], BuiltInTriangleIntersectionAttributes attr)
{
    return vertexAttribute[0] +
        attr.barycentrics.x * (vertexAttribute[1] - vertexAttribute[0]) +
        attr.barycentrics.y * (vertexAttribute[2] - vertexAttribute[0]);
}

// Retrieve hit world position.
float3 HitWorldPosition()
{
    return WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
}
// Load three 16 bit indices.
static uint3 Load3x16BitIndices(uint offsetBytes)
{
    uint3 indices;

	// ByteAdressBuffer loads must be aligned at a 4 byte boundary.
	// Since we need to read three 16 bit indices: { 0, 1, 2 } 
	// aligned at a 4 byte boundary as: { 0 1 } { 2 0 } { 1 2 } { 0 1 } ...
	// we will load 8 bytes (~ 4 indices { a b | c d }) to handle two possible index triplet layouts,
	// based on first index's offsetBytes being aligned at the 4 byte boundary or not:
	//  Aligned:     { 0 1 | 2 - }
	//  Not aligned: { - 0 | 1 2 }
    const uint dwordAlignedOffset = offsetBytes & ~3;
    const uint2 four16BitIndices = l_Indices.Load2(dwordAlignedOffset);

	// Aligned: { 0 1 | 2 - } => retrieve first three 16bit indices
    if (dwordAlignedOffset == offsetBytes)
    {
        indices.x = four16BitIndices.x & 0xffff;
        indices.y = (four16BitIndices.x >> 16) & 0xffff;
        indices.z = four16BitIndices.y & 0xffff;
    }
    else // Notaligned: { - 0 | 1 2 } => retrieve last three 16bit indices
    {
        indices.x = (four16BitIndices.x >> 16) & 0xffff;
        indices.y = four16BitIndices.y & 0xffff;
        indices.z = (four16BitIndices.y >> 16) & 0xffff;
    }

    return indices;
}

#endif // RAYTRACING_COMMON_HLSL
