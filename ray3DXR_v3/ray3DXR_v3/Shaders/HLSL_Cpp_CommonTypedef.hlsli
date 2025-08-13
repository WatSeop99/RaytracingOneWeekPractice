#ifndef HLSL_CPP_COMMON_TYPEDEF_HLSLI
#define HLSL_CPP_COMMON_TYPEDEF_HLSLI

// bxdf.hlsli¿¡ ¸ÂÃã
namespace MaterialType
{
    enum Type
    {
        Default,
        Matte, // Lambertian scattering
        Mirror, // Specular reflector that isn't modified by the Fresnel equations.
        Glass,
        AnalyticalCheckerboardTexture
    };
}

// Light type
enum RT_LIGHT_TYPE
{
	RT_LIGHT_TYPE_DIRECTIONAL,
	RT_LIGHT_TYPE_POINT,
	RT_LIGHT_TYPE_COUNT
};

#endif