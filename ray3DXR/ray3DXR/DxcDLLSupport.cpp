#include "framework.h"
#include "DxcDLLSupport.h"

HRESULT DxcDLLSupport::Initialize()
{
	return InitializeInternal(L"dxcompiler.dll", "DxcCreateInstance");
}

HRESULT DxcDLLSupport::InitializeForDll(const WCHAR* pszDLL, const char* pszENTRY_POINT)
{
	return InitializeInternal(pszDLL, pszENTRY_POINT);
}

HRESULT DxcDLLSupport::CreateInstance(REFCLSID clsid, REFIID riid, IUnknown** ppResult)
{
	if (!ppResult)
	{
		return E_POINTER;
	}
	if (!m_hDLL)
	{
		return E_FAIL;
	}

	HRESULT hr = m_pfnCreateFn(clsid, riid, (LPVOID*)ppResult);
	return hr;
}

HRESULT DxcDLLSupport::CreateInstance2(IMalloc* pMalloc, REFCLSID clsid, REFIID riid, IUnknown** ppResult)
{
	if (!ppResult)
	{
		return E_POINTER;
	}
	if (!m_hDLL)
	{
		return E_FAIL;
	}
	if (!m_pfnCreateFn2)
	{
		return E_FAIL;
	}

	HRESULT hr = m_pfnCreateFn2(pMalloc, clsid, riid, (LPVOID*)ppResult);
	return hr;
}

void DxcDLLSupport::Cleanup()
{
	if (m_hDLL)
	{
		m_pfnCreateFn = nullptr;
		m_pfnCreateFn2 = nullptr;
		FreeLibrary(m_hDLL);
		m_hDLL = nullptr;
	}
}

HMODULE DxcDLLSupport::Detach()
{
	HMODULE hModule = m_hDLL;
	m_hDLL = nullptr;
	return hModule;
}

HRESULT DxcDLLSupport::InitializeInternal(const WCHAR* pszDLL_NAME, const char* pszFN_NAME)
{
	_ASSERT(pszDLL_NAME);
	_ASSERT(pszFN_NAME);

	if (m_hDLL)
	{
		return S_OK;
	}

	m_hDLL = LoadLibraryW(pszDLL_NAME);
	if (!m_hDLL)
	{
		return HRESULT_FROM_WIN32(GetLastError());
	}

	m_pfnCreateFn = (DxcCreateInstanceProc)GetProcAddress(m_hDLL, pszFN_NAME);
	if (!m_pfnCreateFn)
	{
		HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
		FreeLibrary(m_hDLL);
		m_hDLL = nullptr;
		return hr;
	}

	m_pfnCreateFn2 = nullptr;
	char szFnName2[128];
	SIZE_T s = strlen(pszFN_NAME);
	if (s < sizeof(szFnName2) - 2)
	{
		memcpy(szFnName2, pszFN_NAME, s);
		szFnName2[s] = '2';
		szFnName2[s + 1] = '\0';
		m_pfnCreateFn2 = (DxcCreateInstance2Proc)GetProcAddress(m_hDLL, szFnName2);
	}

	return S_OK;
}
