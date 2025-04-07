#include "framework.h"
#include "GeometryUtil.h"
#include "GPUResources.h"
#include "MathUtil.h"
#include "CompiledShaders/Raytracing.hlsl.h"
#include "Renderer.h"

const WCHAR* szHIT_GROUP_NAME = L"MyHitGroup";
const WCHAR* szRAYGEN_SHADER_NAME = L"MyRaygenShader";
const WCHAR* szCLOSEST_HIT_SHADER_NAME = L"MyClosestHitShader";
const WCHAR* szMISS_SHADER_NAME = L"MyMissShader";

bool Renderer::Initialize(HINSTANCE hInstance, WNDPROC pfnWndProc, UINT width, UINT height)
{
	if (!hInstance || hInstance == INVALID_HANDLE_VALUE)
	{
		return false;
	}
	if (!pfnWndProc)
	{
		return false;
	}
	if (width == 0 || height == 0)
	{
		return false;
	}

	m_Width = width;
	m_Height = height;
	m_AspectRatio = (float)width / (float)height;

	// Create windows.
	{
		WNDCLASSEX windowClass = { 0, };
		windowClass.cbSize = sizeof(WNDCLASSEX);
		windowClass.style = CS_HREDRAW | CS_VREDRAW;
		windowClass.lpfnWndProc = pfnWndProc;
		windowClass.hInstance = hInstance;
		windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
		windowClass.lpszClassName = L"DXRClass";
		if (!RegisterClassEx(&windowClass))
		{
			return false;
		}

		RECT windowRect = { 0, 0, (long)width, (long)height };
		AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

		// Create the window and store a handle to it.
		m_hMainWindow = CreateWindow(windowClass.lpszClassName,
									 L"DXRRenderer",
									 WS_OVERLAPPEDWINDOW,
									 CW_USEDEFAULT,
									 CW_USEDEFAULT,
									 windowRect.right - windowRect.left,
									 windowRect.bottom - windowRect.top,
									 nullptr,        // We have no parent window.
									 nullptr,        // We aren't using menus.
									 hInstance,
									 this);
		if (!m_hMainWindow)
		{
			return false;
		}
	}

	if (!InitializeDXGIAdapter())
	{
		return false;
	}

	if (!InitializeD3DDeviceResources())
	{
		return false;
	}

	if (!InitScene())
	{
		return false;
	}

	return true;
}

bool Renderer::Cleanup()
{
	if (m_hFenceEvent)
	{
		// wait for gpu.
		WaitForGPU();

		CloseHandle(m_hFenceEvent);
		m_hFenceEvent = nullptr;
	}

	SAFE_COM_RELEASE(m_VertexBuffer.pResource);
	SAFE_COM_RELEASE(m_IndexBuffer.pResource);
	m_VertexBuffer = INIT_BUFFER;
	m_IndexBuffer = INIT_BUFFER;

	/*for (SIZE_T i = 0, size = m_Geometry.size(); i < size; ++i)
	{
		SAFE_COM_RELEASE(m_Geometry[i].pBottomLevelAccelerationStructure);
	}*/
	for (auto iter = m_BLASTypeCache.begin(), endIter = m_BLASTypeCache.end(); iter != endIter; ++iter)
	{
		SAFE_COM_RELEASE(iter->second.pBottomLevelAS);
	}
	m_Geometry.clear();
	m_BLASTypeCache.clear();

	SAFE_COM_RELEASE(m_pFrameConstants);
	if (m_pMappedConstantData)
	{
		m_pMappedConstantData = nullptr;
	}

	SAFE_COM_RELEASE(m_pRayGenShaderTable);
	SAFE_COM_RELEASE(m_pHitGroupShaderTable);
	SAFE_COM_RELEASE(m_pMissShaderTable);
	SAFE_COM_RELEASE(m_pRaytracingLocalRootSignature);
	SAFE_COM_RELEASE(m_pRaytracingGlobalRootSignature);
	SAFE_COM_RELEASE(m_pStateObject);

	SAFE_COM_RELEASE(m_pTopLevelAccelerationStructure);
	SAFE_COM_RELEASE(m_pRaytracingOutput);

	m_NumRTVDescriptorAlloced = 0;
	m_NumDSVDescriptorAlloced = 0;
	m_NumCBVSRVUAVDescriptorAlloced = 0;
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

	DestroyWindow(m_hMainWindow);
	m_hMainWindow = nullptr;

	if (m_pDevice)
	{
		ULONG refCount = m_pDevice->Release();
		m_pDevice = nullptr;

		if (refCount > 0)
		{
			return false;
		}
	}

	return true;
}

void Renderer::Run()
{
	_ASSERT(m_hMainWindow);

	ShowWindow(m_hMainWindow, SW_SHOWNORMAL);
	UpdateWindow(m_hMainWindow);

	MSG msg = {};
	while (TRUE)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT || msg.message == WM_DESTROY)
			{
				break;
			}
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	WaitForGPU();
}

void Renderer::Update()
{
	_ASSERT(m_hMainWindow);

	m_Timer.Tick();

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

		WCHAR szWindowText[512];
		swprintf_s(szWindowText, 512, L"    fps: %f     ~Million Primary Rays/s: %f    GPU[%d]: %s", fps, MRaysPerSecond, m_AdapterID, m_szAdapterDescription);
		SetWindowText(m_hMainWindow, szWindowText);
	}

	float elapsedTime = (float)m_Timer.GetElapsedSeconds();
	m_Camera.UpdateKeyboard(elapsedTime, &m_Keyboard);
	UpdateCameraMatrices();
}

void Renderer::Render()
{
	_ASSERT(m_hMainWindow);

	// If window is invisible, just return functions.
	long hwndStyle = (long)GetWindowLongPtr(m_hMainWindow, GWL_STYLE);
	if (!(hwndStyle & WS_VISIBLE) || (hwndStyle & WS_MINIMIZE))
	{
		return;
	}

	Prepare();
	DoRaytracing();
	CopyRaytracingOutputToBackbuffer();

	Present(D3D12_RESOURCE_STATE_PRESENT);
}

void Renderer::ChangeSize(UINT width, UINT height)
{
	_ASSERT(m_hMainWindow);
	if (width == 0 || height == 0)
	{
		OutputDebugString(L"width or height is 0.\n");
		return;
	}

	long hwndStyle = (long)GetWindowLongPtr(m_hMainWindow, GWL_STYLE);
	if (hwndStyle & WS_MINIMIZE)
	{
		return;
	}

	m_Width = width;
	m_Height = height;
	m_AspectRatio = (float)width / (float)height;
	m_Camera.SetAspectRatio(m_AspectRatio);

	ReleaseWindowDependentedResources();
	if (!CreateWindowDependentedResources())
	{
		BREAK_IF_FALSE(false);
	}

	if (!CreateRaytracingOutput())
	{
		BREAK_IF_FALSE(false);
	}
}

