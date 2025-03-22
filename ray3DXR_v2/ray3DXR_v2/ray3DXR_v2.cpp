#include "ray3DXR_v2.h"

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
#ifdef _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	Renderer renderer;
	if (!renderer.Initialize(hInstance, WndProc, 1280, 720))
	{
#ifdef _DEBUG
		__debugbreak();
#endif
		return -1;
	}
	renderer.Run();
	if (!renderer.Cleanup())
	{
#ifdef _DEBUG
		__debugbreak();
#endif
		return -1;
	}

#ifdef _DEBUG
	_ASSERT(_CrtCheckMemory());
#endif

	return 0;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	Renderer* pRenderer = (Renderer*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

	switch (msg)
	{
	case WM_CREATE:
	{
		LPCREATESTRUCT pCreateStruct = (LPCREATESTRUCT)lParam;
		SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pCreateStruct->lpCreateParams);
		break;
	}

	case WM_SIZE:
	{
		if (pRenderer)
		{
			UINT width = (UINT)LOWORD(lParam);
			UINT height = (UINT)HIWORD(lParam);
			pRenderer->ChangeSize(width, height);
		}

		break;
	}

	case WM_PAINT:
	{
		if (pRenderer)
		{
			pRenderer->Update();
			pRenderer->Render();
		}

		break;
	}

	case WM_QUIT:
	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	default:
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}

	return 0;
}
