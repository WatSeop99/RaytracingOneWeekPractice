#include "framework.h"
#include "CommonDefine.h"
#include "DescriptorAllocator.h"
#include "DxcDLLSupport.h"
#include "ResourceManager.h"
#include "TextureManager.h"
#include "Application.h"

struct LocalRootSignature
{
	LocalRootSignature(ID3D12Device5* pDevice, const D3D12_ROOT_SIGNATURE_DESC& DESC)
	{
		pRootSig = CreateRootSignature(pDevice, DESC);
		pInterface = pRootSig;
		SubObject.pDesc = &pInterface;
		SubObject.Type = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE;
	}
	~LocalRootSignature()
	{
		if (pRootSig)
		{
			pRootSig->Release();
			pRootSig = nullptr;
			pInterface = nullptr;
		}
	}
	ID3D12RootSignature* CreateRootSignature(ID3D12Device5* pDevice, const D3D12_ROOT_SIGNATURE_DESC& DESC)
	{
		_ASSERT(pDevice);

		ID3DBlob* pSigBlob = nullptr;
		ID3DBlob* pErrorBlob = nullptr;

		HRESULT hr = D3D12SerializeRootSignature(&DESC, D3D_ROOT_SIGNATURE_VERSION_1, &pSigBlob, &pErrorBlob);
		if (FAILED(hr))
		{
			pErrorBlob->Release();
			OutputDebugStringA((const char*)pErrorBlob->GetBufferPointer());
			return nullptr;
		}

		ID3D12RootSignature* pRootSig = nullptr;
		pDevice->CreateRootSignature(0, pSigBlob->GetBufferPointer(), pSigBlob->GetBufferSize(), IID_PPV_ARGS(&pRootSig));
		pSigBlob->Release();
		return pRootSig;
	}

	ID3D12RootSignature* pRootSig;
	ID3D12RootSignature* pInterface;
	D3D12_STATE_SUBOBJECT SubObject;
};
struct GlobalRootSignature
{
	GlobalRootSignature(ID3D12Device5* pDevice, const D3D12_ROOT_SIGNATURE_DESC& DESC)
	{
		pRootSig = CreateRootSignature(pDevice, DESC);
		pInterface = pRootSig;
		SubObject.pDesc = &pInterface;
		SubObject.Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE;
	}
	/*~GlobalRootSignature()
	{
		if (pRootSig)
		{
			pRootSig->Release();
			pRootSig = nullptr;
			pRootSig = nullptr;
		}
	}*/
	ID3D12RootSignature* CreateRootSignature(ID3D12Device5* pDevice, const D3D12_ROOT_SIGNATURE_DESC& DESC)
	{
		_ASSERT(pDevice);

		ID3DBlob* pSigBlob = nullptr;
		ID3DBlob* pErrorBlob = nullptr;

		HRESULT hr = D3D12SerializeRootSignature(&DESC, D3D_ROOT_SIGNATURE_VERSION_1, &pSigBlob, &pErrorBlob);
		if (FAILED(hr))
		{
			pErrorBlob->Release();
			OutputDebugStringA((const char*)pErrorBlob->GetBufferPointer());
			return nullptr;
		}

		ID3D12RootSignature* pRootSig = nullptr;
		pDevice->CreateRootSignature(0, pSigBlob->GetBufferPointer(), pSigBlob->GetBufferSize(), IID_PPV_ARGS(&pRootSig));
		pSigBlob->Release();
		return pRootSig;
	}

	ID3D12RootSignature* pRootSig;
	ID3D12RootSignature* pInterface;
	D3D12_STATE_SUBOBJECT SubObject;
};

DxcDLLSupport g_DxcDllHelper;

void Application::OnLoad()
{
	InitDXR();
	CreateAccelerationStructures();
	CreateRTPipelineState();
	CreateShaderResources();
	CreateShaderTable();
}

