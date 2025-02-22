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

#include "framework.h"
#include "D3D12RaytracingInOneWeekend.h"
#include "DirectXRaytracingHelper.h"
#include "CompiledShaders/Raytracing.hlsl.h"

#include <random>

using namespace std;
using namespace DX;

const WCHAR* D3D12RaytracingInOneWeekend::HIT_GROUP_NAME = L"MyHitGroup";
const WCHAR* D3D12RaytracingInOneWeekend::RAYGEN_SHADER_NAME = L"MyRaygenShader";
const WCHAR* D3D12RaytracingInOneWeekend::CLOSEST_HIT_SHADER_NAME = L"MyClosestHitShader";
const WCHAR* D3D12RaytracingInOneWeekend::MISS_SHADER_NAME = L"MyMissShader";

namespace
{
	// FROM BOOK: 3D Game Programming With DX12 - amazing one, worth to check
	std::pair<std::vector<Vertex>, std::vector<UINT32>> CreateSphere(float radius, UINT32 sliceCount, UINT32 stackCount)
	{
		Vertex topVertex = { { 0.0f, +radius, 0.0f }, 0.0, { 0.0f, 1.0f, 0.0f }, 0.0 };

		std::vector<Vertex> vertices;
		vertices.push_back(topVertex);

		float phiStep = XM_PI / stackCount;
		float thetaStep = 2.0f * XM_PI / sliceCount;

		// Compute vertices for each stack ring (do not count the poles as rings).
		for (UINT32 i = 1; i <= stackCount - 1; ++i)
		{
			float phi = i * phiStep;

			// Vertices of ring.
			for (UINT32 j = 0; j <= sliceCount; ++j)
			{
				float theta = j * thetaStep;

				Vertex v;
				// spherical to cartesian
				v.Position.x = radius * sinf(phi) * cosf(theta);
				v.Position.y = radius * cosf(phi);
				v.Position.z = radius * sinf(phi) * sinf(theta);

				XMVECTOR p = XMLoadFloat3(&v.Position);
				XMStoreFloat3(&v.Normal, XMVector3Normalize(p));

				vertices.push_back(v);
			}
		}

		Vertex bottomVertex = { { 0.0f, -radius, 0.0f }, 0.0, { 0.0f, -1.0f, 0.0f }, 0.0 };
		vertices.push_back(bottomVertex);

		std::vector<UINT32> indices;

		//
		// Compute indices for top stack.  The top stack was written first to the vertex buffer
		// and connects the top pole to the first ring.
		//
		for (UINT32 i = 1; i <= sliceCount; ++i)
		{
			indices.push_back(0);
			indices.push_back(i + 1);
			indices.push_back(i);
		}

		//
		// Compute indices for inner stacks (not connected to poles).
		//
		UINT32 baseIndex = 1;
		UINT32 ringVertexCount = sliceCount + 1;
		for (UINT32 i = 0; i < stackCount - 2; ++i)
		{
			for (UINT32 j = 0; j < sliceCount; ++j)
			{
				indices.push_back(baseIndex + i * ringVertexCount + j);
				indices.push_back(baseIndex + i * ringVertexCount + j + 1);
				indices.push_back(baseIndex + (i + 1) * ringVertexCount + j);

				indices.push_back(baseIndex + (i + 1) * ringVertexCount + j);
				indices.push_back(baseIndex + i * ringVertexCount + j + 1);
				indices.push_back(baseIndex + (i + 1) * ringVertexCount + j + 1);
			}
		}

		//
		// Compute indices for bottom stack.  The bottom stack was written last to the vertex buffer
		// and connects the bottom pole to the bottom ring.
		//

		// South pole vertex was added last.
		UINT32 southPoleIndex = (UINT32)vertices.size() - 1;

		// Offset the indices to the index of the first vertex in the last ring.
		baseIndex = southPoleIndex - ringVertexCount;

		for (UINT32 i = 0; i < sliceCount; ++i)
		{
			indices.push_back(southPoleIndex);
			indices.push_back(baseIndex + i);
			indices.push_back(baseIndex + i + 1);
		}

		return { vertices, indices };
	}
}

D3D12RaytracingInOneWeekend::D3D12RaytracingInOneWeekend(UINT width, UINT height, const WCHAR* pszName) : DXSample(width, height, pszName)
{
	UpdateForSizeChange(width, height);
}

void D3D12RaytracingInOneWeekend::OnInit()
{
	m_pDeviceResources = new DeviceResources(
		DXGI_FORMAT_R8G8B8A8_UNORM,
		DXGI_FORMAT_UNKNOWN,
		FRAME_COUNT,
		D3D_FEATURE_LEVEL_11_0,
		// Sample shows handling of use cases with tearing support, which is OS dependent and has been supported since TH2.
		// Since the sample requires build 1809 (RS5) or higher, we don't need to handle non-tearing cases.
		DeviceResources::REQUIRE_TEARING_SUPPORT,
		m_AdapterIDoverride
	);
	m_pDeviceResources->RegisterDeviceNotify(this);
	m_pDeviceResources->SetWindow(Win32Application::GetHwnd(), m_Width, m_Height);
	m_pDeviceResources->InitializeDXGIAdapter();

	ThrowIfFalse(IsDirectXRaytracingSupported(m_pDeviceResources->GetAdapter()),
				 L"ERROR: DirectX Raytracing is not supported by your OS, GPU and/or driver.\n\n");

	m_pDeviceResources->CreateDeviceResources();
	m_pDeviceResources->CreateWindowSizeDependentResources();

	InitializeScene();

	CreateDeviceDependentResources();
	CreateWindowSizeDependentResources();
}

// Update camera matrices passed into the shader.
void D3D12RaytracingInOneWeekend::UpdateCameraMatrices()
{
	UINT frameIndex = m_pDeviceResources->GetCurrentFrameIndex();

	m_FrameCB[frameIndex].CameraPosition = m_Eye;
	float fovAngleY = 20.0f;
	XMMATRIX view = XMMatrixLookAtRH(m_Eye, m_At, m_Up);
	XMMATRIX proj = XMMatrixPerspectiveFovRH(XMConvertToRadians(fovAngleY), m_AspectRatio, 0.1f, 10000.0f);
	XMMATRIX viewProj = view * proj;

	m_FrameCB[frameIndex].ProjectionToWorld = XMMatrixInverse(nullptr, proj);
	m_FrameCB[frameIndex].ModelViewInverse = XMMatrixInverse(nullptr, view);
}

