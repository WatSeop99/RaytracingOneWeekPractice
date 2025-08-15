#ifndef RAYTRACING_HLSL
#define RAYTRACING_HLSL

#include "HLSL_Cpp_CommonTypedef.hlsli"
#include "Raytracing_common.hlsl"
#include "bxdf.hlsli"

static const float T_OFFSET = 0.01;
	
RadiancePayload TraceRadianceRay(in Ray ray, in uint CurRayRecursionDepth, in uint MaxRecursionDepth, float tMin, float tMax, bool cullNonOpaque, bool cullBackFace)
{
    RadiancePayload rayPayload = (RadiancePayload)0;

    rayPayload.rayRecursionDepth = CurRayRecursionDepth + 1;
    rayPayload.radiance = 0;
	
    if (CurRayRecursionDepth >= MaxRecursionDepth)
    {
        rayPayload.radiance = float3(1, 1, 1);
        return rayPayload;
    }

	// Set the ray's extents.
    RayDesc rayDesc;
    rayDesc.Origin = ray.origin;
    rayDesc.Direction = ray.direction;
    rayDesc.TMin = tMin;
    rayDesc.TMax = tMax;

    uint rayFlags = 0;
    if (cullNonOpaque)
    {
        rayFlags |= RAY_FLAG_CULL_NON_OPAQUE;
    }
    if (cullBackFace)
    {
        rayFlags |= RAY_FLAG_CULL_BACK_FACING_TRIANGLES;
    }
    // TraceRay(Scene, rayFlags, ~0, 0, 2, 0, rayDesc, rayPayload); // radiance
	// TraceRay(Scene, rayFlags, ~0, 1, 2, 1, rayDesc, shadowPayload); // shadow

	//void TraceRay(RaytracingAccelerationStructure AccelerationStructure,
    //      uint RayFlags,
    //      uint InstanceInclusionMask,
    //      uint RayContributionToHitGroupIndex,
    //      uint MultiplierForGeometryContributionToHitGroupIndex,  //stride
    //      uint MissShaderIndex,
    //      RayDesc Ray,
    //      inout payload_t Payload);
    TraceRay(Scene, rayFlags, ~0, 0, 2, 0, rayDesc, rayPayload);

    return rayPayload;
}
// Returns radiance of the traced reflected ray.
float3 TraceReflectedRay(in float3 hitPosition, in float3 wi, in float3 N, inout RadiancePayload rayPayload, in float TMax, in uint MaxRecursionDepth)
{
	// Here we offset ray start along the ray direction instead of surface normal 
	// so that the reflected ray projects to the same screen pixel. 
	// Offsetting by surface normal would result in incorrect mappating in temporally accumulated buffer. 
    float tOffset = T_OFFSET;
    float3 offsetAlongRay = tOffset * wi;

    float3 adjustedHitPosition = hitPosition + offsetAlongRay;

    Ray ray = { adjustedHitPosition, wi };

    float tMin = NEAR_PLANE;
    float tMax = TMax;

    bool cullNonOpaque = false;
    bool cullBackFace = false;
		
    rayPayload = TraceRadianceRay(ray, rayPayload.rayRecursionDepth, MaxRecursionDepth, tMin, tMax, cullNonOpaque, cullBackFace);
    return rayPayload.radiance;
}
	
    
// Returns radiance of the traced refracted ray.
float3 TraceRefractedRay(in float3 hitPosition, in float3 wt, in float3 N, inout RadiancePayload rayPayload, in float TMax, in uint MaxRecursionDepth)
{
    // Here we offset ray start along the ray direction instead of surface normal 
    // so that the reflected ray projects to the same screen pixel. 
    // Offsetting by surface normal would result in incorrect mappating in temporally accumulated buffer. 
    float tOffset = T_OFFSET;
    float3 offsetAlongRay = tOffset * wt;

    float3 adjustedHitPosition = hitPosition + offsetAlongRay;

    Ray ray = { adjustedHitPosition, wt };

    float tMin = NEAR_PLANE;
    float tMax = TMax;

    // TRADEOFF: Performance vs visual quality
    // Cull transparent surfaces when casting a transmission ray for a transparent surface.
    // Spaceship in particular has multiple layer glass causing a substantial perf hit 
    // with multiple bounces along the way.
    // This can cause visual pop ins however, such as in a case of looking at the spaceship's
    // glass cockpit through a window in the house. The cockpit will be skipped in this case.
    bool cullNonOpaque = false; // 나뭇잎등은 NonOpaque이므로 이 경우 non opaque 매시를 컬링하면 나뭇잎 모서리 알파 문제가 생긴다.
    bool cullBackFace = false;
    rayPayload = TraceRadianceRay(ray, rayPayload.rayRecursionDepth, MaxRecursionDepth, tMin, tMax, cullNonOpaque, cullBackFace);

    return rayPayload.radiance;
}
// Trace a shadow ray and return true if it hits any geometry.
bool TraceShadowRayAndReportIfHit(out float tHit, in Ray ray, in bool retrieveTHit, in float TMax)
{
	// Set the ray's extents.
    RayDesc rayDesc;
    rayDesc.Origin = ray.origin;
    rayDesc.Direction = ray.direction;
    rayDesc.TMin = T_OFFSET;
    rayDesc.TMax = TMax;

	// Initialize shadow ray payload.
	// Set the initial value to a hit at TMax. 
	// Miss shader will set it to HitDistanceOnMiss.
	// This way closest and any hit shaders can be skipped if true tHit is not needed. 
    ShadowPayload shadowPayload = { TMax };

    uint rayFlags = RAY_FLAG_CULL_NON_OPAQUE; // ~skip transparent objects
    bool acceptFirstHit = !retrieveTHit;
    if (acceptFirstHit)
    {
	// Performance TIP: Accept first hit if true hit is not neeeded,
	// or has minimal to no impact. The peformance gain can
	// be substantial.
        rayFlags |= RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH;   // hit이벤트가 발생하면 그대로 탐색을 종료한다.
    }

    // Skip closest hit shaders of tHit time is not needed.
    if (!retrieveTHit)
    {
        // closest hit shader를 사용하지 않는다. 가깝든 멀든 광원에 사이에 장애물이 있는지만 확인하면 된다. 이는 miss shader에서 확인 가능하다.
        rayFlags |= RAY_FLAG_SKIP_CLOSEST_HIT_SHADER;
    }
    rayFlags = 0; // RAY_FLAG_CULL_BACK_FACING_TRIANGLES;

    // TraceRay(Scene, rayFlags, ~0, 0, 2, 0, rayDesc, rayPayload); // radiance
	// TraceRay(Scene, rayFlags, ~0, 1, 2, 1, rayDesc, shadowPayload); // shadow

	//void TraceRay(RaytracingAccelerationStructure AccelerationStructure,
    //      uint RayFlags,
    //      uint InstanceInclusionMask,
    //      uint RayContributionToHitGroupIndex,
    //      uint MultiplierForGeometryContributionToHitGroupIndex,  //stride
    //      uint MissShaderIndex,
    //      RayDesc Ray,
    //      inout payload_t Payload);
    TraceRay(Scene, rayFlags, ~0, 1, 2, 1, rayDesc, shadowPayload);

    tHit = shadowPayload.tHit;

    return shadowPayload.tHit > 0;
}
bool TryTraceShadowRayAndReportIfHit(in float3 hitPosition, in float3 direction, in float3 N, in RadiancePayload rayPayload, in float TMax, in uint MaxRecursionDepth)
{
	// hitPosition - ray가 충돌한 점
	// direction - ray가 충돌한 점에서 광원까지의 normalized 방향벡터
	// N - ray가 충돌한 점의 노말벡터
    bool bInShadow = false;
    float tOffset = T_OFFSET;
    float dummyTHit;
		
    if (rayPayload.rayRecursionDepth >= MaxRecursionDepth)
    {
        return false;
    }
    
    Ray visibilityRay = { hitPosition + tOffset * N, direction };
	
	// Only trace if the surface is facing the target.
    if (dot(visibilityRay.direction, N) <= 0)
        return false;
    
    return TraceShadowRayAndReportIfHit(dummyTHit, visibilityRay, false, TMax);
}
float3 Shade(inout RadiancePayload rayPayload, in float3 N, in float3 hitPosition, in RAY_TRACING_MATERIAL material)
{
    uint MaxRadianceRecursionDepth = g_MaxRadianceRayRecursionDepth;
	
    float3 V = -WorldRayDirection();    // View Vector
    float pdf;
    float3 indirectContribution = 0;
    float3 L = 0;
	
    float3 Kd = material.Kd;
    float3 Ks = material.Ks;
    float3 Kr = material.Kr;
    float3 Kt = material.Kt;
    float roughness = material.roughness;
		
	// Direct illumination
    if (!BxDF::IsBlack(material.Kd) || !BxDF::IsBlack(material.Ks))
    {
        for (uint i = 0; i < g_LightCount; i++)
        {
            float3 LightColor = g_LightList[i].Color;
            float3 wi = normalize(g_LightList[i].Pos_Dir) * -1;
            float tMax = g_LightList[i].Rs;
            

			// Raytraced shadows.
            bool isInShadow = TryTraceShadowRayAndReportIfHit(hitPosition, wi, N, rayPayload, tMax, g_MaxShadowRayRecursionDepth);
            // Kd = diffuse , Ks = specular , V = view vector, wi = light vector
            L += BxDF::DirectLighting::Shade(
						material.type,
						Kd,
						Ks,
						LightColor.rgb,
						isInShadow,
						roughness,
						N,
						V,
						wi);
        }
    }
	// Ambient Indirect Illumination
	// Add a default ambient contribution to all hits. 
	// This will be subtracted for hitPositions with 
	// calculated Ambient coefficient in the composition pass.
    L += material.AmbientIntensity * Kd;

	// Specular Indirect Illumination
    bool isReflective = !BxDF::IsBlack(Kr);
    bool isTransmissive = !BxDF::IsBlack(Kt);

	// Handle cases where ray is coming from behind due to imprecision,
	// don't cast reflection rays in that case.
    float smallValue = 1e-6f;
    isReflective = (dot(V, N) > smallValue) ? isReflective : false;

    if (isReflective || isTransmissive)
    {
        if (isReflective && (BxDF::Specular::Reflection::IsTotalInternalReflection(V, N) || material.type == MaterialType::Mirror))
        {
            
            float3 wi = reflect(-V, N);
            RadiancePayload reflectedRayPayLoad = rayPayload;
            L += Kr * TraceReflectedRay(hitPosition, wi, N, reflectedRayPayLoad, FAR_PLANE, MaxRadianceRecursionDepth);
        }
        else // No total internal reflection
        {
            float3 Fo = Ks;
            if (isReflective)
            {
			// Radiance contribution from reflection.
                float3 wi;
                float3 Fr = Kr * BxDF::Specular::Reflection::Sample_Fr(V, wi, N, Fo); // Calculates wi

                RadiancePayload reflectedRayPayLoad = rayPayload;
			// Ref: eq 24.4, [Ray-tracing from the Ground Up]
                L += Fr * TraceReflectedRay(hitPosition, wi, N, reflectedRayPayLoad, FAR_PLANE, MaxRadianceRecursionDepth);
            }

            if (isTransmissive)
            {
			    // Radiance contribution from refraction.
                float3 wt;
                float3 Ft = Kt * BxDF::Specular::Transmission::Sample_Ft(V, wt, N, Fo); // Calculates wt

                RadiancePayload refractedRayPayLoad = rayPayload;

                L += Ft * TraceRefractedRay(hitPosition, wt, N, refractedRayPayLoad, FAR_PLANE, MaxRadianceRecursionDepth);
            }
        }
    }
	
    return L;
}
	
