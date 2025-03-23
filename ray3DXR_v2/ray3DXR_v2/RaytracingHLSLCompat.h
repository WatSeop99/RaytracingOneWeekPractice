#ifndef RAYTRACINGHLSLCOMPAT_H
#define RAYTRACINGHLSLCOMPAT_H

#ifdef HLSL
#include "HLSLCompat.h"
#else
using namespace DirectX;
typedef UINT32 Index;
#endif

struct FrameBuffer
{
	XMMATRIX ProjectionToWorld;
	XMMATRIX ModelViewInverse;
	XMVECTOR CameraPosition;
};

struct MeshBuffer
{
	XMFLOAT4 Albedo;
	int MeshID;
	int MaterialID;
	int VerticesOffset;
	int IndicesOffset;
};

struct Vertex
{
	XMFLOAT3 Position;
	float pad1;
	XMFLOAT3 Normal;
	float pad2;
};

#endif