void Renderer::ProcessMosuseMove(int x, int y)
{
	m_Mouse.MouseX = x;
	m_Mouse.MouseY = y;

	// 마우스 커서의 위치를 NDC로 변환.
	// 마우스 커서는 좌측 상단 (0, 0), 우측 하단(width-1, height-1).
	// NDC는 좌측 하단이 (-1, -1), 우측 상단(1, 1).
	m_Mouse.MouseNDCX = (float)x * 2.0f / (float)m_Width - 1.0f;
	m_Mouse.MouseNDCY = (float)(-y) * 2.0f / (float)m_Height + 1.0f;

	// 커서가 화면 밖으로 나갔을 경우 범위 조절.
	m_Mouse.MouseNDCX = Clamp(m_Mouse.MouseNDCX, -1.0f, 1.0f);
	m_Mouse.MouseNDCY = Clamp(m_Mouse.MouseNDCY, -1.0f, 1.0f);

	// 카메라 시점 회전.
	m_Camera.UpdateMouse(m_Mouse.MouseNDCX, m_Mouse.MouseNDCY);
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
	if (!m_pDXGIFactory)
	{
		BREAK_IF_FALSE(false);
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

				m_AdapterID = adapterIndex;
				swprintf_s(m_szAdapterDescription, 512, desc.Description, wcslen(desc.Description));

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
	descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
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

		WCHAR szDebugName[256];
		swprintf_s(szDebugName, 256, L"CommandAllocator[%d]", i);
		m_ppCommandAllocators[i]->SetName(szDebugName);
		swprintf_s(szDebugName, 256, L"CommandList[%d]", i);
		m_ppCommandLists[i]->SetName(szDebugName);

		m_ppCommandLists[i]->Close();
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


	if (!CreateWindowDependentedResources())
	{
		BREAK_IF_FALSE(false);
		return false;
	}

	if (!CreateDeviceDependentResources())
	{
		BREAK_IF_FALSE(false);
		return false;
	}

	return true;
}

bool Renderer::InitScene()
{
	// Setup camera.
	{
		// Initialize the view and projection inverse matrices.
		m_Camera.SetAspectRatio(m_AspectRatio);
		m_Camera.SetEyePos(DirectX::SimpleMath::Vector3(13.0f, 2.0f, -10.0f));
		m_Camera.SetViewDir(-m_Camera.GetEyePos());

		UpdateCameraMatrices();
	}

	// Apply the initial values to all frames' buffer instances.
	for (UINT i = 0; i < MAX_BACK_BUFFER_COUNT; ++i)
	{
		m_FrameCB[i] = m_FrameCB[m_FrameIndex];
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
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = MAX_BACK_BUFFER_COUNT;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.SampleDesc.Quality = 0;
	swapChainDesc.Scaling = DXGI_SCALING_NONE;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
	swapChainDesc.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	if (m_Option & DeviceOption_AllowTearing)
	{
		swapChainDesc.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
	}

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

		D3D12_RESOURCE_DESC desc = m_ppRenderTargets[i]->GetDesc();

		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		rtvDesc.Format = m_BackBufferFormat;
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvDescriptor(m_pRTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
		m_pDevice->CreateRenderTargetView(m_ppRenderTargets[i], &rtvDesc, rtvDescriptor);
		++m_NumRTVDescriptorAlloced;

		WCHAR szDebugName[64];
		swprintf_s(szDebugName, 64, L"RenderTaget[%d]", i);
		m_ppRenderTargets[i]->SetName(szDebugName);
	}

	m_BackBufferFormat = swapChainDesc.Format;
	m_FrameIndex = m_pSwapChain->GetCurrentBackBufferIndex();

	return true;
}

void Renderer::ReleaseWindowDependentedResources()
{
	SAFE_COM_RELEASE(m_pRaytracingOutput);
	m_RaytracingOutputResourceUAVDescriptorHeapIndex = 0xFFFFFFFF;

	for (UINT i = 0; i < MAX_BACK_BUFFER_COUNT; ++i)
	{
		SAFE_COM_RELEASE(m_ppRenderTargets[i]);
	}
	m_NumRTVDescriptorAlloced = 0;

	SAFE_COM_RELEASE(m_pSwapChain);

	m_BackBufferFormat = DXGI_FORMAT_UNKNOWN;
	m_FrameIndex = 0;
}

bool Renderer::CreateDeviceDependentResources()
{
	_ASSERT(m_pDevice);

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

	if (!CreateConstantBuffers())
	{
		return false;
	}

	if (!BuildShaderTables())
	{
		return false;
	}

	if (!BuildAccelerationStructures())
	{
		return false;
	}

	if (!CreateRaytracingOutput())
	{
		return false;
	}

	return true;
}

bool Renderer::CreateConstantBuffers()
{
	_ASSERT(m_pDevice);

	if (m_pFrameConstants)
	{
		return false;
	}

	HRESULT hr;

	const CD3DX12_HEAP_PROPERTIES UPLOAD_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	SIZE_T cbSize = MAX_BACK_BUFFER_COUNT * sizeof(AlignedSceneConstantBuffer);
	const D3D12_RESOURCE_DESC CONSTANT_BUFFER_DESC = CD3DX12_RESOURCE_DESC::Buffer(cbSize);

	hr = m_pDevice->CreateCommittedResource(&UPLOAD_HEAP_PROPERTIES,
											D3D12_HEAP_FLAG_NONE,
											&CONSTANT_BUFFER_DESC,
											D3D12_RESOURCE_STATE_GENERIC_READ,
											nullptr,
											IID_PPV_ARGS(&m_pFrameConstants));
	if (FAILED(hr))
	{
		BREAK_IF_FAILED(hr);
		return false;
	}
	m_pFrameConstants->SetName(L"FrameConstants");

	CD3DX12_RANGE readRange(0, 0);
	hr = (m_pFrameConstants->Map(0, nullptr, (void**)&m_pMappedConstantData));
	if (FAILED(hr))
	{
		BREAK_IF_FAILED(hr);
		return false;
	}

	return true;
}

bool Renderer::CreateRaytracingOutput()
{
	if (m_pRaytracingOutput)
	{
		return false;
	}

	// Create the output resource.The dimensions and format should match the swap - chain.
	CD3DX12_RESOURCE_DESC uavDesc = CD3DX12_RESOURCE_DESC::Tex2D(m_BackBufferFormat, m_Width, m_Height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	CD3DX12_HEAP_PROPERTIES defaultHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
	HRESULT hr = m_pDevice->CreateCommittedResource(&defaultHeapProperties,
													D3D12_HEAP_FLAG_NONE,
													&uavDesc,
													D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
													nullptr,
													IID_PPV_ARGS(&m_pRaytracingOutput));
	if (FAILED(hr))
	{
		BREAK_IF_FAILED(hr);
		return false;
	}
	m_pRaytracingOutput->SetName(L"m_pRaytracingOutput");


	D3D12_CPU_DESCRIPTOR_HANDLE uavDescriptorHandle = {};
	m_RaytracingOutputResourceUAVDescriptorHeapIndex = AllocateDescriptor(&uavDescriptorHandle, m_RaytracingOutputResourceUAVDescriptorHeapIndex);

	D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
	UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	m_pDevice->CreateUnorderedAccessView(m_pRaytracingOutput, nullptr, &UAVDesc, uavDescriptorHandle);

	return true;
}

bool Renderer::CreateRootSignature()
{
	// Global Root Signature

	if (m_pRaytracingGlobalRootSignature)
	{
		return false;
	}
	{
		CD3DX12_DESCRIPTOR_RANGE ranges[2]; // Perfomance TIP: Order from most frequent to least frequent.
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);  // 1 output texture
		ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 1);  // 2 static index and vertex buffers.

		CD3DX12_ROOT_PARAMETER rootParameters[GlobalRootSignatureParams_Count];
		rootParameters[GlobalRootSignatureParams_OutputViewSlot].InitAsDescriptorTable(1, &ranges[0]);
		rootParameters[GlobalRootSignatureParams_AccelerationStructureSlot].InitAsShaderResourceView(0);
		rootParameters[GlobalRootSignatureParams_SceneConstantSlot].InitAsConstantBufferView(0);
		rootParameters[GlobalRootSignatureParams_VertexBufferSlot].InitAsDescriptorTable(1, &ranges[1]);

		CD3DX12_ROOT_SIGNATURE_DESC globalRootSignatureDesc(ARRAYSIZE(rootParameters), rootParameters);
		SerializeAndCreateRaytracingRootSignature(&globalRootSignatureDesc, &m_pRaytracingGlobalRootSignature);
	}


	// Local Root Signature

	if (m_pRaytracingLocalRootSignature)
	{
		return false;
	}
	{
		CD3DX12_ROOT_PARAMETER rootParameters[LocalRootSignatureParams_Count];
		rootParameters[LocalRootSignatureParams_MeshBufferSlot].InitAsConstants(sizeof(MeshBuffer), 1);

		CD3DX12_ROOT_SIGNATURE_DESC localRootSignatureDesc(ARRAYSIZE(rootParameters), rootParameters);
		localRootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
		SerializeAndCreateRaytracingRootSignature(&localRootSignatureDesc, &m_pRaytracingLocalRootSignature);
	}

	return true;
}

bool Renderer::CreateRaytracingPipelineStateObject()
{
	_ASSERT(m_pDevice);
	_ASSERT(g_pRaytracing);

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
	UINT payloadSize = sizeof(DirectX::XMFLOAT4) + sizeof(UINT) + sizeof(DirectX::XMFLOAT4);// float4 pixelColor
	UINT attributeSize = sizeof(DirectX::XMFLOAT2);  // float2 barycentrics
	pShaderConfig->Config(payloadSize, attributeSize);

	CreateLocalRootSignatureSubobjects(&raytracingPipeline);

	CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT* pGlobalRootSignature = raytracingPipeline.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
	pGlobalRootSignature->SetRootSignature(m_pRaytracingGlobalRootSignature);

	CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT* pPipelineConfig = raytracingPipeline.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
	UINT maxRecursionDepth = 1; // ~ primary rays only. 
	pPipelineConfig->Config(maxRecursionDepth);

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
	BLASData initBLASData = { 0, };
	const int SLICE_COUNT = 16;
	const int STACK_COUNT = 32;

	SIZE_T totalNumIndices = 0;
	SIZE_T totalNumVertices = 0;

	{
		Geometry geom = {};
		if (!CreateSphere(1000.0f, SLICE_COUNT, 512, &geom.Vertices, &geom.Indices))
		{
			BREAK_IF_FALSE(false);
			return false;
		}

		geom.Albedo = { 0.5f, 0.5f, 0.5f, 1.0f };
		geom.MaterialID = MaterialType_Lambertian;
		geom.BLASType = BLASType_BigSphere;

		DirectX::SimpleMath::Vector3 pos(0.0f, -1000.0f, 0.0f);
		DirectX::SimpleMath::Matrix transform = DirectX::SimpleMath::Matrix::CreateTranslation(pos);
		geom.Transform = DirectX::XMMatrixTranspose(DirectX::XMLoadFloat4x4(&transform));

		m_Geometry.push_back(geom);
		totalNumIndices += geom.Indices.size();
		totalNumVertices += geom.Vertices.size();

		m_BLASTypeCache.insert(std::make_pair("BigSphere", initBLASData));
	}

	{
		Geometry geom = {};
		if (!CreateSphere(0.2f, SLICE_COUNT, STACK_COUNT, &geom.Vertices, &geom.Indices))
		{
			BREAK_IF_FALSE(false);
			return false;
		}
		for (int a = -11; a < 11; ++a)
		{
			for (int b = -11; b < 11; ++b)
			{
				double chooseMat = RandomDouble();
				DirectX::SimpleMath::Vector3 center((float)a + 0.9f * RandomFloat(), 0.2f, (float)b + 0.9f * RandomFloat());
				float length = (center - DirectX::SimpleMath::Vector3(4.0f, 0.2f, 0.0f)).Length();

				geom.BLASType = BLASType_SmallSphere;

				if (length > 0.9f)
				{
					int materialType = 0;

					if (chooseMat < 0.8)
					{
						// diffuse
						geom.MaterialID = MaterialType_Lambertian;
						DirectX::XMStoreFloat4(&geom.Albedo, RandomColor() * RandomColor());
					}
					else if (chooseMat < 0.95)
					{
						// metal
						geom.MaterialID = MaterialType_Metallic;
						float fuzz = RandomFloat(0.0f, 0.5f);
						DirectX::XMStoreFloat4(&geom.Albedo, DirectX::XMVectorSet(RandomFloat(0.5f, 1.0f), RandomFloat(0.5f, 1.0f), RandomFloat(0.5f, 1.0f), fuzz));
					}
					else
					{
						// glass
						geom.MaterialID = MaterialType_Dielectric;
						DirectX::XMStoreFloat4(&geom.Albedo, DirectX::XMVectorSet(1.5f, 1.5f, 1.5f, 1.5f));
					}



					DirectX::SimpleMath::Matrix transform = DirectX::SimpleMath::Matrix::CreateTranslation(center);
					geom.Transform = DirectX::XMMatrixTranspose(DirectX::XMLoadFloat4x4(&transform));

					m_Geometry.push_back(geom);
					totalNumIndices += geom.Indices.size();
					totalNumVertices += geom.Vertices.size();
				}
			}
			m_BLASTypeCache.insert(std::make_pair("SmallSphere", initBLASData));
		}
	}
	{
		Geometry geom = {};
		if (!CreateSphere(1.0f, SLICE_COUNT, STACK_COUNT, &geom.Vertices, &geom.Indices))
		{
			BREAK_IF_FALSE(false);
			return false;
		}

		{
			geom.Albedo = DirectX::XMFLOAT4(1.5f, 1.5f, 1.5f, 1.5f);
			geom.MaterialID = MaterialType_Dielectric;
			geom.BLASType = BLASType_MiddleSphere;

			DirectX::SimpleMath::Matrix transform = DirectX::SimpleMath::Matrix::CreateTranslation(DirectX::SimpleMath::Vector3::UnitY);
			geom.Transform = DirectX::XMMatrixTranspose(DirectX::XMLoadFloat4x4(&transform));

			m_Geometry.push_back(geom);
			totalNumIndices += geom.Indices.size();
			totalNumVertices += geom.Vertices.size();
		}
		{
			geom.Albedo = DirectX::XMFLOAT4(0.4f, 0.2f, 0.1f, 0.0f);
			geom.MaterialID = MaterialType_Lambertian;
			geom.BLASType = BLASType_MiddleSphere;

			DirectX::SimpleMath::Vector3 pos(4.0f, 1.0f, 0.0f);
			DirectX::SimpleMath::Matrix transform = DirectX::SimpleMath::Matrix::CreateTranslation(pos);
			geom.Transform = DirectX::XMMatrixTranspose(DirectX::XMLoadFloat4x4(&transform));

			m_Geometry.push_back(geom);
			totalNumIndices += geom.Indices.size();
			totalNumVertices += geom.Vertices.size();
		}
		{
			geom.Albedo = DirectX::XMFLOAT4(0.7f, 0.6f, 0.5f, 0.0f);
			geom.MaterialID = MaterialType_Metallic;
			geom.BLASType = BLASType_MiddleSphere;

			DirectX::SimpleMath::Vector3 pos(-4.0f, 1.0f, 0.0f);
			DirectX::SimpleMath::Matrix transform = DirectX::SimpleMath::Matrix::CreateTranslation(pos);
			geom.Transform = DirectX::XMMatrixTranspose(DirectX::XMLoadFloat4x4(&transform));

			m_Geometry.push_back(geom);
			totalNumIndices += geom.Indices.size();
			totalNumVertices += geom.Vertices.size();
		}

		m_BLASTypeCache.insert(std::make_pair("MiddleSphere", initBLASData));
	}


	/*for (Geometry const& geometry : m_Geometry)
	{
		totalNumIndices += geometry.Indices.size();
		totalNumVertices += geometry.Vertices.size();
	}*/
	/*for (SIZE_T i = 0, size = m_Geometry.size(); i < size; ++i)
	{
		Geometry& geom = m_Geometry[i];
		switch (geom.BLASType)
		{
		case BLASType_BigSphere:
			if (!bDataInsertFlags[BLASType_BigSphere])
			{
				bDataInsertFlags[BLASType_BigSphere] = true;
				totalNumIndices += geom.Indices.size();
				totalNumVertices += geom.Vertices.size();
			}
			break;

		case BLASType_MiddleSphere:
			if (!bDataInsertFlags[BLASType_MiddleSphere])
			{
				bDataInsertFlags[BLASType_MiddleSphere] = true;
				totalNumIndices += geom.Indices.size();
				totalNumVertices += geom.Vertices.size();
			}
			break;

		case BLASType_SmallSphere:
			if (!bDataInsertFlags[BLASType_SmallSphere])
			{
				bDataInsertFlags[BLASType_SmallSphere] = true;
				totalNumIndices += geom.Indices.size();
				totalNumVertices += geom.Vertices.size();
			}
			break;

		default:
			BREAK_IF_FALSE(false);
			break;
		}
	}*/

	std::vector<Index> indices(totalNumIndices);
	std::vector<Vertex> vertices(totalNumVertices);
	SIZE_T vertexOffset = 0;
	SIZE_T indexOffset = 0;
	bool bDataInsertFlags[BLASType_Count] = { false, };
	for (SIZE_T i = 0, size = m_Geometry.size(); i < size; ++i)
	{
		Geometry& geom = m_Geometry[i];

		switch (geom.BLASType)
		{
		case BLASType_BigSphere:
			if (bDataInsertFlags[BLASType_BigSphere])
			{
				continue;
			}
			break;

		case BLASType_MiddleSphere:
			if (bDataInsertFlags[BLASType_MiddleSphere])
			{
				continue;
			}
			break;

		case BLASType_SmallSphere:
			if (bDataInsertFlags[BLASType_SmallSphere])
			{
				continue;
			}
			break;

		default:
			BREAK_IF_FALSE(false);
			break;
		}

		bDataInsertFlags[geom.BLASType] = true;

		memcpy(&vertices[vertexOffset], geom.Vertices.data(), geom.Vertices.size() * sizeof(Vertex));
		memcpy(indices.data() + indexOffset, geom.Indices.data(), geom.Indices.size() * sizeof(Index));
		geom.IndicesOffsetInBytes = indexOffset * sizeof(Index);
		geom.VerticesOffsetInBytes = vertexOffset * sizeof(Vertex);

		vertexOffset += geom.Vertices.size();
		indexOffset += geom.Indices.size();
	}
	if (!AllocateUploadBuffer(vertices.data(), vertices.size() * sizeof(Vertex), (ID3D12Resource**)&m_VertexBuffer.pResource, L"VertexBuffer"))
	{
		BREAK_IF_FALSE(false);
		return false;
	}
	if (!AllocateUploadBuffer(indices.data(), indices.size() * sizeof(Index), (ID3D12Resource**)&m_IndexBuffer.pResource, L"IndexBuffer"))
	{
		BREAK_IF_FALSE(false);
		return false;
	}


	// Vertex buffer is passed to the shader along with index buffer as a descriptor table.
	// Vertex buffer descriptor must follow index buffer descriptor in the descriptor heap.
	UINT descriptorIndexIB = CreateBufferSRV(&m_IndexBuffer, (UINT)indices.size(), sizeof(Index));
	UINT descriptorIndexVB = CreateBufferSRV(&m_VertexBuffer, (UINT)vertices.size(), sizeof(Vertex));
	if (descriptorIndexVB != descriptorIndexIB + 1)
	{
		OutputDebugString(L"Vertex Buffer descriptor index must follow that of Index Buffer descriptor index!\n");
		BREAK_IF_FALSE(false);
		return false;
	}

	return true;
}

bool Renderer::BuildAccelerationStructures()
{
	_ASSERT(m_ppCommandLists[m_FrameIndex]);
	_ASSERT(m_ppCommandAllocators[m_FrameIndex]);
	_ASSERT(!m_pTopLevelAccelerationStructure);

	ID3D12GraphicsCommandList4* pCommandList = m_ppCommandLists[m_FrameIndex];

	std::vector<ID3D12Resource*> scratches; // just to expand lifetime
	std::vector<D3D12_RAYTRACING_INSTANCE_DESC> instanceDescriptors;
	std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geometryDescs;

	// Reset the command list for the acceleration structure construction.
	pCommandList->Reset(m_ppCommandAllocators[m_FrameIndex], nullptr);

	int i = 0;
	std::map<std::string, BLASData>::iterator endIter = m_BLASTypeCache.end();
	for (SIZE_T i = 0, size = m_Geometry.size(); i < size; ++i)
	{
		Geometry& geometry = m_Geometry[i];
		ID3D12Resource2** ppBottomLevelAccelerationStructure = &geometry.pBottomLevelAccelerationStructure;
		_ASSERT(ppBottomLevelAccelerationStructure && !*ppBottomLevelAccelerationStructure);

		std::map<std::string, BLASData>::iterator iter;
		switch (geometry.BLASType)
		{
		case BLASType_BigSphere:
			iter = m_BLASTypeCache.find("BigSphere");
			break;

		case BLASType_MiddleSphere:
			iter = m_BLASTypeCache.find("MiddleSphere");
			break;

		case BLASType_SmallSphere:
			iter = m_BLASTypeCache.find("SmallSphere");
			break;

		default:
			BREAK_IF_FALSE(false);
			break;
		}
		if (iter == endIter)
		{
			BREAK_IF_FALSE(false);
			return false;
		}

		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC bottomLevelBuildDesc = {};
		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO bottomLevelPrebuildInfo = {};
		if (iter->second.pBottomLevelAS == nullptr)
		{
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
			//D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_MINIMIZE_MEMORY;

			D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS* pBottomLevelInputs = &bottomLevelBuildDesc.Inputs;
			pBottomLevelInputs->DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
			pBottomLevelInputs->Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
			pBottomLevelInputs->NumDescs = 1;
			pBottomLevelInputs->Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
			pBottomLevelInputs->pGeometryDescs = &geometryDesc;

			m_pDevice->GetRaytracingAccelerationStructurePrebuildInfo(pBottomLevelInputs, &bottomLevelPrebuildInfo);
			if (bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes <= 0)
			{
				BREAK_IF_FALSE(false);
				return false;
			}

			if (!AllocateUAVBuffer(bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes, (ID3D12Resource**)ppBottomLevelAccelerationStructure, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, L"BottomLevelAccelerationStructure"))
			{
				BREAK_IF_FALSE(false);
				return false;
			}

			memcpy(&(iter->second.BottomLevelPrebuildInfo), &bottomLevelPrebuildInfo, sizeof(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO));
			iter->second.pBottomLevelAS = *ppBottomLevelAccelerationStructure;


			ID3D12Resource* pScratchResource = nullptr;
			if (!AllocateUAVBuffer(bottomLevelPrebuildInfo.ScratchDataSizeInBytes, &pScratchResource, D3D12_RESOURCE_STATE_COMMON, L"ScratchResource"))
			{
				BREAK_IF_FALSE(false);
				return false;
			}
			scratches.push_back(pScratchResource); // extend up to execute

			bottomLevelBuildDesc.ScratchAccelerationStructureData = pScratchResource->GetGPUVirtualAddress();
			bottomLevelBuildDesc.DestAccelerationStructureData = (*ppBottomLevelAccelerationStructure)->GetGPUVirtualAddress();

			// Build acceleration structure.
			pCommandList->BuildRaytracingAccelerationStructure(&bottomLevelBuildDesc, 0, nullptr);
			CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::UAV(*ppBottomLevelAccelerationStructure);
			pCommandList->ResourceBarrier(1, &barrier);
		}
		else
		{
			memcpy(&bottomLevelPrebuildInfo, &(iter->second.BottomLevelPrebuildInfo), sizeof(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO));
			*ppBottomLevelAccelerationStructure = (ID3D12Resource2*)(iter->second.pBottomLevelAS);
		}

		/*
		* 실험해볼 것.
		* 만약, 0.2f짜리 구를 blas 하나만 사용해서 쓸 수 있으면,
		* 초기에 blas 하나 생성해서 캐싱하고,
		* 이거 사용해서 instance desc만 바꿔주면 된다.
		*
		* 캐싱된 ID3D12Resource는 밑에 있는 D3D12_RAYTRACING_INSTANCE_DESC의 AccelerationStructure에 넣어주고,
		* instanceID, mask, InstanceContributionToHitGroupIndex는 그대로 써주면 된다.
		*
		* 이걸 같게 맞추면 안돼나? 생각할 수 있는데,
		* Tlas와 Blas의 존재 의의를 잘생각해보면 답은 간단하다.
		* Tlas는 Blas를 참조하는 instance들의 모음을 구성한 것이다.
		* 즉, 렌더링에 필요한 instance들은 모두 1개씩 존재해야한다.
		* 이는 각 instance가 GPU에서 작업될때, 작업되는 셰이더를 한 세트로 판별하여 구분하기 위함이다.
		* 그러니깐, instance-hit sharder 짝을 각각 구분하기 위해 붙여주는 인덱스다.
		* 그래서 이건 instance마다 각각 구분하는게 맞으니, i 그대로 유지하는거다.
		*
		* 여전히 안됨.
		* 생각해보니, 샘플에서도 같은 머터리얼에 대해서만 blas를 생성하고 인스턴스를 생성했던 것 같음.
		* 그러면 smallsphere에 대해서도 이를 나눠야 하는걸까?
		*/


		D3D12_RAYTRACING_INSTANCE_DESC instanceDesc = {};
		memcpy(instanceDesc.Transform, geometry.Transform.r, 12 * sizeof(float));
		//instanceDesc.InstanceMask = 1;
		instanceDesc.InstanceMask = 0xFF;
		instanceDesc.InstanceID = i;
		//instanceDesc.InstanceContributionToHitGroupIndex = i;
		instanceDesc.InstanceContributionToHitGroupIndex = iter->second.InstanceContributionToHitGroupIndex;
		instanceDesc.AccelerationStructure = (*ppBottomLevelAccelerationStructure)->GetGPUVirtualAddress();

		instanceDescriptors.push_back(instanceDesc);

		++i;
	}

	ID3D12Resource* pInstanceDescs = nullptr;
	if (!AllocateUploadBuffer(instanceDescriptors.data(), sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * instanceDescriptors.size(), &pInstanceDescs, L"InstanceDescs"))
	{
		BREAK_IF_FALSE(false);
		return false;
	}

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC topLevelBuildDesc = {};
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS* pTopLevelInputs = &topLevelBuildDesc.Inputs;
	pTopLevelInputs->DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	pTopLevelInputs->Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
	pTopLevelInputs->NumDescs = (UINT)geometryDescs.size();
	pTopLevelInputs->Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
	pTopLevelInputs->InstanceDescs = pInstanceDescs->GetGPUVirtualAddress();

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO topLevelPrebuildInfo = {};
	m_pDevice->GetRaytracingAccelerationStructurePrebuildInfo(pTopLevelInputs, &topLevelPrebuildInfo);
	if (topLevelPrebuildInfo.ResultDataMaxSizeInBytes <= 0)
	{
		__debugbreak();
	}


	ID3D12Resource* pScratchResource = nullptr;
	if (!AllocateUAVBuffer(topLevelPrebuildInfo.ScratchDataSizeInBytes, &pScratchResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"ScratchResource"))
	{
		BREAK_IF_FALSE(false);
		return false;
	}
	if (!AllocateUAVBuffer(topLevelPrebuildInfo.ResultDataMaxSizeInBytes, (ID3D12Resource**)&m_pTopLevelAccelerationStructure, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, L"TopLevelAccelerationStructure"))
	{
		BREAK_IF_FALSE(false);
		return false;
	}

	// Top Level Acceleration Structure desc
	topLevelBuildDesc.DestAccelerationStructureData = m_pTopLevelAccelerationStructure->GetGPUVirtualAddress();
	topLevelBuildDesc.ScratchAccelerationStructureData = pScratchResource->GetGPUVirtualAddress();

	pCommandList->BuildRaytracingAccelerationStructure(&topLevelBuildDesc, 0, nullptr);

	// Kick off acceleration structure construction.
	ExecuteCommandList();

	// Wait for GPU to finish as the locally created temporary GPU resources will get released once we go out of scope.
	Fence();
	WaitForGPU();

	for (SIZE_T i = 0, size = scratches.size(); i < size; ++i)
	{
		SAFE_COM_RELEASE(scratches[i]);
	}
	SAFE_COM_RELEASE(pScratchResource);
	SAFE_COM_RELEASE(pInstanceDescs);

	return true;

//LB_FAILED:
//	for (SIZE_T i = 0, size = scratches.size(); i < size; ++i)
//	{
//		if (scratches[i])
//		{
//			scratches[i]->Release();
//		}
//	}
//	if (pScratchResource)
//	{
//		pScratchResource->Release();
//	}
//	if (pInstanceDescs)
//	{
//		pInstanceDescs->Release();
//	}
//
//	return true;
}

bool Renderer::BuildShaderTables()
{
	_ASSERT(m_pDevice);
	_ASSERT(!m_pRayGenShaderTable);
	_ASSERT(!m_pMissShaderTable);
	_ASSERT(!m_pHitGroupShaderTable);

	void* pRayGenShaderIdentifier = nullptr;
	void* pMissShaderIdentifier = nullptr;
	void* pHitGroupShaderIdentifier = nullptr;

	// Get shader identifiers.
	UINT shaderIdentifierSize;
	{
		ID3D12StateObjectProperties* pStateObjectProperties = nullptr;
		if (FAILED(m_pStateObject->QueryInterface(IID_PPV_ARGS(&pStateObjectProperties))))
		{
			__debugbreak();
		}

		pRayGenShaderIdentifier = pStateObjectProperties->GetShaderIdentifier(szRAYGEN_SHADER_NAME);
		pMissShaderIdentifier = pStateObjectProperties->GetShaderIdentifier(szMISS_SHADER_NAME);
		pHitGroupShaderIdentifier = pStateObjectProperties->GetShaderIdentifier(szHIT_GROUP_NAME);

		shaderIdentifierSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;

		pStateObjectProperties->Release();
	}

	// Ray gen shader table
	{
		UINT numShaderRecords = 1;
		UINT shaderRecordSize = shaderIdentifierSize;
		ShaderTable rayGenShaderTable(m_pDevice, numShaderRecords, shaderRecordSize, L"RayGenShaderTable");
		rayGenShaderTable.PushBack(ShaderRecord(pRayGenShaderIdentifier, shaderIdentifierSize));
		m_pRayGenShaderTable = rayGenShaderTable.GetResource();
		m_pRayGenShaderTable->AddRef();
	}

	// Miss shader table
	{
		UINT numShaderRecords = 1;
		UINT shaderRecordSize = shaderIdentifierSize;
		ShaderTable missShaderTable(m_pDevice, numShaderRecords, shaderRecordSize, L"MissShaderTable");
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
		bool bBLASTypeInsertedFlag[BLASType_Count] = { false, };

		// vertex, index offset을 BLAS cache에 맞춰줘야 한다.
		for (SIZE_T i = 0, size = m_Geometry.size(); i < size; ++i)
		{
			Geometry& geom = m_Geometry[i];
			switch (geom.BLASType)
			{
			case BLASType_BigSphere:
				if (bBLASTypeInsertedFlag[BLASType_BigSphere])
				{
					continue;
				}
				break;

			case BLASType_MiddleSphere:
				if (bBLASTypeInsertedFlag[BLASType_MiddleSphere])
				{
					continue;
				}
				break;

			case BLASType_SmallSphere:
				if (bBLASTypeInsertedFlag[BLASType_SmallSphere])
				{
					continue;
				}
				break;

			default:
				BREAK_IF_FALSE(false);
				break;
			}

			bBLASTypeInsertedFlag[geom.BLASType] = true;

			MeshBuffer meshBuffer;
			meshBuffer.MeshID = (int)i;
			meshBuffer.MaterialID = m_Geometry[i].MaterialID;
			meshBuffer.Albedo = m_Geometry[i].Albedo;
			meshBuffer.VerticesOffset = verticesOffset;
			meshBuffer.IndicesOffset = indicesOffset;

			verticesOffset += (UINT)m_Geometry[i].Vertices.size();
			indicesOffset += (UINT)m_Geometry[i].Indices.size();

			args.push_back(meshBuffer);
		}

		UINT numShaderRecords = (UINT)args.size();
		UINT shaderRecordSize = shaderIdentifierSize + sizeof(MeshBuffer);
		ShaderTable hitGroupShaderTable(m_pDevice, numShaderRecords, shaderRecordSize, L"HitGroupShaderTable");
		for (SIZE_T i = 0, size = args.size(); i < size; ++i)
		{
			// 뭔가 돌면서 contributionIndex를 설정해줘야 하는 듯 하다..
			// 이게 도대체 무슨 역할을 하는지는 모르겠음.

			UINT shaderRecordOffset = (UINT)hitGroupShaderTable.GetNumShaderRecords();
			{
				std::map<std::string, BLASData>::iterator iter;
				switch (m_Geometry[i].BLASType)
				{
				case BLASType_BigSphere:
					iter = m_BLASTypeCache.find("BigSphere");
					break;

				case BLASType_MiddleSphere:
					iter = m_BLASTypeCache.find("MiddleSphere");
					break;

				case BLASType_SmallSphere:
					iter = m_BLASTypeCache.find("SmallSphere");
					break;

				default:
					BREAK_IF_FALSE(false);
					break;
				}

				if (iter == m_BLASTypeCache.end())
				{
					BREAK_IF_FALSE(false);
					return false;
				}

				if (iter->second.InstanceContributionToHitGroupIndex == 0)
				{
					iter->second.InstanceContributionToHitGroupIndex = shaderRecordOffset;
				}

				WCHAR szDebugString[MAX_PATH] = { 0, };
				swprintf_s(szDebugString, MAX_PATH, L"%s: %u", iter->first.c_str(), shaderRecordOffset);
				OutputDebugString(szDebugString);
			}

			MeshBuffer* pArg = &args[i];
			hitGroupShaderTable.PushBack(ShaderRecord(pHitGroupShaderIdentifier, shaderIdentifierSize, pArg, sizeof(MeshBuffer)));
		}

		m_HitgroupShaderTableSize = hitGroupShaderTable.GetShaderRecordSize();
		m_pHitGroupShaderTable = hitGroupShaderTable.GetResource();
		m_pHitGroupShaderTable->AddRef();
	}

	return true;
}

bool Renderer::AllocateUploadBuffer(void* pData, UINT64 dataSize, ID3D12Resource** ppOutResource, const WCHAR* pszResourceName)
{
	_ASSERT(m_pDevice);
	_ASSERT(dataSize > 0);
	_ASSERT(ppOutResource);

	if (*ppOutResource)
	{
		return false;
	}


	HRESULT hr;

	CD3DX12_HEAP_PROPERTIES uploadHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
	CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(dataSize);
	hr = m_pDevice->CreateCommittedResource(&uploadHeapProperties,
											D3D12_HEAP_FLAG_NONE,
											&bufferDesc,
											D3D12_RESOURCE_STATE_GENERIC_READ,
											nullptr,
											IID_PPV_ARGS(ppOutResource));
	if (FAILED(hr))
	{
		BREAK_IF_FAILED(hr);
		return false;
	}
	if (pszResourceName)
	{
		(*ppOutResource)->SetName(pszResourceName);
	}

	if (pData)
	{
		CD3DX12_RANGE readRange(0, 0);
		void* pMappedData = nullptr;

		hr = (*ppOutResource)->Map(0, &readRange, &pMappedData);
		if (FAILED(hr))
		{
			BREAK_IF_FAILED(hr);
			return false;
		}
		memcpy(pMappedData, pData, dataSize);
		(*ppOutResource)->Unmap(0, nullptr);
	}

	return true;
}

bool Renderer::AllocateUAVBuffer(UINT64 bufferSize, ID3D12Resource** ppOutResource, D3D12_RESOURCE_STATES initialResourceState, const WCHAR* pszResourceName)
{
	_ASSERT(m_pDevice);
	_ASSERT(bufferSize > 0);
	_ASSERT(ppOutResource);

	if (*ppOutResource)
	{
		return false;
	}


	HRESULT hr;

	CD3DX12_HEAP_PROPERTIES uploadHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
	CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	hr = m_pDevice->CreateCommittedResource(&uploadHeapProperties,
											D3D12_HEAP_FLAG_NONE,
											&bufferDesc,
											initialResourceState,
											nullptr,
											IID_PPV_ARGS(ppOutResource));
	if (FAILED(hr))
	{
		BREAK_IF_FAILED(hr);
		return false;
	}
	if (pszResourceName)
	{
		(*ppOutResource)->SetName(pszResourceName);
	}

	return true;
}

UINT Renderer::AllocateDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE* pOutCPUDescriptor, UINT descriptorIndexToUse)
{
	_ASSERT(m_pCBVSRVUAVDescriptorHeap);
	_ASSERT(m_pDevice);
	_ASSERT(pOutCPUDescriptor);

	D3D12_CPU_DESCRIPTOR_HANDLE descriptorHeapCPUBase = m_pCBVSRVUAVDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = m_pCBVSRVUAVDescriptorHeap->GetDesc();
	if (m_NumCBVSRVUAVDescriptorAlloced == heapDesc.NumDescriptors || descriptorIndexToUse < m_NumCBVSRVUAVDescriptorAlloced)
	{
		return UINT_MAX;
	}

	descriptorIndexToUse = m_NumCBVSRVUAVDescriptorAlloced++;
	*pOutCPUDescriptor = CD3DX12_CPU_DESCRIPTOR_HANDLE(descriptorHeapCPUBase, descriptorIndexToUse, m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

	return descriptorIndexToUse;
}

UINT Renderer::CreateBufferSRV(D3DBuffer* pOutBuffer, UINT numElements, UINT elementSize)
{
	_ASSERT(m_pDevice);
	_ASSERT(m_pCBVSRVUAVDescriptorHeap);
	_ASSERT(pOutBuffer);
	_ASSERT(numElements >= 1);

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

	UINT descriptorIndex = AllocateDescriptor(&pOutBuffer->CPUDescriptorHandle);
	m_pDevice->CreateShaderResourceView(pOutBuffer->pResource, &srvDesc, pOutBuffer->CPUDescriptorHandle);
	pOutBuffer->GPUDescriptorHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_pCBVSRVUAVDescriptorHeap->GetGPUDescriptorHandleForHeapStart(),
																	descriptorIndex,
																	m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

	return descriptorIndex;
}

void Renderer::UpdateCameraMatrices()
{
	m_FrameCB[m_FrameIndex].CameraPosition = m_Camera.GetEyePos();
	DirectX::XMMATRIX view = m_Camera.GetView();
	DirectX::XMMATRIX proj = m_Camera.GetProjection();

	m_FrameCB[m_FrameIndex].ProjectionToWorld = DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(nullptr, proj));
	m_FrameCB[m_FrameIndex].ModelViewInverse = DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(nullptr, view));
}

