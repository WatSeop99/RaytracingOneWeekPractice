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
#include "DeviceResources.h"
#include "Win32Application.h"

using namespace DX;
using namespace std;

using Microsoft::WRL::ComPtr;

namespace
{
    inline DXGI_FORMAT NoSRGB(DXGI_FORMAT fmt)
    {
        switch (fmt)
        {
            case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:   return DXGI_FORMAT_R8G8B8A8_UNORM;
            case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:   return DXGI_FORMAT_B8G8R8A8_UNORM;
            case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:   return DXGI_FORMAT_B8G8R8X8_UNORM;
            default:                                return fmt;
        }
    }
};

// Constructor for DeviceResources.
DeviceResources::DeviceResources(DXGI_FORMAT backBufferFormat, DXGI_FORMAT depthBufferFormat, UINT backBufferCount, D3D_FEATURE_LEVEL minFeatureLevel, UINT flags, UINT adapterIDoverride) :
    m_BackBufferFormat(backBufferFormat),
    m_DepthBufferFormat(depthBufferFormat),
    m_BackBufferCount(backBufferCount),
    m_D3DMinFeatureLevel(minFeatureLevel),
    m_Options(flags),
    m_AdapterIDoverride(adapterIDoverride)
{
    if (backBufferCount > MAX_BACK_BUFFER_COUNT)
    {
        throw out_of_range("backBufferCount too large");
    }

    if (minFeatureLevel < D3D_FEATURE_LEVEL_11_0)
    {
        throw out_of_range("minFeatureLevel too low");
    }
    if (m_Options & REQUIRE_TEARING_SUPPORT)
    {
        m_Options |= ALLOW_TEARING;
    }
}

// Destructor for DeviceResources.
DeviceResources::~DeviceResources()
{
    // Ensure that the GPU is no longer referencing resources that are about to be destroyed.
    WaitForGPU();

    if (m_hFenceEvent)
    {
        CloseHandle(m_hFenceEvent);
        m_hFenceEvent = nullptr;
    }

    for (UINT n = 0; n < m_BackBufferCount; ++n)
    {
        if (m_ppCommandAllocators[n])
        {
            m_ppCommandAllocators[n]->Release();
            m_ppCommandAllocators[n] = nullptr;
        }
        if (m_ppRenderTargets[n])
        {
            m_ppRenderTargets[n]->Release();
            m_ppRenderTargets[n] = nullptr;
        }
    }
    if (m_pDepthStencil)
    {
        m_pDepthStencil->Release();
        m_pDepthStencil = nullptr;
    }
    if (m_pFence)
    {
        m_pFence->Release();
        m_pFence = nullptr;
    }
    if (m_pRTVDescriptorHeap)
    {
        m_pRTVDescriptorHeap->Release();
        m_pRTVDescriptorHeap = nullptr;
    }
    if (m_pDSVDescriptorHeap)
    {
        m_pDSVDescriptorHeap->Release();
        m_pDSVDescriptorHeap = nullptr;
    }
    if (m_pCommandList)
    {
        m_pCommandList->Release();
        m_pCommandList = nullptr;
    }
    if (m_pCommandQueue)
    {
        m_pCommandQueue->Release();
        m_pCommandQueue = nullptr;
    }
    if (m_pSwapChain)
    {
        m_pSwapChain->Release();
        m_pSwapChain = nullptr;
    }
    if (m_pDXGIFactory)
    {
        m_pDXGIFactory->Release();
        m_pDXGIFactory = nullptr;
    }
    if (m_pAdapter)
    {
        m_pAdapter->Release();
        m_pAdapter = nullptr;
    }
    if (m_pD3DDevice)
    {
        /*ULONG ref = m_pD3DDevice->Release();
        if (ref != 0)
        {
            __debugbreak();
        }*/
        m_pD3DDevice->Release();
        m_pD3DDevice = nullptr;
    }
}

// Configures DXGI Factory and retrieve an adapter.
void DeviceResources::InitializeDXGIAdapter()
{
    bool bDebugDXGI = false;

#ifdef _DEBUG
    // Enable the debug layer (requires the Graphics Tools "optional feature").
    // NOTE: Enabling the debug layer after device creation will invalidate the active device.
    ID3D12Debug* pDebugController = nullptr;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&pDebugController))))
    {
        pDebugController->EnableDebugLayer();
        pDebugController->Release();
        pDebugController = nullptr;
    }
    else
    {
        OutputDebugStringA("WARNING: Direct3D Debug Device is not available\n");
    }

    IDXGIInfoQueue* pDXGIInfoQueue = nullptr;
    if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&pDXGIInfoQueue))))
    {
        bDebugDXGI = true;

        ThrowIfFailed(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&m_pDXGIFactory)));

        pDXGIInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true);
        pDXGIInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true);

        pDXGIInfoQueue->Release();
        pDXGIInfoQueue = nullptr;
    }