// Initialize scene rendering parameters.
void D3D12RaytracingInOneWeekend::InitializeScene()
{
	UINT frameIndex = m_pDeviceResources->GetCurrentFrameIndex();

	// Setup camera.
	{
		// Initialize the view and projection inverse matrices.
		m_Eye = { 13.0f, 2.0f, 3.0f, 1.0f };
		m_At = { 0.0f, 0.0f, 0.0f, 1.0f };
		m_Up = { 0.0f, 1.0f, 0.0f, 1.0f };

		UpdateCameraMatrices();
	}

	// Apply the initial values to all frames' buffer instances.
	for (FrameBuffer& sceneCB : m_FrameCB)
	{
		sceneCB = m_FrameCB[frameIndex];
	}
}

// Create constant buffers.
void D3D12RaytracingInOneWeekend::CreateConstantBuffers()
{
	ID3D12Device* pDevice = m_pDeviceResources->GetD3DDevice();
	UINT frameCount = m_pDeviceResources->GetBackBufferCount();

	// Create the constant buffer memory and map the CPU and GPU addresses
	const D3D12_HEAP_PROPERTIES UPLOAD_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

	// Allocate one constant buffer per frame, since it gets updated every frame.
	SIZE_T cbSize = frameCount * sizeof(AlignedSceneConstantBuffer);
	const D3D12_RESOURCE_DESC CONSTANT_BUFFER_DESC = CD3DX12_RESOURCE_DESC::Buffer(cbSize);

	ThrowIfFailed(pDevice->CreateCommittedResource(
		&UPLOAD_HEAP_PROPERTIES,
		D3D12_HEAP_FLAG_NONE,
		&CONSTANT_BUFFER_DESC,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_pPerFrameConstants)));

	// Map the constant buffer and cache its heap pointers.
	// We don't unmap this until the app closes. Keeping buffer mapped for the lifetime of the resource is okay.
	CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
	ThrowIfFailed(m_pPerFrameConstants->Map(0, nullptr, (void**)&m_pMappedConstantData));
}

// Create resources that depend on the device.
void D3D12RaytracingInOneWeekend::CreateDeviceDependentResources()
{
	// Initialize raytracing pipeline.

	// Create raytracing interfaces: raytracing device and commandlist.
	CreateRaytracingInterfaces();

	// Create root signatures for the shaders.
	CreateRootSignatures();

	// Create a raytracing pipeline state object which defines the binding of shaders, state and resources to be used during raytracing.
	CreateRaytracingPipelineStateObject();

	// Create a heap for descriptors.
	CreateDescriptorHeap();

	// Build geometry to be used in the sample.
	BuildGeometry();

	// Build raytracing acceleration structures from the generated geometry.
	BuildAccelerationStructures();


	// Create constant buffers for the geometry and the scene.
	CreateConstantBuffers();

	// Build shader tables, which define shaders and their local root arguments.
	BuildShaderTables();

	// Create an output 2D texture to store the raytracing result to.
	CreateRaytracingOutputResource();
}

void D3D12RaytracingInOneWeekend::SerializeAndCreateRaytracingRootSignature(D3D12_ROOT_SIGNATURE_DESC* pDesc, ID3D12RootSignature** ppRootSig)
{
	ID3D12Device* pDevice = m_pDeviceResources->GetD3DDevice();
	ID3DBlob* pBlob = nullptr;
	ID3DBlob* pError = nullptr;

	if (FAILED(D3D12SerializeRootSignature(pDesc, D3D_ROOT_SIGNATURE_VERSION_1, &pBlob, &pError)))
	{
		if (pError)
		{
			OutputDebugStringA((const char*)pError->GetBufferPointer());
			pError->Release();
		}

		__debugbreak();
	}

	if (FAILED(pDevice->CreateRootSignature(1, pBlob->GetBufferPointer(), pBlob->GetBufferSize(), IID_PPV_ARGS(ppRootSig))))
	{
		__debugbreak();
	}

	pBlob->Release();
}

void D3D12RaytracingInOneWeekend::CreateRootSignatures()
{
	ID3D12Device* pDevice = m_pDeviceResources->GetD3DDevice();

	// Global Root Signature
	// This is a root signature that is shared across all raytracing shaders invoked during a DispatchRays() call.
	{
		CD3DX12_DESCRIPTOR_RANGE ranges[2]; // Perfomance TIP: Order from most frequent to least frequent.
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);  // 1 output texture
		ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 1);  // 2 static index and vertex buffers.

		CD3DX12_ROOT_PARAMETER rootParameters[GlobalRootSignatureParams::Count];
		rootParameters[GlobalRootSignatureParams::OutputViewSlot].InitAsDescriptorTable(1, &ranges[0]);
		rootParameters[GlobalRootSignatureParams::AccelerationStructureSlot].InitAsShaderResourceView(0);
		rootParameters[GlobalRootSignatureParams::SceneConstantSlot].InitAsConstantBufferView(0);
		rootParameters[GlobalRootSignatureParams::VertexBuffersSlot].InitAsDescriptorTable(1, &ranges[1]);
		CD3DX12_ROOT_SIGNATURE_DESC globalRootSignatureDesc(ARRAYSIZE(rootParameters), rootParameters);
		SerializeAndCreateRaytracingRootSignature(&globalRootSignatureDesc, &m_pRaytracingGlobalRootSignature);
	}

	// Local Root Signature
	// This is a root signature that enables a shader to have unique arguments that come from shader tables.
	{
		CD3DX12_ROOT_PARAMETER rootParameters[LocalRootSignatureParams::Count];
		// TODO: remove this 
		rootParameters[LocalRootSignatureParams::MeshBufferSlot].InitAsConstants(sizeof(MeshBuffer), 1);
		CD3DX12_ROOT_SIGNATURE_DESC localRootSignatureDesc(ARRAYSIZE(rootParameters), rootParameters);
		localRootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
		SerializeAndCreateRaytracingRootSignature(&localRootSignatureDesc, &m_pRaytracingLocalRootSignature);
	}
}

