#include "framework.h"
#include "GeometryUtil.h"
#include "MathUtil.h"
#include "Renderer.h"

const WCHAR* szHIT_GROUP_NAME = L"MyHitGroup";
const WCHAR* szRAYGEN_SHADER_NAME = L"MyRaygenShader";
const WCHAR* szCLOSEST_HIT_SHADER_NAME = L"MyClosestHitShader";
const WCHAR* szMISS_SHADER_NAME = L"MyMissShader";

bool Renderer::Cleanup()
{
	// wait for gpu.

	if (m_hFenceEvent)
	{
		CloseHandle(m_hFenceEvent);
		m_hFenceEvent = nullptr;
	}

	SAFE_COM_RELEASE(m_VertexBuffer.pResource);
	SAFE_COM_RELEASE(m_IndexBuffer.pResource);
	m_VertexBuffer = INIT_BUFFER;
	m_IndexBuffer = INIT_BUFFER;

	for (SIZE_T i = 0, size = m_Geometry.size(); i < size; ++i)
	{
		SAFE_COM_RELEASE(m_Geometry[i].pBottomLevelAccelerationStructure);
	}
	m_Geometry.clear();

	SAFE_COM_RELEASE(m_pFrameConstants);
	if (m_pMappedConstantData)
	{
		delete m_pMappedConstantData;
		m_pMappedConstantData = nullptr;
	}

	SAFE_COM_RELEASE(m_pRayGenShaderTable);
	SAFE_COM_RELEASE(m_pHitGroupShaderTable);
	SAFE_COM_RELEASE(m_pMissShaderTable);
	SAFE_COM_RELEASE(m_pRaytracingLocalRootSignature);
	SAFE_COM_RELEASE(m_pRaytracingGlobalRootSignature);
	SAFE_COM_RELEASE(m_pStateObject);

	SAFE_COM_RELEASE(m_pRaytracingOutput);

	m_RTVDescriptorSize = 0;
	m_DSVDescriptorSize = 0;
	m_CBVSRVUAVDescriptorSize = 0;
	SAFE_COM_RELEASE(m_pCBVSRVUAVDescriptorHeap);
	SAFE_COM_RELEASE(m_pDSVDescriptorHeap);
	SAFE_COM_RELEASE(m_pRTVDescriptorHeap);

	for (SIZE_T i = 0; i < MAX_BACK_BUFFER_COUNT; ++i)
	{
		m_FenceValues[i] = 0;
	}
	SAFE_COM_RELEASE(m_pFence);

	SAFE_COM_RELEASE(m_pDepthStencil);
	for (SIZE_T i = 0; i < MAX_BACK_BUFFER_COUNT; ++i)
	{
		SAFE_COM_RELEASE(m_ppRenderTargets[i]);
		SAFE_COM_RELEASE(m_ppCommandAllocators[i]);
		SAFE_COM_RELEASE(m_ppCommandLists[i]);
	}

	SAFE_COM_RELEASE(m_pCommandQueue);
	SAFE_COM_RELEASE(m_pSwapChain);
	SAFE_COM_RELEASE(m_pDXGIAdapter);
	SAFE_COM_RELEASE(m_pDXGIFactory);
	SAFE_COM_RELEASE(m_pDevice);

	DestroyWindow(m_hMainWindow);
	m_hMainWindow = nullptr;

	return true;
}

bool Renderer::InitializeDXGIAdapter()
{
	HRESULT hr;
	bool bDebugDXGI = false;

#ifdef _DEBUG
	ID3D12Debug6* pDebugController = nullptr;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&pDebugController))))
	{
		pDebugController->EnableDebugLayer();
		pDebugController->Release();
		pDebugController = nullptr;
	}

	IDXGIInfoQueue* pDXGIInfoQueue = nullptr;
	if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&pDXGIInfoQueue))))
	{
		bDebugDXGI = true;

		hr = CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&m_pDXGIFactory));
		if (FAILED(hr))
		{
			__debugbreak();
		}

		pDXGIInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true);
		pDXGIInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true);

		pDXGIInfoQueue->Release();
		pDXGIInfoQueue = nullptr;
	}
#endif

	if (!bDebugDXGI)
	{
		hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&m_pDXGIFactory));
		if (FAILED(hr))
		{
			BREAK_IF_FAILED(hr);
			return false;
		}
	}

	BOOL bAllowTearing = FALSE;
	hr = m_pDXGIFactory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &bAllowTearing, sizeof(BOOL));
	if (FAILED(hr) || !bAllowTearing)
	{
		OutputDebugString(L"Variable refresh rate displays are not supported.\n");
		m_Option &= ~DeviceOption_AllowTearing;
	}

	return true;
}