#endif

    if (!bDebugDXGI)
    {
        ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&m_pDXGIFactory)));
    }

    // Determines whether tearing support is available for fullscreen borderless windows.
    if (m_Options & (ALLOW_TEARING | REQUIRE_TEARING_SUPPORT))
    {
        BOOL allowTearing = FALSE;

        IDXGIFactory5* pFactory5 = nullptr;
        HRESULT hr = m_pDXGIFactory->QueryInterface(&pFactory5);
        if (SUCCEEDED(hr))
        {
            hr = pFactory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing));

            pFactory5->Release();
            pFactory5 = nullptr;
        }

        if (FAILED(hr) || !allowTearing)
        {
            OutputDebugStringA("WARNING: Variable refresh rate displays are not supported.\n");
            if (m_Options & REQUIRE_TEARING_SUPPORT)
            {
                ThrowIfFailed(false, L"Error: Sample must be run on an OS with tearing support.\n");
            }
            m_Options &= ~ALLOW_TEARING;
        }
    }

    InitializeAdapter(&m_pAdapter);
}

// Configures the Direct3D device, and stores handles to it and the device context.
void DeviceResources::CreateDeviceResources()
{
    // Create the DX12 API device object.
    ThrowIfFailed(D3D12CreateDevice(m_pAdapter, m_D3DMinFeatureLevel, IID_PPV_ARGS(&m_pD3DDevice)));

#ifndef NDEBUG
    // Configure debug device (if active).
    ID3D12InfoQueue* pD3DInfoQueue = nullptr;
    if (SUCCEEDED(m_pD3DDevice->QueryInterface(&pD3DInfoQueue)))
    {
#ifdef _DEBUG
        pD3DInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
        pD3DInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
#endif
        D3D12_MESSAGE_ID hide[] =
        {
            D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,
            D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE
        };
        D3D12_INFO_QUEUE_FILTER filter = {};
        filter.DenyList.NumIDs = ARRAYSIZE(hide);
        filter.DenyList.pIDList = hide;
        pD3DInfoQueue->AddStorageFilterEntries(&filter);

        pD3DInfoQueue->Release();
        pD3DInfoQueue = nullptr;
    }
#endif

    // Determine maximum supported feature level for this device
    const D3D_FEATURE_LEVEL FEATURE_LEVEL[] =
    {
        D3D_FEATURE_LEVEL_12_2,
        D3D_FEATURE_LEVEL_12_1,
        D3D_FEATURE_LEVEL_12_0,
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
    };

    D3D12_FEATURE_DATA_FEATURE_LEVELS featLevels =
    {
        ARRAYSIZE(FEATURE_LEVEL), FEATURE_LEVEL, D3D_FEATURE_LEVEL_11_0
    };

    HRESULT hr = m_pD3DDevice->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, &featLevels, sizeof(featLevels));
    if (SUCCEEDED(hr))
    {
        m_D3DFeatureLevel = featLevels.MaxSupportedFeatureLevel;
    }
    else
    {
        m_D3DFeatureLevel = m_D3DMinFeatureLevel;
    }

    // Create the command queue.
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    ThrowIfFailed(m_pD3DDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_pCommandQueue)));

    // Create descriptor heaps for render target views and depth stencil views.
    D3D12_DESCRIPTOR_HEAP_DESC rtvDescriptorHeapDesc = {};
    rtvDescriptorHeapDesc.NumDescriptors = m_BackBufferCount;
    rtvDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

    ThrowIfFailed(m_pD3DDevice->CreateDescriptorHeap(&rtvDescriptorHeapDesc, IID_PPV_ARGS(&m_pRTVDescriptorHeap)));

    m_RTVDescriptorSize = m_pD3DDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    if (m_DepthBufferFormat != DXGI_FORMAT_UNKNOWN)
    {
        D3D12_DESCRIPTOR_HEAP_DESC dsvDescriptorHeapDesc = {};
        dsvDescriptorHeapDesc.NumDescriptors = 1;
        dsvDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;

        ThrowIfFailed(m_pD3DDevice->CreateDescriptorHeap(&dsvDescriptorHeapDesc, IID_PPV_ARGS(&m_pDSVDescriptorHeap)));
    }

    // Create a command allocator for each back buffer that will be rendered to.
    for (UINT n = 0; n < m_BackBufferCount; ++n)
    {
        ThrowIfFailed(m_pD3DDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_ppCommandAllocators[n])));
    }

    // Create a command list for recording graphics commands.
    ThrowIfFailed(m_pD3DDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_ppCommandAllocators[0], nullptr, IID_PPV_ARGS(&m_pCommandList)));
    ThrowIfFailed(m_pCommandList->Close());

    // Create a fence for tracking GPU execution progress.
    ThrowIfFailed(m_pD3DDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_pFence)));
    //m_fenceValues[m_backBufferIndex]++;

    m_hFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!m_hFenceEvent || m_hFenceEvent == INVALID_HANDLE_VALUE)
    {
        ThrowIfFailed(E_FAIL, L"CreateEvent failed.\n");
    }
}

