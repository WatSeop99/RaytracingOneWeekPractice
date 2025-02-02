#pragma once

#include <d3d12.h>

struct Buffer
{
	ID3D12Resource* pResource;
	D3D12_CPU_DESCRIPTOR_HANDLE CPUHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE GPUHandle;
};

struct TextureBuffer
{
	ID3D12Resource* pResource;
	ID3D12Resource* pUpload;
	D3D12_CPU_DESCRIPTOR_HANDLE CPUHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE GPUHandle;
	UINT HeapIndex = UINT_MAX;
};

struct GeometryBuffer
{
	Buffer VertexBuffer;
	Buffer IndexBuffer;
};

struct GeometryInstance
{
	struct Buffer
	{
		UINT StartIndex;
		UINT Count;
		D3D12_GPU_DESCRIPTOR_HANDLE GPUDescriptorHandle;
		union
		{
			D3D12_GPU_VIRTUAL_ADDRESS IndexBuffer;
			D3D12_GPU_VIRTUAL_ADDRESS_AND_STRIDE VertexBuffer;
		};
	};

	GeometryInstance::Buffer VB;
	GeometryInstance::Buffer IB;

	D3D12_GPU_VIRTUAL_ADDRESS Transform = 0;
	UINT MaterialID = UINT_MAX;
	bool bIsVertexAnimated = false;
	//D3D12_GPU_DESCRIPTOR_HANDLE DiffuseTexture;
	//D3D12_GPU_DESCRIPTOR_HANDLE NormalTexture;
	D3D12_RAYTRACING_GEOMETRY_FLAGS GeometryFlags = D3D12_RAYTRACING_GEOMETRY_FLAG_NONE;
};

__declspec(align(16)) struct SceneData
{
	DirectX::XMFLOAT3 CameraPos;
	DirectX::XMFLOAT4X4 Projection;
	DirectX::XMFLOAT4X4 InverseProjection;
	float MaxRayRecursionDepth;
	float SceneTime;
	UINT LightCount;

	float dummy[2];
};
__declspec(align(16)) struct LightSource
{
	DirectX::XMFLOAT3 Center;
	UINT LightGeomType;
	float Width;
	float Height;
	float Radius;

	float dummy;
};