bool Renderer::InitializeD3DDeviceResources()
{
	if (!m_pDXGIFactory || !m_pDXGIAdapter)
	{
		__debugbreak();
		return false;
	}

	HRESULT hr;

	const D3D_FEATURE_LEVEL FEATURE_LEVELS[5] =
	{
		D3D_FEATURE_LEVEL_12_2,
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};
	const SIZE_T NUM_FEATURE_LEVELS = ARRAYSIZE(FEATURE_LEVELS);
	IDXGIAdapter1* pAdapter = nullptr;

	for (SIZE_T featureLevelIndex = 0; featureLevelIndex < NUM_FEATURE_LEVELS; ++featureLevelIndex)
	{
		UINT adapterIndex = 0;
		while (m_pDXGIFactory->EnumAdapters1(adapterIndex, &pAdapter) != DXGI_ERROR_NOT_FOUND)
		{
			DXGI_ADAPTER_DESC1 desc = {};
			pAdapter->GetDesc1(&desc);

			hr = D3D12CreateDevice(pAdapter, FEATURE_LEVELS[featureLevelIndex], IID_PPV_ARGS(&m_pDevice));
			if (SUCCEEDED(hr))
			{
				pAdapter->QueryInterface(IID_PPV_ARGS(&m_pDXGIAdapter));
				SAFE_COM_RELEASE(pAdapter);

				goto LB_EXIT_LOOP;
			}

			SAFE_COM_RELEASE(pAdapter);
			++adapterIndex;
		}
	}

LB_EXIT_LOOP:
	if (FAILED(hr))
	{
		BREAK_IF_FAILED(hr);
		return false;
	}
	m_pDevice->SetName(L"D3DDevice");


	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	hr = m_pDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_pCommandQueue));
	if (FAILED(hr))
	{
		BREAK_IF_FAILED(hr);
		return false;
	}
	m_pCommandQueue->SetName(L"CommandQueue");


	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {};
	descriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	descriptorHeapDesc.NumDescriptors = MAX_BACK_BUFFER_COUNT;

	hr = m_pDevice->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&m_pRTVDescriptorHeap));
	if (FAILED(hr))
	{
		BREAK_IF_FAILED(hr);
		return false;
	}
	m_pRTVDescriptorHeap->SetName(L"RTVDescriptorHeap");

	descriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	descriptorHeapDesc.NumDescriptors = 1;

	hr = m_pDevice->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&m_pDSVDescriptorHeap));
	if (FAILED(hr))
	{
		BREAK_IF_FAILED(hr);
		return false;
	}
	m_pDSVDescriptorHeap->SetName(L"DSVDescriptorHeap");

	descriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	descriptorHeapDesc.NumDescriptors = 512;

	hr = m_pDevice->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&m_pCBVSRVUAVDescriptorHeap));
	if (FAILED(hr))
	{
		BREAK_IF_FAILED(hr);
		return false;
	}
	m_pCBVSRVUAVDescriptorHeap->SetName(L"CBVSRVUAVDescriptorHeap");


	for (UINT i = 0; i < MAX_BACK_BUFFER_COUNT; ++i)
	{
		hr = m_pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_ppCommandAllocators[i]));
		if (FAILED(hr))
		{
			BREAK_IF_FAILED(hr);
			return false;
		}

		hr = m_pDevice->CreateCommandList1(0, D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&m_ppCommandLists[i]));
		if (FAILED(hr))
		{
			BREAK_IF_FAILED(hr);
			return false;
		}


		hr = m_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&m_ppRenderTargets[i]));
		if (FAILED(hr))
		{
			BREAK_IF_FAILED(hr);
			return false;
		}

		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		rtvDesc.Format = m_BackBufferFormat;
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvDescriptor(m_pRTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
		m_pDevice->CreateRenderTargetView(m_ppRenderTargets[i], &rtvDesc, rtvDescriptor);


		WCHAR szDebugName[256];
		swprintf_s(szDebugName, 256, L"CommandAllocator[%d]", i);
		m_ppCommandAllocators[i]->SetName(szDebugName);
		swprintf_s(szDebugName, 256, L"CommandList[%d]", i);
		m_ppCommandLists[i]->SetName(szDebugName);
		swprintf_s(szDebugName, 256, L"RenderTaget[%d]", i);
		m_ppRenderTargets[i]->SetName(szDebugName);

		m_ppCommandLists[i]->Close();
	}


	if (!CreateWindowDependentedResources())
	{
		__debugbreak();
		return false;
	}


	hr = m_pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_pFence));
	if (FAILED(hr))
	{
		BREAK_IF_FAILED(hr);
		return false;
	}

	m_hFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (!m_hFenceEvent || m_hFenceEvent == INVALID_HANDLE_VALUE)
	{
		BREAK_IF_FAILED(hr);
		return false;
	}

	return true;
}