[shader("raygeneration")]
void MyRaygenShader_RadianceRay()
{
    uint2 launchIndex = DispatchRaysIndex().xy;
    uint2 launchDim = DispatchRaysDimensions().xy;

    float2 CurPixel = (float2)launchIndex.xy + float2(0.5f, 0.5f); // 현재 픽셀의 좌표(픽셀의 중앙)
    float2 Res = (float2)launchDim.xy;

    float4 ray_view = float4(
		(((2.0f * ((float)CurPixel.x) / (float)Res.x)) - 1.0f) * g_DecompProj.rcp_m11,
		-(((2.0f * ((float)CurPixel.y) / (float)Res.y)) - 1.0f) * g_DecompProj.rcp_m22,
		1.0, 0.0);

    float4 ray_world = mul(ray_view, g_ViewInv);
	//ray_world.xyz *= (1.0 / 100.0);
    ray_world.xyz = normalize(ray_world.xyz);
    float3 worldDir = ray_world.xyz;
    float3 worldOrigin = float3(g_CameraPosition.xyz);

    Ray ray =
    {
        worldOrigin,
		worldDir
    };
    uint CurRayRecursionDepth = 0;
    bool cullNonOpaque = false;
    bool cullBackFace = false;
    RadiancePayload rayPayload = TraceRadianceRay(ray, CurRayRecursionDepth, g_MaxRadianceRayRecursionDepth, NEAR_PLANE, FAR_PLANE, cullNonOpaque, cullBackFace);
	
    g_OutputDiffuse[launchIndex.xy] = float4(rayPayload.radiance, 1);
    g_OutputDepth[launchIndex.xy] = rayPayload.depth;
}
[shader("closesthit")]
void MyClosestHitShader_RadianceRay(inout RadiancePayload rayPayload, in BuiltInTriangleIntersectionAttributes attr)
{
    float3 hitPosition = HitWorldPosition();

	// Get the base index of the triangle's first 16 bit index.
    uint InstID = InstanceID();
    //uint CustomInstIndex = GetInstanceIndex(InstID); // CRayTracingManager에서 발급한 인덱스-오브젝트 인덱스
    uint SystemInstIndex = InstanceIndex(); // The autogenerated index of the current instance in the top-level structure.
    
    // PrimitiveIndex() 
    // The autogenerated index of the primitive within the geometry inside the bottom-level acceleration structure instance.For D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES, this is the triangle index within the geometry object.
    
    // 충돌로 판정된 BLAS->Geometry[n]내의 삼각형 인덱스를 먼저 얻는다. 이것으로 index buffer에서의 시작 index를 구한다.
    uint baseIndex = PrimitiveIndex() * g_TriangleIndexStride;

    // 삼각형의 세 점을 얻기 위해 세 점의 index를 구한다.
    uint3 indices = l_Indices.Load3(baseIndex);
    float2 CurTexCoord = 0;
    float4 CurColor = float4(0, 0, 0, 1);
    float4 texDiffuse = float4(0, 0, 0, 0);
    float3 texNormal = float3(0.5, 0.5, 1);

    float3 VertexNormals[3] =
    {
        l_Vertices[indices[0]].Normal,
		l_Vertices[indices[1]].Normal,
		l_Vertices[indices[2]].Normal,
    };
    float3 VertexTangent[3] =
    {
        l_Vertices[indices[0]].Tangent,
		l_Vertices[indices[1]].Tangent,
		l_Vertices[indices[2]].Tangent,
    };
		
    float4 Color[3] =
    {
        l_Vertices[indices[0]].Color,
		l_Vertices[indices[1]].Color,
		l_Vertices[indices[2]].Color
    };
    float2 TexCoord[3] =
    {
        l_Vertices[indices[0]].TexCoord.xy,
		l_Vertices[indices[1]].TexCoord.xy,
		l_Vertices[indices[2]].TexCoord.xy
    };
    CurColor = HitAttribute(Color, attr);
    CurTexCoord = HitAttribute(TexCoord, attr);

    texDiffuse = l_texDiffuse.SampleLevel(samplerPoint, CurTexCoord, 0);
	//texNormal = l_texNormal.SampleLevel(samplerWrap, CurTexCoord, 0).rgb;

	// tangent vector
	// 오브젝트의 local좌표계에서의 노말
    float3 LocalNormal = HitAttribute(VertexNormals, attr);
    float3 LocalTangent = HitAttribute(VertexTangent, attr);
	
	// 면의 뒷면에 충돌했을 경우 노멀을 뒤집어준다.
    //float orientation = HitKind() == HIT_KIND_TRIANGLE_FRONT_FACE ? 1 : -1;
    //LocalNormal *= orientation;
    //LocalTangent *= orientation;

	// 월드좌표계에서의 노말	
    float3 WorldNormal = normalize(mul(LocalNormal, (float3x3)ObjectToWorld4x3())); // Transposed Matrix
    float3 WorldTangent = normalize(mul(LocalTangent, (float3x3)ObjectToWorld4x3())); // Transposed Matrix
    float3 WorldBinormal = cross(WorldTangent, WorldNormal);
   
    float3 tan_normal = texNormal.rgb * 2 - 1;
    float3 surfaceNormal = (tan_normal.xxx * WorldTangent) + (tan_normal.yyy * WorldBinormal) + (tan_normal.zzz * WorldNormal);
   	
    float4 PrjPos = mul(float4(hitPosition.xyz, 1), g_ViewProj);
    PrjPos /= PrjPos.w;
    rayPayload.depth = saturate(PrjPos.z);
		
    RAY_TRACING_MATERIAL material;
    material.Kd = texDiffuse.rgb * l_rayGeomCB.mtl.opacity;
    material.type = l_rayGeomCB.mtl.type;
    material.Ks = l_rayGeomCB.mtl.Ks;
    material.roughness = l_rayGeomCB.mtl.roughness;
    material.Kr = l_rayGeomCB.mtl.Kr;
    material.AmbientIntensity = l_rayGeomCB.mtl.AmbientIntensity;
    material.Kt = l_rayGeomCB.mtl.Kt;
    
    rayPayload.radiance = Shade(rayPayload, surfaceNormal, hitPosition, material);
}

