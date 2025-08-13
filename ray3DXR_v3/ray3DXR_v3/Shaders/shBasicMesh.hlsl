Texture2D texDiffuse : register(t0);
SamplerState samplerDiffuse : register(s0);

cbuffer CONSTANT_BUFFER_DEFAULT : register(b0)
{
    matrix g_matWorld;
    matrix g_matView;
	matrix g_matProj;
    float g_fRadius;
    float g_fInterpolation;
    uint g_VertexCount;
    float g_fReserved3;
};
struct VSInput
{
    float4 Pos : POSITION;
    float3 Normal : NORMAL;
    float3 Tangent : TANGENT;
    float4 Color : COLOR0;
    float2 TexCoord : TEXCOORD0;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float2 TexCoord : TEXCOORD0;
};

PSInput VSMain(VSInput input)
{
    PSInput result = (PSInput)0;
    
    
    matrix matViewProj = mul(g_matView, g_matProj);        // view x proj
    matrix matWorldViewProj = mul(g_matWorld, matViewProj);   // world x view x proj
    result.position = mul(input.Pos, matWorldViewProj); // pojtected vertex = vertex x world x view x proj
    result.TexCoord = input.TexCoord;
    result.color = input.Color;
    
    return result;
}
PSInput VSDeform(VSInput input)
{
    PSInput result = (PSInput)0;
    
    float3 v_normal = normalize((float3)input.Pos - float3(0, 0, 0));
    float3 TargetPos = v_normal * g_fRadius;
    float3 Pos = lerp((float3)input.Pos, TargetPos, g_fInterpolation);
    
    matrix matViewProj = mul(g_matView, g_matProj);        // view x proj
    matrix matWorldViewProj = mul(g_matWorld, matViewProj);   // world x view x proj
    result.position = mul(float4(Pos, 1), matWorldViewProj); // pojtected vertex = vertex x world x view x proj
    result.TexCoord = input.TexCoord;
    result.color = input.Color;
    
    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    float4 texColor = texDiffuse.Sample(samplerDiffuse, input.TexCoord);
    return texColor * input.color;
}

#include "Raytracing_typedef.hlsl"

//struct BasicVertex
//{
//    float3 Pos;
//    float3 Normal;
//    float3 Tangent;
//    float4 Color;
//    float2 TexCoord;
//};


StructuredBuffer<BasicVertex> g_Vertices : register(t3);
RWStructuredBuffer<BasicVertex> g_OutVertices : register(u8);

[numthreads(1024, 1, 1)]
void CSDeform(uint3 groupID : SV_GroupID, uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint CurVertexIndex = dispatchThreadId.x;
    if (CurVertexIndex > g_VertexCount)
        return;
    
    float3 SrcPos = float3(g_Vertices[CurVertexIndex].Pos);
    float3 Tangent = float3(g_Vertices[CurVertexIndex].Tangent);
    
    float3 Normal = normalize((float3)SrcPos - float3(0, 0, 0));
    float3 TargetPos = Normal * g_fRadius;
    float3 Pos = lerp((float3)SrcPos, TargetPos, g_fInterpolation);
    
    float3 Binormal = normalize(cross(Tangent, Normal));
    Tangent = normalize(cross(Binormal, Normal));
    
    g_OutVertices[CurVertexIndex].Pos = Pos;
    g_OutVertices[CurVertexIndex].Normal = Normal;
    g_OutVertices[CurVertexIndex].Tangent = Tangent;
    g_OutVertices[CurVertexIndex].TexCoord = g_Vertices[CurVertexIndex].TexCoord;
    g_OutVertices[CurVertexIndex].Color = g_Vertices[CurVertexIndex].Color;
}