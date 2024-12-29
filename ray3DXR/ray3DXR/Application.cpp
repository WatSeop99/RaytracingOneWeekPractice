#include "framework.h"
#include "Application.h"

Application::~Application()
{}

void Application::OnLoad()
{
	InitDXR();
	CreateAccelerationStructures();
	CreateRTPipelineState();
	CreateShaderResources();
	CreateShaderTable();
}

void Application::OnFrameRender()
{}

void Application::OnShutdown()
{
	++m_FenceValue;

	m_pCommandQueue->Signal(m_pFence, m_FenceValue);
	m_pFence->SetEventOnCompletion(m_FenceValue, m_hFenceEvent);
	WaitForSingleObject(m_hFenceEvent, INFINITE);
}

void Application::InitDXR()
{
#ifdef _DEBUG
	ID3D12Debug* pDebug = nullptr;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&pDebug))))
	{
		pDebug->EnableDebugLayer();
		pDebug->Release();
		pDebug = nullptr;
	}
#endif

	HRESULT hr;
	
	IDXGIFactory4* pDXGIFactory = nullptr;
	hr = CreateDXGIFactory1(IID_PPV_ARGS(&pDXGIFactory));
	if (FAILED(hr))
	{
		__debugbreak();
	}
	
	m_pDevice = CreateDevice(pDXGIFactory);
	if (!m_pDevice)
	{
		__debugbreak();
	}
	
	m_pCommandQueue = CreateCommandQueue(m_pDevice);
	if (!m_pCommandQueue)
	{
		__debugbreak();
	}

	m_pSwapChain = CreateSwapChain(pDXGIFactory, m_hwnd, m_Width, m_Height,  DXGI_FORMAT_R8G8B8A8_UNORM, m_pCommandQueue);
	if (!m_pSwapChain)
	{
		__debugbreak();
	}

	m_RTVHeap.pHeap;

	for (UINT i = 0, size = _countof(m_FrameObjects); i < size; ++i)
	{
		hr = m_pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_FrameObjects[i].pCommandAllocator));
		if (FAILED(hr))
		{
			__debugbreak();
		}

		hr = m_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&m_FrameObjects[i].pSwapChainBuffer));
		if (FAILED(hr))
		{
			__debugbreak();
		}

		m_FrameObjects[i].RTVHandle = CreateRTV(m_pDevice, m_FrameObjects[i].pSwapChainBuffer, m_RTVHeap.pHeap, &m_RTVHeap.UsedEntries, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
	}

	hr = m_pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_FrameObjects[0].pCommandAllocator, nullptr, IID_PPV_ARGS(&m_pCommandList));
	if (FAILED(hr))
	{
		__debugbreak();
	}

	hr = m_pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_pFence));
	if (FAILED(hr))
	{
		__debugbreak();
	}

	m_hFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (!m_hFenceEvent || m_hFenceEvent == INVALID_HANDLE_VALUE)
	{
		__debugbreak();
	}
}

UINT Application::BeginFrame()
{
	ID3D12DescriptorHeap* ppHeaps[] = { m_pCBVSRVUAVHeap };
	m_pCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	return m_pSwapChain->GetCurrentBackBufferIndex();
}

void Application::EndFrame()
{
	/// Change swap chain state.
	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_FrameObjects[m_FrameIndex].pSwapChainBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT);
	m_pCommandList->ResourceBarrier(1, &barrier);


	/// Execute queue.
	m_pCommandList->Close();

	ID3D12CommandList* ppCommandLists[] = { m_pCommandList };
	m_pCommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
	++m_FenceValue;
	m_pCommandQueue->Signal(m_pFence, m_FenceValue);

	
	/// Swap buffer.
	m_pSwapChain->Present(0, 0);


	/// Change frame index.
	m_FrameIndex = m_pSwapChain->GetCurrentBackBufferIndex();


	/// Wait previous frame.
	if (m_FenceValue > SWAP_CHAIN_BUFFER)
	{
		m_pFence->SetEventOnCompletion(m_FenceValue - SWAP_CHAIN_BUFFER + 1, m_hFenceEvent);
		WaitForSingleObject(m_hFenceEvent, INFINITE);
	}

	/// Reset command lists.
	m_FrameObjects[m_FrameIndex].pCommandAllocator->Reset();
	m_pCommandList->Reset(m_FrameObjects[m_FrameIndex].pCommandAllocator, nullptr);
}