bool Renderer::CreateWindowDependentedResources()
{
	_ASSERT(m_pDevice);

	HRESULT hr;

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.Width = m_Width;
	swapChainDesc.Height = m_Height;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_SHADER_INPUT;
	swapChainDesc.BufferCount = MAX_BACK_BUFFER_COUNT;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.SampleDesc.Quality = 0;
	swapChainDesc.Scaling = DXGI_SCALING_NONE;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
	swapChainDesc.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	DXGI_SWAP_CHAIN_FULLSCREEN_DESC fsSwapChainDesc = {};
	fsSwapChainDesc.Windowed = TRUE;

	IDXGISwapChain1* pSwapChain1 = nullptr;
	hr = m_pDXGIFactory->CreateSwapChainForHwnd(m_pCommandQueue, m_hMainWindow, &swapChainDesc, &fsSwapChainDesc, nullptr, &pSwapChain1);
	if (FAILED(hr))
	{
		BREAK_IF_FAILED(hr);
		return false;
	}

	SAFE_COM_RELEASE(m_pSwapChain);
	pSwapChain1->QueryInterface(IID_PPV_ARGS(&m_pSwapChain));
	SAFE_COM_RELEASE(pSwapChain1);


	for (UINT i = 0; i < MAX_BACK_BUFFER_COUNT; ++i)
	{
		SAFE_COM_RELEASE(m_ppRenderTargets[i]);

		hr = m_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&m_ppRenderTargets[i]));
		if (FAILED(hr))
		{
			BREAK_IF_FAILED(hr);
			return false;
		}

		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		rtvDesc.Format = m_BackBufferFormat;
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvDescriptor(m_pRTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
		m_pDevice->CreateRenderTargetView(m_ppRenderTargets[i], &rtvDesc, rtvDescriptor);

		WCHAR szDebugName[256];
		swprintf_s(szDebugName, 256, L"RenderTaget[%d]", i);
		m_ppRenderTargets[i]->SetName(szDebugName);

		m_ppCommandLists[i]->Close();
	}

	m_BackBufferFormat = swapChainDesc.Format;
	m_FrameIndex = m_pSwapChain->GetCurrentBackBufferIndex();

	return true;
}

bool Renderer::CreateDeviceDependentResources()
{
	if (!CreateRootSignature())
	{
		return false;
	}

	if (!CreateRaytracingPipelineStateObject())
	{
		return false;
	}

	if (!BuildGeometry())
	{
		return false;
	}

	if (!BuildAccelerationStructures())
	{
		return false;
	}

	if (!CreateConstantBuffers())
	{
		return false;
	}

	if (!BuildShaderTables())
	{
		return false;
	}

	// Create raytracing output reosurce.


	
	return true;
}

bool Renderer::CreateConstantBuffers()
{
	return false;
}

bool Renderer::CreateRootSignature()
{
	_ASSERT(!m_pRaytracingGlobalRootSignature);
	_ASSERT(!m_pRaytracingLocalRootSignature);

	// Global Root Signature

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

	// Local Root Signature

	CD3DX12_ROOT_PARAMETER rootParameters[LocalRootSignatureParams::Count];
	rootParameters[LocalRootSignatureParams::MeshBufferSlot].InitAsConstants(sizeof(MeshBuffer), 1);
	
	CD3DX12_ROOT_SIGNATURE_DESC localRootSignatureDesc(ARRAYSIZE(rootParameters), rootParameters);
	localRootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
	SerializeAndCreateRaytracingRootSignature(&localRootSignatureDesc, &m_pRaytracingLocalRootSignature);

	return true;
}