// These resources need to be recreated every time the window size is changed.
void DeviceResources::CreateWindowSizeDependentResources()
{
    if (!m_hwndWindow)
    {
        ThrowIfFailed(E_HANDLE, L"Call SetWindow with a valid Win32 window handle.\n");
    }

    // Wait until all previous GPU work is complete.
    WaitForGPU();

    // Release resources that are tied to the swap chain and update fence values.
    for (UINT n = 0; n < m_BackBufferCount; ++n)
    {
        if (m_ppRenderTargets[n])
        {
            m_ppRenderTargets[n]->Release();
            m_ppRenderTargets[n] = nullptr;
        }
        m_FenceValues[n] = 0;
    }

    // Determine the render target size in pixels.
    UINT backBufferWidth = max(m_OutputSize.right - m_OutputSize.left, 1);
    UINT backBufferHeight = max(m_OutputSize.bottom - m_OutputSize.top, 1);
    DXGI_FORMAT backBufferFormat = NoSRGB(m_BackBufferFormat);

    // If the swap chain already exists, resize it, otherwise create one.
    if (m_pSwapChain)
    {
        // If the swap chain already exists, resize it.
        HRESULT hr = m_pSwapChain->ResizeBuffers(
            m_BackBufferCount,
            backBufferWidth,
            backBufferHeight,
            backBufferFormat,
            (m_Options & ALLOW_TEARING) ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0
        );

        if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
        {
#ifdef _DEBUG
            char buff[64];
            sprintf_s(buff, "Device Lost on ResizeBuffers: Reason code 0x%08X\n", (hr == DXGI_ERROR_DEVICE_REMOVED) ? m_pD3DDevice->GetDeviceRemovedReason() : hr);
            OutputDebugStringA(buff);
#endif
            // If the device was removed for any reason, a new device and swap chain will need to be created.
            HandleDeviceLost();

            // Everything is set up now. Do not continue execution of this method. HandleDeviceLost will reenter this method 
            // and correctly set up the new device.
            return;
        }
        else
        {
            ThrowIfFailed(hr);
        }
    }
    else
    {
        // Create a descriptor for the swap chain.
        DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
        swapChainDesc.Width = backBufferWidth;
        swapChainDesc.Height = backBufferHeight;
        swapChainDesc.Format = backBufferFormat;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.BufferCount = m_BackBufferCount;
        swapChainDesc.SampleDesc.Count = 1;
        swapChainDesc.SampleDesc.Quality = 0;
        swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
        swapChainDesc.Flags = (m_Options & ALLOW_TEARING) ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

        DXGI_SWAP_CHAIN_FULLSCREEN_DESC fsSwapChainDesc = { 0 };
        fsSwapChainDesc.Windowed = TRUE;

        // Create a swap chain for the window.
        IDXGISwapChain1* pSwapChain = nullptr;;

        // DXGI does not allow creating a swapchain targeting a window which has fullscreen styles(no border + topmost).
        // Temporarily remove the topmost property for creating the swapchain.
        bool bPrevIsFullscreen = Win32Application::IsFullscreen();
        if (bPrevIsFullscreen)
        {
            Win32Application::SetWindowZorderToTopMost(false);
        }

        ThrowIfFailed(m_pDXGIFactory->CreateSwapChainForHwnd(m_pCommandQueue, m_hwndWindow, &swapChainDesc, &fsSwapChainDesc, nullptr, &pSwapChain));

        if (bPrevIsFullscreen)
        {
            Win32Application::SetWindowZorderToTopMost(true);
        }

        ThrowIfFailed(pSwapChain->QueryInterface(IID_PPV_ARGS(&m_pSwapChain)));
        pSwapChain->Release();
        pSwapChain = nullptr;

        // With tearing support enabled we will handle ALT+Enter key presses in the
        // window message loop rather than let DXGI handle it by calling SetFullscreenState.
        if (IsTearingSupported())
        {
            m_pDXGIFactory->MakeWindowAssociation(m_hwndWindow, DXGI_MWA_NO_ALT_ENTER);
        }
    }

    // Obtain the back buffers for this window which will be the final render targets
    // and create render target views for each of them.
    for (UINT n = 0; n < m_BackBufferCount; ++n)
    {
        ThrowIfFailed(m_pSwapChain->GetBuffer(n, IID_PPV_ARGS(&m_ppRenderTargets[n])));

        WCHAR name[25];
        swprintf_s(name, L"Render target %u", n);
        m_ppRenderTargets[n]->SetName(name);

        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = m_BackBufferFormat;
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvDescriptor(m_pRTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), n, m_RTVDescriptorSize);
        m_pD3DDevice->CreateRenderTargetView(m_ppRenderTargets[n], &rtvDesc, rtvDescriptor);
    }

    // Reset the index to the current back buffer.
    m_BackBufferIndex = m_pSwapChain->GetCurrentBackBufferIndex();

    if (m_DepthBufferFormat != DXGI_FORMAT_UNKNOWN)
    {
        // Allocate a 2-D surface as the depth/stencil buffer and create a depth/stencil view
        // on this surface.
        CD3DX12_HEAP_PROPERTIES depthHeapProperties(D3D12_HEAP_TYPE_DEFAULT);

        D3D12_RESOURCE_DESC depthStencilDesc = CD3DX12_RESOURCE_DESC::Tex2D(
            m_DepthBufferFormat,
            backBufferWidth,
            backBufferHeight,
            1, // This depth stencil view has only one texture.
            1  // Use a single mipmap level.
        );
        depthStencilDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
        depthOptimizedClearValue.Format = m_DepthBufferFormat;
        depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
        depthOptimizedClearValue.DepthStencil.Stencil = 0;

        ThrowIfFailed(m_pD3DDevice->CreateCommittedResource(&depthHeapProperties,
                                                           D3D12_HEAP_FLAG_NONE,
                                                           &depthStencilDesc,
                                                           D3D12_RESOURCE_STATE_DEPTH_WRITE,
                                                           &depthOptimizedClearValue,
                                                           IID_PPV_ARGS(&m_pDepthStencil)));

        m_pDepthStencil->SetName(L"Depth stencil");

        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = m_DepthBufferFormat;
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

        m_pD3DDevice->CreateDepthStencilView(m_pDepthStencil, &dsvDesc, m_pDSVDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
    }

    // Set the 3D rendering viewport and scissor rectangle to target the entire window.
    m_ScreenViewport.TopLeftX = m_ScreenViewport.TopLeftY = 0.0f;
    m_ScreenViewport.Width = (float)backBufferWidth;
    m_ScreenViewport.Height = (float)backBufferHeight;
    m_ScreenViewport.MinDepth = D3D12_MIN_DEPTH;
    m_ScreenViewport.MaxDepth = D3D12_MAX_DEPTH;

    m_ScissorRect.left = m_ScissorRect.top = 0;
    m_ScissorRect.right = backBufferWidth;
    m_ScissorRect.bottom = backBufferHeight;
}

