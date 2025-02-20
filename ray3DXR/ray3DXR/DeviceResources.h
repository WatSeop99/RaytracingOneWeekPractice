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

//
// DeviceResources.h - A wrapper for the Direct3D 12 device and swapchain
//

#pragma once

namespace DX
{
    // Provides an interface for an application that owns DeviceResources to be notified of the device being lost or created.
    interface IDeviceNotify
    {
        virtual void OnDeviceLost() = 0;
        virtual void OnDeviceRestored() = 0;
    };

    // Controls all the DirectX device resources.
    class DeviceResources
    {
    public:
        static const UINT ALLOW_TEARING = 0x1;
        static const UINT REQUIRE_TEARING_SUPPORT = 0x2;

    private:
        static const SIZE_T MAX_BACK_BUFFER_COUNT = 3;

    public:
        DeviceResources() = delete;
        DeviceResources(DXGI_FORMAT backBufferFormat = DXGI_FORMAT_B8G8R8A8_UNORM,
            DXGI_FORMAT depthBufferFormat = DXGI_FORMAT_D32_FLOAT,
            UINT backBufferCount = 2,
            D3D_FEATURE_LEVEL minFeatureLevel = D3D_FEATURE_LEVEL_11_0,
            UINT flags = 0,
            UINT adapterIDoverride = UINT_MAX);
        ~DeviceResources();

        void InitializeDXGIAdapter();
        inline void SetAdapterOverride(UINT adapterID) { m_AdapterIDoverride = adapterID; }
        void CreateDeviceResources();
        void CreateWindowSizeDependentResources();
        void SetWindow(HWND hwndWindow, int width, int height);
        bool WindowSizeChanged(int width, int height, bool bMinimized);
        void HandleDeviceLost();
        void RegisterDeviceNotify(IDeviceNotify* pDeviceNotify)
        {
            m_pDeviceNotify = pDeviceNotify;

            // On RS4 and higher, applications that handle device removal 
            // should declare themselves as being able to do so
            __if_exists(DXGIDeclareAdapterRemovalSupport)
            {
                if (pDeviceNotify)
                {
                    if (FAILED(DXGIDeclareAdapterRemovalSupport()))
                    {
                        OutputDebugString(L"Warning: application failed to declare adapter removal support.\n");
                    }
                }
            }
        }

        void Prepare(D3D12_RESOURCE_STATES beforeState = D3D12_RESOURCE_STATE_PRESENT);
        void Present(D3D12_RESOURCE_STATES beforeState = D3D12_RESOURCE_STATE_RENDER_TARGET);
        void ExecuteCommandList();
        void Fence();
        void WaitForGPU();

        // Device Accessors.
        inline RECT GetOutputSize() const { return m_OutputSize; }
        inline bool IsWindowVisible() const { return m_bIsWindowVisible; }
        inline bool IsTearingSupported() const { return m_Options & ALLOW_TEARING; }