bool Renderer::CreateRaytracingPipelineStateObject()
{
	_ASSERT(m_pDevice);

	CD3DX12_STATE_OBJECT_DESC raytracingPipeline(D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE);

	CD3DX12_DXIL_LIBRARY_SUBOBJECT* pLib = raytracingPipeline.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
	D3D12_SHADER_BYTECODE libdxil = CD3DX12_SHADER_BYTECODE((void*)g_pRaytracing, ARRAYSIZE(g_pRaytracing));
	pLib->SetDXILLibrary(&libdxil);
	pLib->DefineExport(szRAYGEN_SHADER_NAME);
	pLib->DefineExport(szCLOSEST_HIT_SHADER_NAME);
	pLib->DefineExport(szMISS_SHADER_NAME);

	CD3DX12_HIT_GROUP_SUBOBJECT* pHitGroup = raytracingPipeline.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
	pHitGroup->SetClosestHitShaderImport(szCLOSEST_HIT_SHADER_NAME);
	pHitGroup->SetHitGroupExport(szHIT_GROUP_NAME);
	pHitGroup->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);

	CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT* pShaderConfig = raytracingPipeline.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
	UINT payloadSize = sizeof(XMFLOAT4) + sizeof(UINT) + sizeof(XMFLOAT4);// float4 pixelColor
	UINT attributeSize = sizeof(XMFLOAT2);  // float2 barycentrics
	pShaderConfig->Config(payloadSize, attributeSize);

	CreateLocalRootSignatureSubobjects(&raytracingPipeline);

	CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT* pGlobalRootSignature = raytracingPipeline.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
	pGlobalRootSignature->SetRootSignature(m_pRaytracingGlobalRootSignature);

	CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT* pPipelineConfig = raytracingPipeline.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
	UINT maxRecursionDepth = 1; // ~ primary rays only. 
	pPipelineConfig->Config(maxRecursionDepth);

//#if _DEBUG
//	PrintStateObjectDesc(raytracingPipeline);
//#endif

	// Create the state object.
	HRESULT hr = m_pDevice->CreateStateObject(raytracingPipeline, IID_PPV_ARGS(&m_pStateObject));
	if (FAILED(hr))
	{
		BREAK_IF_FAILED(hr);
		return false;
	}

	return true;
}

void Renderer::CreateLocalRootSignatureSubobjects(CD3DX12_STATE_OBJECT_DESC* pRaytracingPipeline)
{
	_ASSERT(pRaytracingPipeline);

	CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT* pLocalRootSignature = pRaytracingPipeline->CreateSubobject<CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
	pLocalRootSignature->SetRootSignature(m_pRaytracingLocalRootSignature);
	
	CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT* pRootSignatureAssociation = pRaytracingPipeline->CreateSubobject<CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
	pRootSignatureAssociation->SetSubobjectToAssociate(*pLocalRootSignature);
	pRootSignatureAssociation->AddExport(szHIT_GROUP_NAME);
}