// Create raytracing device and command list.
void D3D12RaytracingInOneWeekend::CreateRaytracingInterfaces()
{
	ID3D12Device* pDevice = m_pDeviceResources->GetD3DDevice();
	ID3D12GraphicsCommandList* pCommandList = m_pDeviceResources->GetCommandList();

	ThrowIfFailed(pDevice->QueryInterface(IID_PPV_ARGS(&m_pDXRDevice)), L"Couldn't get DirectX Raytracing interface for the device.\n");
	ThrowIfFailed(pCommandList->QueryInterface(IID_PPV_ARGS(&m_pDXRCommandList)), L"Couldn't get DirectX Raytracing interface for the command list.\n");
}

// Local root signature and shader association
// This is a root signature that enables a shader to have unique arguments that come from shader tables.
void D3D12RaytracingInOneWeekend::CreateLocalRootSignatureSubobjects(CD3DX12_STATE_OBJECT_DESC* pRaytracingPipeline)
{
	// Ray gen and miss shaders in this sample are not using a local root signature and thus one is not associated with them.

	// Local root signature to be used in a hit group.
	CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT* pLocalRootSignature = pRaytracingPipeline->CreateSubobject<CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
	pLocalRootSignature->SetRootSignature(m_pRaytracingLocalRootSignature);
	// Define explicit shader association for the local root signature. 
	{
		CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT* pRootSignatureAssociation = pRaytracingPipeline->CreateSubobject<CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
		pRootSignatureAssociation->SetSubobjectToAssociate(*pLocalRootSignature);
		pRootSignatureAssociation->AddExport(HIT_GROUP_NAME);
	}
}

// Create a raytracing pipeline state object (RTPSO).
// An RTPSO represents a full set of shaders reachable by a DispatchRays() call,
// with all configuration options resolved, such as local signatures and other state.
void D3D12RaytracingInOneWeekend::CreateRaytracingPipelineStateObject()
{
	// Create 7 subobjects that combine into a RTPSO:
	// Subobjects need to be associated with DXIL exports (i.e. shaders) either by way of default or explicit associations.
	// Default association applies to every exported shader entrypoint that doesn't have any of the same type of subobject associated with it.
	// This simple sample utilizes default shader association except for local root signature subobject
	// which has an explicit association specified purely for demonstration purposes.
	// 1 - DXIL library
	// 1 - Triangle hit group
	// 1 - Shader config
	// 2 - Local root signature and association
	// 1 - Global root signature
	// 1 - Pipeline config
	CD3DX12_STATE_OBJECT_DESC raytracingPipeline(D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE);


	// DXIL library
	// This contains the shaders and their entrypoints for the state object.
	// Since shaders are not considered a subobject, they need to be passed in via DXIL library subobjects.
	CD3DX12_DXIL_LIBRARY_SUBOBJECT* pLib = raytracingPipeline.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
	D3D12_SHADER_BYTECODE libdxil = CD3DX12_SHADER_BYTECODE((void*)g_pRaytracing, ARRAYSIZE(g_pRaytracing));
	pLib->SetDXILLibrary(&libdxil);
	// Define which shader exports to surface from the library.
	// If no shader exports are defined for a DXIL library subobject, all shaders will be surfaced.
	// In this sample, this could be ommited for convenience since the sample uses all shaders in the library. 
	{
		pLib->DefineExport(RAYGEN_SHADER_NAME);
		pLib->DefineExport(CLOSEST_HIT_SHADER_NAME);
		pLib->DefineExport(MISS_SHADER_NAME);
	}

	// Triangle hit group
	// A hit group specifies closest hit, any hit and intersection shaders to be executed when a ray intersects the geometry's triangle/AABB.
	// In this sample, we only use triangle geometry with a closest hit shader, so others are not set.
	CD3DX12_HIT_GROUP_SUBOBJECT* pHitGroup = raytracingPipeline.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
	pHitGroup->SetClosestHitShaderImport(CLOSEST_HIT_SHADER_NAME);
	pHitGroup->SetHitGroupExport(HIT_GROUP_NAME);
	pHitGroup->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);

	// Shader config
	// Defines the maximum sizes in bytes for the ray payload and attribute structure.
	CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT* pShaderConfig = raytracingPipeline.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
	UINT payloadSize = sizeof(XMFLOAT4) + sizeof(UINT) + sizeof(XMFLOAT4);// float4 pixelColor
	UINT attributeSize = sizeof(XMFLOAT2);  // float2 barycentrics
	pShaderConfig->Config(payloadSize, attributeSize);

	// Local root signature and shader association
	// This is a root signature that enables a shader to have unique arguments that come from shader tables.
	CreateLocalRootSignatureSubobjects(&raytracingPipeline);

	// Global root signature
	// This is a root signature that is shared across all raytracing shaders invoked during a DispatchRays() call.
	CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT* pGlobalRootSignature = raytracingPipeline.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
	pGlobalRootSignature->SetRootSignature(m_pRaytracingGlobalRootSignature);

	// Pipeline config
	// Defines the maximum TraceRay() recursion depth.
	CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT* pPipelineConfig = raytracingPipeline.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
	// PERFOMANCE TIP: Set max recursion depth as low as needed 
	// as drivers may apply optimization strategies for low recursion depths.
	UINT maxRecursionDepth = 1; // ~ primary rays only. 
	pPipelineConfig->Config(maxRecursionDepth);

#if _DEBUG
	PrintStateObjectDesc(raytracingPipeline);
#endif

	// Create the state object.
	ThrowIfFailed(m_pDXRDevice->CreateStateObject(raytracingPipeline, IID_PPV_ARGS(&m_pDXRStateObject)), L"Couldn't create DirectX Raytracing state object.\n");
}

// Create 2D output texture for raytracing.
void D3D12RaytracingInOneWeekend::CreateRaytracingOutputResource()
{
	ID3D12Device* pDevice = m_pDeviceResources->GetD3DDevice();
	DXGI_FORMAT backbufferFormat = m_pDeviceResources->GetBackBufferFormat();

	// Create the output resource. The dimensions and format should match the swap-chain.
	CD3DX12_RESOURCE_DESC uavDesc = CD3DX12_RESOURCE_DESC::Tex2D(backbufferFormat, m_Width, m_Height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	CD3DX12_HEAP_PROPERTIES defaultHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
	ThrowIfFailed(pDevice->CreateCommittedResource(
		&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &uavDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&m_pRaytracingOutput)));
	m_pRaytracingOutput->SetName(L"m_pRaytracingOutput");

	D3D12_CPU_DESCRIPTOR_HANDLE uavDescriptorHandle = {};
	m_raytracingOutputResourceUAVDescriptorHeapIndex = AllocateDescriptor(&uavDescriptorHandle, m_raytracingOutputResourceUAVDescriptorHeapIndex);
	
	D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
	UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	pDevice->CreateUnorderedAccessView(m_pRaytracingOutput, nullptr, &UAVDesc, uavDescriptorHandle);
	m_raytracingOutputResourceUAVGpuDescriptor = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_pDescriptorHeap->GetGPUDescriptorHandleForHeapStart(), m_raytracingOutputResourceUAVDescriptorHeapIndex, m_DescriptorSize);
}

