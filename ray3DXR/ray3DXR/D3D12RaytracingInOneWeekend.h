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

#pragma once

#include "DXSample.h"
#include "StepTimer.h"
#include "RaytracingHlslCompat.h"

namespace GlobalRootSignatureParams
{
	enum Value
	{
		OutputViewSlot = 0,
		AccelerationStructureSlot,
		SceneConstantSlot,
		VertexBuffersSlot,
		Count
	};
}

namespace LocalRootSignatureParams
{
	enum Value
	{
		MeshBufferSlot = 0,
		Count
	};
}

class D3D12RaytracingInOneWeekend : public DXSample
{
private:
	union AlignedSceneConstantBuffer
	{
		FrameBuffer Constants;
		UINT8 AlignmentPadding[D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT];
	};
	struct D3DBuffer
	{
		ID3D12Resource* pResource;
		D3D12_CPU_DESCRIPTOR_HANDLE CPUDescriptorHandle;
		D3D12_GPU_DESCRIPTOR_HANDLE GPUDescriptorHandle;
	};
	struct Geometry
	{
		std::vector<Vertex> Vertices;
		std::vector<UINT32> Indices;
		XMFLOAT4 Albedo;
		int MaterialID;

		XMMATRIX Transform;

		SIZE_T IndicesOffsetInBytes;
		SIZE_T VerticesOffsetInBytes;


		// Acceleration structure
		ID3D12Resource* pBottomLevelAccelerationStructure;
	};
#define INIT_BUFFER { nullptr, { 0xFFFFFFFFFFFFFFFF, }, { 0xFFFFFFFFFFFFFFFF, } }
#define INIT_GEOMETRY { {}, {}, { 0.0f, 0.0f, 0.0f, 1.0f }, 0, XMMatrixIdentity(), 0, 0, nullptr }

	static const UINT FRAME_COUNT = 3;

	static const WCHAR* HIT_GROUP_NAME;
	static const WCHAR* RAYGEN_SHADER_NAME;
	static const WCHAR* CLOSEST_HIT_SHADER_NAME;
	static const WCHAR* MISS_SHADER_NAME;

	// We'll allocate space for several of these and they will need to be padded for alignment.
	static_assert(sizeof(FrameBuffer) < D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, "Checking the size here.");

public:
	D3D12RaytracingInOneWeekend(UINT width, UINT height, const WCHAR* pszName);

	// IDeviceNotify
	virtual void OnDeviceLost() override;
	virtual void OnDeviceRestored() override;

	// Messages
	virtual void OnInit();
	virtual void OnUpdate();
	virtual void OnRender();
	virtual void OnSizeChanged(UINT width, UINT height, bool bMinimized);
	virtual void OnDestroy();
	inline virtual IDXGISwapChain* GetSwapchain() { return m_pDeviceResources->GetSwapChain(); }

private:
	void UpdateCameraMatrices();
	void InitializeScene();
	void RecreateD3D();
	void DoRaytracing();
	void CreateConstantBuffers();
	void CreateDeviceDependentResources();
	void CreateWindowSizeDependentResources();
	void ReleaseDeviceDependentResources();
	void ReleaseWindowSizeDependentResources();
	void CreateRaytracingInterfaces();
	void SerializeAndCreateRaytracingRootSignature(D3D12_ROOT_SIGNATURE_DESC* pDesc, ID3D12RootSignature** ppRootSig);
	void CreateRootSignatures();
	void CreateLocalRootSignatureSubobjects(CD3DX12_STATE_OBJECT_DESC* pRaytracingPipeline);
	void CreateRaytracingPipelineStateObject();
	void CreateDescriptorHeap();
	void CreateRaytracingOutputResource();
	void BuildGeometry();
	void BuildAccelerationStructures();
	void BuildShaderTables();
	void UpdateForSizeChange(UINT clientWidth, UINT clientHeight);
	void CopyRaytracingOutputToBackbuffer();
	void CalculateFrameStats();
	UINT AllocateDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE* pCPUDescriptor, UINT descriptorIndexToUse = UINT_MAX);
	UINT CreateBufferSRV(D3DBuffer* pBuffer, UINT numElements, UINT elementSize);

private:
	AlignedSceneConstantBuffer* m_pMappedConstantData = nullptr;
	ID3D12Resource* m_pPerFrameConstants = nullptr;

	// DirectX Raytracing (DXR) attributes
	ID3D12Device5* m_pDXRDevice = nullptr;
	ID3D12GraphicsCommandList5* m_pDXRCommandList = nullptr;
	ID3D12StateObject* m_pDXRStateObject = nullptr;

	// Root signatures
	ID3D12RootSignature* m_pRaytracingGlobalRootSignature = nullptr;
	ID3D12RootSignature* m_pRaytracingLocalRootSignature = nullptr;

	// Descriptors
	ID3D12DescriptorHeap* m_pDescriptorHeap = nullptr;
	UINT m_DescriptorsAllocated = 0;
	UINT m_DescriptorSize = 0;

	// Raytracing scene
	FrameBuffer m_FrameCB[FRAME_COUNT] = {};

	// Geometry
	ID3D12Resource* m_pTopLevelAccelerationStructure = nullptr;

	std::vector<Geometry> m_geometry;

	D3DBuffer m_VertexBuffer = INIT_BUFFER;
	D3DBuffer m_IndexBuffer = INIT_BUFFER;

	// Raytracing output
	ComPtr<ID3D12Resource> m_pRaytracingOutput = nullptr;
	D3D12_GPU_DESCRIPTOR_HANDLE m_raytracingOutputResourceUAVGpuDescriptor;
	UINT m_raytracingOutputResourceUAVDescriptorHeapIndex = UINT_MAX;

	// Shader tables
	ID3D12Resource* m_pMissShaderTable = nullptr;
	ID3D12Resource* m_pHitGroupShaderTable = nullptr;
	ID3D12Resource* m_pRayGenShaderTable = nullptr;

	// Application state
	StepTimer m_Timer;
	float m_CurRotationAngleRad = 0.0f;
	XMVECTOR m_Eye;
	XMVECTOR m_At;
	XMVECTOR m_Up;
};
