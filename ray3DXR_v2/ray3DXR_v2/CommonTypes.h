#pragma once

#include "RaytracingHLSLCompat.h"
#include "framework.h"

enum GlobalRootSignatureParams
{
	GlobalRootSignatureParams_OutputViewSlot = 0,
	GlobalRootSignatureParams_AccelerationStructureSlot,
	GlobalRootSignatureParams_SceneConstantSlot,
	GlobalRootSignatureParams_VertexBufferSlot,
	GlobalRootSignatureParams_Count
};
enum LocalRootSignatureParams
{
	LocalRootSignatureParams_MeshBufferSlot = 0,
	LocalRootSignatureParams_Count
};

union AlignedSceneConstantBuffer
{
	FrameBuffer Constants;
	BYTE AlignmentPadding[D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT];
};

struct D3DBuffer
{
	ID3D12Resource2* pResource = nullptr;
	D3D12_CPU_DESCRIPTOR_HANDLE CPUDescriptorHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE GPUDescriptorHandle;
};

struct Geometry
{
	std::vector<Vertex> Vertices;
	std::vector<UINT32> Indices;
	DirectX::XMFLOAT4 Albedo;
	int MaterialID;

	DirectX::XMMATRIX Transform;

	SIZE_T IndicesOffsetInBytes;
	SIZE_T VerticesOffsetInBytes;

	ID3D12Resource2* pBottomLevelAccelerationStructure;
};

#define INIT_BUFFER { nullptr, { 0xFFFFFFFFFFFFFFFF, }, { 0xFFFFFFFFFFFFFFFF, } }
#define INIT_GEOMETRY { {}, {}, { 0.0f, 0.0f, 0.0f, 1.0f }, 0, XMMatrixIdentity(), 0, 0, nullptr }