[shader("miss")]
void MyMissShader_RadianceRay(inout RadiancePayload rayPayload)
{
    rayPayload.radiance = g_texSky.SampleLevel(samplerClamp, WorldRayDirection(), 0).rgb;
    rayPayload.depth = 1.2;
}

[shader("anyhit")]
void MyAnyHitShader_RadianceRay(inout RadiancePayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    float3 hitPosition = HitWorldPosition();
    
	// Get the base index of the triangle's first 16 bit index.
    uint InstID = InstanceID();
    uint SystemInstIndex = InstanceIndex(); // The autogenerated index of the current instance in the top-level structure.

    uint baseIndex = PrimitiveIndex() * g_TriangleIndexStride;
    
	// Load up 3 16 bit indices for the triangle.
    uint3 indices = l_Indices.Load3(baseIndex);
    float2 CurTexCoord = 0;
    float4 CurColor = float4(0, 0, 0, 1);
    float4 texDiffuse = float4(0, 0, 0, 0);
	
    float2 TexCoord[3] =
    {
        l_Vertices[indices[0]].TexCoord.xy,
		l_Vertices[indices[1]].TexCoord.xy,
		l_Vertices[indices[2]].TexCoord.xy
    };
    CurTexCoord = HitAttribute(TexCoord, attr);

    texDiffuse = l_texDiffuse.SampleLevel(samplerPoint, CurTexCoord, 0);
    
    float alpha = texDiffuse.a;
    
    if (alpha < RT_ALPHA_TEST_THRESHOLD)
    {
        // 알파값이 0에 가까운 경우. 이번 hit 이벤트는 무시하고 탐색을 계속 진행.
        IgnoreHit();
    }
    else
    {
        //
		// AcceptHitAndEndSearch() - 이번 hit를 받아들여서 탐색을 멈추고 closest hit shader가 호출되도록 설정.
        //
        // 아무것도 하지 않음. - 탐색은 계속 진행되며 최종적으로 (이번 hit point를 포함하여)가장 가까운 hit point에 대해 closest hit shader가 호출됨. 
		//
    }
}
[shader("closesthit")]
void MyClosestHitShader_ShadowRay(inout ShadowPayload rayPayload, in BuiltInTriangleIntersectionAttributes attr)
{
    rayPayload.tHit = RayTCurrent();
}
	
