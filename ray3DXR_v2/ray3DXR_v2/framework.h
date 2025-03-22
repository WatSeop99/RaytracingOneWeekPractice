#pragma once

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")

#include "targetver.h"
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files
#include <Windows.h>

// DirectX Header Files
#include <dxgi1_6.h>
#include <d3d12.h>
#include <DirectXMath.h>
#include "d3dx12.h"
#ifdef _DEBUG
#include <dxgidebug.h>
#endif

// C RunTime Header Files
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <crtdbg.h>

// C++ RunTime Header Files
#include <sstream>
#include <iomanip>
#include <string>
#include <vector>
#include <unordered_map>

#define SAFE_COM_RELEASE(x) if (x) { (x)->Release(); (x) = nullptr; }

#ifdef _DEBUG
#define BREAK_IF_FAILED(hr) if (FAILED(hr)) __debugbreak()
//#define new DEBUG_NEW
#else
#define BREAK_IF_FAILED(hr) 
#endif
