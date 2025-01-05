#pragma once

typedef HRESULT(__stdcall* DxcCreateInstanceProc)(REFCLSID rclsid, REFIID riid, LPVOID* ppv);
typedef HRSEULT(__stdcall* DxcCreateInstance2Proc)(IMalloc* pMalloc, REFCLSID rclsid, REFIID riid, LPVOID* ppv);

class DxcDLLSupport
{
public:
	DxcDLLSupport() = default;
	DxcDLLSupport(DxcDLLSupport&& other)
	{
		m_hDLL = other.m_hDLL;
		other.m_hDLL = nullptr;

		m_pfnCreateFn = other.m_pfnCreateFn;
		other.m_pfnCreateFn = nullptr;

		m_pfnCreateFn2 = other.m_pfnCreateFn2;
		other.m_pfnCreateFn2 = nullptr;
	}
	~DxcDLLSupport() { Cleanup(); }

	HRESULT Initialize();
	HRESULT InitializeForDll(const WCHAR* pszDLL, const char* pszENTRY_POINT);

	template <typename TInterface>
	HRESULT CreateInstance(REFCLSID clsid, TInterface** ppResult)
	{
		return CreateInstance(clsid, __uuidof(TInterface), (IUnknown**)ppResult);
	}
	HRESULT CreateInstance(REFCLSID clsid, REFIID riid, IUnknown** ppResult);

	template <typename TInterface>
	HRESULT CreateInstance2(IMalloc* pMalloc, REFCLSID clsid, TInterface** ppResult)
	{
		return CreateInstance2(pMalloc, clsid, __uuidof(TInterface), (IUnknown**)ppResult);
	}
	HRESULT CreateInstance2(IMalloc* pMalloc, REFCLSID clsid, REFIID riid, IUnknown** ppResult);

	inline bool HasCreateWithMalloc() const { return (m_pfnCreateFn2 != nullptr); }
	inline bool IsEnabled() const { return (m_hDLL != nullptr); }

	void Cleanup();

	HMODULE Detach();

protected:
	HRESULT InitializeInternal(const WCHAR* pszDLL_NAME, const char* pszFN_NAME);

protected:
	HMODULE m_hDLL = nullptr;
	DxcCreateInstanceProc m_pfnCreateFn = nullptr;
	DxcCreateInstance2Proc m_pfnCreateFn2 = nullptr;
};