[shader("miss")]
void MyMissShader_ShadowRay(inout ShadowPayload rayPayload)
{
    // hit이벤트가 발생하지 않는다면(충돌하는 매시가 없다면 rayPayload.tHit = 0이 된다.
    rayPayload.tHit = HitDistanceOnMiss;
}
[shader("anyhit")]
void MyAnyHitShader_ShadowRay(inout ShadowPayload rayPayload, in BuiltInTriangleIntersectionAttributes attr)
{
    float3 hitPosition = HitWorldPosition();
    
	// Get the base index of the triangle's first 16 bit index.
    uint InstID = InstanceID();
    uint SystemInstIndex = InstanceIndex(); // The autogenerated index of the current instance in the top-level structure.

    uint baseIndex = PrimitiveIndex() * g_TriangleIndexStride;

	// Load up 3 16 bit indices for the triangle.
    uint3 indices = l_Indices.Load3(baseIndex);
    float2 CurTexCoord = 0;
    float4 CurColor = float4(0, 0, 0, 1);
    float4 texDiffuse = float4(0, 0, 0, 0);
	
    float2 TexCoord[3] =
    {
        l_Vertices[indices[0]].TexCoord.xy,
		l_Vertices[indices[1]].TexCoord.xy,
		l_Vertices[indices[2]].TexCoord.xy
    };
    CurTexCoord = HitAttribute(TexCoord, attr);

    texDiffuse = l_texDiffuse.SampleLevel(samplerPoint, CurTexCoord, 0);
    
    float alpha = texDiffuse.a;
    
    if (alpha < RT_ALPHA_TEST_THRESHOLD)
    {
        // 알파값이 0에 가까운 경우. 이번 hit 이벤트는 무시하고 탐색을 계속 진행.
        IgnoreHit();
    }
    else
    {
        // 불투명/반투명 매시는 그림자를 드리울 수 있으므로 여기서 탐색을 중단하고 closeset shader호출. 
        // RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER 조합으로 설정된 경우 closest hit shader는 호출되지 않지만
        // Miss Shader호출을 막음으로서 closest hit shader 호출 없이 그림자가 드리워지는지 여부 판별 가능.
        AcceptHitAndEndSearch();
    }
}
#endif // RAYTRACING_HLSL