void Application::CreateAccelerationStructures()
{
	m_pVertexBuffer = CreateTriangleVB(m_pDevice);
	if (!m_pVertexBuffer)
	{
		__debugbreak();
	}

	AccelerationStructureBuffers bottomLevelBuffers = CreateBottomLevelAS(m_pDevice, m_pCommandList, m_pVertexBuffer);
	AccelerationStructureBuffers topLevelBuffers = CreateTopLevelAS(m_pDevice, m_pCommandList, bottomLevelBuffers.pResult, &m_TlasSize);

	// The tutorial doesn't have any resource lifetime management, so we flush and sync here. This is not required by the DXR spec - you can submit the list whenever you like as long as you take care of the resources lifetime.
	m_FenceValue = SubmitCommandList(m_pCommandList, m_pCommandQueue, m_pFence, m_FenceValue);
	m_pFence->SetEventOnCompletion(m_FenceValue, m_hFenceEvent);
	WaitForSingleObject(m_hFenceEvent, INFINITE);

	UINT bufferIndex = m_pSwapChain->GetCurrentBackBufferIndex();
	m_pCommandList->Reset(m_FrameObjects[0].pCommandAllocator, nullptr);

	// Store the AS buffers. The rest of the buffers will be released once we exit the function
	m_pTopLevelAS = topLevelBuffers.pResult;
	m_pBottomLevelAS = bottomLevelBuffers.pResult;
}

void Application::CreateRTPipelineState()
{
	// Need 10 subobjects:
	//  1 for the DXIL library
	//  1 for hit-group
	//  2 for RayGen root-signature (root-signature and the subobject association)
	//  2 for the root-signature shared between miss and hit shaders (signature and association)
	//  2 for shader config (shared between all programs. 1 for the config, 1 for association)
	//  1 for pipeline config
	//  1 for the global root signature

	D3D12_STATE_SUBOBJECT subObjects[10] = {};
	UINT index = 0;
	HRESULT hr;

	// Create the DXIL library
	DxilLibrary dxilLib = CreateDxilLibrary();
	subobjects[index++] = dxilLib.stateSubobject; // 0 Library

	HitProgram hitProgram(nullptr, kClosestHitShader, kHitGroup);
	subobjects[index++] = hitProgram.subObject; // 1 Hit Group

	// Create the ray-gen root-signature and association
	LocalRootSignature rgsRootSignature(mpDevice, createRayGenRootDesc().desc);
	subobjects[index] = rgsRootSignature.subobject; // 2 RayGen Root Sig

	UINT rgsRootIndex = index++; // 2
	ExportAssociation rgsRootAssociation(&kRayGenShader, 1, &(subobjects[rgsRootIndex]));
	subobjects[index++] = rgsRootAssociation.subobject; // 3 Associate Root Sig to RGS

	// Create the miss- and hit-programs root-signature and association
	D3D12_ROOT_SIGNATURE_DESC emptyDesc = {};
	emptyDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
	LocalRootSignature hitMissRootSignature(mpDevice, emptyDesc);
	subobjects[index] = hitMissRootSignature.subobject; // 4 Root Sig to be shared between Miss and CHS

	UINT hitMissRootIndex = index++; // 4
	const WCHAR* missHitExportName[] = { kMissShader, kClosestHitShader };
	ExportAssociation missHitRootAssociation(missHitExportName, arraysize(missHitExportName), &(subobjects[hitMissRootIndex]));
	subobjects[index++] = missHitRootAssociation.subobject; // 5 Associate Root Sig to Miss and CHS

	// Bind the payload size to the programs
	ShaderConfig shaderConfig(sizeof(float) * 2, sizeof(float) * 3);
	subobjects[index] = shaderConfig.subobject; // 6 Shader Config

	UINT shaderConfigIndex = index++; // 6
	const WCHAR* shaderExports[] = { kMissShader, kClosestHitShader, kRayGenShader };
	ExportAssociation configAssociation(shaderExports, arraysize(shaderExports), &(subobjects[shaderConfigIndex]));
	subobjects[index++] = configAssociation.subobject; // 7 Associate Shader Config to Miss, CHS, RGS

	// Create the pipeline config
	PipelineConfig config(1);
	subobjects[index++] = config.subobject; // 8

	// Create the global root signature and store the empty signature
	GlobalRootSignature root(mpDevice, {});
	m_pEmptyRootSignature = root.pRootSig;
	subobjects[index++] = root.subobject; // 9

	// Create the state
	D3D12_STATE_OBJECT_DESC desc = {};
	desc.NumSubobjects = index; // 10
	desc.pSubobjects = subobjects.data();
	desc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;

	hr = m_pDevice->CreateStateObject(&desc, IID_PPV_ARGS(&m_pPipelineState));
	if (FAILED(hr))
	{
		__debugbreak();
	}
}