void D3D12RaytracingInOneWeekend::CreateDescriptorHeap()
{
	ID3D12Device* pDevice = m_pDeviceResources->GetD3DDevice();

	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {};
	// Allocate a heap for 3 descriptors:
	// 2 - vertex and index buffer SRVs
	// 1 - raytracing output texture SRV
	descriptorHeapDesc.NumDescriptors = 3;
	descriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	descriptorHeapDesc.NodeMask = 0;
	pDevice->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&m_pDescriptorHeap));
	m_pDescriptorHeap->SetName(L"m_pDescriptorHeap");

	m_DescriptorSize = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

inline double random_double(double from, double to)
{
	static std::uniform_real_distribution<double> s_Distribution(from, to);
	static std::mt19937 s_Generator;
	return s_Distribution(s_Generator);
}

inline double random_double()
{
	return random_double(0, 1.0);
}

inline float random_float(float from, float to)
{
	static std::uniform_real_distribution<float> s_Distribution(0.0, 1.0);
	static std::mt19937 s_Generator;
	return s_Distribution(s_Generator);
}

inline float random_float()
{
	return random_float(0.0f, 1.0f);
}

namespace
{
	XMVECTOR randomColor()
	{
		return { random_float(), random_float(), random_float() };
	}
}

// Build geometry used in the sample.
void D3D12RaytracingInOneWeekend::BuildGeometry()
{
	auto device = m_pDeviceResources->GetD3DDevice();

	const int LAMBERTIAN = 0;
	const int METALLIC = 1;
	const int DIELECTRIC = 2;

	const int SLICE_COUNT = 16;
	const int STACK_COUNT = 32;
	{
		std::pair<std::vector<Vertex>, std::vector<UINT32>> sphereGeometry = CreateSphere(1000.0f, SLICE_COUNT, 512);

		Geometry geom;
		geom.Vertices = sphereGeometry.first;
		geom.Indices = sphereGeometry.second;
		geom.Albedo = { 0.5f, 0.5f, 0.5f, 1.0f };
		geom.MaterialID = LAMBERTIAN;
		geom.Transform = XMMatrixTranspose(XMMatrixTranslation(0.0f, -1000.0f, 0.0f));

		m_geometry.push_back(geom);
	}

	{
		for (int a = -11; a < 11; a++)
		{
			for (int b = -11; b < 11; b++)
			{
				double choose_mat = random_double();
				XMVECTOR center = { (float)a + 0.9f * random_float(), 0.2f, (float)b + 0.9f * random_float() };
				XMVECTOR length = XMVector3Length(XMVectorSubtract(center, { 4.0f, 0.2f, 0.0f }));
				if (XMVectorGetX(length) > 0.9f)
				{
					XMVECTOR materialParameter;
					int materialType = 0;

					if (choose_mat < 0.8)
					{
						// diffuse
						materialType = LAMBERTIAN;
						XMVECTOR albedo = randomColor() * randomColor();
						materialParameter = albedo;
					}
					else if (choose_mat < 0.95)
					{
						// metal
						materialType = METALLIC;
						float fuzz = random_float(0, 0.5);
						materialParameter = { random_float(0.5f, 1.0f), random_float(0.5f, 1.0f), random_float(0.5f, 1.0f), fuzz };
					}
					else
					{
						// glass
						materialType = DIELECTRIC;
						materialParameter = { 1.5, 1.5, 1.5, 1.5 };
					}

					std::pair<std::vector<Vertex>, std::vector<UINT32>> sphereGeometry = CreateSphere(0.2f, SLICE_COUNT, STACK_COUNT);
					XMFLOAT4 float4Material;
					XMStoreFloat4(&float4Material, materialParameter);

					Geometry geom;
					geom.Vertices = sphereGeometry.first;
					geom.Indices = sphereGeometry.second;
					geom.Albedo = float4Material;
					geom.MaterialID = materialType;
					geom.Transform = XMMatrixTranspose(XMMatrixTranslationFromVector(center));
					m_geometry.push_back(geom);
				}
			}
		}
		{
			std::pair<std::vector<Vertex>, std::vector<UINT32>> sphereGeometry = CreateSphere(1.0f, SLICE_COUNT, STACK_COUNT);

			Geometry geom;
			geom.Vertices = sphereGeometry.first;
			geom.Indices = sphereGeometry.second;
			geom.Albedo = { 1.5f, 1.5f, 1.5f, 1.5f };
			geom.MaterialID = DIELECTRIC;
			geom.Transform = XMMatrixTranspose(XMMatrixTranslation(0.0f, 1.0f, 0.0f));
			m_geometry.push_back(geom);
		}

		{
			std::pair<std::vector<Vertex>, std::vector<UINT32>> sphereGeometry = CreateSphere(1.0f, SLICE_COUNT, STACK_COUNT);

			Geometry geom;
			geom.Vertices = sphereGeometry.first;
			geom.Indices = sphereGeometry.second;
			geom.Albedo = { 0.4f, 0.2f, 0.1f, 0.0f };
			geom.MaterialID = LAMBERTIAN;
			geom.Transform = XMMatrixTranspose(XMMatrixTranslation(-4.0f, 1.0f, 0.0f));
			m_geometry.push_back(geom);
		}

		{
			std::pair<std::vector<Vertex>, std::vector<UINT32>> sphereGeometry = CreateSphere(1.0f, SLICE_COUNT, STACK_COUNT);

			Geometry geom;
			geom.Vertices = sphereGeometry.first;
			geom.Indices = sphereGeometry.second;
			geom.Albedo = { 0.7f, 0.6f, 0.5f, 0.0f };
			geom.MaterialID = METALLIC;
			geom.Transform = XMMatrixTranspose(XMMatrixTranslation(4.0f, 1.0f, 0.0f));
			m_geometry.push_back(geom);
		}
	}


	SIZE_T totalNumIndices = 0;
	SIZE_T totalNumVertices = 0;

	for (Geometry const& geometry : m_geometry)
	{
		totalNumIndices += geometry.Indices.size();
		totalNumVertices += geometry.Vertices.size();
	}

	std::vector<UINT32> indices(totalNumIndices);
	std::vector<Vertex> vertices(totalNumVertices);
	SIZE_T vertexOffset = 0;
	SIZE_T indexOffset = 0;
	for (auto& geom : m_geometry)
	{
		memcpy(&vertices[vertexOffset], geom.Vertices.data(), geom.Vertices.size() * sizeof(geom.Vertices[0]));
		memcpy(indices.data() + indexOffset, geom.Indices.data(), geom.Indices.size() * sizeof(geom.Indices[0]));
		geom.IndicesOffsetInBytes = indexOffset * sizeof(geom.Indices[0]);
		geom.VerticesOffsetInBytes = vertexOffset * sizeof(geom.Vertices[0]);

		vertexOffset += geom.Vertices.size();
		indexOffset += geom.Indices.size();
	}
	AllocateUploadBuffer(device, vertices.data(), vertices.size() * sizeof(Vertex), &m_VertexBuffer.pResource);
	AllocateUploadBuffer(device, indices.data(), indices.size() * sizeof(UINT), &m_IndexBuffer.pResource);


	// Vertex buffer is passed to the shader along with index buffer as a descriptor table.
	// Vertex buffer descriptor must follow index buffer descriptor in the descriptor heap.
	UINT descriptorIndexIB = CreateBufferSRV(&m_IndexBuffer, (UINT)indices.size(), sizeof(INT));
	UINT descriptorIndexVB = CreateBufferSRV(&m_VertexBuffer, (UINT)vertices.size(), sizeof(vertices[0]));
	ThrowIfFalse(descriptorIndexVB == descriptorIndexIB + 1, L"Vertex Buffer descriptor index must follow that of Index Buffer descriptor index!");
}