bool Renderer::BuildGeometry()
{
	const int LAMBERTIAN = 0;
	const int METALLIC = 1;
	const int DIELECTRIC = 2;

	const int SLICE_COUNT = 16;
	const int STACK_COUNT = 32;
	{
		std::pair<std::vector<Vertex>, std::vector<UINT32>> sphereGeometry = CreateSphere(1000.0f, SLICE_COUNT, 512);

		Geometry geom = {};
		geom.Vertices = sphereGeometry.first;
		geom.Indices = sphereGeometry.second;
		geom.Albedo = { 0.5f, 0.5f, 0.5f, 1.0f };
		geom.MaterialID = LAMBERTIAN;
		geom.Transform = XMMatrixTranspose(XMMatrixTranslation(0.0f, -1000.0f, 0.0f));

		m_Geometry.push_back(geom);
	}

	{
		for (int a = -11; a < 11; a++)
		{
			for (int b = -11; b < 11; b++)
			{
				double chooseMat = RandomDouble();
				XMVECTOR center = { (float)a + 0.9f * RandomFloat(), 0.2f, (float)b + 0.9f * RandomFloat() };
				XMVECTOR length = XMVector3Length(XMVectorSubtract(center, { 4.0f, 0.2f, 0.0f }));
				if (XMVectorGetX(length) > 0.9f)
				{
					XMVECTOR materialParameter;
					int materialType = 0;

					if (chooseMat < 0.8)
					{
						// diffuse
						materialType = LAMBERTIAN;
						XMVECTOR albedo = RandomColor() * RandomColor();
						materialParameter = albedo;
					}
					else if (chooseMat < 0.95)
					{
						// metal
						materialType = METALLIC;
						float fuzz = RandomFloat(0, 0.5);
						materialParameter = { RandomFloat(0.5f, 1.0f), RandomFloat(0.5f, 1.0f), RandomFloat(0.5f, 1.0f), fuzz };
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
					m_Geometry.push_back(geom);
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
			m_Geometry.push_back(geom);
		}

		{
			std::pair<std::vector<Vertex>, std::vector<UINT32>> sphereGeometry = CreateSphere(1.0f, SLICE_COUNT, STACK_COUNT);

			Geometry geom;
			geom.Vertices = sphereGeometry.first;
			geom.Indices = sphereGeometry.second;
			geom.Albedo = { 0.4f, 0.2f, 0.1f, 0.0f };
			geom.MaterialID = LAMBERTIAN;
			geom.Transform = XMMatrixTranspose(XMMatrixTranslation(-4.0f, 1.0f, 0.0f));
			m_Geometry.push_back(geom);
		}

		{
			std::pair<std::vector<Vertex>, std::vector<UINT32>> sphereGeometry = CreateSphere(1.0f, SLICE_COUNT, STACK_COUNT);

			Geometry geom;
			geom.Vertices = sphereGeometry.first;
			geom.Indices = sphereGeometry.second;
			geom.Albedo = { 0.7f, 0.6f, 0.5f, 0.0f };
			geom.MaterialID = METALLIC;
			geom.Transform = XMMatrixTranspose(XMMatrixTranslation(4.0f, 1.0f, 0.0f));
			m_Geometry.push_back(geom);
		}
	}


	SIZE_T totalNumIndices = 0;
	SIZE_T totalNumVertices = 0;

	for (Geometry const& geometry : m_Geometry)
	{
		totalNumIndices += geometry.Indices.size();
		totalNumVertices += geometry.Vertices.size();
	}

	std::vector<UINT32> indices(totalNumIndices);
	std::vector<Vertex> vertices(totalNumVertices);
	SIZE_T vertexOffset = 0;
	SIZE_T indexOffset = 0;
	for (Geometry& geom : m_Geometry)
	{
		memcpy(&vertices[vertexOffset], geom.Vertices.data(), geom.Vertices.size() * sizeof(geom.Vertices[0]));
		memcpy(indices.data() + indexOffset, geom.Indices.data(), geom.Indices.size() * sizeof(geom.Indices[0]));
		geom.IndicesOffsetInBytes = indexOffset * sizeof(geom.Indices[0]);
		geom.VerticesOffsetInBytes = vertexOffset * sizeof(geom.Vertices[0]);

		vertexOffset += geom.Vertices.size();
		indexOffset += geom.Indices.size();
	}
	AllocateUploadBuffer(m_pDevice, vertices.data(), vertices.size() * sizeof(Vertex), &m_VertexBuffer.pResource);
	AllocateUploadBuffer(m_pDevice, indices.data(), indices.size() * sizeof(UINT32), &m_IndexBuffer.pResource);


	// Vertex buffer is passed to the shader along with index buffer as a descriptor table.
	// Vertex buffer descriptor must follow index buffer descriptor in the descriptor heap.
	UINT descriptorIndexIB = CreateBufferSRV(&m_IndexBuffer, (UINT)indices.size(), sizeof(UINT32));
	UINT descriptorIndexVB = CreateBufferSRV(&m_VertexBuffer, (UINT)vertices.size(), sizeof(Vertex));
	ThrowIfFalse(descriptorIndexVB == descriptorIndexIB + 1, L"Vertex Buffer descriptor index must follow that of Index Buffer descriptor index!");

	return false;
}

bool Renderer::BuildAccelerationStructures()
{
	return false;
}

bool Renderer::BuildShaderTables()
{
	return false;
}

void Renderer::UpdateCameraMatrices()
{}

void Renderer::DoRaytracing()
{}

void Renderer::SerializeAndCreateRaytracingRootSignature(D3D12_ROOT_SIGNATURE_DESC* pDesc, ID3D12RootSignature** ppRootSig)
{
	_ASSERT(m_pDevice);
	_ASSERT(pDesc);
	_ASSERT(ppRootSig && !*ppRootSig);

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

	if (FAILED(m_pDevice->CreateRootSignature(1, pBlob->GetBufferPointer(), pBlob->GetBufferSize(), IID_PPV_ARGS(ppRootSig))))
	{
		__debugbreak();
	}

	pBlob->Release();
}