void Application::OnFrameRender()
{
	UINT rtvIndex = BeginFrame();

	// Let's raytrace
	ResourceBarrier(m_pCommandList, m_pOutputResource, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	D3D12_DISPATCH_RAYS_DESC raytraceDesc = {};
	raytraceDesc.Width = m_Width;
	raytraceDesc.Height = m_Height;
	raytraceDesc.Depth = 1;

	// RayGen is the first entry in the shader-table
	raytraceDesc.RayGenerationShaderRecord.StartAddress = m_pShaderTable->GetGPUVirtualAddress() + 0 * m_ShaderTableEntrySize;
	raytraceDesc.RayGenerationShaderRecord.SizeInBytes = m_ShaderTableEntrySize;

	// Miss is the second entry in the shader-table
	size_t missOffset = m_ShaderTableEntrySize;
	raytraceDesc.MissShaderTable.StartAddress = m_pShaderTable->GetGPUVirtualAddress() + missOffset;
	raytraceDesc.MissShaderTable.StrideInBytes = m_ShaderTableEntrySize;
	raytraceDesc.MissShaderTable.SizeInBytes = m_ShaderTableEntrySize;   // Only a s single miss-entry

	// Hit is the third entry in the shader-table
	size_t hitOffset = 2 * m_ShaderTableEntrySize;
	raytraceDesc.HitGroupTable.StartAddress = m_pShaderTable->GetGPUVirtualAddress() + hitOffset;
	raytraceDesc.HitGroupTable.StrideInBytes = m_ShaderTableEntrySize;
	raytraceDesc.HitGroupTable.SizeInBytes = m_ShaderTableEntrySize;

	// Bind the empty root signature
	m_pCommandList->SetComputeRootSignature(m_pEmptyRootSignature);

	// Dispatch
	m_pCommandList->SetPipelineState1(m_pPipelineState);
	m_pCommandList->DispatchRays(&raytraceDesc);

	// Copy the results to the back-buffer
	ResourceBarrier(m_pCommandList, m_pOutputResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
	ResourceBarrier(m_pCommandList, m_FrameObjects[rtvIndex].pSwapChainBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST);
	m_pCommandList->CopyResource(m_FrameObjects[rtvIndex].pSwapChainBuffer, m_pOutputResource);

	EndFrame();
}

void Application::OnShutdown()
{
	UINT64 lastFenceValue = Fence();
	WaitForGPU(lastFenceValue);

	if (m_pResourceManager)
	{
		delete m_pResourceManager;
		m_pResourceManager = nullptr;
	}
	if (m_pTextureManager)
	{
		delete m_pTextureManager;
		m_pTextureManager = nullptr;
	}

	m_pCBVSRVUAVHeap->Release();
	m_pOutputResource->Release();

	m_pShaderTable->Release();

	m_pEmptyRootSignature->Release();
	m_pPipelineState->Release();

	m_pBottomLevelAS->Release();
	m_pTopLevelAS->Release();
	m_pVertexBuffer->Release();

	m_RTVHeap.pHeap->Release();
	if (m_pRTVAllocator)
	{
		delete m_pRTVAllocator;
		m_pRTVAllocator = nullptr;
	}
	if (m_pCBVSRVUAVAllocator)
	{
		delete m_pCBVSRVUAVAllocator;
		m_pCBVSRVUAVAllocator = nullptr;
	}

	for (UINT i = 0; i < SWAP_CHAIN_BUFFER; ++i)
	{
		m_FrameObjects[i].pCommandAllocator->Release();
		m_FrameObjects[i].pSwapChainBuffer->Release();
	}

	CloseHandle(m_hFenceEvent);
	m_pFence->Release();
	m_pCommandList->Release();
	m_pSwapChain->Release();
	m_pCommandQueue->Release();
	m_pDevice->Release();
}

UINT64 Application::Fence()
{
	++m_FenceValue;
	m_pCommandQueue->Signal(m_pFence, m_FenceValue);
	return m_FenceValue;
}

void Application::WaitForGPU(UINT64 expectedValue)
{
	if (m_pFence->GetCompletedValue() < expectedValue)
	{
		m_pFence->SetEventOnCompletion(expectedValue, m_hFenceEvent);
		WaitForSingleObject(m_hFenceEvent, INFINITE);
	}
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

	m_RTVHeap.pHeap = CreateDescriptorHeap(m_pDevice, RTV_HEAP_SIZE, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, false);
	if (!m_RTVHeap.pHeap)
	{
		__debugbreak();
	}

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

	m_hFenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
	if (!m_hFenceEvent || m_hFenceEvent == INVALID_HANDLE_VALUE)
	{
		__debugbreak();
	}

	m_pResourceManager = new ResourceManager;
	m_pResourceManager->Initialize(this);

	m_pTextureManager = new TextureManager;
	m_pTextureManager->Initialize(this);

	m_pRTVAllocator = new DescriptorAllocator;
	m_pRTVAllocator->Initialize(m_pDevice, 5, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	m_pCBVSRVUAVAllocator = new DescriptorAllocator;
	m_pCBVSRVUAVAllocator->Initialize(m_pDevice, 256, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

UINT Application::BeginFrame()
{
	_ASSERT(m_pCBVSRVUAVHeap);
	_ASSERT(m_pCommandList);

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
	Fence();

	
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
	UINT64 lastFenceValue = SubmitCommandList();
	WaitForGPU(lastFenceValue);

	m_pCommandList->Reset(m_FrameObjects[0].pCommandAllocator, nullptr);

	// Store the AS buffers. The rest of the buffers will be released once we exit the function
	m_pTopLevelAS = topLevelBuffers.pResult;
	m_pBottomLevelAS = bottomLevelBuffers.pResult;

	bottomLevelBuffers.pScratch->Release();
	topLevelBuffers.pScratch->Release();
	topLevelBuffers.pInstanceDesc->Release();
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
	DXILLibrary dxilLib = CreateDXILLibrary();
	subObjects[index++] = dxilLib.StateSubObject; // 0 Library

	HitProgram hitProgram(nullptr, pszCLOSEST_HIT_SHADER, pszHIT_GROUP);
	subObjects[index++] = hitProgram.SubObject; // 1 Hit Group

	// Create the ray-gen root-signature and association
	LocalRootSignature rgsRootSignature(m_pDevice, CreateRayGenRootDesc().Desc);
	subObjects[index] = rgsRootSignature.SubObject; // 2 RayGen Root Sig

	UINT rgsRootIndex = index++; // 2
	ExportAssociation rgsRootAssociation(&pszRAY_GEN_SHADER, 1, &subObjects[rgsRootIndex]);
	subObjects[index++] = rgsRootAssociation.SubObject; // 3 Associate Root Sig to RGS

	// Create the miss- and hit-programs root-signature and association
	D3D12_ROOT_SIGNATURE_DESC emptyDesc = {};
	emptyDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
	LocalRootSignature hitMissRootSignature(m_pDevice, emptyDesc);
	subObjects[index] = hitMissRootSignature.SubObject; // 4 Root Sig to be shared between Miss and CHS

	UINT hitMissRootIndex = index++; // 4
	const WCHAR* pszMISS_HIT_EXPORT_NAME[] = { pszMISS_SHADER, pszCLOSEST_HIT_SHADER };
	ExportAssociation missHitRootAssociation(pszMISS_HIT_EXPORT_NAME, _countof(pszMISS_HIT_EXPORT_NAME), &subObjects[hitMissRootIndex]);
	subObjects[index++] = missHitRootAssociation.SubObject; // 5 Associate Root Sig to Miss and CHS

	// Bind the payload size to the programs
	ShaderConfig shaderConfig(sizeof(float) * 2, sizeof(float) * 3);
	subObjects[index] = shaderConfig.SubObject; // 6 Shader Config

	UINT shaderConfigIndex = index++; // 6
	const WCHAR* pszSHADER_EXPORTS[] = { pszMISS_SHADER, pszCLOSEST_HIT_SHADER, pszRAY_GEN_SHADER };
	ExportAssociation configAssociation(pszSHADER_EXPORTS, _countof(pszSHADER_EXPORTS), &(subObjects[shaderConfigIndex]));
	subObjects[index++] = configAssociation.SubObject; // 7 Associate Shader Config to Miss, CHS, RGS

	// Create the pipeline config
	PipelineConfig config(1);
	subObjects[index++] = config.SubObject; // 8

	// Create the global root signature and store the empty signature
	GlobalRootSignature root(m_pDevice, {});
	m_pEmptyRootSignature = root.pRootSig;
	subObjects[index++] = root.SubObject; // 9

	// Create the state
	D3D12_STATE_OBJECT_DESC desc = {};
	desc.NumSubobjects = index; // 10
	desc.pSubobjects = subObjects;
	desc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;

	hr = m_pDevice->CreateStateObject(&desc, IID_PPV_ARGS(&m_pPipelineState));
	if (FAILED(hr))
	{
		__debugbreak();
	}
}

void Application::CreateShaderTable()
{
	/** The shader-table layout is as follows:
		Entry 0 - Ray-gen program
		Entry 1 - Miss program
		Entry 2 - Hit program
		All entries in the shader-table must have the same size, so we will choose it base on the largest required entry.
		The ray-gen program requires the largest entry - sizeof(program identifier) + 8 bytes for a descriptor-table.
		The entry size must be aligned up to D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT
	*/

	HRESULT hr = S_OK;

	// Calculate the size and create the buffer
	m_ShaderTableEntrySize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
	m_ShaderTableEntrySize += 8; // The ray-gen's descriptor table
	m_ShaderTableEntrySize = ALIGN_TO(D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT, m_ShaderTableEntrySize);
	UINT shaderTableSize = m_ShaderTableEntrySize * 3;

	// For simplicity, we create the shader-table on the upload heap. You can also create it on the default heap
	CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD, 0, 0);
	m_pShaderTable = CreateBuffer(m_pDevice, shaderTableSize, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, uploadHeapProps);

	// Map the buffer
	UINT8* pData = nullptr;
	hr = m_pShaderTable->Map(0, nullptr, (void**)&pData);
	if (FAILED(hr))
	{
		__debugbreak();
	}

	ID3D12StateObjectProperties* pRtsoProps = nullptr;
	m_pPipelineState->QueryInterface(IID_PPV_ARGS(&pRtsoProps));

	// Entry 0 - ray-gen program ID and descriptor data
	memcpy(pData, pRtsoProps->GetShaderIdentifier(pszRAY_GEN_SHADER), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
	UINT64 heapStart = m_pCBVSRVUAVHeap->GetGPUDescriptorHandleForHeapStart().ptr;
	*(UINT64*)(pData + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES) = heapStart;

	// Entry 1 - miss program
	memcpy(pData + m_ShaderTableEntrySize, pRtsoProps->GetShaderIdentifier(pszMISS_SHADER), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);

	// Entry 2 - hit program
	UINT8* pHitEntry = pData + m_ShaderTableEntrySize * 2; // +2 skips the ray-gen and miss entries
	memcpy(pHitEntry, pRtsoProps->GetShaderIdentifier(pszHIT_GROUP), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);

	// Unmap
	m_pShaderTable->Unmap(0, nullptr);

	pRtsoProps->Release();
}

void Application::CreateShaderResources()
{
	HRESULT hr = S_OK;
	CD3DX12_HEAP_PROPERTIES defaultHeapProps(D3D12_HEAP_TYPE_DEFAULT, 0, 0);

	// Create the output resource. The dimensions and format should match the swap-chain
	D3D12_RESOURCE_DESC resDesc = {};
	resDesc.DepthOrArraySize = 1;
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // The backbuffer is actually DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, but sRGB formats can't be used with UAVs. We will convert to sRGB ourselves in the shader
	resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	resDesc.Height = m_Height;
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resDesc.MipLevels = 1;
	resDesc.SampleDesc.Count = 1;
	resDesc.Width = m_Width;
	hr = m_pDevice->CreateCommittedResource(&defaultHeapProps, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_COPY_SOURCE, nullptr, IID_PPV_ARGS(&m_pOutputResource)); // Starting as copy-source to simplify onFrameRender()

	// Create an SRV/UAV descriptor heap. Need 2 entries - 1 SRV for the scene and 1 UAV for the output
	m_pCBVSRVUAVHeap = CreateDescriptorHeap(m_pDevice, 2, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);

	// Create the UAV. Based on the root signature we created it should be the first entry
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	m_pDevice->CreateUnorderedAccessView(m_pOutputResource, nullptr, &uavDesc, m_pCBVSRVUAVHeap->GetCPUDescriptorHandleForHeapStart());

	// Create the TLAS SRV right after the UAV. Note that we are using a different SRV desc here
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.RaytracingAccelerationStructure.Location = m_pTopLevelAS->GetGPUVirtualAddress();
	D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = m_pCBVSRVUAVHeap->GetCPUDescriptorHandleForHeapStart();
	srvHandle.ptr += m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	m_pDevice->CreateShaderResourceView(nullptr, &srvDesc, srvHandle);
}

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

	/*CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD, 0, 0);
	ID3D12Resource* pBuffer = CreateBuffer(pDevice, sizeof(VERTICES), D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, uploadHeapProps);
	if (!pBuffer)
	{
		return nullptr;
	}
	
	UINT8* pData = nullptr;
	pBuffer->Map(0, nullptr, (void**)&pData);
	memcpy(pData, VERTICES, sizeof(VERTICES));
	pBuffer->Unmap(0, nullptr);*/
	Buffer* pBufferData = m_pResourceManager->CreateVertexBuffer(sizeof(DirectX::XMFLOAT3), 3, (void*)VERTICES);
	ID3D12Resource* pBuffer = pBufferData->pResource;
	pBuffer->AddRef();

	pBufferData->pResource->Release();
	delete pBufferData;

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

ID3D12RootSignature* Application::CreateRootSignature(ID3D12Device5* pDevice, const D3D12_ROOT_SIGNATURE_DESC& DESC)
{
	_ASSERT(pDevice);

	ID3DBlob* pSigBlob = nullptr;
	ID3DBlob* pErrorBlob = nullptr;
	
	HRESULT hr = D3D12SerializeRootSignature(&DESC, D3D_ROOT_SIGNATURE_VERSION_1, &pSigBlob, &pErrorBlob);
	if (FAILED(hr))
	{
		pErrorBlob->Release();
		OutputDebugStringA((const char*)pErrorBlob->GetBufferPointer());
		return nullptr;
	}

	ID3D12RootSignature* pRootSig = nullptr;
	pDevice->CreateRootSignature(0, pSigBlob->GetBufferPointer(), pSigBlob->GetBufferSize(), IID_PPV_ARGS(&pRootSig));
	pSigBlob->Release();
	return pRootSig;
}

DXILLibrary Application::CreateDXILLibrary()
{
	ID3DBlob* pDXILLib = CompileLibrary(L"Shader.hlsl", L"lib_6_3");
	const WCHAR* pszENTRY_POINTS[] = { pszRAY_GEN_SHADER, pszMISS_SHADER, pszCLOSEST_HIT_SHADER };
	return DXILLibrary(pDXILLib, pszENTRY_POINTS, _countof(pszENTRY_POINTS));
}

RootSignatureDesc Application::CreateRayGenRootDesc()
{
	RootSignatureDesc desc = {};
	desc.Range.resize(2);
	
	// gOutput
	desc.Range[0].BaseShaderRegister = 0;
	desc.Range[0].NumDescriptors = 1;
	desc.Range[0].RegisterSpace = 0;
	desc.Range[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	desc.Range[0].OffsetInDescriptorsFromTableStart = 0;

	// gRtScene
	desc.Range[1].BaseShaderRegister = 0;
	desc.Range[1].NumDescriptors = 1;
	desc.Range[1].RegisterSpace = 0;
	desc.Range[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	desc.Range[1].OffsetInDescriptorsFromTableStart = 1;

	desc.RootParams.resize(1);
	desc.RootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	desc.RootParams[0].DescriptorTable.NumDescriptorRanges = 2;
	desc.RootParams[0].DescriptorTable.pDescriptorRanges = desc.Range.data();

	// Create the desc
	desc.Desc.NumParameters = 1;
	desc.Desc.pParameters = desc.RootParams.data();
	desc.Desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;

	return desc;
}

ID3DBlob* Application::CompileLibrary(const WCHAR* pszFILE_NAME, const WCHAR* pszTARGET_STRING)
{
	HRESULT hr = S_OK;
	
	IDxcCompiler* pCompiler = nullptr;
	IDxcLibrary* pLibrary = nullptr;
	hr = g_DxcDllHelper.Initialize();
	if (FAILED(hr))
	{
		__debugbreak();
	}
	hr = g_DxcDllHelper.CreateInstance(CLSID_DxcCompiler, &pCompiler);
	if (FAILED(hr))
	{
		__debugbreak();
	}
	hr = g_DxcDllHelper.CreateInstance(CLSID_DxcLibrary, &pLibrary);
	if (FAILED(hr))
	{
		__debugbreak();
	}

	std::ifstream shaderFile(pszFILE_NAME);
	if (!shaderFile.good())
	{
		pCompiler->Release();
		pLibrary->Release();

		OutputDebugStringA("Can't open file!\n");

		return nullptr;
	}
	std::stringstream strStream;
	strStream << shaderFile.rdbuf();
	std::string shader = strStream.str();

	IDxcBlobEncoding* pTextBlob = nullptr;
	hr = pLibrary->CreateBlobWithEncodingFromPinned((BYTE*)shader.c_str(), (UINT)shader.size(), 0, &pTextBlob);
	if (FAILED(hr))
	{
		pCompiler->Release();
		pLibrary->Release();

		return nullptr;
	}

	IDxcOperationResult* pResult = nullptr;
	hr = pCompiler->Compile(pTextBlob, pszFILE_NAME, L"", pszTARGET_STRING, nullptr, 0, nullptr, 0, nullptr, &pResult);
	if (FAILED(hr))
	{
		pTextBlob->Release();
		pCompiler->Release();
		pLibrary->Release();

		return nullptr;
	}

	pResult->GetStatus(&hr);
	if (FAILED(hr))
	{
		IDxcBlobEncoding* pError = nullptr;
		pResult->GetErrorBuffer(&pError);

		OutputDebugStringA((const char*)pError->GetBufferPointer());

		pError->Release();
		pResult->Release();
		pTextBlob->Release();
		pCompiler->Release();
		pLibrary->Release();

		return nullptr;
	}

	IDxcBlob* pBlob = nullptr;
	pResult->GetResult(&pBlob);

	pResult->Release();
	pTextBlob->Release();
	pCompiler->Release();
	pLibrary->Release();

	return (ID3DBlob*)pBlob;
}

UINT64 Application::SubmitCommandList()
{
	_ASSERT(m_pCommandList);
	_ASSERT(m_pCommandQueue);
	_ASSERT(m_pFence);

	m_pCommandList->Close();

	ID3D12CommandList* ppCommandLists[] = { m_pCommandList, };
	m_pCommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	return Fence();
}

void Application::ResourceBarrier(ID3D12GraphicsCommandList4* pCommandList, ID3D12Resource* pResource, D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter)
{
	_ASSERT(pCommandList);
	_ASSERT(pResource);

	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = pResource;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = stateBefore;
	barrier.Transition.StateAfter = stateAfter;
	pCommandList->ResourceBarrier(1, &barrier);
}