void Application::CreateShaderTable()
{}

void Application::CreateShaderResources()
{}

ID3D12Device5* Application::CreateDevice(IDXGIFactory4* pDXGIFactory)
{
	_ASSERT(pDXGIFactory);

	HRESULT hr;

	IDXGIAdapter1* pAdapter = nullptr;

	for (UINT i = 0; pDXGIFactory->EnumAdapters1(i, &pAdapter) != DXGI_ERROR_NOT_FOUND; ++i)
	{
		DXGI_ADAPTER_DESC1 desc = {};
		pAdapter->GetDesc1(&desc);

		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
		{
			pAdapter->Release();
			pAdapter = nullptr;
			continue;
		}

#ifdef _DEBUG
		ID3D12Debug* pDX12Debug = nullptr;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&pDX12Debug))))
		{
			pDX12Debug->EnableDebugLayer();
			pDX12Debug->Release();
			pDX12Debug = nullptr;
		}
#endif

		ID3D12Device5* pDevice = nullptr;
		hr = D3D12CreateDevice(pAdapter, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&pDevice));
		pAdapter->Release();
		pAdapter = nullptr;

		if (FAILED(hr))
		{
			continue;
		}

		D3D12_FEATURE_DATA_D3D12_OPTIONS5 features5 = {};
		hr = pDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &features5, sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS5));
		if (SUCCEEDED(hr) && features5.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED)
		{
			return pDevice;
		}
	}

	return nullptr;
}

ID3D12CommandQueue* Application::CreateCommandQueue(ID3D12Device5* pDevice)
{
	_ASSERT(pDevice);
	
	D3D12_COMMAND_QUEUE_DESC desc = {};
	desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	ID3D12CommandQueue* pQueue = nullptr;
	pDevice->CreateCommandQueue(&desc, IID_PPV_ARGS(&pQueue));

	return pQueue;
}

IDXGISwapChain3* Application::CreateSwapChain(IDXGIFactory4* pFactory, HWND hwnd, UINT width, UINT height, DXGI_FORMAT format, ID3D12CommandQueue* pCommandQueue)
{
	_ASSERT(pFactory);
	_ASSERT(hwnd);
	_ASSERT(width > 0 && height > 0);
	_ASSERT(pCommandQueue);

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.BufferCount = SWAP_CHAIN_BUFFER;
	swapChainDesc.Width = width;
	swapChainDesc.Height = height;
	swapChainDesc.Format = format;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.SampleDesc.Count = 1;

	IDXGISwapChain1* pSwapChain = nullptr;
	HRESULT hr = pFactory->CreateSwapChainForHwnd(pCommandQueue, hwnd, &swapChainDesc, nullptr, nullptr, &pSwapChain);
	if (FAILED(hr))
	{
		return nullptr;
	}

	IDXGISwapChain3* pSwapChain3 = nullptr;
	pSwapChain->QueryInterface(IID_PPV_ARGS(&pSwapChain3));
	pSwapChain->Release();

	return pSwapChain3;
}

ID3D12DescriptorHeap* Application::CreateDescriptorHeap(ID3D12Device5* pDevice, UINT count, D3D12_DESCRIPTOR_HEAP_TYPE type, bool bShaderVisible)
{
	_ASSERT(pDevice);
	_ASSERT(count > 0);

	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.NumDescriptors = count;
	desc.Type = type;
	desc.Flags = (bShaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE);

	ID3D12DescriptorHeap* pHeap = nullptr;
	pDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&pHeap));

	return pHeap;
}

