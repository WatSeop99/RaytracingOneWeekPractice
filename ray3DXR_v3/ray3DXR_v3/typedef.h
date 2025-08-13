#pragma once

#include <DirectXMath.h>
#include "LinkedList.h"

using namespace DirectX;

struct BasicVertex
{
	XMFLOAT3 position;
	XMFLOAT3 normal;
	XMFLOAT3 tangent;
	XMFLOAT4 color;
	XMFLOAT2 texCoord;
};

union RGBA
{
	struct
	{
		BYTE	r;
		BYTE	g;
		BYTE	b;
		BYTE	a;
	};
	BYTE		bColorFactor[4];
};

struct DECOMP_PROJ
{
	float	rcp_m11;
	float	rcp_m22;
	float	m21;
	float	m31;
	float	m32;
	float	m33;
	float	m43;
	float	Reserved0;
};

struct TVERTEX
{
	float u;
	float v;
};
struct FLOAT3
{
	float x;
	float y;
	float z;
};

#define DEFULAT_LOCALE_NAME		L"ko-kr"

static const float NEAR_PLANE = 0.01f;
static const float FAR_PLANE = 800.0f;

inline float XMMatrixExtract(const XMMATRIX* pMatrix, int row, int col)
{
	// 1 base
	float* p = (float*)pMatrix + ((row - 1) * 4) + (col - 1);
	return *p;
}