void Renderer::Prepare(D3D12_RESOURCE_STATES beforeState)
{
	_ASSERT(m_ppCommandAllocators[m_FrameIndex]);
	_ASSERT(m_ppCommandLists[m_FrameIndex]);

	HRESULT hr;

	hr = m_ppCommandAllocators[m_FrameIndex]->Reset();
	if (FAILED(hr))
	{
		__debugbreak();
		return;
	}
	hr = m_ppCommandLists[m_FrameIndex]->Reset(m_ppCommandAllocators[m_FrameIndex], nullptr);
	if (FAILED(hr))
	{
		__debugbreak();
		return;
	}

	if (beforeState != D3D12_RESOURCE_STATE_RENDER_TARGET)
	{
		// Transition the render target into the correct state to allow for drawing into it.
		D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_ppRenderTargets[m_FrameIndex], beforeState, D3D12_RESOURCE_STATE_RENDER_TARGET);
		m_ppCommandLists[m_FrameIndex]->ResourceBarrier(1, &barrier);
	}
}

void Renderer::DoRaytracing()
{
	_ASSERT(m_pDevice);
	_ASSERT(m_ppCommandLists[m_FrameIndex]);
	_ASSERT(m_pCBVSRVUAVDescriptorHeap);
	_ASSERT(m_pRaytracingGlobalRootSignature);
	_ASSERT(m_IndexBuffer.pResource);
	_ASSERT(m_pTopLevelAccelerationStructure);
	_ASSERT(m_pHitGroupShaderTable);
	_ASSERT(m_pMissShaderTable);
	_ASSERT(m_pRayGenShaderTable);
	_ASSERT(m_pStateObject);

	ID3D12GraphicsCommandList5* pCommandList = m_ppCommandLists[m_FrameIndex];

	pCommandList->SetComputeRootSignature(m_pRaytracingGlobalRootSignature);

	// Copy the updated scene constant buffer to GPU.
	memcpy(&m_pMappedConstantData[m_FrameIndex].Constants, &m_FrameCB[m_FrameIndex], sizeof(FrameBuffer));
	ULONGLONG cbGpuAddress = m_pFrameConstants->GetGPUVirtualAddress() + m_FrameIndex * sizeof(AlignedSceneConstantBuffer);
	pCommandList->SetComputeRootConstantBufferView(GlobalRootSignatureParams_SceneConstantSlot, cbGpuAddress);

	// Bind the heaps, acceleration structure and dispatch rays.
	pCommandList->SetDescriptorHeaps(1, &m_pCBVSRVUAVDescriptorHeap);
	// Set index and successive vertex buffer decriptor tables
	CD3DX12_GPU_DESCRIPTOR_HANDLE raytracingOutputGPUDescriptor = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_pCBVSRVUAVDescriptorHeap->GetGPUDescriptorHandleForHeapStart(),
																								m_RaytracingOutputResourceUAVDescriptorHeapIndex,
																								m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
	pCommandList->SetComputeRootDescriptorTable(GlobalRootSignatureParams_VertexBufferSlot, m_IndexBuffer.GPUDescriptorHandle);
	pCommandList->SetComputeRootDescriptorTable(GlobalRootSignatureParams_OutputViewSlot, raytracingOutputGPUDescriptor);
	pCommandList->SetComputeRootShaderResourceView(GlobalRootSignatureParams_AccelerationStructureSlot, m_pTopLevelAccelerationStructure->GetGPUVirtualAddress());

	D3D12_DISPATCH_RAYS_DESC dispatchDesc = {};
	// Since each shader table has only one shader record, the stride is same as the size.
	dispatchDesc.HitGroupTable.StartAddress = m_pHitGroupShaderTable->GetGPUVirtualAddress();
	dispatchDesc.HitGroupTable.SizeInBytes = m_pHitGroupShaderTable->GetDesc().Width;
	//dispatchDesc.HitGroupTable.StrideInBytes = dispatchDesc.HitGroupTable.SizeInBytes / m_Geometry.size();
	dispatchDesc.HitGroupTable.StrideInBytes = m_HitgroupShaderTableSize;
	dispatchDesc.MissShaderTable.StartAddress = m_pMissShaderTable->GetGPUVirtualAddress();
	dispatchDesc.MissShaderTable.SizeInBytes = m_pMissShaderTable->GetDesc().Width;
	dispatchDesc.MissShaderTable.StrideInBytes = dispatchDesc.MissShaderTable.SizeInBytes;
	dispatchDesc.RayGenerationShaderRecord.StartAddress = m_pRayGenShaderTable->GetGPUVirtualAddress();
	dispatchDesc.RayGenerationShaderRecord.SizeInBytes = m_pRayGenShaderTable->GetDesc().Width;
	dispatchDesc.Width = m_Width;
	dispatchDesc.Height = m_Height;
	dispatchDesc.Depth = 1;

	pCommandList->SetPipelineState1(m_pStateObject);
	pCommandList->DispatchRays(&dispatchDesc);
}

