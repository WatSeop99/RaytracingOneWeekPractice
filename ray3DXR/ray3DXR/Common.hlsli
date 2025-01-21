struct ObjectConstants
{
    uint MaterialID;
    bool bAnimated;
    
    float dummy[2];
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
    float2 TexCoord;
};

struct Ray
{
    float3 Origin;
    float3 Direction;
};

struct Payload
{
    float3 Color;
    float3 HitPoint;
    uint RayRecursionDepth;
    float tHit;
};
struct ScatterPayload
{
    float3 Attenuation;
    float PDF;
    Ray SkipPDFRay;
    bool bSkipPDF;
};

#define MATEIRAL_NONE 0
#define MATERIAL_LAMBERTIAN 1
#define MATERIAL_METAL 2
#define MATERIAL_DIELECTRIC 3
#define MATERIAL_DIFFUSELIGHT 4

