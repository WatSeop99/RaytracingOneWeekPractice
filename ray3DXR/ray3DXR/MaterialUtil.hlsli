#ifndef MATERIAL_HLSL
#define MATERIAL_HLSL

#include "Util.hlsli"
#include "PDFUtil.hlsl"

float Reflectance(in float cosine, in float refractionIndex)
{
	float r0 = (1.0f - refractionIndex) / (1.0f + refractionIndex);
	r0 = r0 * r0;
	return (r0 + (1.0f - r0) * pow((1.0f - cosine), 5.0f));
}

float3 CalculateEmissionColor(in Material mat)
{
	float3 retColor = float3(0.0f, 0.0f, 0.0f);
	
	if (mat.MatType == MATERIAL_DIFFUSELIGHT)
	{
		retColor = mat.Color;
	}
	
	return retColor;
}

bool IsScatter(in Material mat, in Ray ray, in Payload payload, in uint hitKind, inout ScatterPayload scatterPayload)
{
    switch (mat.MatType)
    {
        case MATERIAL_LAMBERTIAN:
			{
                scatterPayload.Attenuation = mat.Color;
                scatterPayload.bSkipPDF = false;
            
                float3 normal = normalize(payload.HitNormal);
                scatterPayload.PDF = InitializeONB(normal);
			
                break;
            }
		
        case MATERIAL_METAL:
			{
                float3 reflected = reflect(ray.Direction, payload.HitNormal);
                reflected = normalize(reflected) + (mat.Fuzz * GetRandomUnitVector());
			
                scatterPayload.Attenuation = mat.Color;
                scatterPayload.bSkipPDF = true;
                scatterPayload.SkipPDFRay.Origin = payload.HitPoint;
                scatterPayload.SkipPDFRay.Direction = reflected;
			
                break;
            }
		
        case MATERIAL_DIELECTRIC:
			{
                scatterPayload.Attenuation = float3(1.0f, 1.0f, 1.0f);
                scatterPayload.bSkipPDF = true;
			
                float ri = (hitKind == HIT_KIND_TRIANGLE_FRONT_FACE ? (1.0f / mat.RefractionIndex) : mat.RefractionIndex);
				
                float3 unitDir = normalize(ray.Direction);
                float cosTheta = min(dot(-unitDir, payload.HitNormal), 1.0f);
                float sinTheta = sqrt(1.0f - cosTheta * cosTheta);
			
                bool bCannotRefract = (ri * sinTheta > 1.0f);
                float3 direction;
                if (bCannotRefract || Reflectance(cosTheta, ri) > GetRandomFloatValue())
                {
                    direction = reflect(unitDir, payload.HitNormal);
                }
                else
                {
                    direction = refract(unitDir, payload.HitNormal, ri);
                }
			
                scatterPayload.SkipPDFRay.Origin = payload.HitPoint;
                scatterPayload.SkipPDFRay.Direction = direction;
			
                break;
            }
		
        case MATERIAL_DIFFUSELIGHT:
        default:
            return false;
    }
	
	return true;
}

#endif