void Renderer::CopyRaytracingOutputToBackbuffer()
{
	_ASSERT(m_ppCommandLists[m_FrameIndex]);
	_ASSERT(m_ppRenderTargets[m_FrameIndex]);
	_ASSERT(m_pRaytracingOutput);

	ID3D12GraphicsCommandList* pCommandList = m_ppCommandLists[m_FrameIndex];
	ID3D12Resource* pRenderTarget = m_ppRenderTargets[m_FrameIndex];

	D3D12_RESOURCE_BARRIER preCopyBarriers[2] = {};
	preCopyBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(pRenderTarget, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST);
	preCopyBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(m_pRaytracingOutput, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
	pCommandList->ResourceBarrier(ARRAYSIZE(preCopyBarriers), preCopyBarriers);

	pCommandList->CopyResource(pRenderTarget, m_pRaytracingOutput);

	D3D12_RESOURCE_BARRIER postCopyBarriers[2] = {};
	postCopyBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(pRenderTarget, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT);
	postCopyBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(m_pRaytracingOutput, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	pCommandList->ResourceBarrier(ARRAYSIZE(postCopyBarriers), postCopyBarriers);
}

void Renderer::ExecuteCommandList()
{
	_ASSERT(m_ppCommandLists[m_FrameIndex]);
	_ASSERT(m_pCommandQueue);

	if (FAILED(m_ppCommandLists[m_FrameIndex]->Close()))
	{
		OutputDebugString(L"ExecuteCommandList fail.\n");
		BREAK_IF_FALSE(false);
		return;
	}

	ID3D12CommandList* ppCommandLists[] = { m_ppCommandLists[m_FrameIndex] };
	m_pCommandQueue->ExecuteCommandLists(ARRAYSIZE(ppCommandLists), ppCommandLists);
}

void Renderer::Present(D3D12_RESOURCE_STATES beforeState)
{
	_ASSERT(m_ppCommandLists[m_FrameIndex]);
	_ASSERT(m_pSwapChain);

	if (beforeState != D3D12_RESOURCE_STATE_PRESENT)
	{
		// Transition the render target to the state that allows it to be presented to the display.
		D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_ppRenderTargets[m_FrameIndex], beforeState, D3D12_RESOURCE_STATE_PRESENT);
		m_ppCommandLists[m_FrameIndex]->ResourceBarrier(1, &barrier);
	}

	ExecuteCommandList();

	HRESULT hr;
	if (m_Option & DeviceOption_AllowTearing)
	{
		hr = m_pSwapChain->Present(0, DXGI_PRESENT_ALLOW_TEARING);
	}
	else
	{
		hr = m_pSwapChain->Present(1, 0);
	}

	if (FAILED(hr))
	{
		__debugbreak();
		return;
	}

	Fence();
	WaitForGPU();

	// next frame index.
	m_FrameIndex = m_pSwapChain->GetCurrentBackBufferIndex();
}

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
		return;
	}

	if (FAILED(m_pDevice->CreateRootSignature(1, pBlob->GetBufferPointer(), pBlob->GetBufferSize(), IID_PPV_ARGS(ppRootSig))))
	{
		__debugbreak();
	}

	pBlob->Release();
}

void Renderer::Fence()
{
	_ASSERT(m_pCommandQueue);
	_ASSERT(m_pFence);

	++m_FenceValues[m_FrameIndex];
	if (FAILED(m_pCommandQueue->Signal(m_pFence, m_FenceValues[m_FrameIndex])))
	{
		__debugbreak();
	}
}

void Renderer::WaitForGPU()
{
	_ASSERT(m_pFence);
	_ASSERT(m_hFenceEvent);

	UINT64 lastFenceValue = m_pFence->GetCompletedValue();
	if (lastFenceValue < m_FenceValues[m_FrameIndex])
	{
		m_pFence->SetEventOnCompletion(m_FenceValues[m_FrameIndex], m_hFenceEvent);
		WaitForSingleObject(m_hFenceEvent, INFINITE);
	}
}
