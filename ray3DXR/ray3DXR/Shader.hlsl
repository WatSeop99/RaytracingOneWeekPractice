RaytracingAccelerationStructure g_RtScene : register(t0);
RWTexture2D<float4> g_Output : register(u0);

//ByteAddressBuffer g_Indices : register(t1);
//StructuredBuffer<Vertex> g_Vertices : register(t2);

//uint3 Load3x32BitIndices(uint offsetBytes)
//{
//    uint3 indices;
    
//    // ByteAddressBuffer는 4byte 단위로 읽어올 수 있다.
//    // 어차피 인덱스로 32비트 정수형 데이터를 쓰기 때문에
//    // 인자로 들어온 오프셋 바이트 포함 총 3개의 인덱스를 읽어오면 된다.
//    indices = g_Indices.Load3(offsetBytes);
    
//    return indices;
//}

float3 LinearToSRGB(float3 c)
{
    float3 sq1 = sqrt(c);
    float3 sq2 = sqrt(sq1);
    float3 sq3 = sqrt(sq2);
    float3 srgb = 0.662002687f * sq1 + 0.684122060f * sq2 - 0.323583601f * sq3 - 0.0225411470f * c;
    return srgb;
}

struct RayPayload
{
    float3 Color;
};

[shader("raygeneration")]
void RayGen()
{
    uint3 launchIndex = DispatchRaysIndex();
    uint3 launchDim = DispatchRaysDimensions();

    float2 crd = float2(launchIndex.xy);
    float2 dims = float2(launchDim.xy);

    float2 d = ((crd / dims) * 2.0f - 1.0f);
    float aspectRatio = dims.x / dims.y;

    RayDesc ray;
    ray.Origin = float3(0, 0, -2);
    ray.Direction = normalize(float3(d.x * aspectRatio, -d.y, 1.0f));

    ray.TMin = 0;
    ray.TMax = 100000;

    RayPayload payload;
    TraceRay(g_RtScene, 0 /*rayFlags*/, 0xFF, 0 /* ray index*/, 0, 0, ray, payload);
    float3 col = LinearToSRGB(payload.Color);
    g_Output[launchIndex.xy] = float4(col, 1.0f);
}

[shader("miss")]
void Miss(inout RayPayload payload)
{
    payload.Color = float3(0.4f, 0.6f, 0.2f);
}

[shader("closesthit")]
void ClosestHit(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
    float3 barycentrics = float3(1.0 - attribs.barycentrics.x - attribs.barycentrics.y, attribs.barycentrics.x, attribs.barycentrics.y);

    const float3 A = float3(1, 0, 0);
    const float3 B = float3(0, 1, 0);
    const float3 C = float3(0, 0, 1);

    payload.Color = A * barycentrics.x + B * barycentrics.y + C * barycentrics.z;


    
}