// This method is called when the Win32 window is created (or re-created).
void DeviceResources::SetWindow(HWND hwndWindow, int width, int height)
{
    m_hwndWindow = hwndWindow;

    m_OutputSize.left = m_OutputSize.top = 0;
    m_OutputSize.right = width;
    m_OutputSize.bottom = height;
}

// This method is called when the Win32 window changes size.
// It returns true if window size change was applied.
bool DeviceResources::WindowSizeChanged(int width, int height, bool bMinimized)
{
    m_bIsWindowVisible = !bMinimized;

    if (bMinimized || width == 0 || height == 0)
    {
        return false;
    }

    RECT newRc;
    newRc.left = newRc.top = 0;
    newRc.right = width;
    newRc.bottom = height;
    if (newRc.left == m_OutputSize.left &&
        newRc.top == m_OutputSize.top &&
        newRc.right == m_OutputSize.right &&
        newRc.bottom == m_OutputSize.bottom)
    {
        return false;
    }

    m_OutputSize = newRc;
    CreateWindowSizeDependentResources();
    return true;
}

// Recreate all device resources and set them back to the current state.
void DeviceResources::HandleDeviceLost()
{
    if (m_pDeviceNotify)
    {
        m_pDeviceNotify->OnDeviceLost();
    }

    if (m_hFenceEvent)
    {
        CloseHandle(m_hFenceEvent);
        m_hFenceEvent = nullptr;
    }

    for (UINT n = 0; n < m_BackBufferCount; ++n)
    {
        if (m_ppCommandAllocators[n])
        {
            m_ppCommandAllocators[n]->Release();
            m_ppCommandAllocators[n] = nullptr;
        }
        if (m_ppRenderTargets[n])
        {
            m_ppRenderTargets[n]->Release();
            m_ppRenderTargets[n] = nullptr;
        }
    }

    if (m_pDepthStencil)
    {
        m_pDepthStencil->Release();
        m_pDepthStencil = nullptr;
    }
    if (m_pCommandQueue)
    {
        m_pCommandQueue->Release();
        m_pCommandQueue = nullptr;
    }
    if (m_pCommandList)
    {
        m_pCommandList->Release();
        m_pCommandList = nullptr;
    }
    if (m_pFence)
    {
        m_pFence->Release();
        m_pFence = nullptr;
    }
    if (m_pRTVDescriptorHeap)
    {
        m_pRTVDescriptorHeap->Release();
        m_pRTVDescriptorHeap = nullptr;
    }
    if (m_pDSVDescriptorHeap)
    {
        m_pDSVDescriptorHeap->Release();
        m_pDSVDescriptorHeap = nullptr;
    }
    if (m_pSwapChain)
    {
        m_pSwapChain->Release();
        m_pSwapChain = nullptr;
    }
    if (m_pDXGIFactory)
    {
        m_pDXGIFactory->Release();
        m_pDXGIFactory = nullptr;
    }
    if (m_pAdapter)
    {
        m_pAdapter->Release();
        m_pAdapter = nullptr;
    }
    if (m_pD3DDevice)
    {
        m_pD3DDevice->Release();
        m_pD3DDevice = nullptr;
    }

#ifdef _DEBUG
    {
        IDXGIDebug1* pDXGIDebug = nullptr;
        if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&pDXGIDebug))))
        {
            pDXGIDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_SUMMARY | DXGI_DEBUG_RLO_IGNORE_INTERNAL));

            pDXGIDebug->Release();
            pDXGIDebug = nullptr;
        }
    }
