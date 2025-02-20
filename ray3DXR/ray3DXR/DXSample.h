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

#include "DXSampleHelper.h"
#include "Win32Application.h"
#include "DeviceResources.h"

class DXSample : public DX::IDeviceNotify
{
public:
    DXSample(UINT width, UINT height, const WCHAR* pszName);
    virtual ~DXSample();

    virtual void OnInit() = 0;
    virtual void OnUpdate() = 0;
    virtual void OnRender() = 0;
    virtual void OnSizeChanged(UINT width, UINT height, bool minimized) = 0;
    virtual void OnDestroy() = 0;

    // Samples override the event handlers to handle specific messages.
    virtual void OnKeyDown(UINT8 /*key*/) {}
    virtual void OnKeyUp(UINT8 /*key*/) {}
    virtual void OnWindowMoved(int /*x*/, int /*y*/) {}
    virtual void OnMouseMove(UINT /*x*/, UINT /*y*/) {}
    virtual void OnLeftButtonDown(UINT /*x*/, UINT /*y*/) {}
    virtual void OnLeftButtonUp(UINT /*x*/, UINT /*y*/) {}
    virtual void OnDisplayChanged() {}

    // Overridable members.
    virtual void ParseCommandLineArgs(_In_reads_(argc) WCHAR* argv[], int argc);

    // Accessors.
    inline UINT GetWidth() const { return m_Width; }
    inline UINT GetHeight() const { return m_Height; }
    inline const WCHAR* GetTitle() const { return m_szTitle; }
    inline RECT GetWindowsBounds() const { return m_WindowBounds; }
    inline virtual IDXGISwapChain* GetSwapchain() { return nullptr; }
    inline const DX::DeviceResources* GetDeviceResources() const { return m_pDeviceResources; }

    void UpdateForSizeChange(UINT clientWidth, UINT clientHeight);
    void SetWindowBounds(int left, int top, int right, int bottom);
    const WCHAR* GetAssetFullPath(const WCHAR* pszAssetName);

protected:
    void SetCustomWindowText(const WCHAR* pszText);

protected:
    // Viewport dimensions.
    UINT m_Width;
    UINT m_Height;
    float m_AspectRatio = 0.0f;

    // Window bounds
    RECT m_WindowBounds = { 0,0,0,0 };

    // Override to be able to start without Dx11on12 UI for PIX. PIX doesn't support 11 on 12. 
    bool m_bEnableUI = true;

    // D3D device resources
    UINT m_AdapterIDoverride = UINT_MAX;
    DX::DeviceResources* m_pDeviceResources = nullptr;

private:
    // Root assets path.
    WCHAR m_szAssetsPath[512];

    // Window title.
    WCHAR m_szTitle[512];
};
