#ifndef RAYTRACING_CONST_BUFFERL_HLSL
#define RAYTRACING_CONST_BUFFERL_HLSL

#include "HLSL_Cpp_CommonTypedef.hlsli"

#define INT_MIN     (-2147483647 - 1)
#define INT_MAX       2147483647
#define EPSILON 1e-10
#define DWORD uint
#define UINT uint

#define HitDistanceOnMiss 0

static const uint g_IndexSizeInBytes = 2;
static const uint g_IndicesPerTriangle = 3;
static const uint g_TriangleIndexStride = g_IndicesPerTriangle * g_IndexSizeInBytes;
static const uint MAX_RT_LIGHT_COUNT = 8;
static const float RT_ALPHA_TEST_THRESHOLD = 0.001;

struct BasicVertex
{
    float3 Pos;
    float3 Normal;
    float3 Tangent;
    float4 Color;
    float2 TexCoord;
};
struct DECOMP_PROJ
{
    float rcp_m11;
    float rcp_m22;
    float m21;
    float m31;
    float m32;
    float m33;
    float m43;
    float Reserved0;
};

struct Ray
{
    float3 origin;
    float3 direction;
};

struct RadiancePayload
{
    float3 radiance; // TODO encode
    float depth;
    uint rayRecursionDepth;
};

struct ShadowPayload
{
    float tHit; // Hit time <0,..> on Hit. -1 on miss.
};
static const float NEAR_PLANE = 0.01;
static const float FAR_PLANE = 800.0;

//typedef BuiltInTriangleIntersectionAttributes MyAttributes;



struct RT_LIGHT
{
    float3 Pos_Dir; // 방향성 라이트일땐 방향, 포인트 라이트일땐 위치
    float Rs;
    float3 Color;
    RT_LIGHT_TYPE Type;
};

// input from cpu side
struct BASIC_MATERIAL
{
	float3 Ks;
	MaterialType::Type type;
	float3 Kr;
	float roughness;
	float3 Kt;
	float AmbientIntensity;
    float3 opacity;
    uint Reserved0;
};

// shader internal
struct RAY_TRACING_MATERIAL
{
    float3 Kd;
    float3 Ks;
    float3 Kr;
    float3 Kt;
    MaterialType::Type type;
    float roughness;
    float AmbientIntensity;
};
struct CONSTANT_BUFFER_RT_TRIGROUP
{
    BASIC_MATERIAL mtl;
};
#endif