#endif
    InitializeDXGIAdapter();
    CreateDeviceResources();
    CreateWindowSizeDependentResources();

    if (m_pDeviceNotify)
    {
        m_pDeviceNotify->OnDeviceRestored();
    }
}

// Prepare the command list and render target for rendering.
void DeviceResources::Prepare(D3D12_RESOURCE_STATES beforeState)
{
    // Reset command list and allocator.
    ThrowIfFailed(m_ppCommandAllocators[m_BackBufferIndex]->Reset());
    ThrowIfFailed(m_pCommandList->Reset(m_ppCommandAllocators[m_BackBufferIndex], nullptr));

    if (beforeState != D3D12_RESOURCE_STATE_RENDER_TARGET)
    {
        // Transition the render target into the correct state to allow for drawing into it.
        D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_ppRenderTargets[m_BackBufferIndex], beforeState, D3D12_RESOURCE_STATE_RENDER_TARGET);
        m_pCommandList->ResourceBarrier(1, &barrier);
    }
}

// Present the contents of the swap chain to the screen.
void DeviceResources::Present(D3D12_RESOURCE_STATES beforeState)
{
    if (beforeState != D3D12_RESOURCE_STATE_PRESENT)
    {
        // Transition the render target to the state that allows it to be presented to the display.
        D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_ppRenderTargets[m_BackBufferIndex], beforeState, D3D12_RESOURCE_STATE_PRESENT);
        m_pCommandList->ResourceBarrier(1, &barrier);
    }

    ExecuteCommandList();

    HRESULT hr;
    if (m_Options & ALLOW_TEARING)
    {
        // Recommended to always use tearing if supported when using a sync interval of 0.
        // Note this will fail if in true 'fullscreen' mode.
        hr = m_pSwapChain->Present(0, DXGI_PRESENT_ALLOW_TEARING);
    }
    else
    {
        // The first argument instructs DXGI to block until VSync, putting the application
        // to sleep until the next VSync. This ensures we don't waste any cycles rendering
        // frames that will never be displayed to the screen.
        hr = m_pSwapChain->Present(1, 0);
    }

    // If the device was reset we must completely reinitialize the renderer.
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
    {
#ifdef _DEBUG
        char buff[64] = {};
        sprintf_s(buff, "Device Lost on Present: Reason code 0x%08X\n", (hr == DXGI_ERROR_DEVICE_REMOVED) ? m_pD3DDevice->GetDeviceRemovedReason() : hr);
        OutputDebugStringA(buff);
#endif
        HandleDeviceLost();
    }
    else
    {
        ThrowIfFailed(hr);
        MoveToNextFrame();
    }
}

