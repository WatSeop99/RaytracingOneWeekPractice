#ifndef PARAM_HLSL
#define PARAM_HLSL

#include "CommonDefine.hlsli"

///////////////////////// Global resource ///////////////////////// 

ConstantBuffer<Scene> g_SceneCB : register(c0);

RaytracingAccelerationStructure g_RtScene : register(t0);
RaytracingAccelerationStructure g_RtLightScene : register(t1);
StructuredBuffer<Material> g_Materials : register(t2);
StructuredBuffer<LightSource> g_LightSources : register(t3);

RWTexture2D<float4> g_Output : register(u0);

///////////////////////// Local resource ///////////////////////// 

ConstantBuffer<MaterialCB> l_MaterialCB : register(c0, space1);

StructuredBuffer<uint> l_Indices : register(t0, space1);
StructuredBuffer<Vertex> l_Vertices : register(t1, space1);

//////////////////////////////////////////////////////////////////

#endif
