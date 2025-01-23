#ifndef COMMON_HLSL
#define COMMON_HLSL

struct Scene
{
    float3 CameraPos;
    float4x4 Projection;
    float4x4 InverseProjection;
    float MaxRayRecursionDepth;
    float SceneTime;
    uint LightCount;
};

struct ObjectConstants
{
    uint MaterialID;
    bool bAnimated;
    
    float dummy[2];
};

struct LightSource
{
    float3 Center;
    uint LightGeomType; // quad or sphere.
    float Width; // for quad light source.
    float Height; // for quad light source.
    float Radius; // for sphere light source.
};

struct Material
{
    int MatType;
    float3 Color;
    float Fuzz;
    float RefractionIndex;

    float dummy[2];
};
struct MaterialCB
{
    uint MaterialID;
    float dummy[3];
};

struct Vertex
{
    float3 Position;
    float3 Normal;
    float3 Tangent;
    float2 TexCoord;
};

struct Ray
{
    float3 Origin;
    float3 Direction;
};

struct ONB
{
    float3 Axis[3];
};

struct Payload
{
    float3 Color;
    float3 HitPoint;
    float3 HitNormal;
    float3 HitTangent;
    float RayTime;
    uint RayRecursionDepth;
    uint2 DTID;
};
struct HitFoundPayload
{
    float3 HitPoint;
    float3 HitNormal;
    float3 HitTangent;
    float RayTime;
};
struct ScatterPayload
{
    float3 Attenuation;
    ONB PDF;
    Ray SkipPDFRay;
    bool bSkipPDF;
};


#define MATEIRAL_NONE 0
#define MATERIAL_LAMBERTIAN 1
#define MATERIAL_METAL 2
#define MATERIAL_DIELECTRIC 3
#define MATERIAL_DIFFUSELIGHT 4


#define LIGHT_GEOM_TYPE_QUAD 1
#define LIGHT_GEOM_TYPE_SPHERE 2


#endif
