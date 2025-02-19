// ray3DXR.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "ray3DXR.h"

#include "D3D12RaytracingInOneWeekend.h"

_Use_decl_annotations_
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
#ifdef _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	D3D12RaytracingInOneWeekend sample(1280, 720, L"D3D12 Raytracing");
	return Win32Application::Run(&sample, hInstance, nCmdShow);

#ifdef _DEBUG
	_ASSERT(_CrtCheckMemory());
#endif
}
