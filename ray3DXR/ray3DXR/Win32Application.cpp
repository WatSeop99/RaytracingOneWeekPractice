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
#include "Win32Application.h"
#include "DXSampleHelper.h"

HWND Win32Application::ms_hWnd = nullptr;
bool Win32Application::ms_bFullscreenMode = false;
RECT Win32Application::ms_WindowRect;

int Win32Application::Run(DXSample* pSample, HINSTANCE hInstance, int nCmdShow)
{
    try
    {
        // Parse the command line parameters
        int argc;
        WCHAR** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        pSample->ParseCommandLineArgs(argv, argc);
        LocalFree(argv);

        // Initialize the window class.
        WNDCLASSEX windowClass = { 0, };
        windowClass.cbSize = sizeof(WNDCLASSEX);
        windowClass.style = CS_HREDRAW | CS_VREDRAW;
        windowClass.lpfnWndProc = WindowProc;
        windowClass.hInstance = hInstance;
        windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
        windowClass.lpszClassName = L"DXSampleClass";
        RegisterClassEx(&windowClass);

        RECT windowRect = { 0, 0, (long)pSample->GetWidth(), (long)pSample->GetHeight() };
        AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

        // Create the window and store a handle to it.
        ms_hWnd = CreateWindow(
            windowClass.lpszClassName,
            pSample->GetTitle(),
            WINDOW_STYLE,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            windowRect.right - windowRect.left,
            windowRect.bottom - windowRect.top,
            nullptr,        // We have no parent window.
            nullptr,        // We aren't using menus.
            hInstance,
            pSample);

        // Initialize the sample. OnInit is defined in each child-implementation of DXSample.
        pSample->OnInit();

        ShowWindow(ms_hWnd, nCmdShow);

        // Main sample loop.
        MSG msg = {};
        while (msg.message != WM_QUIT)
        {
            // Process any messages in the queue.
            if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }

        pSample->OnDestroy();

        // Return this part of the WM_QUIT message to Windows.
        return (char)msg.wParam;
    }
    catch (std::exception& e)
    {
        OutputDebugString(L"Application hit a problem: ");
        OutputDebugStringA(e.what());
        OutputDebugString(L"\nTerminating.\n");

        pSample->OnDestroy();
        return EXIT_FAILURE;
    }
}

// Convert a styled window into a fullscreen borderless window and back again.
void Win32Application::ToggleFullscreenWindow(IDXGISwapChain* pSwapChain)
{
    if (ms_bFullscreenMode)
    {
        // Restore the window's attributes and size.
        SetWindowLong(ms_hWnd, GWL_STYLE, WINDOW_STYLE);

        SetWindowPos(
            ms_hWnd,
            HWND_NOTOPMOST,
            ms_WindowRect.left,
            ms_WindowRect.top,
            ms_WindowRect.right - ms_WindowRect.left,
            ms_WindowRect.bottom - ms_WindowRect.top,
            SWP_FRAMECHANGED | SWP_NOACTIVATE);

        ShowWindow(ms_hWnd, SW_NORMAL);
    }
    else
    {
        // Save the old window rect so we can restore it when exiting fullscreen mode.
        GetWindowRect(ms_hWnd, &ms_WindowRect);

        // Make the window borderless so that the client area can fill the screen.
        SetWindowLong(ms_hWnd, GWL_STYLE, WINDOW_STYLE & ~(WS_CAPTION | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SYSMENU | WS_THICKFRAME));

        RECT fullscreenWindowRect;
        try
        {
            if (pSwapChain)
            {
                // Get the settings of the display on which the app's window is currently displayed
                IDXGIOutput* pOutput = nullptr;
                ThrowIfFailed(pSwapChain->GetContainingOutput(&pOutput));

                DXGI_OUTPUT_DESC Desc = {};
                ThrowIfFailed(pOutput->GetDesc(&Desc));
                fullscreenWindowRect = Desc.DesktopCoordinates;

                pOutput->Release();
                pOutput = nullptr;
            }
            else
            {
                // Fallback to EnumDisplaySettings implementation
                throw HrException(S_FALSE);
            }
        }
        catch (HrException& e)
        {
            UNREFERENCED_PARAMETER(e);

            // Get the settings of the primary display
            DEVMODE devMode = {};
            devMode.dmSize = sizeof(DEVMODE);
            EnumDisplaySettings(nullptr, ENUM_CURRENT_SETTINGS, &devMode);

            fullscreenWindowRect = {
                devMode.dmPosition.x,
                devMode.dmPosition.y,
                devMode.dmPosition.x + (long)devMode.dmPelsWidth,
                devMode.dmPosition.y + (long)devMode.dmPelsHeight
            };
        }

        SetWindowPos(
            ms_hWnd,
            HWND_TOPMOST,
            fullscreenWindowRect.left,
            fullscreenWindowRect.top,
            fullscreenWindowRect.right,
            fullscreenWindowRect.bottom,
            SWP_FRAMECHANGED | SWP_NOACTIVATE);


        ShowWindow(ms_hWnd, SW_MAXIMIZE);
    }

    ms_bFullscreenMode = !ms_bFullscreenMode;
}