// Build acceleration structures needed for raytracing.
void D3D12RaytracingInOneWeekend::BuildAccelerationStructures()
{
	ID3D12Device* pDevice = m_pDeviceResources->GetD3DDevice();
	ID3D12GraphicsCommandList* pCommandList = m_pDeviceResources->GetCommandList();
	ID3D12CommandQueue* pCommandQueue = m_pDeviceResources->GetCommandQueue();
	ID3D12CommandAllocator* pCommandAllocator = m_pDeviceResources->GetCommandAllocator();

	std::vector<ID3D12Resource*> scratches; // just to expand lifetime
	std::vector<D3D12_RAYTRACING_INSTANCE_DESC> instanceDescriptors;
	std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geometryDescs;

	// Reset the command list for the acceleration structure construction.
	pCommandList->Reset(pCommandAllocator, nullptr);

	int i = 0;
	for (Geometry& geometry : m_geometry)
	{
		ID3D12Resource*& m_bottomLevelAccelerationStructure = geometry.pBottomLevelAccelerationStructure;

		D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc = {};
		geometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
		geometryDesc.Triangles.VertexBuffer.StartAddress = m_VertexBuffer.pResource->GetGPUVirtualAddress() + geometry.VerticesOffsetInBytes;
		geometryDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(Vertex);
		geometryDesc.Triangles.VertexCount = (UINT)geometry.Vertices.size();
		geometryDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;

		geometryDesc.Triangles.IndexBuffer = m_IndexBuffer.pResource->GetGPUVirtualAddress() + geometry.IndicesOffsetInBytes;
		geometryDesc.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
		geometryDesc.Triangles.IndexCount = (UINT)geometry.Indices.size();

		geometryDescs.push_back(geometryDesc);

		// Get required sizes for an acceleration structure.
		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_MINIMIZE_MEMORY;

		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC bottomLevelBuildDesc = {};
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& bottomLevelInputs = bottomLevelBuildDesc.Inputs;
		bottomLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		bottomLevelInputs.Flags = buildFlags;
		bottomLevelInputs.NumDescs = 1;
		bottomLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
		bottomLevelInputs.pGeometryDescs = &geometryDesc;

		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO bottomLevelPrebuildInfo = {};
		m_pDXRDevice->GetRaytracingAccelerationStructurePrebuildInfo(&bottomLevelInputs, &bottomLevelPrebuildInfo);
		ThrowIfFalse(bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes > 0);

		D3D12_RESOURCE_STATES initialResourceState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
		AllocateUAVBuffer(pDevice, bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes, &m_bottomLevelAccelerationStructure, initialResourceState, L"BottomLevelAccelerationStructure");

		D3D12_RAYTRACING_INSTANCE_DESC instanceDesc = {};
		memcpy(instanceDesc.Transform, geometry.Transform.r, 12 * sizeof(float));
		instanceDesc.InstanceMask = 1;
		instanceDesc.InstanceID = i;
		instanceDesc.InstanceContributionToHitGroupIndex = i;
		instanceDesc.AccelerationStructure = m_bottomLevelAccelerationStructure->GetGPUVirtualAddress();

		instanceDescriptors.push_back(instanceDesc);

		ID3D12Resource* pScratchResource = nullptr;
		AllocateUAVBuffer(pDevice, bottomLevelPrebuildInfo.ScratchDataSizeInBytes, &pScratchResource, D3D12_RESOURCE_STATE_COMMON, L"ScratchResource");
		scratches.push_back(pScratchResource); // extend up to execute

		// Bottom Level Acceleration Structure desc
		{
			bottomLevelBuildDesc.ScratchAccelerationStructureData = pScratchResource->GetGPUVirtualAddress();
			bottomLevelBuildDesc.DestAccelerationStructureData = m_bottomLevelAccelerationStructure->GetGPUVirtualAddress();
		}

		// Build acceleration structure.
		m_pDXRCommandList->BuildRaytracingAccelerationStructure(&bottomLevelBuildDesc, 0, nullptr);
		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::UAV(m_bottomLevelAccelerationStructure);
		pCommandList->ResourceBarrier(1, &barrier);

		++i;
	}

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC topLevelBuildDesc = {};
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& topLevelInputs = topLevelBuildDesc.Inputs;
	topLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	topLevelInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
	topLevelInputs.NumDescs = (UINT)geometryDescs.size();
	topLevelInputs.pGeometryDescs = geometryDescs.data();
	topLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO topLevelPrebuildInfo = {};
	m_pDXRDevice->GetRaytracingAccelerationStructurePrebuildInfo(&topLevelInputs, &topLevelPrebuildInfo);
	ThrowIfFalse(topLevelPrebuildInfo.ResultDataMaxSizeInBytes > 0);


	ID3D12Resource* pScratchResource = nullptr;

	D3D12_RESOURCE_STATES initialResourceState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
	AllocateUAVBuffer(pDevice, topLevelPrebuildInfo.ScratchDataSizeInBytes, &pScratchResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"ScratchResource");
	AllocateUAVBuffer(pDevice, topLevelPrebuildInfo.ResultDataMaxSizeInBytes, &m_pTopLevelAccelerationStructure, initialResourceState, L"TopLevelAccelerationStructure");

	// Top Level Acceleration Structure desc
	ID3D12Resource* pInstanceDescs = nullptr;
	AllocateUploadBuffer(m_pDeviceResources->GetD3DDevice(), instanceDescriptors.data(), sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * instanceDescriptors.size(),
						 &pInstanceDescs, L"InstanceDescs");

	topLevelBuildDesc.DestAccelerationStructureData = m_pTopLevelAccelerationStructure->GetGPUVirtualAddress();
	topLevelBuildDesc.ScratchAccelerationStructureData = pScratchResource->GetGPUVirtualAddress();
	topLevelBuildDesc.Inputs.InstanceDescs = pInstanceDescs->GetGPUVirtualAddress();
	
	m_pDXRCommandList->BuildRaytracingAccelerationStructure(&topLevelBuildDesc, 0, nullptr);

	// Kick off acceleration structure construction.
	m_pDeviceResources->ExecuteCommandList();

	// Wait for GPU to finish as the locally created temporary GPU resources will get released once we go out of scope.
	m_pDeviceResources->Fence();
	m_pDeviceResources->WaitForGPU();


	for (SIZE_T i = 0, size = scratches.size(); i < size; ++i)
	{
		scratches[i]->Release();
	}
	pScratchResource->Release();
	pInstanceDescs->Release();
}

