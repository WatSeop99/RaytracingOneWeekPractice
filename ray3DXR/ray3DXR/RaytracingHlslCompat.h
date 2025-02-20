//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#ifndef RAYTRACINGHLSLCOMPAT_H
#define RAYTRACINGHLSLCOMPAT_H

#ifdef HLSL
#include "HlslCompat.h"
#else
using namespace DirectX;

// Shader will use byte encoding to access indices.
typedef UINT16 Index;
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

#endif // RAYTRACINGHLSLCOMPAT_H