void Win32Application::SetWindowZorderToTopMost(bool setToTopMost)
{
    RECT windowRect = {};
    GetWindowRect(ms_hWnd, &windowRect);

	SetWindowPos(ms_hWnd,
				 (setToTopMost ? HWND_TOPMOST : HWND_NOTOPMOST),
				 windowRect.left,
				 windowRect.top,
				 windowRect.right - windowRect.left,
				 windowRect.bottom - windowRect.top,
				 SWP_FRAMECHANGED | SWP_NOACTIVATE);
}

// Main message handler for the sample.
LRESULT CALLBACK Win32Application::WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    DXSample* pSample = (DXSample*)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    switch (message)
    {
        case WM_CREATE:
        {
            // Save the DXSample* passed in to CreateWindow.
            LPCREATESTRUCT pCreateStruct = (LPCREATESTRUCT)lParam;
            SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)pCreateStruct->lpCreateParams);
            return 0;
        }

        case WM_KEYDOWN:
            if (pSample)
            {
                pSample->OnKeyDown((UINT8)wParam);
            }
            return 0;

        case WM_KEYUP:
            if (pSample)
            {
                pSample->OnKeyUp((UINT8)wParam);
            }
            return 0;

        case WM_SYSKEYDOWN:
            // Handle ALT+ENTER:
            if ((wParam == VK_RETURN) && (lParam & (1 << 29)))
            {
                if (pSample && pSample->GetDeviceResources()->IsTearingSupported())
                {
                    ToggleFullscreenWindow(pSample->GetSwapchain());
                    return 0;
                }
            }
            // Send all other WM_SYSKEYDOWN messages to the default WndProc.
            break;

        case WM_PAINT:
            if (pSample)
            {
                pSample->OnUpdate();
                pSample->OnRender();
            }
            return 0;

        case WM_SIZE:
            if (pSample)
            {
                RECT windowRect = {};
                GetWindowRect(hWnd, &windowRect);
                pSample->SetWindowBounds(windowRect.left, windowRect.top, windowRect.right, windowRect.bottom);

                RECT clientRect = {};
                GetClientRect(hWnd, &clientRect);
                pSample->OnSizeChanged(clientRect.right - clientRect.left, clientRect.bottom - clientRect.top, wParam == SIZE_MINIMIZED);
            }
            return 0;

        case WM_MOVE:
            if (pSample)
            {
                RECT windowRect = {};
                GetWindowRect(hWnd, &windowRect);
                pSample->SetWindowBounds(windowRect.left, windowRect.top, windowRect.right, windowRect.bottom);

                int xPos = (int)(short)LOWORD(lParam);
                int yPos = (int)(short)HIWORD(lParam);
                pSample->OnWindowMoved(xPos, yPos);
            }
            return 0;

        case WM_DISPLAYCHANGE:
            if (pSample)
            {
                pSample->OnDisplayChanged();
            }
            return 0;

        case WM_MOUSEMOVE:
            if (pSample && (UINT8)wParam == MK_LBUTTON)
            {
                UINT x = LOWORD(lParam);
                UINT y = HIWORD(lParam);
                pSample->OnMouseMove(x, y);
            }
            return 0;

        case WM_LBUTTONDOWN:
        {
            UINT x = LOWORD(lParam);
            UINT y = HIWORD(lParam);
            pSample->OnLeftButtonDown(x, y);

            return 0;
        }

        case WM_LBUTTONUP:
        {
            UINT x = LOWORD(lParam);
            UINT y = HIWORD(lParam);
            pSample->OnLeftButtonUp(x, y);

            return 0;
        }

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }

    // Handle any messages the switch statement didn't.
    return DefWindowProc(hWnd, message, wParam, lParam);
}