// Build shader tables.
// This encapsulates all shader records - shaders and the arguments for their local root signatures.
void D3D12RaytracingInOneWeekend::BuildShaderTables()
{
	ID3D12Device* pDevice = m_pDeviceResources->GetD3DDevice();

	void* pRayGenShaderIdentifier = nullptr;
	void* pMissShaderIdentifier = nullptr;
	void* pHitGroupShaderIdentifier = nullptr;

	// Get shader identifiers.
	UINT shaderIdentifierSize;
	{
		ID3D12StateObjectProperties* pStateObjectProperties = nullptr;
		ThrowIfFailed(m_pDXRStateObject->QueryInterface(IID_PPV_ARGS(&pStateObjectProperties)));

		pRayGenShaderIdentifier = pStateObjectProperties->GetShaderIdentifier(RAYGEN_SHADER_NAME);
		pMissShaderIdentifier = pStateObjectProperties->GetShaderIdentifier(MISS_SHADER_NAME);
		pHitGroupShaderIdentifier = pStateObjectProperties->GetShaderIdentifier(HIT_GROUP_NAME);

		shaderIdentifierSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;

		pStateObjectProperties->Release();
	}

	// Ray gen shader table
	{
		UINT numShaderRecords = 1;
		UINT shaderRecordSize = shaderIdentifierSize;
		ShaderTable rayGenShaderTable(pDevice, numShaderRecords, shaderRecordSize, L"RayGenShaderTable");
		rayGenShaderTable.PushBack(ShaderRecord(pRayGenShaderIdentifier, shaderIdentifierSize));
		m_pRayGenShaderTable = rayGenShaderTable.GetResource();
		m_pRayGenShaderTable->AddRef();
	}

	// Miss shader table
	{
		UINT numShaderRecords = 1;
		UINT shaderRecordSize = shaderIdentifierSize;
		ShaderTable missShaderTable(pDevice, numShaderRecords, shaderRecordSize, L"MissShaderTable");
		missShaderTable.PushBack(ShaderRecord(pMissShaderIdentifier, shaderIdentifierSize));
		m_pMissShaderTable = missShaderTable.GetResource();
		m_pMissShaderTable->AddRef();
	}

	// This is very important moment which we usually can't see in "intro-demos", 
	// I especcialy left this in very simple form so you can track how to work with a few shader table records,
	// we need this toi populate local root arguments in shader
	// Hit group shader table
	{
		UINT verticesOffset = 0;
		UINT indicesOffset = 0;

		std::vector<MeshBuffer> args;

		for (SIZE_T i = 0, size = m_geometry.size(); i < size; ++i)
		{
			MeshBuffer meshBuffer;
			meshBuffer.MeshID = (int)i;
			meshBuffer.MaterialID = m_geometry[i].MaterialID;
			meshBuffer.Albedo = m_geometry[i].Albedo;
			meshBuffer.VerticesOffset = verticesOffset;
			meshBuffer.IndicesOffset = indicesOffset;

			verticesOffset += (UINT)m_geometry[i].Vertices.size();
			indicesOffset += (UINT)m_geometry[i].Indices.size();

			args.push_back(meshBuffer);
		}

		UINT numShaderRecords = (UINT)args.size();
		UINT shaderRecordSize = shaderIdentifierSize + sizeof(MeshBuffer);
		ShaderTable hitGroupShaderTable(pDevice, numShaderRecords, shaderRecordSize, L"HitGroupShaderTable");
		for (MeshBuffer& arg : args)
		{
			hitGroupShaderTable.PushBack(ShaderRecord(pHitGroupShaderIdentifier, shaderIdentifierSize, &arg, sizeof(MeshBuffer)));
		}

		m_pHitGroupShaderTable = hitGroupShaderTable.GetResource();
		m_pHitGroupShaderTable->AddRef();
	}
}

