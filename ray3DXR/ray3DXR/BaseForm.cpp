#include "framework.h"
#include "BaseForm.h"

LRESULT CALLBACK MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
		case WM_CLOSE:
			DestroyWindow(hwnd);
			break;

		case WM_DESTROY:
			PostQuitMessage(0);
			break;

		case WM_KEYDOWN:
			if (wParam == VK_ESCAPE)
			{
				PostQuitMessage(0);
			}
			break;

		default:
			return DefWindowProc(hwnd, msg, wParam, lParam);
	}

	return 0;
}

void BaseForm::Run()
{
	m_Width = 1920;
	m_Height = 1080;

	m_hwnd = CreateWindowHandle(L"Raytracing", m_Width, m_Height);
	if (!m_hwnd || m_hwnd == INVALID_HANDLE_VALUE)
	{
		__debugbreak();
	}

	OnLoad();

	ShowWindow(m_hwnd, SW_SHOWNORMAL);


	// Main loop.

	MSG msg = { 0, };
	while (true)
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
		else
		{
			OnFrameRender();
		}
	}

	OnShutdown();
	DestroyWindow(m_hwnd);
	m_hwnd = nullptr;
}

HWND BaseForm::CreateWindowHandle(const WCHAR* pszWINDOW_TITLE, UINT width, UINT height)
{
	const WCHAR* pszCLASS_NAME = L"RaytracingSample(DXR)";
	DWORD winStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;

	WNDCLASS windowClass = {};
	windowClass.lpfnWndProc = MsgProc;
	windowClass.hInstance = m_hInstance;
	windowClass.lpszClassName = pszCLASS_NAME;
	
	if (!RegisterClass(&windowClass))
	{
		return nullptr;
	}

	RECT rect = { 0, };
	rect.right = (long)width;
	rect.bottom = (long)height;
	AdjustWindowRect(&rect, winStyle, FALSE);

	int windowWidth = rect.right - rect.left;
	int windowHeight = rect.bottom - rect.top;
	HWND hwndOut = nullptr;
	hwndOut = CreateWindowEx(0,
							 pszCLASS_NAME,
							 pszWINDOW_TITLE,
							 winStyle,
							 CW_USEDEFAULT, CW_USEDEFAULT, 
							 windowWidth, windowHeight, 
							 nullptr, 
							 nullptr,
							 m_hInstance,
							 nullptr);
	return hwndOut;
}