D3D12_CPU_DESCRIPTOR_HANDLE Application::CreateRTV(ID3D12Device5* pDevice, ID3D12Resource* pResource, ID3D12DescriptorHeap* pHeap, UINT* outUsedHeapEntries, DXGI_FORMAT format)
{
	_ASSERT(pDevice);
	_ASSERT(pResource);
	_ASSERT(pHeap);
	_ASSERT(outUsedHeapEntries);

	D3D12_RENDER_TARGET_VIEW_DESC desc = {};
	desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	desc.Format = format;
	desc.Texture2D.MipSlice = 0;

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = pHeap->GetCPUDescriptorHandleForHeapStart();
	rtvHandle.ptr += (*outUsedHeapEntries) * pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	++(*outUsedHeapEntries);

	pDevice->CreateRenderTargetView(pResource, &desc, rtvHandle);
	return rtvHandle;
}

ID3D12Resource* Application::CreateTriangleVB(ID3D12Device5* pDevice)
{
	_ASSERT(pDevice);

	const DirectX::XMFLOAT3 VERTICES[] =
	{
		DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f),
		DirectX::XMFLOAT3(0.866f,  -0.5f, 0.0f),
		DirectX::XMFLOAT3(-0.866f, -0.5f, 0.0f)
	};

	CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD, 0, 0);
	ID3D12Resource* pBuffer = CreateBuffer(pDevice, sizeof(VERTICES), D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, uploadHeapProps);
	if (!pBuffer)
	{
		__debugbreak();
		return nullptr;
	}
	
	UINT8* pData = nullptr;
	pBuffer->Map(0, nullptr, (void**)&pData);
	memcpy(pData, VERTICES, sizeof(VERTICES));
	pBuffer->Unmap(0, nullptr);

	return pBuffer;
}

AccelerationStructureBuffers Application::CreateBottomLevelAS(ID3D12Device5* pDevice, ID3D12GraphicsCommandList4* pCommandList, ID3D12Resource* pVertexBuffer)
{
	_ASSERT(pDevice);
	_ASSERT(pCommandList);
	_ASSERT(pVertexBuffer);

	D3D12_RAYTRACING_GEOMETRY_DESC geomDesc = {};
	geomDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
	geomDesc.Triangles.VertexBuffer.StartAddress = pVertexBuffer->GetGPUVirtualAddress();
	geomDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(DirectX::XMFLOAT3);
	geomDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
	geomDesc.Triangles.VertexCount = 3;
	geomDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

	// Get the size requirements for the scratch and AS buffers
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
	inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
	inputs.NumDescs = 1;
	inputs.pGeometryDescs = &geomDesc;
	inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info = {};
	pDevice->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);

	// Create the buffers. They need to support UAV, and since we are going to immediately use them, we create them with an unordered-access state
	CD3DX12_HEAP_PROPERTIES defaultHeapProps(D3D12_HEAP_TYPE_DEFAULT, 0, 0);
	AccelerationStructureBuffers buffers = { 0, };
	buffers.pScratch = CreateBuffer(pDevice, info.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, defaultHeapProps);
	buffers.pResult = CreateBuffer(pDevice, info.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, defaultHeapProps);

	// Create the bottom-level AS
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC asDesc = {};
	asDesc.Inputs = inputs;
	asDesc.DestAccelerationStructureData = buffers.pResult->GetGPUVirtualAddress();
	asDesc.ScratchAccelerationStructureData = buffers.pScratch->GetGPUVirtualAddress();

	pCommandList->BuildRaytracingAccelerationStructure(&asDesc, 0, nullptr);

	// We need to insert a UAV barrier before using the acceleration structures in a raytracing operation
	D3D12_RESOURCE_BARRIER uavBarrier = {};
	uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	uavBarrier.UAV.pResource = buffers.pResult;
	pCommandList->ResourceBarrier(1, &uavBarrier);

	return buffers;
}