// Update frame-based values.
void D3D12RaytracingInOneWeekend::OnUpdate()
{
	m_Timer.Tick();
	CalculateFrameStats();

	float elapsedTime = (float)m_Timer.GetElapsedSeconds();
	UINT frameIndex = m_pDeviceResources->GetCurrentFrameIndex();
	UINT prevFrameIndex = m_pDeviceResources->GetPreviousFrameIndex();

	// Rotate the camera around Y axis.
	{
		float secondsToRotateAround = 60.0f;
		float angleToRotateBy = 360.0f * (elapsedTime / secondsToRotateAround);
		XMMATRIX rotate = XMMatrixRotationY(XMConvertToRadians(angleToRotateBy));
		m_Eye = XMVector3Transform(m_Eye, rotate);
		m_Up = XMVector3Transform(m_Up, rotate);
		m_At = XMVector3Transform(m_At, rotate);
		UpdateCameraMatrices();
	}
}

void D3D12RaytracingInOneWeekend::DoRaytracing()
{
	ID3D12GraphicsCommandList* pCommandList = m_pDeviceResources->GetCommandList();
	UINT frameIndex = m_pDeviceResources->GetCurrentFrameIndex();

	pCommandList->SetComputeRootSignature(m_pRaytracingGlobalRootSignature);

	// Copy the updated scene constant buffer to GPU.
	memcpy(&m_pMappedConstantData[frameIndex].Constants, &m_FrameCB[frameIndex], sizeof(FrameBuffer));
	ULONGLONG cbGpuAddress = m_pPerFrameConstants->GetGPUVirtualAddress() + frameIndex * sizeof(AlignedSceneConstantBuffer);
	pCommandList->SetComputeRootConstantBufferView(GlobalRootSignatureParams::SceneConstantSlot, cbGpuAddress);

	// Bind the heaps, acceleration structure and dispatch rays.
	pCommandList->SetDescriptorHeaps(1, &m_pDescriptorHeap);
	// Set index and successive vertex buffer decriptor tables
	pCommandList->SetComputeRootDescriptorTable(GlobalRootSignatureParams::VertexBuffersSlot, m_IndexBuffer.GPUDescriptorHandle);
	pCommandList->SetComputeRootDescriptorTable(GlobalRootSignatureParams::OutputViewSlot, m_raytracingOutputResourceUAVGpuDescriptor);
	pCommandList->SetComputeRootShaderResourceView(GlobalRootSignatureParams::AccelerationStructureSlot, m_pTopLevelAccelerationStructure->GetGPUVirtualAddress());

	D3D12_DISPATCH_RAYS_DESC dispatchDesc = {};
	// Since each shader table has only one shader record, the stride is same as the size.
	dispatchDesc.HitGroupTable.StartAddress = m_pHitGroupShaderTable->GetGPUVirtualAddress();
	dispatchDesc.HitGroupTable.SizeInBytes = m_pHitGroupShaderTable->GetDesc().Width;
	dispatchDesc.HitGroupTable.StrideInBytes = dispatchDesc.HitGroupTable.SizeInBytes / m_geometry.size();
	dispatchDesc.MissShaderTable.StartAddress = m_pMissShaderTable->GetGPUVirtualAddress();
	dispatchDesc.MissShaderTable.SizeInBytes = m_pMissShaderTable->GetDesc().Width;
	dispatchDesc.MissShaderTable.StrideInBytes = dispatchDesc.MissShaderTable.SizeInBytes;
	dispatchDesc.RayGenerationShaderRecord.StartAddress = m_pRayGenShaderTable->GetGPUVirtualAddress();
	dispatchDesc.RayGenerationShaderRecord.SizeInBytes = m_pRayGenShaderTable->GetDesc().Width;
	dispatchDesc.Width = m_Width;
	dispatchDesc.Height = m_Height;
	dispatchDesc.Depth = 1;
	m_pDXRCommandList->SetPipelineState1(m_pDXRStateObject);
	m_pDXRCommandList->DispatchRays(&dispatchDesc);
}

// Update the application state with the new resolution.
void D3D12RaytracingInOneWeekend::UpdateForSizeChange(UINT width, UINT height)
{
	DXSample::UpdateForSizeChange(width, height);
}