        // Direct3D Accessors.
        inline IDXGIAdapter1* GetAdapter() const { return m_pAdapter; }
        inline ID3D12Device* GetD3DDevice() const { return m_pD3DDevice; }
        inline IDXGIFactory4* GetDXGIFactory() const { return m_pDXGIFactory; }
        inline IDXGISwapChain3* GetSwapChain() const { return m_pSwapChain; }
        inline D3D_FEATURE_LEVEL GetDeviceFeatureLevel() const { return m_D3DFeatureLevel; }
        inline ID3D12Resource* GetRenderTarget() const { return m_ppRenderTargets[m_BackBufferIndex]; }
        inline ID3D12Resource* GetDepthStencil() const { return m_pDepthStencil; }
        inline ID3D12CommandQueue* GetCommandQueue() const { return m_pCommandQueue; }
        inline ID3D12CommandAllocator* GetCommandAllocator() const { return m_ppCommandAllocators[m_BackBufferIndex]; }
        inline ID3D12GraphicsCommandList* GetCommandList() const { return m_pCommandList; }
        inline DXGI_FORMAT GetBackBufferFormat() const { return m_BackBufferFormat; }
        inline DXGI_FORMAT GetDepthBufferFormat() const { return m_DepthBufferFormat; }
        inline D3D12_VIEWPORT GetScreenViewport() const { return m_ScreenViewport; }
        inline D3D12_RECT GetScissorRect() const { return m_ScissorRect; }
        inline UINT GetCurrentFrameIndex() const { return m_BackBufferIndex; }
        inline UINT GetPreviousFrameIndex() const { return m_BackBufferIndex == 0 ? m_BackBufferCount - 1 : m_BackBufferIndex - 1; }
        inline UINT GetBackBufferCount() const { return m_BackBufferCount; }
        inline UINT GetDeviceOptions() const { return m_Options; }
        inline const WCHAR* GetAdapterDescription() const { return m_AdapterDescription.c_str(); }
        inline UINT GetAdapterID() const { return m_AdapterID; }

        inline CD3DX12_CPU_DESCRIPTOR_HANDLE GetRenderTargetView() const
        {
            return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_pRTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), m_BackBufferIndex, m_RTVDescriptorSize);
        }
        inline CD3DX12_CPU_DESCRIPTOR_HANDLE GetDepthStencilView() const
        {
            return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_pDSVDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
        }

    private:
        void MoveToNextFrame();
        void InitializeAdapter(IDXGIAdapter1** ppAdapter);

    private:
        UINT m_AdapterIDoverride;
        UINT m_BackBufferIndex = 0;
        IDXGIAdapter1* m_pAdapter = nullptr;
        UINT m_AdapterID = UINT_MAX;
        std::wstring m_AdapterDescription;

        // Direct3D objects.
        ID3D12Device* m_pD3DDevice = nullptr;
        ID3D12CommandQueue* m_pCommandQueue = nullptr;
        ID3D12GraphicsCommandList* m_pCommandList = nullptr;
        ID3D12CommandAllocator* m_ppCommandAllocators[MAX_BACK_BUFFER_COUNT] = { nullptr, };

        // Swap chain objects.
        IDXGIFactory4* m_pDXGIFactory = nullptr;
        IDXGISwapChain3* m_pSwapChain = nullptr;
        ID3D12Resource* m_ppRenderTargets[MAX_BACK_BUFFER_COUNT] = { nullptr, };
        ID3D12Resource* m_pDepthStencil = nullptr;

        // Presentation fence objects.
        ID3D12Fence* m_pFence = nullptr;
        UINT64 m_FenceValues[MAX_BACK_BUFFER_COUNT] = { 0, };
        HANDLE m_hFenceEvent = nullptr;

        // Direct3D rendering objects.
        ID3D12DescriptorHeap* m_pRTVDescriptorHeap = nullptr;
        ID3D12DescriptorHeap* m_pDSVDescriptorHeap = nullptr;
        UINT m_RTVDescriptorSize = 0;
        D3D12_VIEWPORT m_ScreenViewport = {};
        D3D12_RECT m_ScissorRect = {};

        // Direct3D properties.
        DXGI_FORMAT m_BackBufferFormat;
        DXGI_FORMAT m_DepthBufferFormat;
        UINT m_BackBufferCount;
        D3D_FEATURE_LEVEL m_D3DMinFeatureLevel;

        // Cached device properties.
        HWND m_hwndWindow = nullptr;
        D3D_FEATURE_LEVEL m_D3DFeatureLevel = D3D_FEATURE_LEVEL_11_0;
        RECT m_OutputSize = { 0, 0, 1, 1 };
        bool m_bIsWindowVisible = true;

        // DeviceResources options (see flags above)
        UINT m_Options;

        // The IDeviceNotify can be held directly as it owns the DeviceResources.
        IDeviceNotify* m_pDeviceNotify = nullptr;
    };
}