// Send the command list off to the GPU for processing.
void DeviceResources::ExecuteCommandList()
{
    ThrowIfFailed(m_pCommandList->Close());

    ID3D12CommandList* ppCommandLists[] = { m_pCommandList };
    m_pCommandQueue->ExecuteCommandLists(ARRAYSIZE(ppCommandLists), ppCommandLists);
}

void DX::DeviceResources::Fence()
{
    ++m_FenceValues[m_BackBufferIndex];
    if (FAILED(m_pCommandQueue->Signal(m_pFence, m_FenceValues[m_BackBufferIndex])))
    {
        __debugbreak();
    }
}

void DX::DeviceResources::WaitForGPU()
{
    UINT64 lastFenceValue = m_pFence->GetCompletedValue();
    if (lastFenceValue < m_FenceValues[m_BackBufferIndex])
    {
        m_pFence->SetEventOnCompletion(m_FenceValues[m_BackBufferIndex], m_hFenceEvent);
        WaitForSingleObject(m_hFenceEvent, INFINITE);
    }
}

// Prepare to render the next frame.
void DeviceResources::MoveToNextFrame()
{
    Fence();
    WaitForGPU();

    m_BackBufferIndex = m_pSwapChain->GetCurrentBackBufferIndex();
}

// This method acquires the first high-performance hardware adapter that supports Direct3D 12.
// If no such adapter can be found, try WARP. Otherwise throw an exception.
void DeviceResources::InitializeAdapter(IDXGIAdapter1** ppAdapter)
{
    *ppAdapter = nullptr;

    IDXGIAdapter1* pAdapter = nullptr;
    IDXGIFactory6* pFactory6 = nullptr;
    HRESULT hr = m_pDXGIFactory->QueryInterface(&pFactory6);
    if (FAILED(hr))
    {
        throw exception("DXGI 1.6 not supported");
    }
    for (UINT adapterID = 0; DXGI_ERROR_NOT_FOUND != pFactory6->EnumAdapterByGpuPreference(adapterID, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&pAdapter)); ++adapterID)
    {
        if (m_AdapterIDoverride != UINT_MAX && adapterID != m_AdapterIDoverride)
        {
            continue;
        }

        DXGI_ADAPTER_DESC1 desc = {};
        ThrowIfFailed(pAdapter->GetDesc1(&desc));

        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
        {
            // Don't select the Basic Render Driver adapter.
            continue;
        }

        // Check to see if the adapter supports Direct3D 12, but don't create the actual device yet.
        if (SUCCEEDED(D3D12CreateDevice(pAdapter, m_D3DMinFeatureLevel, _uuidof(ID3D12Device), nullptr)))
        {
            m_AdapterID = adapterID;
            wcsncpy_s(m_AdapterDescription, 512, desc.Description, wcslen(desc.Description));
#ifdef _DEBUG
            WCHAR buff[256];
            swprintf_s(buff, L"Direct3D Adapter (%u): VID:%04X, PID:%04X - %ls\n", adapterID, desc.VendorId, desc.DeviceId, desc.Description);
            OutputDebugStringW(buff);
#endif
            break;
        }
    }
    pFactory6->Release();
    pFactory6 = nullptr;

#if !defined(NDEBUG)
    if (!pAdapter && m_AdapterIDoverride == UINT_MAX)
    {
        // Try WARP12 instead
        if (FAILED(m_pDXGIFactory->EnumWarpAdapter(IID_PPV_ARGS(&pAdapter))))
        {
            throw exception("WARP12 not available. Enable the 'Graphics Tools' optional feature");
        }

        OutputDebugStringA("Direct3D Adapter - WARP12\n");
    }
#endif

    if (!pAdapter)
    {
        if (m_AdapterIDoverride != UINT_MAX)
        {
            throw exception("Unavailable adapter requested.");
        }
        else
        {
            throw exception("Unavailable adapter.");
        }
    }

    *ppAdapter = pAdapter;
}