// Copy the raytracing output to the backbuffer.
void D3D12RaytracingInOneWeekend::CopyRaytracingOutputToBackbuffer()
{
	ID3D12GraphicsCommandList* pCommandList = m_pDeviceResources->GetCommandList();
	ID3D12Resource* pRenderTarget = m_pDeviceResources->GetRenderTarget();

	D3D12_RESOURCE_BARRIER preCopyBarriers[2];
	preCopyBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(pRenderTarget, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST);
	preCopyBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(m_pRaytracingOutput, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
	pCommandList->ResourceBarrier(ARRAYSIZE(preCopyBarriers), preCopyBarriers);

	pCommandList->CopyResource(pRenderTarget, m_pRaytracingOutput);

	D3D12_RESOURCE_BARRIER postCopyBarriers[2];
	postCopyBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(pRenderTarget, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT);
	postCopyBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(m_pRaytracingOutput, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	pCommandList->ResourceBarrier(ARRAYSIZE(postCopyBarriers), postCopyBarriers);
}

// Create resources that are dependent on the size of the main window.
void D3D12RaytracingInOneWeekend::CreateWindowSizeDependentResources()
{
	CreateRaytracingOutputResource();
	UpdateCameraMatrices();
}

// Release resources that are dependent on the size of the main window.
void D3D12RaytracingInOneWeekend::ReleaseWindowSizeDependentResources()
{
	if (m_pRaytracingOutput)
	{
		ULONG ref = m_pRaytracingOutput->Release();
		m_pRaytracingOutput = nullptr;
	}
	//m_pRaytracingOutput.Reset();
}

// Release all resources that depend on the device.
void D3D12RaytracingInOneWeekend::ReleaseDeviceDependentResources()
{
	for (auto& geometry : m_geometry)
	{
		if (geometry.pBottomLevelAccelerationStructure)
		{
			geometry.pBottomLevelAccelerationStructure->Release();
			geometry.pBottomLevelAccelerationStructure = nullptr;
		}
	}

	if (m_pRaytracingGlobalRootSignature)
	{
		m_pRaytracingGlobalRootSignature->Release();
		m_pRaytracingGlobalRootSignature = nullptr;
	}
	if (m_pRaytracingLocalRootSignature)
	{
		m_pRaytracingLocalRootSignature->Release();
		m_pRaytracingLocalRootSignature = nullptr;
	}

	if (m_pDescriptorHeap)
	{
		m_pDescriptorHeap->Release();
		m_pDescriptorHeap = nullptr;
	}
	m_DescriptorsAllocated = 0;
	m_raytracingOutputResourceUAVDescriptorHeapIndex = UINT_MAX;
	if (m_pPerFrameConstants)
	{
		m_pPerFrameConstants->Unmap(0, nullptr);
		m_pMappedConstantData = nullptr;

		m_pPerFrameConstants->Release();
		m_pPerFrameConstants = nullptr;
	}
	if (m_pRayGenShaderTable)
	{
		m_pRayGenShaderTable->Release();
		m_pRayGenShaderTable = nullptr;
	}
	if (m_pMissShaderTable)
	{
		m_pMissShaderTable->Release();
		m_pMissShaderTable = nullptr;
	}
	if (m_pHitGroupShaderTable)
	{
		m_pHitGroupShaderTable->Release();
		m_pHitGroupShaderTable = nullptr;
	}
	if (m_IndexBuffer.pResource)
	{
		m_IndexBuffer.pResource->Release();
		m_IndexBuffer.pResource = nullptr;
	}
	if (m_VertexBuffer.pResource)
	{
		m_VertexBuffer.pResource->Release();
		m_VertexBuffer.pResource = nullptr;
	}
	if (m_pTopLevelAccelerationStructure)
	{
		m_pTopLevelAccelerationStructure->Release();
		m_pTopLevelAccelerationStructure = nullptr;
	}
	
	if (m_pDXRCommandList)
	{
		m_pDXRCommandList->Release();
		m_pDXRCommandList = nullptr;
	}
	if (m_pDXRStateObject)
	{
		m_pDXRStateObject->Release();
		m_pDXRStateObject = nullptr;
	}
	if (m_pDXRDevice)
	{
		m_pDXRDevice->Release();
		m_pDXRDevice = nullptr;
	}
}

void D3D12RaytracingInOneWeekend::RecreateD3D()
{
	m_pDeviceResources->WaitForGPU(); 
	m_pDeviceResources->HandleDeviceLost();
}

// Render the scene.
void D3D12RaytracingInOneWeekend::OnRender()
{
	if (!m_pDeviceResources->IsWindowVisible())
	{
		return;
	}

	m_pDeviceResources->Prepare();
	DoRaytracing();
	CopyRaytracingOutputToBackbuffer();

	m_pDeviceResources->Present(D3D12_RESOURCE_STATE_PRESENT);
}

void D3D12RaytracingInOneWeekend::OnDestroy()
{
	// Let GPU finish before releasing D3D resources.
	m_pDeviceResources->WaitForGPU();
	OnDeviceLost();
}

// Release all device dependent resouces when a device is lost.
void D3D12RaytracingInOneWeekend::OnDeviceLost()
{
	ReleaseWindowSizeDependentResources();
	ReleaseDeviceDependentResources();
}

// Create all device dependent resources when a device is restored.
void D3D12RaytracingInOneWeekend::OnDeviceRestored()
{
	CreateDeviceDependentResources();
	CreateWindowSizeDependentResources();
}

// Compute the average frames per second and million rays per second.
void D3D12RaytracingInOneWeekend::CalculateFrameStats()
{
	static int s_FrameCnt = 0;
	static double s_ElapsedTime = 0.0f;

	double totalTime = m_Timer.GetTotalSeconds();
	++s_FrameCnt;

	// Compute averages over one second period.
	if ((totalTime - s_ElapsedTime) >= 1.0f)
	{
		float diff = (float)(totalTime - s_ElapsedTime);
		float fps = (float)s_FrameCnt / diff; // Normalize to an exact second.

		s_FrameCnt = 0;
		s_ElapsedTime = totalTime;

		float MRaysPerSecond = (m_Width * m_Height * fps) / (float)1e6;

		wstringstream windowText;
		windowText << setprecision(2) << fixed
			<< L"    fps: " << fps << L"     ~Million Primary Rays/s: " << MRaysPerSecond
			<< L"    GPU[" << m_pDeviceResources->GetAdapterID() << L"]: " << m_pDeviceResources->GetAdapterDescription();
		SetCustomWindowText(windowText.str().c_str());
	}
}

// Handle OnSizeChanged message event.
void D3D12RaytracingInOneWeekend::OnSizeChanged(UINT width, UINT height, bool bMinimized)
{
	if (!m_pDeviceResources->WindowSizeChanged(width, height, bMinimized))
	{
		return;
	}

	UpdateForSizeChange(width, height);

	ReleaseWindowSizeDependentResources();
	CreateWindowSizeDependentResources();
}

// Allocate a descriptor and return its index. 
// If the passed descriptorIndexToUse is valid, it will be used instead of allocating a new one.
UINT D3D12RaytracingInOneWeekend::AllocateDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE* pCPUDescriptor, UINT descriptorIndexToUse)
{
	D3D12_CPU_DESCRIPTOR_HANDLE descriptorHeapCpuBase = m_pDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	if (descriptorIndexToUse >= m_pDescriptorHeap->GetDesc().NumDescriptors)
	{
		descriptorIndexToUse = m_DescriptorsAllocated++;
	}
	*pCPUDescriptor = CD3DX12_CPU_DESCRIPTOR_HANDLE(descriptorHeapCpuBase, descriptorIndexToUse, m_DescriptorSize);
	return descriptorIndexToUse;
}

// Create SRV for a buffer.
UINT D3D12RaytracingInOneWeekend::CreateBufferSRV(D3DBuffer* pBuffer, UINT numElements, UINT elementSize)
{
	ID3D12Device* pDevice = m_pDeviceResources->GetD3DDevice();

	// SRV
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Buffer.NumElements = numElements;
	if (elementSize == 0)
	{
		srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
		srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
		srvDesc.Buffer.StructureByteStride = 0;
	}
	else
	{
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
		srvDesc.Buffer.StructureByteStride = elementSize;
	}

	UINT descriptorIndex = AllocateDescriptor(&pBuffer->CPUDescriptorHandle);
	pDevice->CreateShaderResourceView(pBuffer->pResource, &srvDesc, pBuffer->CPUDescriptorHandle);
	pBuffer->GPUDescriptorHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_pDescriptorHeap->GetGPUDescriptorHandleForHeapStart(), descriptorIndex, m_DescriptorSize);
	return descriptorIndex;
}