AccelerationStructureBuffers Application::CreateTopLevelAS(ID3D12Device5* pDevice, ID3D12GraphicsCommandList4* pCommandList, ID3D12Resource* pBottomLevelAS, UINT64* pTlasSize)
{
	_ASSERT(pDevice);
	_ASSERT(pCommandList);
	_ASSERT(pBottomLevelAS);
	_ASSERT(pTlasSize);

	// First, get the size of the TLAS buffers and create them
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
	inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
	inputs.NumDescs = 1;
	inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info;
	pDevice->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);

	// Create the buffers
	CD3DX12_HEAP_PROPERTIES defaultHeapProps(D3D12_HEAP_TYPE_DEFAULT, 0, 0);
	AccelerationStructureBuffers buffers = { 0, };
	buffers.pScratch = CreateBuffer(pDevice, info.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, defaultHeapProps);
	buffers.pResult = CreateBuffer(pDevice, info.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, defaultHeapProps);
	*pTlasSize = info.ResultDataMaxSizeInBytes;

	// The instance desc should be inside a buffer, create and map the buffer
	CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD, 0, 0);
	buffers.pInstanceDesc = CreateBuffer(pDevice, sizeof(D3D12_RAYTRACING_INSTANCE_DESC), D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, uploadHeapProps);
	D3D12_RAYTRACING_INSTANCE_DESC* pInstanceDesc = nullptr;
	buffers.pInstanceDesc->Map(0, nullptr, (void**)&pInstanceDesc);

	// Initialize the instance desc. We only have a single instance
	pInstanceDesc->InstanceID = 0;                            // This value will be exposed to the shader via InstanceID()
	pInstanceDesc->InstanceContributionToHitGroupIndex = 0;   // This is the offset inside the shader-table. We only have a single geometry, so the offset 0
	pInstanceDesc->Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
	// Identity matrix
	DirectX::XMFLOAT4X4 matrix = 
	{
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	};
	memcpy(pInstanceDesc->Transform, &matrix, sizeof(pInstanceDesc->Transform));
	pInstanceDesc->AccelerationStructure = pBottomLevelAS->GetGPUVirtualAddress();
	pInstanceDesc->InstanceMask = 0xFF;

	// Unmap
	buffers.pInstanceDesc->Unmap(0, nullptr);

	// Create the TLAS
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC asDesc = {};
	asDesc.Inputs = inputs;
	asDesc.Inputs.InstanceDescs = buffers.pInstanceDesc->GetGPUVirtualAddress();
	asDesc.DestAccelerationStructureData = buffers.pResult->GetGPUVirtualAddress();
	asDesc.ScratchAccelerationStructureData = buffers.pScratch->GetGPUVirtualAddress();

	pCommandList->BuildRaytracingAccelerationStructure(&asDesc, 0, nullptr);

	// We need to insert a UAV barrier before using the acceleration structures in a raytracing operation
	D3D12_RESOURCE_BARRIER uavBarrier = {};
	uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	uavBarrier.UAV.pResource = buffers.pResult;
	pCommandList->ResourceBarrier(1, &uavBarrier);

	return buffers;
}

ID3D12Resource* Application::CreateBuffer(ID3D12Device5* pDevice, UINT64 size, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES initState, const D3D12_HEAP_PROPERTIES& heapProps)
{
	_ASSERT(pDevice);

	D3D12_RESOURCE_DESC bufDesc = {};
	bufDesc.Alignment = 0;
	bufDesc.DepthOrArraySize = 1;
	bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	bufDesc.Flags = flags;
	bufDesc.Format = DXGI_FORMAT_UNKNOWN;
	bufDesc.Height = 1;
	bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	bufDesc.MipLevels = 1;
	bufDesc.SampleDesc.Count = 1;
	bufDesc.SampleDesc.Quality = 0;
	bufDesc.Width = size;

	ID3D12Resource* pBuffer = nullptr;
	pDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &bufDesc, initState, nullptr, IID_PPV_ARGS(&pBuffer));
	return pBuffer;
}

UINT64 Application::SubmitCommandList(ID3D12GraphicsCommandList* pCommandList, ID3D12CommandQueue* pCommandQueue, ID3D12Fence* pFence, UINT64 fenceValue)
{
	_ASSERT(pCommandList);
	_ASSERT(pCommandQueue);
	_ASSERT(pFence);

	pCommandList->Close();

	ID3D12CommandList* ppCommandLists[] = { pCommandList, };
	pCommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	++fenceValue;
	pCommandQueue->Signal(pFence, fenceValue);
	return fenceValue;
}
