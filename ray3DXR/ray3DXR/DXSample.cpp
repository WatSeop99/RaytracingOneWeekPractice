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
#include "DXSample.h"

using namespace Microsoft::WRL;
using namespace std;

DXSample::DXSample(UINT width, UINT height, const WCHAR* pszName) :
    m_Width(width),
    m_Height(height)
{
    WCHAR assetsPath[512];
    GetAssetsPath(assetsPath, _countof(assetsPath));
    wcsncpy_s(m_szAssetsPath, 512, assetsPath, wcslen(assetsPath));

    wcsncpy_s(m_szTitle, 512, pszName, wcslen(pszName));

    UpdateForSizeChange(width, height);
}

DXSample::~DXSample()
{
    if (m_pDeviceResources)
    {
        delete m_pDeviceResources;
        m_pDeviceResources = nullptr;
    }
}

void DXSample::UpdateForSizeChange(UINT clientWidth, UINT clientHeight)
{
    m_Width = clientWidth;
    m_Height = clientHeight;
    m_AspectRatio = (float)clientWidth / (float)clientHeight;
}

// Helper function for resolving the full path of assets.
const WCHAR* DXSample::GetAssetFullPath(const WCHAR* pszAssetName)
{
    wcscat_s(m_szAssetsPath, 512, pszAssetName);
    return m_szAssetsPath;
}


// Helper function for setting the window's title text.
void DXSample::SetCustomWindowText(const WCHAR* pszText)
{
    WCHAR windowText[1024];
    swprintf_s(windowText, 1024, L"%s: %s", m_szTitle, pszText);
    SetWindowText(Win32Application::GetHwnd(), windowText);
}

// Helper function for parsing any supplied command line args.
_Use_decl_annotations_
void DXSample::ParseCommandLineArgs(WCHAR* argv[], int argc)
{
    for (int i = 1; i < argc; ++i)
    {
        // -disableUI
        if (_wcsnicmp(argv[i], L"-disableUI", wcslen(argv[i])) == 0 ||
            _wcsnicmp(argv[i], L"/disableUI", wcslen(argv[i])) == 0)
        {
            m_bEnableUI = false;
        }
        // -forceAdapter [id]
        else if (_wcsnicmp(argv[i], L"-forceAdapter", wcslen(argv[i])) == 0 ||
                 _wcsnicmp(argv[i], L"/forceAdapter", wcslen(argv[i])) == 0)
        {
            ThrowIfFalse(i + 1 < argc, L"Incorrect argument format passed in.");

            m_AdapterIDoverride = _wtoi(argv[i + 1]);
            i++;
        }
    }

}

void DXSample::SetWindowBounds(int left, int top, int right, int bottom)
{
    m_WindowBounds.left = (long)left;
    m_WindowBounds.top = (long)top;
    m_WindowBounds.right = (long)right;
    m_WindowBounds.bottom = (long)bottom;
}