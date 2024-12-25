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

}

void Application::CreateRTPipelineState()
{}

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
