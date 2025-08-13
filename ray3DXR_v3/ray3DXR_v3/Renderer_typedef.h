#pragma once

#include "./Shaders/HLSL_Cpp_CommonTypedef.hlsli"

const UINT SWAP_CHAIN_FRAME_COUNT = 3;
const UINT MAX_PENDING_FRAME_COUNT = SWAP_CHAIN_FRAME_COUNT - 1;
const UINT PAYLOAD_SIZE = 20;

const UINT MAX_RT_LIGHT_COUNT = 8;

struct RT_LIGHT
{
	XMFLOAT3 Pos_Dir;
	float Rs;
	XMFLOAT3 Color;
	RT_LIGHT_TYPE Type;
};

struct CONSTANT_BUFFER_RAY_TRACING
{
	XMMATRIX	matViewProj;
	XMMATRIX	matViewInv;
	DECOMP_PROJ	DecompProj;
	XMVECTOR	CameraPosition;
	float Near;
	float Far;
	UINT MaxRadianceRayRecursionDepth;
	UINT MaxShadowRayRecursionDepth;
	UINT LightCount;
	UINT Reserved0;
	UINT Reserved1;
	UINT Reserved2;
	RT_LIGHT LightList[MAX_RT_LIGHT_COUNT];
};

struct CONSTANT_BUFFER_DEFAULT
{
	XMMATRIX	matWorld;
	XMMATRIX	matView;
	XMMATRIX	matProj;
	float fRadis;
	float fInterpolation;
	UINT VertexCount;
	float fReserved3;
};
struct CONSTANT_BUFFER_SPRITE
{
	XMFLOAT2 ScreenRes;
	XMFLOAT2 Pos;
	XMFLOAT2 Scale;
	XMFLOAT2 TexSize;
	XMFLOAT2 TexSampePos;
	XMFLOAT2 TexSampleSize;
	float	Z;
	float	Alpha;
	float	Reserved0;
	float	Reserved1;
};


enum CONSTANT_BUFFER_TYPE
{
	CONSTANT_BUFFER_TYPE_DEFAULT,
	CONSTANT_BUFFER_TYPE_SPRITE,
	CONSTANT_BUFFER_TYPE_RAY_TRACING,
	CONSTANT_BUFFER_TYPE_COUNT
};

struct CONSTANT_BUFFER_PROPERTY
{
	CONSTANT_BUFFER_TYPE type;
	UINT Size;
};

struct TEXTURE_HANDLE
{
	ID3D12Resource*	pTexResource;
	ID3D12Resource*	pUploadBuffer;
	D3D12_CPU_DESCRIPTOR_HANDLE srv;
	BOOL	bUpdated;
	BOOL	bFromFile;
	DWORD	dwRefCount;
	void* pSearchHandle;
	SORT_LINK	Link;
};

struct FONT_HANDLE
{
	IDWriteTextFormat*	pTextFormat;
	float fFontSize;
	WCHAR	wchFontFamilyName[512];
};

struct BASIC_MATERIAL
{
	XMFLOAT3 Ks;
	MaterialType::Type type;
	XMFLOAT3 Kr;
	float roughness;
	XMFLOAT3 Kt;
	float AmbientIntensity;
	XMFLOAT3 opacity;
	UINT Reserved0;
};
struct CONSTANT_BUFFER_RT_TRIGROUP
{
	BASIC_MATERIAL mtl;
};
struct ROOT_ARG
{
	CONSTANT_BUFFER_RT_TRIGROUP cb;
	D3D12_GPU_DESCRIPTOR_HANDLE srvVB;
	D3D12_GPU_DESCRIPTOR_HANDLE srvIB;
	D3D12_GPU_DESCRIPTOR_HANDLE srvTexDiffuse;
	D3D12_GPU_DESCRIPTOR_HANDLE srvTexNormal;
};

const DWORD MAX_TRIGROUP_COUNT_PER_BLAS = 16;

struct BLAS_BUILD_TRIGROUP_INFO
{
	ID3D12Resource*	pIB;
	TEXTURE_HANDLE*	pDiffuseTexHandle;
	TEXTURE_HANDLE*	pNormalTexHandle;
	DWORD	dwIndexNum;
	BOOL	bNotOpaque;
	BASIC_MATERIAL mtl;
};

struct BLAS_INSTANCE
{
	void* pSrcMeshObj;
	ID3D12Resource*	pBLAS;
	
	// 1개의 MehsObject -> N개의 BLAS일 수 있으므로 dest버퍼는 BLAS별로 가지고 있어야한다.
	ID3D12Resource* pVBResourceUpdated;
	D3D12_CPU_DESCRIPTOR_HANDLE	uavVBResourceUpdated;
	XMMATRIX matTransform;
	

	SORT_LINK Link;
	DWORD	dwID;
	BOOL	bAllowUpdate;
	BOOL	bMustUpdatBlas;
	UINT	ShaderRecordIndex;
	DWORD	dwVertexCount;
	DWORD	dwTriGroupCount;
	D3D12_RAYTRACING_GEOMETRY_DESC pGeomDescList[MAX_TRIGROUP_COUNT_PER_BLAS] = {};

	// local param area
	D3D12_CPU_DESCRIPTOR_HANDLE	srvCpuHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE	srvGpuHandle;
	ROOT_ARG	pRootArg[1];

};
