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
		BREAK_IF_FALSE(false);
		return -1;
	}
	renderer.Run();
	if (!renderer.Cleanup())
	{
		BREAK_IF_FALSE(false);
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

	case WM_MOUSEMOVE:
	{
		if (pRenderer)
		{
			pRenderer->ProcessMosuseMove(LOWORD(lParam), HIWORD(lParam));
		}

		break;
	}

	case WM_LBUTTONDOWN:
	{
		if (pRenderer)
		{
			pRenderer->SetMouseLButton(true);
		}

		break;
	}

	case WM_LBUTTONUP:
	{
		if (pRenderer)
		{
			pRenderer->SetMouseLButton(false);
		}

		break;
	}

	case WM_RBUTTONDOWN:
	{
		if (pRenderer)
		{
			pRenderer->SetMouseRButton(true);
		}

		break;
	}

	case WM_RBUTTONUP:
	{
		if (pRenderer)
		{
			pRenderer->SetMouseRButton(false);
		}

		break;
	}

	case WM_KEYDOWN:
	{
		if (pRenderer)
		{
			if (wParam == VK_ESCAPE)
			{
				PostQuitMessage(0);
				break;
			}
			pRenderer->SetKeyboardKey(wParam, true);
		}

		break;
	}

	case WM_KEYUP:
	{
		if (pRenderer)
		{
			pRenderer->SetKeyboardKey(wParam, false);
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
