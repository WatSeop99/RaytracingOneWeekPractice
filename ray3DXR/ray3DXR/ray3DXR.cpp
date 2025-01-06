#include "framework.h"
#include "ray3DXR.h"

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
#ifdef _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	Application app(hInstance);
	app.Run();

#ifdef _DEBUG
	_CrtCheckMemory();
#endif

	return 0;
}