// header.h : include file for standard system include files,
// or project specific include files
//

#pragma once

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")

#include "targetver.h"
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files
#include <Windows.h>
#include <wrl.h>
#include <shellapi.h>

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
